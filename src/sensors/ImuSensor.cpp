#include "sensors/ImuSensor.h"

#include <Arduino.h>
#include <ICM42670P.h>
#include <Wire.h>

#include "config.h"
#include "pins.h"

// Match the proven sketch's standalone, static-lifetime library object.
ICM42670 IMU(Wire, 0);

ImuSensor::ImuSensor() = default;

bool ImuSensor::begin() {
  // TDK's begin() initializes Wire internally, so configure its pins first.
  Wire.setPins(IMU_SDA, IMU_SCL);
  status_ = IMU.begin();
  if (status_ != 0) {
    available_ = false;
    return false;
  }

  IMU.startAccel(Config::IMU_SAMPLE_RATE_HZ, Config::IMU_ACCEL_RANGE_G);
  IMU.startGyro(Config::IMU_SAMPLE_RATE_HZ, Config::IMU_GYRO_RANGE_DPS);
  delay(100);

  available_ = true;
  return true;
}

bool ImuSensor::read(ImuReading& reading) {
  if (!available_) {
    return false;
  }

  inv_imu_sensor_event_t event{};
  status_ = IMU.getDataFromRegisters(event);
  if (status_ != 0) {
    return false;
  }

  reading.accelX = event.accel[0];
  reading.accelY = event.accel[1];
  reading.accelZ = event.accel[2];
  reading.gyroX = event.gyro[0];
  reading.gyroY = event.gyro[1];
  reading.gyroZ = event.gyro[2];
  return true;
}

bool ImuSensor::available() const { return available_; }

int ImuSensor::status() const { return status_; }
