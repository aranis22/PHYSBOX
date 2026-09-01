#include "apps/ImuBallApp.h"

#include <Arduino.h>
#include <cmath>
#include <cstdio>

namespace {
// Physics tuning. Values are intentionally grouped for physical adjustment.
constexpr float BALL_ACCEL_SCALE = 420.0f;  // pixels / second^2 / g
constexpr float BALL_DAMPING = 2.15f;       // exponential velocity damping
constexpr float BALL_BOUNCE = 0.58f;
constexpr float BALL_MAX_SPEED = 310.0f;  // pixels / second
constexpr float IMU_FILTER_ALPHA = 0.14f;
constexpr float IMU_DEADZONE = 0.025f;  // g

// The ICM-42670-P is configured for +/-16 g and +/-2000 dps.
constexpr float ACCEL_LSB_PER_G = 2048.0f;
constexpr float GYRO_LSB_PER_DPS = 16.4f;

// Initial board-to-landscape mapping. These signs are isolated for physical
// direction validation without touching the proven IMU or TFT drivers.
constexpr float SCREEN_X_FROM_ACCEL_X = -1.0f;
constexpr float SCREEN_Y_FROM_ACCEL_Y = 1.0f;

constexpr uint32_t FRAME_INTERVAL_MS = 33;
constexpr uint32_t VALUE_INTERVAL_MS = 500;
constexpr uint32_t SERIAL_INTERVAL_MS = 1000;
constexpr uint8_t CALIBRATION_SAMPLES = 32;
constexpr uint16_t CALIBRATION_MAX_ATTEMPTS = 200;

constexpr int16_t PLOT_LEFT = 10;
constexpr int16_t PLOT_TOP = 10;
constexpr int16_t PLOT_WIDTH = 310;
constexpr int16_t PLOT_HEIGHT = 300;
constexpr int16_t PLOT_RIGHT = PLOT_LEFT + PLOT_WIDTH - 1;
constexpr int16_t PLOT_BOTTOM = PLOT_TOP + PLOT_HEIGHT - 1;
constexpr int16_t ORIGIN_X = PLOT_LEFT + PLOT_WIDTH / 2;
constexpr int16_t ORIGIN_Y = PLOT_TOP + PLOT_HEIGHT / 2;
constexpr int16_t GRID_SPACING = 30;
constexpr int16_t PUCK_RADIUS = 12;
constexpr int16_t PUCK_DIRTY_RADIUS = 15;

constexpr uint16_t COLOR_BACKGROUND = 0xF7DF;
constexpr uint16_t COLOR_PLOT = 0xFFFF;
constexpr uint16_t COLOR_GRID = 0xDF3C;
constexpr uint16_t COLOR_AXIS = 0x5C98;
constexpr uint16_t COLOR_BORDER = 0xADFB;
constexpr uint16_t COLOR_TEXT = 0x29AE;
constexpr uint16_t COLOR_MUTED = 0x6BCF;
constexpr uint16_t COLOR_PUCK = 0xCA67;
constexpr uint16_t COLOR_PUCK_DARK = 0x99A5;
constexpr uint16_t COLOR_PUCK_LIGHT = 0xEBED;
constexpr uint16_t COLOR_SHADOW = 0xB618;

float applyDeadzone(float value) {
  if (fabsf(value) <= IMU_DEADZONE) {
    return 0.0f;
  }
  return copysignf((fabsf(value) - IMU_DEADZONE) / (1.0f - IMU_DEADZONE),
                   value);
}

float clampFloat(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}
}  // namespace

ImuBallApp::ImuBallApp(PhysboxDisplay& display, ImuSensor& imu)
    : display_(display), imu_(imu) {}

bool ImuBallApp::begin() {
  Serial.println();
  Serial.println("PHYSBOX IMU puck");
  Serial.println("Starting TFT...");
  display_.begin();
  Serial.println("TFT OK");

  display_.fillScreen(COLOR_BACKGROUND);
  display_.setTextWrap(false);
  display_.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  display_.setTextSize(2);
  display_.setCursor(342, 145);
  display_.print("STARTING");

  Serial.println("Starting ICM-42670-P...");
  if (!imu_.begin()) {
    Serial.print("IMU FAILED: ");
    Serial.println(imu_.status());
    display_.fillRect(330, 130, 150, 55, COLOR_BACKGROUND);
    display_.setTextColor(0xF800, COLOR_BACKGROUND);
    display_.setCursor(342, 145);
    display_.print("IMU FAIL");
    return false;
  }
  Serial.println("IMU OK");

  if (!calibrate()) {
    Serial.println("IMU calibration failed");
    return false;
  }

  drawStaticScreen();
  drawPuck(previousPuckX_, previousPuckY_);
  drawValues();

  previousFrameMs_ = millis();
  previousValuesMs_ = previousFrameMs_;
  previousSerialMs_ = previousFrameMs_;
  fpsWindowStartMs_ = previousFrameMs_;
  Serial.print("Axis mapping: screen X = -accel X, screen Y = +accel Y; offsets ");
  Serial.print(accelOffsetX_, 4);
  Serial.print(", ");
  Serial.println(accelOffsetY_, 4);
  return true;
}

bool ImuBallApp::calibrate() {
  display_.fillRect(330, 120, 150, 75, COLOR_BACKGROUND);
  display_.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display_.setTextSize(1);
  display_.setCursor(342, 132);
  display_.print("KEEP PHYSBOX LEVEL");
  display_.setCursor(342, 151);
  display_.print("CALIBRATING IMU...");

  int32_t sumX = 0;
  int32_t sumY = 0;
  uint8_t collected = 0;
  uint16_t attempts = 0;
  while (collected < CALIBRATION_SAMPLES &&
         attempts < CALIBRATION_MAX_ATTEMPTS) {
    ++attempts;
    if (imu_.read(latestReading_)) {
      sumX += latestReading_.accelX;
      sumY += latestReading_.accelY;
      ++collected;
    }
    delay(10);
  }
  if (collected != CALIBRATION_SAMPLES) {
    return false;
  }
  accelOffsetX_ = static_cast<float>(sumX) /
                  (CALIBRATION_SAMPLES * ACCEL_LSB_PER_G);
  accelOffsetY_ = static_cast<float>(sumY) /
                  (CALIBRATION_SAMPLES * ACCEL_LSB_PER_G);
  return true;
}

void ImuBallApp::update() {
  const uint32_t nowMs = millis();
  if (nowMs - previousFrameMs_ < FRAME_INTERVAL_MS) {
    return;
  }

  float dtSeconds = (nowMs - previousFrameMs_) * 0.001f;
  previousFrameMs_ = nowMs;
  if (dtSeconds > 0.05f) {
    dtSeconds = 0.05f;
  }

  if (!imu_.read(latestReading_)) {
    return;
  }

  const float rawX = latestReading_.accelX / ACCEL_LSB_PER_G;
  const float rawY = latestReading_.accelY / ACCEL_LSB_PER_G;
  const float mappedX = SCREEN_X_FROM_ACCEL_X * (rawX - accelOffsetX_);
  const float mappedY = SCREEN_Y_FROM_ACCEL_Y * (rawY - accelOffsetY_);
  filteredAccelX_ += (mappedX - filteredAccelX_) * IMU_FILTER_ALPHA;
  filteredAccelY_ += (mappedY - filteredAccelY_) * IMU_FILTER_ALPHA;

  advancePhysics(dtSeconds, applyDeadzone(filteredAccelX_),
                 applyDeadzone(filteredAccelY_));

  const int16_t puckX = static_cast<int16_t>(lroundf(positionX_));
  const int16_t puckY = static_cast<int16_t>(lroundf(positionY_));
  if (puckX != previousPuckX_ || puckY != previousPuckY_) {
    restorePlotRegion(previousPuckX_ - PUCK_DIRTY_RADIUS,
                      previousPuckY_ - PUCK_DIRTY_RADIUS,
                      PUCK_DIRTY_RADIUS * 2 + 3,
                      PUCK_DIRTY_RADIUS * 2 + 3);
    drawPuck(puckX, puckY);
    previousPuckX_ = puckX;
    previousPuckY_ = puckY;
  }

  if (nowMs - previousValuesMs_ >= VALUE_INTERVAL_MS) {
    drawValues();
    // Schedule from completion so a slow text pass can never make the next
    // pass immediately overdue and starve puck frames.
    previousValuesMs_ = millis();
  }

  updateFps(nowMs);
  if (nowMs - previousSerialMs_ >= SERIAL_INTERVAL_MS) {
    previousSerialMs_ = nowMs;
    Serial.print("IMU g mapped: ");
    Serial.print(mappedX, 3);
    Serial.print(' ');
    Serial.print(mappedY, 3);
    Serial.print("  puck: ");
    Serial.print(puckX);
    Serial.print(' ');
    Serial.print(puckY);
    Serial.print("  fps: ");
    Serial.println(measuredFps_, 1);
  }
}

void ImuBallApp::drawStaticScreen() {
  display_.fillScreen(COLOR_BACKGROUND);
  display_.fillRect(PLOT_LEFT, PLOT_TOP, PLOT_WIDTH, PLOT_HEIGHT, COLOR_PLOT);
  restorePlotRegion(PLOT_LEFT, PLOT_TOP, PLOT_WIDTH, PLOT_HEIGHT);
  display_.drawRect(PLOT_LEFT, PLOT_TOP, PLOT_WIDTH, PLOT_HEIGHT, COLOR_BORDER);

  display_.setTextSize(1);
  display_.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display_.setCursor(340, 23);
  display_.print("ACCELERATION  g");
  display_.setCursor(340, 138);
  display_.print("GYROSCOPE  d/s");
  display_.setCursor(340, 286);
  display_.print("IMU PUCK");
}

void ImuBallApp::restorePlotRegion(int16_t x, int16_t y, int16_t width,
                                   int16_t height) {
  const int16_t clippedX = x > PLOT_LEFT + 1 ? x : PLOT_LEFT + 1;
  const int16_t clippedY = y > PLOT_TOP + 1 ? y : PLOT_TOP + 1;
  const int16_t right = x + width - 1;
  const int16_t bottom = y + height - 1;
  const int16_t clippedRight = right < PLOT_RIGHT - 1 ? right : PLOT_RIGHT - 1;
  const int16_t clippedBottom =
      bottom < PLOT_BOTTOM - 1 ? bottom : PLOT_BOTTOM - 1;
  if (clippedX > clippedRight || clippedY > clippedBottom) {
    return;
  }

  display_.fillRect(clippedX, clippedY, clippedRight - clippedX + 1,
                    clippedBottom - clippedY + 1, COLOR_PLOT);

  for (int16_t gridX = ORIGIN_X; gridX <= PLOT_RIGHT; gridX += GRID_SPACING) {
    if (gridX >= clippedX && gridX <= clippedRight) {
      display_.fillRect(gridX, clippedY, gridX == ORIGIN_X ? 2 : 1,
                        clippedBottom - clippedY + 1,
                        gridX == ORIGIN_X ? COLOR_AXIS : COLOR_GRID);
    }
  }
  for (int16_t gridX = ORIGIN_X - GRID_SPACING; gridX >= PLOT_LEFT;
       gridX -= GRID_SPACING) {
    if (gridX >= clippedX && gridX <= clippedRight) {
      display_.fillRect(gridX, clippedY, 1, clippedBottom - clippedY + 1,
                        COLOR_GRID);
    }
  }
  for (int16_t gridY = ORIGIN_Y; gridY <= PLOT_BOTTOM; gridY += GRID_SPACING) {
    if (gridY >= clippedY && gridY <= clippedBottom) {
      display_.fillRect(clippedX, gridY, clippedRight - clippedX + 1,
                        gridY == ORIGIN_Y ? 2 : 1,
                        gridY == ORIGIN_Y ? COLOR_AXIS : COLOR_GRID);
    }
  }
  for (int16_t gridY = ORIGIN_Y - GRID_SPACING; gridY >= PLOT_TOP;
       gridY -= GRID_SPACING) {
    if (gridY >= clippedY && gridY <= clippedBottom) {
      display_.fillRect(clippedX, gridY, clippedRight - clippedX + 1, 1,
                        COLOR_GRID);
    }
  }
}

void ImuBallApp::drawPuck(int16_t x, int16_t y) {
  display_.fillCircle(x + 2, y + 3, PUCK_RADIUS + 1, COLOR_SHADOW);
  display_.fillCircle(x, y, PUCK_RADIUS, COLOR_PUCK_DARK);
  display_.fillCircle(x, y, PUCK_RADIUS - 1, COLOR_PUCK);
  display_.fillCircle(x - 4, y - 4, 3, COLOR_PUCK_LIGHT);
}

void ImuBallApp::drawValues() {
  const float accelX = latestReading_.accelX / ACCEL_LSB_PER_G;
  const float accelY = latestReading_.accelY / ACCEL_LSB_PER_G;
  const float accelZ = latestReading_.accelZ / ACCEL_LSB_PER_G;
  const float gyroX = latestReading_.gyroX / GYRO_LSB_PER_DPS;
  const float gyroY = latestReading_.gyroY / GYRO_LSB_PER_DPS;
  const float gyroZ = latestReading_.gyroZ / GYRO_LSB_PER_DPS;

  drawValueLine(43, 'X', accelX);
  drawValueLine(69, 'Y', accelY);
  drawValueLine(95, 'Z', accelZ);
  drawValueLine(158, 'X', gyroX);
  drawValueLine(184, 'Y', gyroY);
  drawValueLine(210, 'Z', gyroZ);
}

void ImuBallApp::drawValueLine(int16_t y, char axis, float value) {
  char formatted[16];
  snprintf(formatted, sizeof(formatted), "%c%+6.2f", axis, value);
  display_.fillRect(338, y, 142, 18, COLOR_BACKGROUND);
  display_.setTextSize(2);
  // The line was just cleared, so transparent glyphs avoid rewriting every
  // background pixel through the small-font renderer.
  display_.setTextColor(COLOR_TEXT);
  display_.setCursor(340, y + 1);
  display_.print(formatted);
}

void ImuBallApp::advancePhysics(float dtSeconds, float accelX, float accelY) {
  velocityX_ += accelX * BALL_ACCEL_SCALE * dtSeconds;
  velocityY_ += accelY * BALL_ACCEL_SCALE * dtSeconds;

  const float damping = expf(-BALL_DAMPING * dtSeconds);
  velocityX_ *= damping;
  velocityY_ *= damping;

  const float speed = sqrtf(velocityX_ * velocityX_ + velocityY_ * velocityY_);
  if (speed > BALL_MAX_SPEED) {
    const float scale = BALL_MAX_SPEED / speed;
    velocityX_ *= scale;
    velocityY_ *= scale;
  }

  positionX_ += velocityX_ * dtSeconds;
  positionY_ += velocityY_ * dtSeconds;

  constexpr float minX = PLOT_LEFT + PUCK_RADIUS;
  constexpr float maxX = PLOT_RIGHT - PUCK_RADIUS;
  constexpr float minY = PLOT_TOP + PUCK_RADIUS;
  constexpr float maxY = PLOT_BOTTOM - PUCK_RADIUS;
  if (positionX_ < minX || positionX_ > maxX) {
    positionX_ = clampFloat(positionX_, minX, maxX);
    velocityX_ *= -BALL_BOUNCE;
  }
  if (positionY_ < minY || positionY_ > maxY) {
    positionY_ = clampFloat(positionY_, minY, maxY);
    velocityY_ *= -BALL_BOUNCE;
  }
}

void ImuBallApp::updateFps(uint32_t nowMs) {
  ++frameCount_;
  const uint32_t elapsed = nowMs - fpsWindowStartMs_;
  if (elapsed >= 1000) {
    measuredFps_ = frameCount_ * 1000.0f / elapsed;
    frameCount_ = 0;
    fpsWindowStartMs_ = nowMs;
  }
}
