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

// --------------------------------------------------
// CONFIGURACIÓN
// --------------------------------------------------

std::array<String, (size_t)FileRole::COUNT> filePaths = {
    "/system.log", "/data.csv", "/pending.csv"};

Logger miLogger(14, 2, 15, 13, filePaths);

RFM9xCompat lora;

const char *access_token = "xiviFJp7l0Bssksv2GZo";

// --------------------------------------------------
// SENSORES
// --------------------------------------------------

FlowSensor flow(25);

PressureSensor pressure(MS5837::MS5837_02BA, 21, 22);

ProximitySensor proximity(Serial2, 34, -1);

LevelSensor tankLevel({12, 35, 39, 36});

// --------------------------------------------------
// RTOS
// --------------------------------------------------

QueueHandle_t measureQueue;

SemaphoreHandle_t spiMutex;
SemaphoreHandle_t i2cMutex;

volatile uint32_t dynamicReportPeriod = 12000;

// --------------------------------------------------
// PROTOTIPOS
// --------------------------------------------------

void DataAcquisitionTask(void *pv);
void StorageTask(void *pv);
void UplinkTask(void *pv);

// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  Serial.begin(115200);

  delay(1000);

  spiMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  Wire.begin(21, 22);
  Wire.setClock(100000);

  if (miLogger.begin(spiMutex, 50)) {
    miLogger.log("Sistema iniciado", Level::INFO);
  }

  pressure.begin();

  proximity.begin();

  tankLevel.begin();

  measureQueue = xQueueCreate(30, sizeof(TelemetryData));

  xTaskCreatePinnedToCore(DataAcquisitionTask, "Sensors", 4096, NULL, 2, NULL,
                          1);

  xTaskCreatePinnedToCore(StorageTask, "Storage", 4096, NULL, 1, NULL, 1);

  xTaskCreatePinnedToCore(UplinkTask, "Uplink", 8192, NULL, 1, NULL, 0);
}

void loop() { vTaskDelete(NULL); }

// --------------------------------------------------
// DATA ACQUISITION
// --------------------------------------------------

void DataAcquisitionTask(void *pv) {

  flow.begin();

  TelemetryData data;

  uint32_t lastReportTime = 0;

  for (;;) {

    proximity.update();

    if (millis() - lastReportTime >= dynamicReportPeriod) {

      lastReportTime = millis();

      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {

        data.pressure = pressure.read();

        data.temperature = pressure.temperature();

        data.depth = pressure.depth();

        xSemaphoreGive(i2cMutex);
      }

      data.flow_Lmin = flow.read();

      data.totalLiters = flow.getTotalLiters();

      data.proximity_cm = proximity.read();

      data.waterLevel = tankLevel.read();

      time(&data.timestamp);

      if (!xQueueSend(measureQueue, &data, 0)) {

        miLogger.log("Measure queue full", Level::WARNING);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// --------------------------------------------------
// STORAGE TASK
// --------------------------------------------------

void StorageTask(void *pv) {

  TelemetryData sData;

  for (;;) {

    miLogger.processInternalLogs();

    if (xQueueReceive(measureQueue, &sData, portMAX_DELAY) == pdTRUE) {

      char buffer[150];

      snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.2f",
               sData.flow_Lmin, sData.totalLiters, sData.pressure, sData.depth,
               sData.proximity_cm, sData.waterLevel, sData.temperature);

      // ONLY pending.csv
      miLogger.logPending(buffer, sData.timestamp);
    }
  }
}

// --------------------------------------------------
// UPLINK TASK
// --------------------------------------------------

void UplinkTask(void *pv) {

  uint32_t lastWiFiRetry = 0;
  uint32_t lastNTPSync = 0;

  uint32_t wifiRetryInterval = 5000;

  const uint32_t MAX_WIFI_RETRY = 120000;
  const uint32_t NTP_INTERVAL = 86400000;

  auto timeIsValid = []() -> bool {
    time_t now;

    time(&now);

    // ~ year 2023+
    return now > 1700000000;
  };

  auto maintainConnectivity = [&]() {
    // ---------------- WIFI ----------------

    if (WiFi.status() != WL_CONNECTED) {

      if (millis() - lastWiFiRetry >= wifiRetryInterval || lastWiFiRetry == 0) {

        lastWiFiRetry = millis();

        char msg[80];

        snprintf(msg, sizeof(msg), "[Uplink] WiFi reconnect attempt (%lus)",
                 wifiRetryInterval / 1000);

        miLogger.log(msg, Level::INFO);

        if (WiFi.getMode() != WIFI_STA) {
          WiFi.mode(WIFI_STA);
        }

        if (WiFi.status() != WL_CONNECTED) {

          WiFi.disconnect(false, true);

          vTaskDelay(pdMS_TO_TICKS(200));

          WiFi.begin("Android_Anibal", "A10101234");
          // WiFi.begin("hvaldivia", "kiannymatricula");
        }

        // Exponential backoff
        wifiRetryInterval *= 2;

        if (wifiRetryInterval > MAX_WIFI_RETRY)
          wifiRetryInterval = MAX_WIFI_RETRY;
      }

      return;
    }

    // Reset retry interval after successful reconnect
    wifiRetryInterval = 5000;

    // ---------------- NTP ----------------

    if (lastNTPSync == 0 || millis() - lastNTPSync > NTP_INTERVAL ||
        !timeIsValid()) {

      miLogger.log("[Uplink] Synchronizing time...", Level::INFO);

      configTime(-6 * 3600, 0, "pool.ntp.org");

      struct tm timeinfo;

      if (getLocalTime(&timeinfo, 10000)) {

        lastNTPSync = millis();

        miLogger.log("[Uplink] Time synchronized", Level::INFO);

      } else {

        miLogger.log("[Uplink] Failed to synchronize time", Level::WARNING);
      }
    }
  };

  // --------------------------------------------------
  // MAIN LOOP
  // --------------------------------------------------

  for (;;) {

    maintainConnectivity();

    // ---------------- DYNAMIC THROTTLING ----------------

    size_t backlog = miLogger.pendingBacklog();

    // Dynamic throttling based on pending backlog size

    if (backlog < 10000) {

      // <5KB
      dynamicReportPeriod = 120000; // 2 min
    } else if (backlog < 20000) {

      // <10 KB
      dynamicReportPeriod = 600000; // 10 min
    } else if (backlog < 40000) {

      // <15 KB
      dynamicReportPeriod = 1800000; // 30 min
    } else if (backlog < 60000) {

      // <20 KB
      dynamicReportPeriod = 3600000; // 60 min
    } else {

      // Emergency mode
      // 2 horas max
      dynamicReportPeriod = 7200000;
    }

    // ---------------- READ NEXT FIFO LINE ----------------

    // --------------------------------------------------
    // READ NEXT FIFO LINE
    // --------------------------------------------------

    String line;

    size_t nextOffset = 0;

    if (!miLogger.getNextPending(line, nextOffset)) {

      vTaskDelay(pdMS_TO_TICKS(100));

      continue;
    }

    // ---------------- PARSE CSV ----------------

    char date[12];
    char timeStr[12];

    float f;
    float tl;
    float p;
    float d;
    float px;
    float temp;

    int wl;

    int matched = sscanf(line.c_str(), "%10[^,],%8[^,],%f,%f,%f,%f,%f,%d,%f",
                         date, timeStr, &f, &tl, &p, &d, &px, &wl, &temp);

    if (matched != 9) {

      miLogger.log("Malformed pending CSV line", Level::WARNING);

      // Skip malformed line
      miLogger.advancePending(nextOffset);

      vTaskDelay(pdMS_TO_TICKS(500));

      continue;
    }

    // ---------------- REBUILD TIMESTAMP ----------------

    struct tm tm_info = {0};

    int y, mo, day;
    int h, m, s;

    sscanf(date, "%d-%d-%d", &y, &mo, &day);

    sscanf(timeStr, "%d:%d:%d", &h, &m, &s);

    tm_info.tm_year = y - 1900;
    tm_info.tm_mon = mo - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = h;
    tm_info.tm_min = m;
    tm_info.tm_sec = s;

    time_t ts = mktime(&tm_info);

    // ---------------- BUILD JSON ----------------

    char payload[256];

    char timeFormatted[32];

    strftime(timeFormatted, sizeof(timeFormatted), "%Y-%m-%d %H:%M:%S",
             &tm_info);

    snprintf(payload, sizeof(payload),
             "{\"F\":%.1f,"
             "\"T\":%.1f,"
             "\"P\":%.2f,"
             "\"D\":%.2f,"
             "\"DL\":%d,"
             "\"Px\":%.2f,"
             "\"TL\":%.2f,"
             "\"TS\":\"%s\"}",
             f, temp, p, d, wl, px, tl, timeFormatted);

    bool success = false;

    bool usedHTTP = false;
    bool usedLoRa = false;

    // ---------------- HTTPS ----------------

    if (WiFi.status() == WL_CONNECTED && timeIsValid()) {

      WiFiClientSecure client;

      client.setInsecure();

      HTTPClient http;

      String url = "https://thingsboard.nm.35-208-114-233.nip.io/api/v1/" +
                   String(access_token) + "/telemetry";

      if (http.begin(client, url)) {

        http.addHeader("Content-Type", "application/json");

        int response = http.POST(payload);

        // =====================================================
        // FIX:
        // SOLO 2xx ES ÉXITO
        // =====================================================

        if (response >= 200 && response < 300) {

          success = true;

          usedHTTP = true;

          miLogger.log("HTTP telemetry sent", Level::INFO);

        } else {

          char err[96];

          snprintf(err, sizeof(err), "HTTP POST failed: %d", response);

          miLogger.log(err, Level::WARNING);
        }

        http.end();

        client.stop();

      } else {

        miLogger.log("HTTP begin failed", Level::WARNING);
      }
    }

    // ---------------- LORA FALLBACK ----------------

    if (!success && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {

      if (lora.send_msg(payload, 100, true)) {

        success = true;

        usedLoRa = true;

      } else {

        miLogger.log("LoRa upload failed", Level::WARNING);
      }

      xSemaphoreGive(spiMutex);
    }

    // ---------------- SUCCESS ----------------

    if (success) {

      const char *modeStr = usedHTTP ? "HTTP" : usedLoRa ? "LORA" : "FAIL";

      char finalLine[180];

      snprintf(finalLine, sizeof(finalLine),
               "%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%s", f, tl, p, d, px, wl, temp,
               modeStr);

      // Save confirmed upload
      miLogger.logMeasure(finalLine, ts);

      // =====================================================
      // FIX:
      // AVANZAR OFFSET REAL
      // =====================================================

      miLogger.advancePending(nextOffset);

      continue;
    }

    // ---------------- FAILURE BACKOFF ----------------

    if (WiFi.status() == WL_CONNECTED) {

      // Fast retry while connected
      vTaskDelay(pdMS_TO_TICKS(200));

    } else {

      // Slow retry while disconnected
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}