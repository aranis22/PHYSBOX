#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <ESP_I2S.h>
#include <ICM42670P.h>

// ============================================================
// PHYSBOX PIN MAP
// ============================================================

// ---------- TFT ILI9488 ----------
#define TFT_CS       10
#define TFT_RST      15
#define TFT_DC       14
#define TFT_MOSI     11
#define TFT_SCLK     12
#define TFT_MISO     13
#define TFT_BL       16
#define TOUCH_CS     17

// ---------- ICM-42670-P ----------
#define IMU_SDA       8
#define IMU_SCL       9

// ---------- SPH0641LU4H-1 PDM MIC ----------
#define MIC_CLK       4
#define MIC_DATA      6


// ============================================================
// COLORS - RGB565
// ============================================================

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_YELLOW  0xFFE0


// ============================================================
// RAW ILI9488 + ADAFRUIT GFX WRAPPER
// ============================================================

SPIClass tftSPI(FSPI);

class RawILI9488 : public Adafruit_GFX {

public:

  RawILI9488() : Adafruit_GFX(320, 480) {}

  void begin() {

    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_BL, OUTPUT);

    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(TFT_BL, HIGH);

    tftSPI.begin(
      TFT_SCLK,
      TFT_MISO,
      TFT_MOSI,
      TFT_CS
    );

    // Hardware reset
    digitalWrite(TFT_RST, HIGH);
    delay(20);

    digitalWrite(TFT_RST, LOW);
    delay(30);

    digitalWrite(TFT_RST, HIGH);
    delay(150);


    // ---------- ILI9488 INIT ----------

    const uint8_t gammaP[] = {
      0x00,0x03,0x09,0x08,0x16,
      0x0A,0x3F,0x78,0x4C,0x09,
      0x0A,0x08,0x16,0x1A,0x0F
    };

    const uint8_t gammaN[] = {
      0x00,0x16,0x19,0x03,0x0F,
      0x05,0x32,0x45,0x46,0x04,
      0x0E,0x0D,0x35,0x37,0x0F
    };

    commandData(0xE0, gammaP, 15);
    commandData(0xE1, gammaN, 15);

    uint8_t d1[] = {0x17, 0x15};
    commandData(0xC0, d1, 2);

    uint8_t d2[] = {0x41};
    commandData(0xC1, d2, 1);

    uint8_t d3[] = {0x00, 0x12, 0x80};
    commandData(0xC5, d3, 3);

    // RGB666 / 18-bit required for ILI9488 SPI
    uint8_t pixfmt = 0x66;
    commandData(0x3A, &pixfmt, 1);

    uint8_t b0 = 0x00;
    commandData(0xB0, &b0, 1);

    uint8_t b1 = 0xA0;
    commandData(0xB1, &b1, 1);

    uint8_t b4 = 0x02;
    commandData(0xB4, &b4, 1);

    uint8_t b6[] = {0x02, 0x02, 0x3B};
    commandData(0xB6, b6, 3);

    uint8_t b7 = 0xC6;
    commandData(0xB7, &b7, 1);

    uint8_t f7[] = {0xA9,0x51,0x2C,0x82};
    commandData(0xF7, f7, 4);

    command(0x11);       // sleep out
    delay(150);

    command(0x29);       // display on
    delay(50);

    // Landscape 480 x 320
    setRotation(1);

    fillScreen(C_BLACK);
  }


  void setRotation(uint8_t r) override {

    r &= 3;

    Adafruit_GFX::setRotation(r);

    uint8_t madctl;

    switch (r) {

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


  void startWrite() override {

    tftSPI.beginTransaction(
      SPISettings(
        10000000,
        MSBFIRST,
        SPI_MODE0
      )
    );

    digitalWrite(TFT_CS, LOW);
  }


  void endWrite() override {

    digitalWrite(TFT_CS, HIGH);
    tftSPI.endTransaction();
  }


  void drawPixel(
    int16_t x,
    int16_t y,
    uint16_t color
  ) override {

    if (
      x < 0 ||
      y < 0 ||
      x >= width() ||
      y >= height()
    )
      return;

    startWrite();
    writePixel(x, y, color);
    endWrite();
  }


  void writePixel(
    int16_t x,
    int16_t y,
    uint16_t color
  ) override {

    if (
      x < 0 ||
      y < 0 ||
      x >= width() ||
      y >= height()
    )
      return;

    setWindow(x, y, x, y);

    sendColor(color);
  }


  void fillRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    uint16_t color
  ) override {

    startWrite();
    writeFillRect(x, y, w, h, color);
    endWrite();
  }


  void writeFillRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    uint16_t color
  ) override {

    if (w <= 0 || h <= 0)
      return;

    if (x < 0) {
      w += x;
      x = 0;
    }

    if (y < 0) {
      h += y;
      y = 0;
    }

    if (x + w > width())
      w = width() - x;

    if (y + h > height())
      h = height() - y;

    if (w <= 0 || h <= 0)
      return;

    setWindow(
      x,
      y,
      x + w - 1,
      y + h - 1
    );

    uint8_t r;
    uint8_t g;
    uint8_t b;

    colorTo666(color, r, g, b);

    uint32_t count =
      (uint32_t)w * (uint32_t)h;

    for (uint32_t i = 0; i < count; i++) {
      tftSPI.transfer(r);
      tftSPI.transfer(g);
      tftSPI.transfer(b);
    }
  }


private:

  void command(uint8_t c) {

    tftSPI.beginTransaction(
      SPISettings(10000000, MSBFIRST, SPI_MODE0)
    );

    digitalWrite(TFT_CS, LOW);
    digitalWrite(TFT_DC, LOW);

    tftSPI.transfer(c);

    digitalWrite(TFT_CS, HIGH);

    tftSPI.endTransaction();
  }


  void commandData(
    uint8_t c,
    const uint8_t *data,
    size_t len
  ) {

    tftSPI.beginTransaction(
      SPISettings(10000000, MSBFIRST, SPI_MODE0)
    );

    digitalWrite(TFT_CS, LOW);

    digitalWrite(TFT_DC, LOW);
    tftSPI.transfer(c);

    digitalWrite(TFT_DC, HIGH);

    for (size_t i = 0; i < len; i++)
      tftSPI.transfer(data[i]);

    digitalWrite(TFT_CS, HIGH);

    tftSPI.endTransaction();
  }


  void setWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1
  ) {

    // CASET
    digitalWrite(TFT_DC, LOW);
    tftSPI.transfer(0x2A);

    digitalWrite(TFT_DC, HIGH);

    tftSPI.transfer(x0 >> 8);
    tftSPI.transfer(x0);
    tftSPI.transfer(x1 >> 8);
    tftSPI.transfer(x1);


    // PASET
    digitalWrite(TFT_DC, LOW);
    tftSPI.transfer(0x2B);

    digitalWrite(TFT_DC, HIGH);

    tftSPI.transfer(y0 >> 8);
    tftSPI.transfer(y0);
    tftSPI.transfer(y1 >> 8);
    tftSPI.transfer(y1);


    // RAMWR
    digitalWrite(TFT_DC, LOW);
    tftSPI.transfer(0x2C);

    digitalWrite(TFT_DC, HIGH);
  }


  void colorTo666(
    uint16_t color,
    uint8_t &r,
    uint8_t &g,
    uint8_t &b
  ) {

    r = ((color >> 11) & 0x1F) << 3;
    g = ((color >> 5)  & 0x3F) << 2;
    b = ( color        & 0x1F) << 3;
  }


  void sendColor(uint16_t color) {

    uint8_t r;
    uint8_t g;
    uint8_t b;

    colorTo666(color, r, g, b);

    tftSPI.transfer(r);
    tftSPI.transfer(g);
    tftSPI.transfer(b);
  }
};


RawILI9488 tft;


// ============================================================
// IMU
// ============================================================

// AD0 is grounded on your schematic -> address LSB = 0
ICM42670 IMU(Wire, 0);

bool imuOK = false;


// ============================================================
// PDM MICROPHONE
// ============================================================

// PDM RX is supported on I2S0
I2SClass PDM;

bool micOK = false;

int16_t micBuffer[256];


// ============================================================
// SCREEN HELPERS
// ============================================================

void printValue(
  int y,
  const char *label,
  long value
) {

  // Erase only this row
  tft.fillRect(
    140,
    y,
    330,
    25,
    C_BLACK
  );

  tft.setCursor(20, y);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.print(label);

  tft.setCursor(150, y);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.print(value);
}


void drawScreen() {

  tft.fillScreen(C_BLACK);

  tft.setTextSize(2);

  tft.setCursor(15, 10);
  tft.setTextColor(C_YELLOW, C_BLACK);
  tft.println("PHYSBOX PCB DIAGNOSTIC");

  tft.setCursor(15, 40);

  if (imuOK) {
    tft.setTextColor(C_GREEN, C_BLACK);
    tft.print("IMU: OK");
  }
  else {
    tft.setTextColor(C_RED, C_BLACK);
    tft.print("IMU: FAIL");
  }

  tft.setCursor(250, 40);

  if (micOK) {
    tft.setTextColor(C_GREEN, C_BLACK);
    tft.print("MIC: OK");
  }
  else {
    tft.setTextColor(C_RED, C_BLACK);
    tft.print("MIC: FAIL");
  }

  tft.setTextColor(C_WHITE, C_BLACK);
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("PHYSBOX bring-up");


  // ----------------------------------------------------------
  // TFT
  // ----------------------------------------------------------

  Serial.println("Starting TFT...");

  tft.begin();
  tft.setTextSize(2);

  Serial.println("TFT OK");


  // ----------------------------------------------------------
  // IMU
  // ----------------------------------------------------------

  Serial.println("Starting ICM-42670-P...");

  /*
     IMPORTANT:
     TDK's library internally calls Wire.begin().
     Therefore set the ESP32 default Wire pins FIRST.
  */

  Wire.setPins(
    IMU_SDA,
    IMU_SCL
  );

  int imuStatus = IMU.begin();

  if (imuStatus == 0) {

    IMU.startAccel(
      100,     // 100 Hz
      16       // +/-16 g
    );

    IMU.startGyro(
      100,     // 100 Hz
      2000     // +/-2000 dps
    );

    delay(100);

    imuOK = true;

    Serial.println("IMU OK");
  }
  else {

    Serial.print("IMU FAILED: ");
    Serial.println(imuStatus);
  }


  // ----------------------------------------------------------
  // PDM MIC
  // ----------------------------------------------------------

  Serial.println("Starting PDM microphone...");

  PDM.setPinsPdmRx(
    MIC_CLK,
    MIC_DATA
  );

  micOK = PDM.begin(
    I2S_MODE_PDM_RX,
    16000,
    I2S_DATA_BIT_WIDTH_16BIT,
    I2S_SLOT_MODE_MONO
  );

  if (micOK)
    Serial.println("MIC OK");
  else
    Serial.println("MIC FAILED");


  drawScreen();

  delay(500);
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ==========================================================
  // IMU RAW DATA
  // ==========================================================

  if (imuOK) {

    inv_imu_sensor_event_t evt;

    int result =
      IMU.getDataFromRegisters(evt);

    if (result == 0) {

      printValue(
        75,
        "ACC X:",
        evt.accel[0]
      );

      printValue(
        100,
        "ACC Y:",
        evt.accel[1]
      );

      printValue(
        125,
        "ACC Z:",
        evt.accel[2]
      );

      printValue(
        160,
        "GYR X:",
        evt.gyro[0]
      );

      printValue(
        185,
        "GYR Y:",
        evt.gyro[1]
      );

      printValue(
        210,
        "GYR Z:",
        evt.gyro[2]
      );


      Serial.print("A: ");

      Serial.print(evt.accel[0]);
      Serial.print(" ");

      Serial.print(evt.accel[1]);
      Serial.print(" ");

      Serial.print(evt.accel[2]);

      Serial.print("   G: ");

      Serial.print(evt.gyro[0]);
      Serial.print(" ");

      Serial.print(evt.gyro[1]);
      Serial.print(" ");

      Serial.print(evt.gyro[2]);
    }
  }


  // ==========================================================
  // MICROPHONE RAW DATA
  // ==========================================================

  if (micOK) {

    size_t bytesRead =
      PDM.readBytes(
        (char *)micBuffer,
        sizeof(micBuffer)
      );

    int samples =
      bytesRead / sizeof(int16_t);

    if (samples > 0) {

      int16_t lastSample =
        micBuffer[samples - 1];

      int32_t peak = 0;

      for (int i = 0; i < samples; i++) {

        int32_t v = micBuffer[i];

        if (v < 0)
          v = -v;

        if (v > peak)
          peak = v;
      }


      printValue(
        255,
        "MIC RAW:",
        lastSample
      );

      printValue(
        280,
        "MIC PEAK:",
        peak
      );


      Serial.print("   MIC raw: ");
      Serial.print(lastSample);

      Serial.print(" peak: ");
      Serial.println(peak);
    }
  }

  delay(100);
}