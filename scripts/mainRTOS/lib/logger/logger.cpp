#include "logger.h"

Logger::Logger(int8_t sck, int8_t miso, int8_t mosi, int8_t ss,
               const std::array<String, (size_t)FileRole::COUNT> &files)
    : _filenames(files), _sck(sck), _miso(miso), _mosi(mosi), _ss(ss),
      _spiSD(HSPI) {}

bool Logger::begin() {
  _spiSD.begin(_sck, _miso, _mosi, _ss);
  if (!SD.begin(_ss, _spiSD))
    return false;

  _mutex = xSemaphoreCreateMutex();

  for (const auto &path : _filenames) {
    // Si el archivo no existe, lo creamos y escribimos cabecera
    if (!SD.exists(path)) {
      File f = SD.open(path, FILE_WRITE);
      if (f) {
        if (path.endsWith(".csv")) {
          f.println(
              "Date,Time,LogType,Flow,Total,Pressure,Depth,Proximity,Level");
        } else {
          f.println("--- Sistema Iniciado ---");
        }
        f.close();
      }
    }
  }
  return true;
}

// 1. Cambiamos el espacio por una coma entre fecha y hora
String Logger::formatTimestamp(time_t rawTime) {
  struct tm timeinfo;
  localtime_r(&rawTime, &timeinfo);
  char buffer[25];
  // Cambiado: %Y-%m-%d,%H:%M:%S (Nota la coma)
  strftime(buffer, sizeof(buffer), "%Y-%m-%d,%H:%M:%S", &timeinfo);
  return String(buffer);
}

// 2. Quitamos los corchetes para que sea solo el texto
String Logger::levelToString(Level level) {
  switch (level) {
  case Level::DEBUG:
    return "DEBUG";
  case Level::INFO:
    return "INFO";
  case Level::WARNING:
    return "WARNING";
  case Level::ERROR:
    return "ERROR";
  case Level::CRITICAL:
    return "CRITICAL";
  default:
    return "UNKNOWN";
  }
}

// 3. Quitamos el espacio y los dos puntos, usamos COMAS
bool Logger::log(const String &message, Level level, FileRole role,
                 time_t externalTime) {
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) != pdTRUE)
    return false;

  String fileName = _filenames[(size_t)role];
  String ts = formatTimestamp(externalTime);

  String line;
  if (role == FileRole::MEASURE) {
    // Para el CSV: Date,Time,LogType,Mensaje(datos...)
    line = ts + "," + levelToString(level) + "," + message;
  } else {
    // Para el LOG de sistema seguimos usando un formato legible
    line = ts + " [" + levelToString(level) + "]: " + message;
  }

  File file = SD.open(fileName, FILE_APPEND);
  if (file) {
    file.println(line);
    file.close();
    xSemaphoreGive(_mutex);
    return true;
  }
  xSemaphoreGive(_mutex);
  return false;
}