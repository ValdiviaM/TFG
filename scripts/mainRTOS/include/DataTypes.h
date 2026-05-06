#pragma once
#include <Arduino.h>
#include <time.h>

// 1. Enums used by the whole system
enum class Level { DEBUG, INFO, WARNING, ERROR, CRITICAL };
enum class FileRole { LOG, MEASURE, PENDING_DATA, COUNT };
enum class TransMode { PENDING, MQTT, LORA, FAIL };

// 2. SystemLog Struct (ONLY ONCE)
struct SystemLog {
  char msg[128];
  Level level;
  time_t timestamp;
};

// 3. Telemetry Data Struct
struct TelemetryData {
  float flow_Lmin;
  float totalLiters;
  float pressure;
  float temperature;
  float depth;
  float proximity_cm;
  uint8_t waterLevel;
  time_t timestamp;
  TransMode sentVia;
};