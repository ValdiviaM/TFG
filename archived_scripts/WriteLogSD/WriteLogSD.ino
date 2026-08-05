#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

/* WIFI */
const char* ssid = "Apartamento 1";
const char* password = "Aloiman007";

/* Pines SD */
#define SD_CS   13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCK  14

/* LoRa CS */
#define LORA_CS 18

SPIClass spiSD(VSPI);

/*--------------------------------------------------*/
/* WIFI + NTP                                       */
/*--------------------------------------------------*/

void initTime(){

  Serial.println("Conectando WiFi...");

  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");

  configTime(-6 * 3600, 0, "pool.ntp.org");

  struct tm timeinfo;

  while(!getLocalTime(&timeinfo)){
    Serial.println("Esperando NTP...");
    delay(1000);
  }

  Serial.println("Hora sincronizada");
}

/*--------------------------------------------------*/
/* Timestamp                                         */
/*--------------------------------------------------*/

String getTimestamp(){

  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)){
    return "00/00/00 00:00";
  }

  char buffer[30];

  strftime(buffer, sizeof(buffer), "%d/%m/%y %H:%M:%S", &timeinfo);

  return String(buffer);
}

/*--------------------------------------------------*/
/* Logger                                            */
/*--------------------------------------------------*/

void logMessage(String message){

  String line = getTimestamp() + ": " + message;

  File file = SD.open("/log.txt", FILE_APPEND);

  if(!file){
    Serial.println("ERROR abriendo log.txt");
    return;
  }

  file.println(line);
  file.close();

  Serial.println(line);

  /* comprobar tamaño real */
  File check = SD.open("/log.txt", FILE_READ);

  Serial.print("Tamano log.txt: ");
  Serial.println(check.size());

  check.close();
}

/*--------------------------------------------------*/
/* Inicializar SD                                    */
/*--------------------------------------------------*/

bool initSD(){

  Serial.println("Inicializando SD...");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if(!SD.begin(SD_CS, spiSD)){
    Serial.println("ERROR inicializando SD");
    return false;
  }

  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No hay SD");
    return false;
  }

  Serial.print("Tipo SD: ");

  if(cardType == CARD_SDHC) Serial.println("SDHC");
  else if(cardType == CARD_SD) Serial.println("SDSC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);

  Serial.print("Tamano SD: ");
  Serial.print(cardSize);
  Serial.println(" MB");

  if(!SD.exists("/log.txt")){
    Serial.println("Creando log.txt");
    File file = SD.open("/log.txt", FILE_WRITE);
    file.close();
  }

  return true;
}

/*--------------------------------------------------*/

void setup(){

  Serial.begin(115200);
  delay(2000);

  /* desactivar LoRa */
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);

  initTime();

  if(!initSD()){
    Serial.println("Error SD");
    while(true);
  }

  logMessage("Sistema iniciado");
}

/*--------------------------------------------------*/

void loop(){

  int sensor2 = random(2000,3000);

  logMessage("Sensor 2: " + String(sensor2));

  delay(3000);

  logMessage("Ha ocurrido un error");

  delay(3000);

  logMessage("Recibido de Lora: Hola desde nodo 2");

  delay(5000);
}