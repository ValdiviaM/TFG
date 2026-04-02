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
LevelSensor tankLevel({32, 35, 39, 36});

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
  Serial.println("\n--- INICIANDO SISTEMA ---");

  // 1. Mutex de SPI
  spiMutex = xSemaphoreCreateMutex();

  // 2. Inicializar SD y Logger (Esto escribe las cabeceras si el archivo es
  // nuevo)
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    if (miLogger.begin()) {
      Serial.println("SD: Inicializada y cabeceras verificadas.");
    } else {
      Serial.println("SD: ERROR al montar.");
    }
    lora.begin(1);
    xSemaphoreGive(spiMutex);
  }

  mqtt.setServer(mqtt_server, 1883);
  // 4. Sensores
  pressure.begin();
  proximity.begin();
  tankLevel.begin();

  // 5. Crear Colas
  measureQueue = xQueueCreate(30, sizeof(TelemetryData));
  uplinkQueue = xQueueCreate(30, sizeof(TelemetryData));
  errorQueue = xQueueCreate(30, sizeof(SystemLog));

  // 6. Lanzar Tareas
  xTaskCreatePinnedToCore(DataAcquisitionTask, "Sensors", 4096, NULL, 2, NULL,
                          1);
  xTaskCreatePinnedToCore(StorageTask, "Storage", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(UplinkTask, "Uplink", 8192, NULL, 1, NULL, 0);
}

void loop() { vTaskDelete(NULL); }

// --- IMPLEMENTACIÓN DE TAREAS ---

void DataAcquisitionTask(void *pv) {
  flow.begin(); // Inicializar ISR aquí
  TelemetryData data;
  SystemLog sysLog;
  uint32_t lastRead = 0;
  int period = 15000;
  for (;;) {
    proximity.update();

    if (millis() - lastRead > period) {
      lastRead = millis();

      data.flow_Lmin = flow.read();
      data.totalLiters = flow.getTotalLiters();
      data.pressure = pressure.read();
      data.temperature = pressure.temperature();
      data.depth = pressure.depth();
      data.proximity_cm = proximity.read();
      data.waterLevel = tankLevel.read();
      time(&data.timestamp);

      xQueueSend(measureQueue, &data, 0);
      xQueueSend(uplinkQueue, &data, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void StorageTask(void *pv) {
  TelemetryData sData;
  SystemLog lMsg;

  for (;;) {
    // 1. Procesar Errores
    if (xQueueReceive(errorQueue, &lMsg, 0) == pdTRUE) {
      if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        miLogger.log(lMsg.msg, lMsg.level, FileRole::LOG, lMsg.timestamp);
        xSemaphoreGive(spiMutex);
      }
    }

    // 2. Procesar Mediciones
    if (xQueueReceive(measureQueue, &sData, pdMS_TO_TICKS(100)) == pdTRUE) {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f,%.2f,%d",
               sData.flow_Lmin, sData.totalLiters, sData.pressure, sData.depth,
               sData.proximity_cm, sData.waterLevel);

      if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        miLogger.log(buffer, Level::INFO, FileRole::MEASURE, sData.timestamp);
        xSemaphoreGive(spiMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void UplinkTask(void *pv) {
  TelemetryData uData;
  uint32_t lastWiFiRetry = 0;
  uint32_t lastNTPSync = 0;
  const uint32_t RETRY_INTERVAL =
      300000; // 5 minutos entre intentos de reconexión
  const uint32_t NTP_INTERVAL = 86400000; // 24 horas para re-sincronizar hora
  auto reconnectMQTT = []() {
    while (!mqtt.connected()) {
      mqtt.connect("ESP32Client", access_token, NULL);
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  };
  for (;;) {
    // --- GESTIÓN DE CONEXIÓN ---
    if (WiFi.status() != WL_CONNECTED) {
      // Si no hay WiFi, intentamos reconectar cada 5 minutos

      if (millis() - lastWiFiRetry > RETRY_INTERVAL || lastWiFiRetry == 0) {
        lastWiFiRetry = millis();
        Serial.println("[WiFi] Intentando reconectar...");
        WiFi.disconnect();
        WiFi.begin("Henry ", "Coque1428");
      }
    } else {
      // Si hay WiFi, verificamos si necesitamos sincronizar la hora
      // (Si es la primera vez o pasaron 24h)
      if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED ||
          (millis() - lastNTPSync > NTP_INTERVAL)) {

        Serial.println("[NTP] Sincronizando hora...");
        configTime(-6 * 3600, 0, "pool.ntp.org");
        lastNTPSync = millis();
      }
    }

    // --- PROCESAMIENTO DE DATOS ---
    if (xQueueReceive(uplinkQueue, &uData, pdMS_TO_TICKS(1000)) == pdTRUE) {

      char timeStr[32];
      struct tm tm_info;
      localtime_r(&uData.timestamp, &tm_info);
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

      // 1. Preparar Payload (Mismo formato que la SD para consistencia)
      char payload[160];
      snprintf(payload, sizeof(payload),
               "{\"F\":%.1f,\"T\":%.1f,\"P\":%.2f,\"D\":%.2f,\"DL\":%d,\"Px\":%"
               ".2f,\"TL\":%.2f,\"TS\":\"%s\"}",
               uData.flow_Lmin, uData.temperature, uData.pressure, uData.depth,
               uData.waterLevel, uData.proximity_cm, uData.totalLiters,
               timeStr);

      // 2. Enviar por LoRa (Fallback Siempre Activo)
      if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        lora.send_msg(payload, 100, true);
        xSemaphoreGive(spiMutex);
      }

      // 3. Enviar por WiFi (Solo si está conectado)
      if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) {
          reconnectMQTT();
        }
        mqtt.loop();
        Serial.print("MQTT state: ");
        Serial.println(mqtt.state());
        Serial.println("Intentando publicar");
        Serial.println(mqtt.publish("v1/devices/me/telemetry", payload));
        Serial.println("Publicado");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}