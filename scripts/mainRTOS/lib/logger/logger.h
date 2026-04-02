#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <array>
#include <time.h>

enum class Level { DEBUG, INFO, WARNING, ERROR, CRITICAL };
enum class FileRole { LOG, MEASURE, COUNT };

class Logger {
private:
  std::array<String, (size_t)FileRole::COUNT> _filenames;
  int8_t _sck, _miso, _mosi, _ss;
  SPIClass _spiSD;
  SemaphoreHandle_t _mutex;

  String formatTimestamp(time_t rawTime);
  String levelToString(Level level);

public:
  Logger(int8_t sck, int8_t miso, int8_t mosi, int8_t ss,
         const std::array<String, (size_t)FileRole::COUNT> &files);
  bool begin();
  bool log(const String &message, Level level = Level::INFO,
           FileRole role = FileRole::LOG, time_t externalTime = 0);
};