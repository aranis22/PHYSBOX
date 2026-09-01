#include "display/PhysboxDisplay.h"

#include <Arduino.h>
#include <SPI.h>

#include "colors.h"
#include "config.h"
#include "pins.h"

SPIClass tftSPI(FSPI);

PhysboxDisplay::PhysboxDisplay() : Adafruit_GFX(320, 480) {}

void PhysboxDisplay::begin() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);

  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);

  tftSPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);

  digitalWrite(TFT_RST, HIGH);
  delay(20);
  digitalWrite(TFT_RST, LOW);
  delay(30);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  // Known-good ILI9488 initialization sequence from the bring-up sketch.
  const uint8_t gammaPositive[] = {
      0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
      0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F};
  const uint8_t gammaNegative[] = {
      0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
      0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F};
  commandData(0xE0, gammaPositive, sizeof(gammaPositive));
  commandData(0xE1, gammaNegative, sizeof(gammaNegative));

  uint8_t powerControl1[] = {0x17, 0x15};
  uint8_t powerControl2[] = {0x41};
  uint8_t vcomControl[] = {0x00, 0x12, 0x80};
  uint8_t pixelFormat = 0x66;  // RGB666 / 18-bit
  uint8_t interfaceMode = 0x00;
  uint8_t frameRate = 0xA0;
  uint8_t inversionControl = 0x02;
  uint8_t displayFunction[] = {0x02, 0x02, 0x3B};
  uint8_t entryMode = 0xC6;
  uint8_t adjustControl[] = {0xA9, 0x51, 0x2C, 0x82};

  commandData(0xC0, powerControl1, sizeof(powerControl1));
  commandData(0xC1, powerControl2, sizeof(powerControl2));
  commandData(0xC5, vcomControl, sizeof(vcomControl));
  commandData(0x3A, &pixelFormat, 1);
  commandData(0xB0, &interfaceMode, 1);
  commandData(0xB1, &frameRate, 1);
  commandData(0xB4, &inversionControl, 1);
  commandData(0xB6, displayFunction, sizeof(displayFunction));
  commandData(0xB7, &entryMode, 1);
  commandData(0xF7, adjustControl, sizeof(adjustControl));

  command(0x11);  // Sleep out.
  delay(150);
  command(0x29);  // Display on.
  delay(50);

  setRotation(1);  // 480 x 320 landscape.
  fillScreen(Colors::BLACK);
}

void PhysboxDisplay::setRotation(uint8_t rotation) {
  rotation &= 3;
  Adafruit_GFX::setRotation(rotation);

  uint8_t madctl;
  switch (rotation) {
    case 0:
      madctl = 0x48;
      break;
    case 1:
      madctl = 0x28;
      break;
    case 2:
      madctl = 0x88;
      break;
    default:
      madctl = 0xE8;
      break;
  }
  commandData(0x36, &madctl, 1);
}

void PhysboxDisplay::startWrite() {
  tftSPI.beginTransaction(SPISettings(Config::TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
}

void PhysboxDisplay::endWrite() {
  digitalWrite(TFT_CS, HIGH);
  tftSPI.endTransaction();
}

void PhysboxDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= width() || y >= height()) {
    return;
  }
  startWrite();
  writePixel(x, y, color);
  endWrite();
}

void PhysboxDisplay::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= width() || y >= height()) {
    return;
  }
  setWindow(x, y, x, y);
  sendColor(color);
}

void PhysboxDisplay::fillRect(int16_t x, int16_t y, int16_t width,
                              int16_t height, uint16_t color) {
  startWrite();
  writeFillRect(x, y, width, height, color);
  endWrite();
}

void PhysboxDisplay::writeFillRect(int16_t x, int16_t y, int16_t width,
                                   int16_t height, uint16_t color) {
  if (width <= 0 || height <= 0) {
    return;
  }
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > this->width()) {
    width = this->width() - x;
  }
  if (y + height > this->height()) {
    height = this->height() - y;
  }
  if (width <= 0 || height <= 0) {
    return;
  }

  setWindow(x, y, x + width - 1, y + height - 1);
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  colorTo666(color, red, green, blue);

  const uint32_t count = static_cast<uint32_t>(width) * height;
  for (uint32_t i = 0; i < count; ++i) {
    tftSPI.transfer(red);
    tftSPI.transfer(green);
    tftSPI.transfer(blue);
  }
}

void PhysboxDisplay::command(uint8_t value) {
  tftSPI.beginTransaction(SPISettings(Config::TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(value);
  digitalWrite(TFT_CS, HIGH);
  tftSPI.endTransaction();
}

void PhysboxDisplay::commandData(uint8_t commandValue, const uint8_t* data,
                                 size_t length) {
  tftSPI.beginTransaction(SPISettings(Config::TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(commandValue);
  digitalWrite(TFT_DC, HIGH);
  for (size_t i = 0; i < length; ++i) {
    tftSPI.transfer(data[i]);
  }
  digitalWrite(TFT_CS, HIGH);
  tftSPI.endTransaction();
}

void PhysboxDisplay::setWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                               uint16_t y1) {
  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(0x2A);  // CASET
  digitalWrite(TFT_DC, HIGH);
  tftSPI.transfer(x0 >> 8);
  tftSPI.transfer(x0);
  tftSPI.transfer(x1 >> 8);
  tftSPI.transfer(x1);

  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(0x2B);  // PASET
  digitalWrite(TFT_DC, HIGH);
  tftSPI.transfer(y0 >> 8);
  tftSPI.transfer(y0);
  tftSPI.transfer(y1 >> 8);
  tftSPI.transfer(y1);

  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(0x2C);  // RAMWR
  digitalWrite(TFT_DC, HIGH);
}

void PhysboxDisplay::colorTo666(uint16_t color, uint8_t& red, uint8_t& green,
                                uint8_t& blue) {
  red = ((color >> 11) & 0x1F) << 3;
  green = ((color >> 5) & 0x3F) << 2;
  blue = (color & 0x1F) << 3;
}

void PhysboxDisplay::sendColor(uint16_t color) {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  colorTo666(color, red, green, blue);
  tftSPI.transfer(red);
  tftSPI.transfer(green);
  tftSPI.transfer(blue);
}
