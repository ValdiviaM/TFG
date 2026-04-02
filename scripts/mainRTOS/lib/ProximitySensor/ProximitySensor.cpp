#include "ProximitySensor.h"

// ===============================
// Constructor
// ===============================
ProximitySensor::ProximitySensor(HardwareSerial &serial, int rx, int tx)
    : serial(serial), rxPin(rx), txPin(tx), indiceLectura(0),
      ultimaDistancia(-1.0f) {}

// ===============================
// begin()
// ===============================
bool ProximitySensor::begin() {
  pinMode(rxPin, INPUT);
  serial.begin(9600, SERIAL_8N1, rxPin, txPin);
  return true;
}

// ===============================
// Filtro mediana
// ===============================
float ProximitySensor::obtenerMediana() {
  float temp[TAMANO_FILTRO];
  memcpy(temp, lecturas, sizeof(lecturas));
  std::sort(temp, temp + TAMANO_FILTRO);
  return temp[TAMANO_FILTRO / 2];
}

// ===============================
// update()
// ===============================
void ProximitySensor::update() {

  while (serial.available() >= 4) {

    if (serial.read() == 0xFF) {

      uint8_t h = serial.read();
      uint8_t l = serial.read();
      uint8_t sum = serial.read();

      if (((h + l) & 0xFF) == sum) {

        float distActual = ((h << 8) | l) / 10.0;

        // filtro de rango
        if (distActual < 790.0 && distActual > 22.0) {

          lecturas[indiceLectura++] = distActual;

          if (indiceLectura >= TAMANO_FILTRO) {
            indiceLectura = 0;
            ultimaDistancia = obtenerMediana();
          }
        }

      } else {
        while (serial.available())
          serial.read();
      }
    }
  }
}

// ===============================
// read()
// ===============================
float ProximitySensor::read() { return ultimaDistancia; }