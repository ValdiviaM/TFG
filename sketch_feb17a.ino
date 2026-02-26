#include <Wire.h>
#include <XPowersLib.h>
#include "RFM9xCompat.h"

XPowersAXP2101 PMU;
RFM9xCompat lora;

void setup() {

    Serial.begin(115200);
    delay(2000);

    Serial.println("Inicializando PMU...");

    Wire.begin(21, 22);

    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, 21, 22)) {
        Serial.println("PMU no detectado");
        while (1);
    }

    // 🔥 ENCENDER LORA (CONFIRMADO FUNCIONAL)
    PMU.setDC3Voltage(3300);
    PMU.enableDC3();

    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2();

    delay(500);

    Serial.println("LoRa Power ON");

    if(!lora.begin(4)) {
        Serial.println("LoRa init failed");
        while(1);
    }

    Serial.println("LoRa Ready");
}

void loop() {
    String msg="";
    uint8_t source;

    if (lora.receive_msg(msg, source)) {
        Serial.print("Gateway dijo: ");
        Serial.println(msg);
        if (msg=="Data!1"){
            Serial.println("Mensaje recibido correctamente");
        }
        delay(100);
        // Intentamos responder con confirmación
        Serial.println("Enviando respuesta con ACK...");
        if (lora.send_msg("Recibido!", source, true)) {
            Serial.println("El Gateway recibio mi respuesta exitosamente.");
        } else {
            Serial.println("El Gateway no respondio el ACK.");
        }
    }
    delay(100);
}