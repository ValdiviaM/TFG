#include <Wire.h>
#include <XPowersLib.h>
#include <RadioLib.h>
#include <SPI.h>

// ==========================
// PMU
// ==========================
XPowersAXP2101 PMU;

// ==========================
// LoRa Pins
// ==========================
#define LORA_SCK   5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS   18
#define LORA_RST  14
#define LORA_DIO0 26

SPIClass spi = SPIClass(VSPI);
SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, -1, spi);

// ==========================
float frequency = 915.0;
String replyMessage = "Hola desde ESP";

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println("Inicializando PMU...");

  Wire.begin(21, 22);

  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, 21, 22)) {
    Serial.println("PMU no detectado");
    while (1);
  }

  // 🔋 Encender rails del SX1276
  PMU.setDC3Voltage(3300);
  PMU.enableDC3();

  PMU.setALDO2Voltage(3300);
  PMU.enableALDO2();

  delay(500);

  Serial.println("LoRa Power ON");

  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  Serial.println("Inicializando LoRa...");

  int state = radio.begin(frequency);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Error init: ");
    Serial.println(state);
    while (1);
  }

  // Match EXACTO con RFM9x
  radio.setSpreadingFactor(10);
  radio.setBandwidth(500.0);
  radio.setCodingRate(5);
  radio.setSyncWord(0x12);
  radio.setCRC(true);

  Serial.println("LoRa listo!");
}

void loop() {

  String received;

  int state = radio.receive(received);

  if (state == RADIOLIB_ERR_NONE) {

    Serial.print("Mensaje recibido: ");
    Serial.println(received);

    radio.transmit(replyMessage);
    Serial.println("Respuesta enviada");

    delay(1000);
  }
}