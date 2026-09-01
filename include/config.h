#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t TFT_SPI_HZ = 10000000;
constexpr uint32_t MIC_SAMPLE_RATE_HZ = 16000;
constexpr size_t MIC_BUFFER_SAMPLES = 256;
constexpr uint16_t IMU_SAMPLE_RATE_HZ = 100;
constexpr uint16_t IMU_ACCEL_RANGE_G = 16;
constexpr uint16_t IMU_GYRO_RANGE_DPS = 2000;
constexpr uint32_t LOOP_DELAY_MS = 100;
}  // namespace Config

