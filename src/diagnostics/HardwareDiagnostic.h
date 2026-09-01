#pragma once

#include "display/PhysboxDisplay.h"
#include "sensors/ImuSensor.h"
#include "sensors/Microphone.h"

class HardwareDiagnostic {
 public:
  HardwareDiagnostic(PhysboxDisplay& display, ImuSensor& imu,
                     Microphone& microphone);

  void begin();
  void update();

 private:
  PhysboxDisplay& display_;
  ImuSensor& imu_;
  Microphone& microphone_;

  void drawScreen();
  void printValue(int y, const char* label, long value);
};
