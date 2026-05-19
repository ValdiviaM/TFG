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

  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    return false;

  bool ok = SD.begin(_ss, _spiSD);

  if (ok) {

    // ---------------- SYSTEM LOG ----------------
    if (!SD.exists("/system.log")) {
      File f = SD.open("/system.log", FILE_WRITE);

      if (f)
        f.close();
    }

    // ---------------- DATA CSV ----------------
    if (!SD.exists("/data.csv")) {
      File f = SD.open("/data.csv", FILE_WRITE);

      if (f) {
        f.println("date,time,flow_Lmin,totalLiters,pressure,depth,"
                  "proximity_cm,waterLevel,temperature,transport");

        f.close();
      }
    }

    // ---------------- PENDING CSV ----------------
    size_t startOffset = 0;

    if (!SD.exists("/pending.csv")) {
      File f = SD.open("/pending.csv", FILE_WRITE);

      if (f) {
        f.println("date,time,flow_Lmin,totalLiters,pressure,depth,"
                  "proximity_cm,waterLevel,temperature");

        startOffset = f.position();

        f.close();
      }
    } else {
      // If file already exists, start from beginning
      File f = SD.open("/pending.csv", FILE_READ);

      if (f) {
        startOffset = 0;
        f.close();
      }
    }

    // ---------------- PENDING INDEX ----------------
    if (!SD.exists("/pending.idx")) {
      File f = SD.open("/pending.idx", FILE_WRITE);

      if (f) {
        char buf[12];

        snprintf(buf, sizeof(buf), "%010lu", (unsigned long)startOffset);

        f.print(buf);

        f.close();
      }
    }
  }

  xSemaphoreGive(_spiMutex);

  return ok;
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
  SystemLog entry;

  while (xQueueReceive(_logQueue, &entry, 0) == pdTRUE) {
    writeToSD(entry.msg, entry.level, FileRole::LOG, entry.timestamp);
  }
}

bool Logger::logMeasure(const String &csvLine, time_t ts) {
  return writeToSD(csvLine, Level::INFO, FileRole::MEASURE, ts);
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
// 1. Update writeToSD to remove "INFO" from CSV files for cleaner parsing

bool Logger::writeToSD(const String &message, Level level, FileRole role,
                       time_t externalTime) {
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return false;

  String fileName = _filenames[(size_t)role];
  String ts = formatTimestamp(externalTime);
  String line;

  // Format: YYYY-MM-DD,HH:MM:SS,data1,data2... (Matches your logMeasure logic)
  if (role == FileRole::MEASURE || role == FileRole::PENDING_DATA) {
    line = ts + "," + message;
  } else {
    line = ts + " [" + levelToString(level) + "]: " + message;
  }

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
// 2. Add the FIFO logic
bool Logger::logPending(const String &csvLine, time_t ts) {
  return writeToSD(csvLine, Level::INFO, FileRole::PENDING_DATA, ts);
}

bool Logger::getNextPending(String &line, size_t &nextOffset) {

  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return false;

  size_t offset = 0;

  // ---------- READ CURRENT OFFSET ----------

  File idxFile = SD.open("/pending.idx", FILE_READ);

  if (idxFile) {

    offset = idxFile.parseInt();

    idxFile.close();
  }

  // ---------- OPEN FILE ----------

  File dataFile =
      SD.open(_filenames[(size_t)FileRole::PENDING_DATA], FILE_READ);

  if (!dataFile) {

    xSemaphoreGive(_spiMutex);

    return false;
  }

  // ---------- EOF ----------

  if (offset >= dataFile.size()) {

    dataFile.close();

    xSemaphoreGive(_spiMutex);

    return false;
  }

  // ---------- SEEK ----------

  dataFile.seek(offset);

  // ---------- READ NEXT LINE ----------

  line = dataFile.readStringUntil('\n');

  // SAVE NEXT OFFSET
  nextOffset = dataFile.position();

  dataFile.close();

  line.trim();

  xSemaphoreGive(_spiMutex);

  // Ignore empty lines
  if (line.isEmpty())
    return false;

  // Ignore header
  if (line.startsWith("date,time"))
    return false;

  return true;
}
bool Logger::advancePending(size_t newOffset) {

  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return false;

  File idxFile = SD.open("/pending.idx", FILE_WRITE);

  if (!idxFile) {

    xSemaphoreGive(_spiMutex);

    return false;
  }

  char buf[16];

  snprintf(buf, sizeof(buf), "%010lu", (unsigned long)newOffset);

  idxFile.print(buf);

  idxFile.close();

  xSemaphoreGive(_spiMutex);

  return true;
}
size_t Logger::pendingBacklog() {
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return 0;
  File dataFile =
      SD.open(_filenames[(size_t)FileRole::PENDING_DATA], FILE_READ);
  if (!dataFile) {
    xSemaphoreGive(_spiMutex);
    return 0;
  }
  size_t totalSize = dataFile.size();
  dataFile.close();

  size_t offset = 0;
  File idxFile = SD.open("/pending.idx", FILE_READ);
  if (idxFile) {
    offset = idxFile.parseInt();
    idxFile.close();
  }
  xSemaphoreGive(_spiMutex);

  return (totalSize > offset) ? (totalSize - offset) : 0;
}