#pragma once
#include <RadioLib.h>
#include <SPI.h>

class RFM9xCompat {
public:
  bool begin(uint8_t node);
  bool send_msg(const char *msg, uint8_t destination, bool with_ack = true);
  bool receive_msg(String &payload, uint8_t &source);

private:
// Pines T-Beam V1.2
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_DIO0 26 // <--- ESTE ES EL PIN DE INTERRUPCIÓN

  SPIClass spi = SPIClass(VSPI);
  SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, -1, spi);

  uint8_t _node;
  uint8_t _seq = 0;
  uint8_t _last_seen_id[256];

  const uint8_t RH_FLAGS_ACK = 0x80;
  const uint8_t RH_FLAGS_RETRY = 0x40;

  // Flag estática para la interrupción
  static volatile bool _receivedFlag;
  static void setFlag();

  void send_ack(uint8_t to, uint8_t id);
  void start_listening();
};