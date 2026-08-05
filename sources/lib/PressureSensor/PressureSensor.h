#pragma once
#include "MS5837.h"
#include "Sensor.h"
#include <Wire.h>

class PressureSensor : public Sensor<float> {
private:
  MS5837 sensor;
  uint8_t sda, scl;
  bool isAlive = false; // Track if sensor initialized correctly

public:
  PressureSensor(uint8_t model, uint8_t _sda, uint8_t _scl)
      : sda(_sda), scl(_scl) {
    sensor.setModel(model);
  }

  bool begin() override {
    Wire.begin(sda, scl);
    Wire.setClock(100000); // Fixed typo: setClock instead of set_clock
    isAlive = sensor.init();
    if (isAlive) {
      sensor.setFluidDensity(997);
    }
    return isAlive;
  }

  float read() override {
    if (!isAlive)
      return 0.0f;

    // The library read() returns void, so we just call it.
    sensor.read();

    float p = sensor.pressure();
    // Basic sanity check: if pressure is 0, something might be wrong
    if (p == 0)
      return 0.0f;

    return p;
  }

  float temperature() { return isAlive ? sensor.temperature() : 0.0f; }
  float depth() { return isAlive ? sensor.depth() : 0.0f; }
};