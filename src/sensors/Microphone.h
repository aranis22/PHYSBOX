#pragma once

#include <cstdint>

struct MicrophoneReading {
  int16_t raw;
  int32_t peak;
};

class Microphone {
 public:
  bool begin();
  bool read(MicrophoneReading& reading);
 bool available() const;

 private:
  bool available_ = false;
};
