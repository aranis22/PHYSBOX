#include "diagnostics/HardwareDiagnostic.h"

#include <Arduino.h>

#include "colors.h"

HardwareDiagnostic::HardwareDiagnostic(PhysboxDisplay& display, ImuSensor& imu,
                                       Microphone& microphone)
    : display_(display), imu_(imu), microphone_(microphone) {}

void HardwareDiagnostic::begin() {
  Serial.println();
  Serial.println("PHYSBOX bring-up");

  Serial.println("Starting TFT...");
  display_.begin();
  display_.setTextSize(2);
  Serial.println("TFT OK");

  Serial.println("Starting ICM-42670-P...");
  if (imu_.begin()) {
    Serial.println("IMU OK");
  } else {
    Serial.print("IMU FAILED: ");
    Serial.println(imu_.status());
  }

  Serial.println("Starting PDM microphone...");
  if (microphone_.begin()) {
    Serial.println("MIC OK");
  } else {
    Serial.println("MIC FAILED");
  }

  drawScreen();
  delay(500);
}

void HardwareDiagnostic::update() {
  ImuReading imuReading{};
  if (imu_.read(imuReading)) {
    printValue(75, "ACC X:", imuReading.accelX);
    printValue(100, "ACC Y:", imuReading.accelY);
    printValue(125, "ACC Z:", imuReading.accelZ);
    printValue(160, "GYR X:", imuReading.gyroX);
    printValue(185, "GYR Y:", imuReading.gyroY);
    printValue(210, "GYR Z:", imuReading.gyroZ);

    Serial.print("A: ");
    Serial.print(imuReading.accelX);
    Serial.print(' ');
    Serial.print(imuReading.accelY);
    Serial.print(' ');
    Serial.print(imuReading.accelZ);
    Serial.print("   G: ");
    Serial.print(imuReading.gyroX);
    Serial.print(' ');
    Serial.print(imuReading.gyroY);
    Serial.print(' ');
    Serial.print(imuReading.gyroZ);
  }

  MicrophoneReading microphoneReading{};
  if (microphone_.read(microphoneReading)) {
    printValue(255, "MIC RAW:", microphoneReading.raw);
    printValue(280, "MIC PEAK:", microphoneReading.peak);

    Serial.print("   MIC raw: ");
    Serial.print(microphoneReading.raw);
    Serial.print(" peak: ");
    Serial.println(microphoneReading.peak);
  }
}

void HardwareDiagnostic::drawScreen() {
  display_.fillScreen(Colors::BLACK);
  display_.setTextSize(2);
  display_.setCursor(15, 10);
  display_.setTextColor(Colors::YELLOW, Colors::BLACK);
  display_.println("PHYSBOX PCB DIAGNOSTIC");

  display_.setCursor(15, 40);
  display_.setTextColor(imu_.available() ? Colors::GREEN : Colors::RED,
                        Colors::BLACK);
  display_.print(imu_.available() ? "IMU: OK" : "IMU: FAIL");

  display_.setCursor(250, 40);
  display_.setTextColor(microphone_.available() ? Colors::GREEN : Colors::RED,
                        Colors::BLACK);
  display_.print(microphone_.available() ? "MIC: OK" : "MIC: FAIL");
  display_.setTextColor(Colors::WHITE, Colors::BLACK);
}

void HardwareDiagnostic::printValue(int y, const char* label, long value) {
  display_.fillRect(140, y, 330, 25, Colors::BLACK);
  display_.setCursor(20, y);
  display_.setTextColor(Colors::CYAN, Colors::BLACK);
  display_.print(label);
  display_.setCursor(150, y);
  display_.setTextColor(Colors::WHITE, Colors::BLACK);
  display_.print(value);
}
