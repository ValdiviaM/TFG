#include <SPI.h>
#include "RFM9xCompat.h"

RFM9xCompat lora;

void setup() {

    Serial.begin(115200);
    delay(2000);

    Serial.println("Inicializando LoRa...");

    if(!lora.begin(1)) {
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