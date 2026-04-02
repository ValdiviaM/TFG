#pragma once

template <typename T> class Sensor {
public:
  virtual bool begin() = 0;
  virtual T read() = 0;
  virtual ~Sensor() {}
};