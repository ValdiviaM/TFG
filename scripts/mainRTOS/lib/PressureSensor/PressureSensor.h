#pragma once
#include "MS5837.h"
#include "Sensor.h"
#include <Wire.h>

class PressureSensor : public Sensor<float> {
private:
  MS5837 sensor;
  uint8_t sda, scl;

public:
  PressureSensor(uint8_t model, uint8_t _sda, uint8_t _scl)
      : sda(_sda), scl(_scl) {
    sensor.setModel(model);
  }

  bool begin() override {
    Wire.begin(sda, scl);
    if (!sensor.init())
      return false;
    sensor.setFluidDensity(997); // Agua dulce
    return true;
  }

  float read() override {
    sensor.read();
    return sensor.pressure();
  }

  // Métodos específicos
  float temperature() { return sensor.temperature(); }
  float depth() { return sensor.depth(); }
  float altitude() { return sensor.altitude(); }
};