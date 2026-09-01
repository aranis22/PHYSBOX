#pragma once

#include <cstdint>

#include "display/PhysboxDisplay.h"
#include "sensors/ImuSensor.h"

class ImuBallApp {
 public:
  ImuBallApp(PhysboxDisplay& display, ImuSensor& imu);

  bool begin();
  void update();

 private:
  PhysboxDisplay& display_;
  ImuSensor& imu_;

  float positionX_ = 165.0f;
  float positionY_ = 160.0f;
  float velocityX_ = 0.0f;
  float velocityY_ = 0.0f;
  float filteredAccelX_ = 0.0f;
  float filteredAccelY_ = 0.0f;
  float accelOffsetX_ = 0.0f;
  float accelOffsetY_ = 0.0f;
  int16_t previousPuckX_ = 165;
  int16_t previousPuckY_ = 160;
  uint32_t previousFrameMs_ = 0;
  uint32_t previousValuesMs_ = 0;
  uint32_t previousSerialMs_ = 0;
  uint32_t frameCount_ = 0;
  uint32_t fpsWindowStartMs_ = 0;
  float measuredFps_ = 0.0f;
  ImuReading latestReading_{};

  bool calibrate();
  void drawStaticScreen();
  void restorePlotRegion(int16_t x, int16_t y, int16_t width,
                         int16_t height);
  void drawPuck(int16_t x, int16_t y);
  void drawValues();
  void drawValueLine(int16_t y, char axis, float value);
  void advancePhysics(float dtSeconds, float accelX, float accelY);
  void updateFps(uint32_t nowMs);
};
