# Waveshare ESP32-S3-Touch-LCD-3.49 — Pin Layout

## How This Was Discovered

The original pin mapping (CS=45, CLK=47, D0=21, D1=48, D2=40, D3=39) came from
the **JC3248W535EN** board — a *different* AXS15231B-based board by a different
manufacturer. It was wrong for this board.

The correct pins were found in the official Waveshare GitHub repo:
https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49

Specifically from `Examples/Arduino/09_LVGL_V8_Test/user_config.h`.

This was independently confirmed by a working ESPHome configuration in the
Home Assistant community forums.

## Correct QSPI Display Pins

| Signal | GPIO |
|--------|------|
| CS     | 9    |
| CLK    | 10   |
| D0     | 11   |
| D1     | 12   |
| D2     | 13   |
| D3     | 14   |
| RST    | 21   |

## Backlight

- **GPIO 8**, active-low (LOW = on, HIGH = off)
- Waveshare factory uses LEDC PWM at 50kHz, 8-bit resolution
- Duty 0 = full brightness, duty 255 = off

## I2C Buses (two separate buses)

### Touch Controller Bus
| Signal | GPIO |
|--------|------|
| SDA    | 17   |
| SCL    | 18   |
| Address | 0x3B (AXS15231B touch) |

### Peripheral Bus
| Signal | GPIO |
|--------|------|
| SDA    | 47   |
| SCL    | 48   |
| Devices | QMI8658 (0x6B), PCF85063 (0x51) |

## SPI Configuration

- **SPI Host**: SPI3_HOST (Arduino_GFX defaults to SPI2_HOST — must override)
- **SPI Mode**: Mode 0 works with Arduino_GFX (Waveshare factory uses Mode 3 with their own driver, but Arduino_GFX's QSPI implementation works with Mode 0)
- Override before include: `#define ESP32QSPI_SPI_HOST SPI3_HOST`

## Key Gotchas

1. **No AXP2101** power management IC on this board (all I2C reads return 0x00)
2. **SD card pins (11, 13, 14) overlap with QSPI display pins** — cannot use both simultaneously
3. The RST pin (GPIO 21) is connected; using it for hardware reset gives more reliable display init than software reset alone
4. USB CDC re-enumerates on ESP32-S3 reset — serial messages in the first ~3 seconds after boot are lost
