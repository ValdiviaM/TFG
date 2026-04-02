#pragma once
#include "Sensor.h"
#include <Arduino.h>
#include <vector>

class LevelSensor : public Sensor<uint8_t> {
private:
  std::vector<uint8_t> _pins;

public:
  // El constructor recibe una lista de pines: {p1, p2, p3...}
  LevelSensor(std::initializer_list<uint8_t> pins) : _pins(pins) {}

  bool begin() override {
    for (uint8_t pin : _pins) {
      // Los pines 34 a 39 NO tienen pull-up interno
      if (pin >= 34 && pin <= 39) {
        pinMode(pin, INPUT); // Requieren resistencia física externa de 10k
      } else {
        pinMode(pin, INPUT_PULLUP);
      }
    }
    return true;
  }

  uint8_t read() override {
    uint8_t result = 0;
    for (size_t i = 0; i < _pins.size(); i++) {
      // Leemos el pin. Si es LOW (suponiendo que el sensor cierra a masa),
      // activamos el bit correspondiente.
      if (digitalRead(_pins[i]) == LOW) {
        result |= (1 << i);
      }
    }
    return result;
    // Si pasaste 3 pines, el máximo será 7 (binario 111)
    // Si pasaste 5 pines, el máximo será 31 (binario 11111)
  }
};