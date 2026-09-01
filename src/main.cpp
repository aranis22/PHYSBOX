#include <Arduino.h>

#include "config.h"
#include "diagnostics/HardwareDiagnostic.h"
#include "display/PhysboxDisplay.h"
#include "sensors/ImuSensor.h"
#include "sensors/Microphone.h"

namespace {
PhysboxDisplay display;
ImuSensor imu;
Microphone microphone;
HardwareDiagnostic diagnostic(display, imu, microphone);
}  // namespace

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(1000);
  diagnostic.begin();
}

void loop() {
  diagnostic.update();
  delay(Config::LOOP_DELAY_MS);
}

