#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(21,22);
  //Wire.setClock(100000);

  Serial.println("Init sensor");
  sensor.setModel(MS5837::MS5837_02BA);

  if (!sensor.init()) {
    Serial.println("Sensor no encontrado");
    while(1);
  }  

  sensor.setFluidDensity(997);

  Serial.println("Sensor OK");
}

void loop() {

sensor.read();

Serial.print("Presion: ");
Serial.print(sensor.pressure());
Serial.println(" mbar");

Serial.print("Temperatura: ");
Serial.print(sensor.temperature());
Serial.println(" C");

Serial.print("Profundidad: ");
Serial.print(sensor.depth());
Serial.println(" m");

Serial.print("Altitud: ");
Serial.print(sensor.altitude());
Serial.println(" m");

delay(1000);}