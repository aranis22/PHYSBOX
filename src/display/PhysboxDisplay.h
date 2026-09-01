#pragma once

#include <Adafruit_GFX.h>

class PhysboxDisplay final : public Adafruit_GFX {
 public:
  PhysboxDisplay();

  void begin();
  void setRotation(uint8_t rotation) override;
  void startWrite() override;
  void endWrite() override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     uint16_t color) override;

 private:
  void command(uint8_t value);
  void commandData(uint8_t command, const uint8_t* data, size_t length);
  void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
  static void colorTo666(uint16_t color, uint8_t& red, uint8_t& green,
                         uint8_t& blue);
  void sendColor(uint16_t color);
};
