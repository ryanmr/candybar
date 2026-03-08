# Power Detection & Charging — Waveshare ESP32-S3-Touch-LCD-3.49

## Charger IC: ETA6098

The board uses an **ETA6098** switching Li-Ion battery charger (U2 in schematic). Its STAT output is wired to TCA9554 port bit P5 (EXIO5).

## Complete TCA9554 Pin Map (P0–P7)

| Port Bit | Net Name | Function |
|----------|----------|----------|
| P0 (EXIO0) | BL_EN | Backlight enable |
| P1 (EXIO1) | ADC_EN | Battery voltage divider enable (LOW = enable) |
| P2 (EXIO2) | IMU_INT2 | QMI8658 IMU interrupt 2 (input) |
| P3 (EXIO3) | RTC_INT | PCF85063 RTC interrupt (input) |
| P4 (EXIO4) | LCD_TE | Display tearing effect signal (input) |
| **P5 (EXIO5)** | **STAT** | **ETA6098 charger status (input)** |
| P6 (EXIO6) | SYS_EN | Power latch (HIGH = on, LOW = off) |
| P7 (EXIO7) | PA_EN | Speaker amplifier enable |

## Detecting USB vs Battery Power

### Method 1: GPIO 16 (BTN_PWR_READ)

GPIO 16 is connected to the power button circuit with a Schottky diode and pull-up arrangement. Waveshare's reference code (`07_BATT_PWR_Test`, `11_FactoryProgram`) uses it to detect power source:

- **LOW** = running on USB power (no button press needed to boot)
- **HIGH** = running on battery (power button was pressed to latch)

```cpp
bool isOnUSBPower() {
    return digitalRead(BTN_PWR_READ) == LOW;
}
```

This pin is already defined as `BTN_PWR_READ` (GPIO 16) in `pins.h` and used for long-press power-off detection.

### Method 2: TCA9554 P5 — Charger STAT Pin

The ETA6098 STAT pin is open-drain:

- **LOW** = actively charging (USB power present, battery not full)
- **HIGH-Z** (floats high via pull-up) = not charging (USB disconnected OR battery fully charged)

```cpp
#define TCA9554_CHRG_STAT 5

bool isCharging() {
    // Ensure P5 is configured as input
    uint8_t config = tca9554ReadReg(TCA_REG_CONFIG);
    config |= (1 << TCA9554_CHRG_STAT);
    tca9554WriteReg(TCA_REG_CONFIG, config);

    // Read input register, STAT is active-low
    uint8_t input = tca9554ReadReg(0x00);  // TCA_REG_INPUT = 0x00
    return !(input & (1 << TCA9554_CHRG_STAT));
}
```

### Combined State Table

| GPIO 16 | TCA9554 P5 | Meaning |
|---------|------------|---------|
| LOW | LOW | USB power, actively charging |
| LOW | HIGH | USB power, battery full |
| HIGH | LOW | Battery power, charging (unusual) |
| HIGH | any | Battery power |

## ESP32-S3 USB Host Detection (Software-Only)

The USB Serial/JTAG controller receives SOF packets every 1ms from a connected USB host. The frame counter register can detect an active USB data connection (not just VBUS power):

```cpp
#include "soc/usb_serial_jtag_reg.h"

bool isUSBHostConnected() {
    static uint32_t lastFrame = 0;
    uint32_t frame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & 0x7FF;
    bool connected = (frame != lastFrame);
    lastFrame = frame;
    return connected;
}
```

Note: This detects a USB **host** sending data, not merely VBUS 5V presence. A dumb USB charger without data lines won't trigger this.

## References

- Waveshare schematic: https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49/ESP32-S3-Touch-LCD-3.49-Schematic.pdf
- Waveshare reference repo: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
- Battery/power test example: `Examples/Arduino/07_BATT_PWR_Test/`
- Factory program: `Examples/ESP-IDF/11_FactoryProgram/components/user_app/user_app.cpp`
