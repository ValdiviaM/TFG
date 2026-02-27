#include <RadioLib.h>
#include <SPI.h>

uint8_t NODE_ID = 1;
uint8_t GATEWAY_ID = 100;

// SPI real del SX1276 en T-Beam
SPIClass spi = SPIClass(VSPI);

// NSS, DIO0, RESET, DIO1(-1), SPI
SX1276 radio = new Module(18, 26, 23, -1, spi);

void setup() {

  Serial.begin(115200);

  // SPI correcto T-Beam
  spi.begin(5, 19, 27, 18);

  int state = radio.begin(
    915.0,  // frecuencia
    500.0,  // bandwidth
    10,     // spreading factor
    5,      // coding rate (4/5)
    0x12,   // sync word
    20,     // potencia
    8       // preamble
  );

  if(state != RADIOLIB_ERR_NONE){
    Serial.println("Radio init failed");
    while(true);
  }

  radio.setCRC(true);

  Serial.println("LoRa RX listo");
}

void loop() {

  uint8_t buffer[64];
  int len = radio.receive(buffer, 64);

  if(len > 0) {

    Serial.print("RAW: ");
    for(int i=0;i<len;i++){
      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    uint8_t to   = buffer[0];
    uint8_t from = buffer[1];

    if(to != NODE_ID) return;

    String payload = "";

    for(int i=4;i<len;i++){
      payload += (char)buffer[i];
    }

    Serial.println(payload);

    if(payload == "Data!1") {
      sendReply(from);
    }
  }
}

void sendReply(uint8_t destination) {

  uint8_t packet[64];

  packet[0] = destination; // To
  packet[1] = NODE_ID;     // From
  packet[2] = 0x01;        // ID
  packet[3] = 0x00;        // Flags

  String msg = "Hola que quiere hacer hoy";

  for(int i=0;i<msg.length();i++){
    packet[4+i] = msg[i];
  }

  radio.transmit(packet, 4 + msg.length());
}