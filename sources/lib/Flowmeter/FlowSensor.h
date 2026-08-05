#pragma once

#include "Sensor.h"
#include <Arduino.h>

class FlowSensor : public Sensor<float> {

private:
  uint8_t pin;
  float calibrationFactor;

  volatile long pulses = 0;

  float flow_Lmin = 0;
  float totalLiters = 0;

  unsigned long lastTime = 0;

  static FlowSensor *instance;

  static void IRAM_ATTR isr();

public:
  FlowSensor(uint8_t sensorPin, float factor = 4.8);

  bool begin() override;

  float read() override;

  float getTotalLiters();
};