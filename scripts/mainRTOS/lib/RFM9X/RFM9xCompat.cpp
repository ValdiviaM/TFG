#include "RFM9xCompat.h"

// Inicializar flag estática
volatile bool RFM9xCompat::_receivedFlag = false;

// Función que se ejecuta cuando el pin DIO0 detecta un paquete
void RFM9xCompat::setFlag() { _receivedFlag = true; }

bool RFM9xCompat::begin(uint8_t node) {
  _node = node;
  memset(_last_seen_id, 0xFF, sizeof(_last_seen_id));
  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  // Configuración: 915MHz, BW 500, SF 10, CR 5, Sync 0x12, Preamble 8
  int state = radio.begin(915.0, 500.0, 10, 5, 0x12, 17, 8);
  if (state != RADIOLIB_ERR_NONE)
    return false;

  // Configurar la función de interrupción para el pin DIO0
  radio.setPacketReceivedAction(setFlag);

  radio.setCRC(true);
  start_listening();
  return true;
}

void RFM9xCompat::start_listening() {
  _receivedFlag = false;
  radio.startReceive();
}

bool RFM9xCompat::send_msg(const char *msg, uint8_t destination,
                           bool with_ack) {
  uint8_t len = strlen(msg);
  uint8_t id = _seq++;
  uint8_t packet[256];

  for (int retry = 0; retry < 5; retry++) {
    packet[0] = destination;
    packet[1] = _node;
    packet[2] = id;
    packet[3] = (retry > 0) ? RH_FLAGS_RETRY : 0x00;
    memcpy(&packet[4], msg, len);

    radio.standby(); // Detener escucha para transmitir
    int state = radio.transmit(packet, len + 4);
    start_listening(); // Volver a escuchar inmediatamente

    if (!with_ack || destination == 0xFF)
      return true;

    // Esperar ACK con el flag de interrupción
    uint32_t startWait = millis();
    while (millis() - startWait < 1000) {
      if (_receivedFlag) {
        uint8_t rxBuf[256];
        size_t rxLen = radio.getPacketLength();
        int rxState = radio.readData(rxBuf, rxLen);

        if (rxState == RADIOLIB_ERR_NONE) {
          if ((rxBuf[3] & RH_FLAGS_ACK) && (rxBuf[2] == id)) {
            start_listening();
            return true;
          }
        }
        start_listening(); // Si no era el ACK, seguir escuchando
      }
    }
    delay(random(100, 200));
  }
  return false;
}

void RFM9xCompat::send_ack(uint8_t to, uint8_t id) {
  uint8_t packet[5] = {to, _node, id, RH_FLAGS_ACK, '!'};
  radio.standby();
  radio.transmit(packet, 5);
  start_listening();
}

bool RFM9xCompat::receive_msg(String &payload, uint8_t &source) {
  // Si la interrupción no se ha disparado, no hay nada que leer
  if (!_receivedFlag)
    return false;

  size_t length = radio.getPacketLength();
  if (length < 4) {
    start_listening();
    return false;
  }

  uint8_t buf[256];
  int state = radio.readData(buf, length);

  // IMPORTANTE: Al usar DIO0, RadioLib limpia los flags de hardware aquí
  if (state != RADIOLIB_ERR_NONE) {
    start_listening();
    return false;
  }

  uint8_t to = buf[0];
  uint8_t from = buf[1];
  uint8_t id = buf[2];
  uint8_t flags = buf[3];

  // Filtrar dirección
  if (to != _node && to != 0xFF) {
    start_listening();
    return false;
  }

  // Ignorar si es un ACK suelto
  if (flags & RH_FLAGS_ACK) {
    start_listening();
    return false;
  }

  // Enviar ACK si es necesario
  if (to != 0xFF) {
    delay(10);
    send_ack(from, id);
  }

  // Filtrar duplicados
  if (_last_seen_id[from] == id) {
    start_listening();
    return false;
  }
  _last_seen_id[from] = id;

  // EXTRAER DATA
  source = from;
  payload = "";
  for (size_t i = 4; i < length; i++) {
    payload += (char)buf[i];
  }

  start_listening();
  return (payload.length() > 0);
}