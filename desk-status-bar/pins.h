#pragma once

// =============================================================
// Pin Definitions for Waveshare ESP32-S3-Touch-LCD-3.49
// Display: AXS15231B via QSPI  |  Touch: AXS15231B via I2C
// Resolution: 172 x 640 (portrait) → rotated to 640 x 172
//
// Source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
// =============================================================

// -- QSPI Display (AXS15231B) --
#define LCD_QSPI_CS   9
#define LCD_QSPI_CLK  10
#define LCD_QSPI_D0   11
#define LCD_QSPI_D1   12
#define LCD_QSPI_D2   13
#define LCD_QSPI_D3   14
#define LCD_RST       21

// -- Backlight (active-low: LOW = on, HIGH = off) --
#define LCD_BL_PIN    8

// -- I2C Bus: Touch Controller --
#define TOUCH_SDA     17
#define TOUCH_SCL     18

// -- I2C Bus: Peripherals (IMU, RTC, etc.) --
#define I2C_SDA       47
#define I2C_SCL       48

// -- Touch Interrupt --
#define TOUCH_INT     3

// -- I2S Audio --
#define I2S_BCLK      9
#define I2S_LRCK      7
#define I2S_SDOUT     5
#define I2S_MCLK      16
#define I2S_SDIN      15  // Mic input

// -- Buttons --
#define BTN_PWR       0
#define BTN_PWR_READ  16    // GPIO16 reads HIGH when PWR button pressed

// -- I2C Device Addresses --
#define TCA9554_ADDR  0x20  // I/O expander (power latch, on peripheral bus)
#define QMI8658_ADDR  0x6B  // IMU
#define PCF85063_ADDR 0x51  // RTC
#define ES8311_ADDR   0x18  // Audio codec
#define TOUCH_ADDR    0x3B  // AXS15231B touch

// -- Battery ADC --
// Battery voltage is on ADC1 Channel 3 (GPIO 4), behind a 3:1 divider.
// TCA9554 pin 1 must be set LOW to enable the voltage divider.
#define BAT_ADC_PIN      4
#define BAT_DIVIDER      3.0f   // voltage divider ratio

// -- TCA9554 I/O Expander Pin Assignments --
#define TCA9554_ADC_EN   1      // LOW = enable battery voltage divider
#define TCA9554_PWR_PIN  6      // HIGH = power latch on, LOW = power off

// -- Display Dimensions --
#define LCD_WIDTH     172
#define LCD_HEIGHT    640

// After rotation (landscape bar mode):
#define SCREEN_W      640
#define SCREEN_H      172
