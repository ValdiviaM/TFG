#pragma once
#include "logger.h"
#include <Arduino.h>

// Estructura para mediciones
struct TelemetryData {
  float flow_Lmin;
  float totalLiters;
  float pressure;
  float temperature;
  float depth;
  float proximity_cm;
  uint8_t waterLevel; // Cambiado de bool a uint8_t
  time_t timestamp;
};

// Estructura para mensajes de sistema/errores
struct SystemLog {
  char msg[64];
  Level level;
  time_t timestamp;
};