/*
 * Medidor de Flujo con ESP32 y Digiten FL-408
 * Placa: LilyGO T3 v1.6.1
 * Sensor Pin: IO26
 */

#include <Arduino.h>

// --- CONFIGURACIÓN ---
const byte sensorPin = 25;
// Factor de calibración: F = 4.8 * Q (L/min)
// Esto significa: 1 litro = aprox 288 pulsos.
// Ajusta este valor si notas que la medición no es exacta.
float factor_calibracion = 4.8; 

// --- VARIABLES ---
volatile long pulsos = 0;
float flujo_Lmin = 0;
float total_litros = 0;
unsigned long tiempoAnterior = 0;

// Función de interrupción (se ejecuta cada vez que el sensor envía un pulso)
void IRAM_ATTR contarPulsos() {
  pulsos++;
}

void setup() {
  Serial.begin(115200);

  // Configuración del pin con PULLUP interno para proteger el ESP32
  pinMode(sensorPin, INPUT_PULLUP);

  // Adjuntar la interrupción al pin 26 en el flanco de bajada (FALLING)
  attachInterrupt(digitalPinToInterrupt(sensorPin), contarPulsos, FALLING);
  
  Serial.println("--- Sistema de Medición de Flujo Iniciado ---");
  tiempoAnterior = millis();
}

void loop() {
  // Realizar el cálculo cada 1 segundo (1000 ms)
  if (millis() - tiempoAnterior > 1000) {
    
    // Deshabilitar interrupciones un momento mientras calculamos
    noInterrupts();
    
    // Frecuencia (pulsos en 1 segundo) = Hz
    long conteoPulsos = pulsos;
    pulsos = 0; // Reiniciar contador para el siguiente segundo
    
    interrupts(); // Reactivar interrupciones

    unsigned long tiempoTranscurrido = millis() - tiempoAnterior;
    tiempoAnterior = millis();

    // Calcular flujo en L/min: (Hz / Factor de Calibración)
    flujo_Lmin = (conteoPulsos) / factor_calibracion;

    // Calcular volumen total: (L/min) / 60 segundos = Litros en este segundo
    float litros_este_segundo = flujo_Lmin / 60.0;
    total_litros += litros_este_segundo;

    // --- SALIDA POR SERIAL ---
    Serial.print("Flujo: ");
    Serial.print(flujo_Lmin, 2);
    Serial.print(" L/min \t");
    Serial.print("Total: ");
    Serial.print(total_litros, 3);
    Serial.println(" Litros");
  }
}