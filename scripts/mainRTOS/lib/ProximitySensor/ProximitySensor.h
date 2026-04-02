#pragma once
#include "Sensor.h"
#include <Arduino.h>
#include <algorithm>

class ProximitySensor : public Sensor<float> {
private:
  HardwareSerial &serial;
  int rxPin, txPin;
  static const int TAMANO_FILTRO = 5;
  float lecturas[TAMANO_FILTRO];
  int indiceLectura = 0;
  float ultimaDistancia = -1.0;

  float obtenerMediana(); // Cambiado de calcularMediana a obtenerMediana para
                          // coincidir con el .cpp

public:
  ProximitySensor(HardwareSerial &ser, int rx, int tx);
  bool begin() override;
  float read() override;
  void update(); // <--- ESTA ERA LA LÍNEA QUE FALTABA
};