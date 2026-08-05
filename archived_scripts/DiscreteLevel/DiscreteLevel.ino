/*
 * Medidor de Nivel de Líquido Sin Contacto
 * Sensor: XKC-Y26-V
 * Placa: LilyGO T3 v1.6.1
 * Pin: IO25 (Incluye LED verde integrado)
 */

#include <Arduino.h>

// Definimos el pin
const int pinNivel = 13; 

// Variable para el estado
bool estadoAgua = false;

void setup() {
  // Iniciamos comunicación serial
  Serial.begin(115200);
  
  // Configuramos el pin como entrada
  // Usamos INPUT normal porque el sensor ya envía voltaje
  pinMode(pinNivel, INPUT);
  
  Serial.println("--- Monitor de Nivel Iniciado ---");
  Serial.println("Esperando detección...");
}

void loop() {
  // Leer el sensor
  int lectura = digitalRead(pinNivel);

  // Si la lectura es ALTA (HIGH), hay agua
  if (lectura == HIGH) {
    if (!estadoAgua) { // Solo imprime si el estado cambia
      Serial.println("ESTADO: [ AGUA DETECTADA ]");
      estadoAgua = true;
    }
  } 
  else {
    if (estadoAgua) { // Solo imprime si el estado cambia
      Serial.println("ESTADO: [ VACÍO / SIN AGUA ]");
      estadoAgua = false;
    }
  }

  // Pequeña pausa para estabilidad
  delay(200);
}