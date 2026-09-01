#include <Arduino.h>

#include "apps/ImuBallApp.h"
#include "config.h"
#include "display/PhysboxDisplay.h"
#include "sensors/ImuSensor.h"

namespace {
PhysboxDisplay display;
ImuSensor imu;
ImuBallApp app(display, imu);
}  // namespace

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(1000);
  app.begin();
}

void loop() {
  app.update();
  delay(1);
}
