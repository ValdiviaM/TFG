#include "FlowSensor.h"

FlowSensor *FlowSensor::instance = nullptr;

FlowSensor::FlowSensor(uint8_t sensorPin, float factor) {
  pin = sensorPin;
  calibrationFactor = factor;
}

bool FlowSensor::begin() { // Cambiar void por bool
  instance = this;
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), isr, FALLING);
  lastTime = millis();
  return true; // Retornar algo
}

void IRAM_ATTR FlowSensor::isr() {
  if (instance)
    instance->pulses++;
}

float FlowSensor::read() {
  if (millis() - lastTime >= 1000) {
    noInterrupts();

    long count = pulses;
    pulses = 0;

    interrupts();

    flow_Lmin = count / calibrationFactor;

    float litersThisSecond = flow_Lmin / 60.0;

    totalLiters += litersThisSecond;

    lastTime = millis();
  }

  return flow_Lmin;
}

float FlowSensor::getTotalLiters() { return totalLiters; }