#include "DataTypes.h"
#include "FlowSensor.h"
#include "LevelSensor.h"
#include "PressureSensor.h"
#include "ProximitySensor.h"
#include "RFM9xCompat.h"
#include "esp_sntp.h"
#include "logger.h"
#include <HTTPClient.h>
#include <WiFi.h>

// --- CONFIGURACIÓN E INSTANCIAS ---
std::array<String, (size_t)FileRole::COUNT> filePaths = {"/system.log",
                                                         "/data.csv"};

Logger miLogger(14, 2, 15, 13, filePaths);
RFM9xCompat lora;

const char *mqtt_server = "172.21.224.154";
const char *access_token = "xiviFJp7l0Bssksv2GZo";

WiFiClient espClient;

FlowSensor flow(25);
PressureSensor pressure(MS5837::MS5837_02BA, 21, 22);
ProximitySensor proximity(Serial2, 34, -1);
LevelSensor tankLevel({12, 35, 39, 36});

const uint32_t REPORT_PERIOD_MS = 12000;  // 15 segundos
const uint32_t SAMPLING_PERIOD_MS = 2000; // mantener sensores vivos

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

  // 1. Mutex SPI
  spiMutex = xSemaphoreCreateMutex();

  // 2. Logger
  if (miLogger.begin(spiMutex, 50)) {
    miLogger.log("Sistema Iniciado", Level::INFO);
  }

  // 3. Sensores
  pressure.begin();
  proximity.begin();
  tankLevel.begin();

  // 4. Colas
  measureQueue = xQueueCreate(30, sizeof(TelemetryData));
  uplinkQueue = xQueueCreate(30, sizeof(TelemetryData));

  // 5. Tareas
  xTaskCreatePinnedToCore(DataAcquisitionTask, "Sensors", 4096, NULL, 2, NULL,
                          1);
  xTaskCreatePinnedToCore(StorageTask, "Storage", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(UplinkTask, "Uplink", 8192, NULL, 1, NULL, 0);
}

void loop() { vTaskDelete(NULL); }

// --- TASKS ---

void DataAcquisitionTask(void *pv) {
  flow.begin();
  TelemetryData data;
  uint32_t lastReportTime = 0;

  for (;;) {
    // Alta frecuencia
    proximity.update();

    // Baja frecuencia
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

      if (!xQueueSend(uplinkQueue, &data, 0)) {
        miLogger.log("Uplink Queue Full!", Level::WARNING);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS));
  }
}

void UplinkTask(void *pv) {
  TelemetryData uData;
  uint32_t lastWiFiRetry = 0;
  uint32_t lastNTPSync = 0;

  const uint32_t RETRY_INTERVAL = 300000;
  const uint32_t NTP_INTERVAL = 86400000;

  auto maintainConnectivity = [&]() {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - lastWiFiRetry > RETRY_INTERVAL || lastWiFiRetry == 0) {
        lastWiFiRetry = millis();
        miLogger.log("[Uplink] Conectando WiFi...");
        WiFi.begin("estudiantes.ie", "Estudiantes2024");
      }
    } else if (lastNTPSync == 0 || (millis() - lastNTPSync > NTP_INTERVAL)) {
      configTime(-6 * 3600, 0, "pool.ntp.org");
      lastNTPSync = millis();
      miLogger.log("[Uplink] Hora sincronizada.");
    }
  };

  for (;;) {
    if (xQueueReceive(uplinkQueue, &uData, pdMS_TO_TICKS(5000)) == pdTRUE) {

      bool success = false;

      char timeStr[32];
      struct tm tm_info;
      localtime_r(&uData.timestamp, &tm_info);

      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

      char payload[160];
      snprintf(payload, sizeof(payload),
               "{\"F\":%.1f,\"T\":%.1f,\"P\":%.2f,\"D\":%.2f,\"DL\":%d,"
               "\"Px\":%.2f,\"TL\":%.2f,\"TS\":\"%s\"}",
               uData.flow_Lmin, uData.temperature, uData.pressure, uData.depth,
               uData.waterLevel, uData.proximity_cm, uData.totalLiters,
               timeStr);

      // HTTP
      if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;

        String url = "https://thingsboard.nm.35-208-114-233.nip.io/api/v1/";
        url += access_token;
        url += "/telemetry";

        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0 && httpResponseCode == 200) {
          miLogger.log("HTTP telemetry sent");
          uData.sentVia = TransMode::MQTT;
          success = true;
        } else {
          miLogger.log("HTTP send failed", Level::WARNING);
        }

        http.end();
      }

      // LoRa fallback
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

      if (!success) {
        uData.sentVia = TransMode::FAIL;
      }

      xQueueSend(measureQueue, &uData, 0);
    }

    maintainConnectivity();
  }
}

void StorageTask(void *pv) {
  TelemetryData sData;

  for (;;) {
    miLogger.processInternalLogs();

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