#pragma once

#include <cstdint>

struct ImuReading {
  int16_t accelX;
  int16_t accelY;
  int16_t accelZ;
  int16_t gyroX;
  int16_t gyroY;
  int16_t gyroZ;
};

class ImuSensor {
 public:
  ImuSensor();

  bool begin();
  bool read(ImuReading& reading);
  bool available() const;
 int status() const;

 private:
  bool available_ = false;
  int status_ = -1;
};
