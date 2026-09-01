#include "sensors/Microphone.h"

#include <Arduino.h>
#include <ESP_I2S.h>

#include "config.h"
#include "pins.h"

// Match the proven sketch's standalone PDM object and sample buffer.
I2SClass PDM;
int16_t micBuffer[Config::MIC_BUFFER_SAMPLES];

bool Microphone::begin() {
  PDM.setPinsPdmRx(MIC_CLK, MIC_DATA);
  available_ = PDM.begin(I2S_MODE_PDM_RX, Config::MIC_SAMPLE_RATE_HZ,
                         I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  return available_;
}

bool Microphone::read(MicrophoneReading& reading) {
  if (!available_) {
    return false;
  }

  size_t bytesRead =
      PDM.readBytes(reinterpret_cast<char*>(micBuffer), sizeof(micBuffer));
  int sampleCount = bytesRead / sizeof(int16_t);
  if (sampleCount <= 0) {
    return false;
  }

  reading.raw = micBuffer[sampleCount - 1];
  reading.peak = 0;
  for (int i = 0; i < sampleCount; ++i) {
    int32_t magnitude = micBuffer[i];
    if (magnitude < 0) {
      magnitude = -magnitude;
    }
    if (magnitude > reading.peak) {
      reading.peak = magnitude;
    }
  }
  return true;
}

bool Microphone::available() const { return available_; }
