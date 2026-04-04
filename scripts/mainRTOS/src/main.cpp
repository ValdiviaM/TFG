#include "DataTypes.h"
#include "FlowSensor.h"
#include "LevelSensor.h"
#include "PressureSensor.h"
#include "ProximitySensor.h"
#include "RFM9xCompat.h"
#include "esp_sntp.h"
#include "logger.h"
#include <PubSubClient.h>
#include <WiFi.h>
// --- CONFIGURACIÓN E INSTANCIAS ---
std::array<String, (size_t)FileRole::COUNT> filePaths = {"/system.log",
                                                         "/data.csv"};
Logger miLogger(14, 2, 15, 13, filePaths);
RFM9xCompat lora;
const char *mqtt_server = "mqtt.thingsboard.cloud";
const char *access_token = "qxhYc2PZRYoLsYtKxiaf";
WiFiClient espClient;
PubSubClient mqtt(espClient);

FlowSensor flow(25);
PressureSensor pressure(MS5837::MS5837_02BA, 21, 22);
ProximitySensor proximity(Serial2, 34, -1);
LevelSensor tankLevel({12, 35, 39, 36});

const uint32_t REPORT_PERIOD_MS = 120000; // 15 segundos
const uint32_t SAMPLING_PERIOD_MS =
    20000; // 50ms para mantener vivo el Serial/Filtro

QueueHandle_t measureQueue;
QueueHandle_t errorQueue;
QueueHandle_t uplinkQueue;
SemaphoreHandle_t spiMutex;

// --- PROTOTIPOS ---
void DataAcquisitionTask(void *pv);
void StorageTask(void *pv);
void UplinkTask(void *pv);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Mutex de SPI
  spiMutex = xSemaphoreCreateMutex();

  // 2. Inicializar SD y Logger (Esto escribe las cabeceras si el archivo es
  // nuevo)
  if (miLogger.begin(spiMutex, 50)) {
    miLogger.log("Sistema Iniciado", Level::INFO);
  }

  mqtt.setServer(mqtt_server, 1883);
  // 4. Sensores
  pressure.begin();
  proximity.begin();
  tankLevel.begin();

  // 5. Crear Colas
  measureQueue = xQueueCreate(30, sizeof(TelemetryData));
  uplinkQueue = xQueueCreate(30, sizeof(TelemetryData));

  // 6. Lanzar Tareas
  xTaskCreatePinnedToCore(DataAcquisitionTask, "Sensors", 4096, NULL, 2, NULL,
                          1);
  xTaskCreatePinnedToCore(StorageTask, "Storage", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(UplinkTask, "Uplink", 8192, NULL, 1, NULL, 0);
}

void loop() { vTaskDelete(NULL); }

// --- IMPLEMENTACIÓN DE TAREAS ---

void DataAcquisitionTask(void *pv) {
  flow.begin();
  TelemetryData data;
  uint32_t lastReportTime = 0;

  for (;;) {
    // 1. HIGH FREQUENCY SAMPLING
    // El sensor de proximidad necesita ser leído constantemente
    proximity.update();

    // 2. LOW FREQUENCY REPORTING
    if (millis() - lastReportTime >= REPORT_PERIOD_MS) {
      lastReportTime = millis();

      data.flow_Lmin = flow.read();
      data.totalLiters = flow.getTotalLiters();
      data.pressure = pressure.read();
      data.temperature = pressure.temperature();
      data.depth = pressure.depth();
      data.proximity_cm = proximity.read();
      data.waterLevel = tankLevel.read();
      data.sentVia = TransMode::PENDING;
      time(&data.timestamp);

      // Solo enviamos a la cola de Uplink.
      // El Uplink se encargará de pasarle el relevo a Storage.
      if (!xQueueSend(uplinkQueue, &data, 0)) {
        miLogger.log("Uplink Queue Full!", Level::WARNING);
      }
    }

    // Delay corto para no saturar CPU pero procesar Serial
    vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS));
  }
}

void UplinkTask(void *pv) {
  TelemetryData uData;
  uint32_t lastWiFiRetry = 0;
  uint32_t lastNTPSync = 0;
  const uint32_t RETRY_INTERVAL = 300000;
  const uint32_t NTP_INTERVAL = 86400000;

  // Helper para mantenimiento
  auto maintainConnectivity = [&]() {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - lastWiFiRetry > RETRY_INTERVAL || lastWiFiRetry == 0) {
        lastWiFiRetry = millis();
        miLogger.log("[Uplink] Conectando WiFi...");
        WiFi.begin("Henry ", "Coque1428");
      }
    } else if (lastNTPSync == 0 || (millis() - lastNTPSync > NTP_INTERVAL)) {
      configTime(-6 * 3600, 0, "pool.ntp.org");
      lastNTPSync = millis();
      miLogger.log("[Uplink] Hora sincronizada.");
    }
  };

  for (;;) {
    // EL TRUCO: Esperar hasta 5 segundos por datos.
    // Si no llegan, el xQueueReceive retorna pdFALSE y el loop continúa,
    // permitiendo que maintainConnectivity() se ejecute.
    if (xQueueReceive(uplinkQueue, &uData, pdMS_TO_TICKS(5000)) == pdTRUE) {

      bool success = false;
      char timeStr[32];
      struct tm tm_info;
      localtime_r(&uData.timestamp, &tm_info);
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

      char payload[160];
      snprintf(payload, sizeof(payload),
               "{\"F\":%.1f,\"T\":%.1f,\"P\":%.2f,\"D\":%.2f,\"DL\":%d,\"Px\":%"
               ".2f,\"TL\":%.2f,\"TS\":\"%s\"}",
               uData.flow_Lmin, uData.temperature, uData.pressure, uData.depth,
               uData.waterLevel, uData.proximity_cm, uData.totalLiters,
               timeStr);

      // 1. Intentar MQTT
      if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) {
          if (mqtt.connect("ESP32Client", access_token, NULL)) {
            miLogger.log("Connected to MQTT");
          } else {
            miLogger.log("Failed to connect MQTT", Level::WARNING);
          }
        }
        if (mqtt.connected() &&
            mqtt.publish("v1/devices/me/telemetry", payload)) {
          uData.sentVia = TransMode::MQTT;
          success = true;
        } else {
          miLogger.log("Failed to send via MQTT", Level::WARNING);
        }
      }

      // 2. Fallback a LoRa
      if (!success) {
        if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
          if (lora.send_msg(payload, 100, true)) {
            uData.sentVia = TransMode::LORA;
            success = true;
          } else {
            miLogger.log("Failed to send via LoRa", Level::WARNING);
          }
          xSemaphoreGive(spiMutex);
        }
      }

      if (!success)
        uData.sentVia = TransMode::FAIL;

      // 3. Notificar a StorageTask que los datos están listos para guardarse
      xQueueSend(measureQueue, &uData, 0);
    }

    // Esto se ejecuta SIEMPRE: ya sea porque llegaron datos o por el timeout de
    // 5s
    maintainConnectivity();
    mqtt.loop(); // Mantener keep-alive de MQTT
  }
}

void StorageTask(void *pv) {
  TelemetryData sData;
  SystemLog lMsg;

  for (;;) {
    // 1. Errores (Polling rápido)
    miLogger.processInternalLogs();
    // 2. Mediciones (Bloqueo total: esta tarea no consume CPU hasta que hay
    // datos)
    if (xQueueReceive(measureQueue, &sData, portMAX_DELAY) == pdTRUE) {
      const char *modeStr = (sData.sentVia == TransMode::MQTT)   ? "MQTT"
                            : (sData.sentVia == TransMode::LORA) ? "LORA"
                                                                 : "FAIL";

      char buffer[150];
      snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f,%.2f,%d,%s",
               sData.flow_Lmin, sData.totalLiters, sData.pressure, sData.depth,
               sData.proximity_cm, sData.waterLevel, modeStr);

      miLogger.logMeasure(buffer, sData.timestamp);
    }
  }
}