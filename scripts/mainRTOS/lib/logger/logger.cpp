#include "logger.h"

Logger::Logger(int8_t sck, int8_t miso, int8_t mosi, int8_t ss,
               const std::array<String, (size_t)FileRole::COUNT> &files)
    : _filenames(files), _sck(sck), _miso(miso), _mosi(mosi), _ss(ss),
      _spiSD(HSPI) {}

bool Logger::begin(SemaphoreHandle_t sharedSpiMutex, size_t queueSize) {
  _spiMutex = sharedSpiMutex;
  _logQueue = xQueueCreate(queueSize, sizeof(SystemLog));
  if (_logQueue == NULL)
    return false;

  _spiSD.begin(_sck, _miso, _mosi, _ss);
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000))) {
    bool ok = SD.begin(_ss, _spiSD);
    xSemaphoreGive(_spiMutex);
    return ok;
  }
  return false;
}

bool Logger::log(const char *msg, Level level) {
  if (_logQueue == NULL)
    return false;

  SystemLog entry;
  strncpy(entry.msg, msg, sizeof(entry.msg) - 1);
  entry.msg[sizeof(entry.msg) - 1] = '\0';
  entry.level = level;
  time(&entry.timestamp);

  return xQueueSend(_logQueue, &entry, 0) == pdTRUE;
}

void Logger::processInternalLogs() {
  SystemLog entry; // Declared here
  while (xQueueReceive(_logQueue, &entry, 0) == pdTRUE) {
    writeToSD(entry.msg, entry.level, FileRole::LOG, entry.timestamp);
  }
}

bool Logger::logMeasure(const String &csvLine, time_t ts) {
  return writeToSD(csvLine, Level::INFO, FileRole::MEASURE, ts);
}

bool Logger::writeToSD(const String &message, Level level, FileRole role,
                       time_t externalTime) {
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return false;

  String fileName = _filenames[(size_t)role];
  String ts = formatTimestamp(externalTime);
  String line = (role == FileRole::MEASURE)
                    ? ts + "," + levelToString(level) + "," + message
                    : ts + " [" + levelToString(level) + "]: " + message;

  File file = SD.open(fileName, FILE_APPEND);
  if (file) {
    file.println(line);
    file.close();
    xSemaphoreGive(_spiMutex);
    return true;
  }
  xSemaphoreGive(_spiMutex);
  return false;
}

String Logger::formatTimestamp(time_t rawTime) {
  struct tm timeinfo;
  localtime_r(&rawTime, &timeinfo);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d,%H:%M:%S", &timeinfo);
  return String(buffer);
}

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