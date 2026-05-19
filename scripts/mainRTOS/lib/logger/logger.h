#pragma once

#include "DataTypes.h" // Important: Include this first
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <array>

class Logger {
private:
  std::array<String, (size_t)FileRole::COUNT> _filenames;

  int8_t _sck, _miso, _mosi, _ss;
  SPIClass _spiSD;

  SemaphoreHandle_t _spiMutex;
  QueueHandle_t _logQueue;

  String formatTimestamp(time_t rawTime);
  String levelToString(Level level);

  bool writeToSD(const String &message, Level level, FileRole role,
                 time_t externalTime);

public:
  Logger(int8_t sck, int8_t miso, int8_t mosi, int8_t ss,
         const std::array<String, (size_t)FileRole::COUNT> &files);

  bool begin(SemaphoreHandle_t sharedSpiMutex, size_t queueSize = 50);

  bool log(const char *msg, Level level = Level::INFO);
  // Add these to the public section of the Logger class
  bool logPending(const String &csvLine, time_t ts);
  bool getNextPending(String &line, size_t &nextOffset);
  bool advancePending(size_t newOffset);
  size_t pendingBacklog();
  bool logMeasure(const String &csvLine, time_t ts);

  void processInternalLogs();
};