# PHYSBOX firmware

PHYSBOX is offline-first physics laboratory firmware for an
ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM). This PlatformIO/Arduino
baseline contains the proven custom PCB hardware interfaces for the ILI9488
display, ICM-42670-P IMU, and SPH0641LU4H-1 PDM microphone. The active app is
an IMU-controlled puck on a coordinate plane, with live acceleration and
gyroscope values.

## Hardware and pins

| Function | GPIO |
|---|---:|
| PDM microphone clock / data | 4 / 6 |
| IMU INT1 / SDA / SCL | 7 / 8 / 9 |
| TFT CS / MOSI / SCK / MISO | 10 / 11 / 12 / 13 |
| TFT DC / reset / backlight | 14 / 15 / 16 |
| Touch controller CS | 17 |

The ILI9488 uses a proven custom Adafruit_GFX-derived SPI driver at 10 MHz in
480 x 320 landscape mode with RGB666 transfers. The IMU is configured for
100 Hz, +/-16 g acceleration, and +/-2000 dps gyroscope data. The microphone
uses the modern `ESP_I2S.h` PDM RX API at 16 kHz, 16-bit mono.

## Tools and commands

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
or the PlatformIO IDE extension. Dependencies and the stable Arduino-ESP32
toolchain are pinned in `platformio.ini` and download automatically.

```powershell
pio run
pio run -t upload
pio device monitor
```

If `pio` is not in `PATH` on Windows, use:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

Do not upload until the intended ESP32-S3 serial port is identified. An
explicit port can be selected with `pio run -t upload --upload-port COMx`.

## Source layout

- `include/` contains canonical pins, firmware constants, and colors.
- `prototype/imu_ball.py` is the standalone 480 x 320 Pygame prototype.
- `src/apps/` contains the embedded puck physics and efficient TFT renderer.
- `src/display/` contains the raw ILI9488 driver.
- `src/sensors/` contains the IMU and PDM microphone modules.
- `src/diagnostics/` retains the reusable hardware diagnostic screen.
- `src/main.cpp` only wires the modules into Arduino `setup()` and `loop()`.
- `working_physbox/working_physbox.ino` remains the unmodified, known-good
  hardware reference and is not part of the PlatformIO build.
