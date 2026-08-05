#include <Arduino.h>
#include <vector>
#include <algorithm>

HardwareSerial MySerial(2); 
#define RX_PIN 13
#define TX_PIN 15

// Configuración del filtro
const int TAMANO_FILTRO = 5; // Número de muestras para promediar
float lecturas[TAMANO_FILTRO];
int indiceLectura = 0;

void setup() {
  Serial.begin(115200);
  pinMode(RX_PIN, INPUT_PULLUP); 
  MySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  Serial.println("--- MONITOR AJ-SR04M FILTRADO ---");
}

// Función para obtener la mediana (elimina picos de ruido)
float obtenerMediana(float *datos, int n) {
  float temp[n];
  memcpy(temp, datos, n * sizeof(float));
  std::sort(temp, temp + n);
  return temp[n / 2];
}

void loop() {
  if (MySerial.available() >= 4) {
    if (MySerial.read() == 0xFF) {
      uint8_t h = MySerial.read();
      uint8_t l = MySerial.read();
      uint8_t sum = MySerial.read();
      
      if (((h + l) & 0xFF) == sum) {
        float distActual = ((h << 8) | l) / 10.0;

        // --- CAPA 1: FILTRO DE RANGO ---
        // Ignorar 800 (error) y valores menores a 24 (punto ciego)
        if (distActual < 790.0 && distActual > 22.0) {
          
          lecturas[indiceLectura] = distActual;
          indiceLectura++;

          // --- CAPA 2: FILTRO DE MEDIANA ---
          if (indiceLectura >= TAMANO_FILTRO) {
            indiceLectura = 0; // Reiniciar para la próxima ráfaga
            float distanciaFiltrada = obtenerMediana(lecturas, TAMANO_FILTRO);

            Serial.print("Distancia Filtrada: ");
            Serial.print(distanciaFiltrada);
            Serial.println(" cm");
          }
        }
      } else {
        // Si el checksum falla, limpiar buffer
        while(MySerial.available()) MySerial.read();
      }
    }
  }
}