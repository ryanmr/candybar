# Desk Status Bar — ESP32-S3-Touch-LCD-3.49

A WiFi-connected always-on desk HUD built for the Waveshare ESP32-S3-Touch-LCD-3.49's unique 640×172 bar display.

## What It Does

```
┌──────────────┬─────────────────┬────────────────┬─────────────┐
│              │                 │                │             │
│   12:34      │   Monday        │   72°F         │  WiFi OK    │
│      :56 PM  │   Mar 2         │   Clear sky    │  -42 dBm    │
│              │   2026          │   Feels 69°    │             │
│              │   Week 9        │   New York     │  Bat: 85%   │
│              │                 │                │  Up: 3h 24m │
│              │                 │                │             │
├──────────────┴─────────────────┴────────────────┴─────────────┤
│                      [ tap to refresh ]                       │
└───────────────────────────────────────────────────────────────┘
  TIME (160px)   DATE (172px)     WEATHER (152px)  STATUS (122px)
```

Shows time, date, weather (via OpenWeatherMap), WiFi signal, battery level, and uptime — all at a glance on your desk.

## Hardware

- **Board**: Waveshare ESP32-S3-Touch-LCD-3.49 (Case A or B)
- **Display**: 3.49" IPS, 172×640, AXS15231B QSPI
- **Battery**: 18650 (Case A) or 3.7V LiPo (Case B)

## Quick Start

### 1. Install Arduino IDE & Board Support

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended)
2. Add ESP32 board support:
   - Go to **File → Preferences → Additional Boards Manager URLs**
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Go to **Tools → Board → Boards Manager**, search `esp32`, install **esp32 by Espressif Systems**

### 2. Install Libraries

Open **Sketch → Include Library → Manage Libraries** and install:

| Library | Author | Notes |
|---------|--------|-------|
| GFX Library for Arduino | moononournation | Display driver (has AXS15231B support) |
| ArduinoJson | Benoit Blanchon | Weather API JSON parsing |

### 3. Configure

Edit `config.h`:

```cpp
#define WIFI_SSID     "YourNetwork"
#define WIFI_PASS     "YourPassword"

#define OWM_API_KEY   "abc123..."       // free at openweathermap.org/api
#define OWM_CITY      "New York"
#define OWM_UNITS     "imperial"        // or "metric"

#define UTC_OFFSET    -5                // your timezone
#define DST_OFFSET    1                 // 1 during daylight saving, 0 otherwise
```

### 4. Board Settings in Arduino IDE

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Default 4MB with spiffs |

### 5. Upload

1. Connect the board via USB-C
2. Select the correct COM port
3. Click **Upload**
4. If upload fails: hold **BOOT** button, press **RESET**, release **BOOT**, then upload again

## Pin Mapping

> These pins match the Waveshare AXS15231B family (3.5B, 3.49). If the display doesn't initialize, check the schematic on the [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49) and update `pins.h`.

| Function | Pin |
|----------|-----|
| QSPI CS | GPIO 9 |
| QSPI CLK | GPIO 10 |
| QSPI D0 | GPIO 11 |
| QSPI D1 | GPIO 12 |
| QSPI D2 | GPIO 13 |
| QSPI D3 | GPIO 14 |
| LCD RST | GPIO 21 |
| Backlight | GPIO 8 (active-low) |
| Touch SDA | GPIO 17 |
| Touch SCL | GPIO 18 |
| I2C SDA | GPIO 47 |
| I2C SCL | GPIO 48 |

## Project Structure

```
desk-status-bar/
├── desk-status-bar.ino   # Main sketch — WiFi, NTP, weather, rendering
├── config.h              # Your WiFi, API key, timezone settings
├── pins.h                # Hardware pin definitions
└── README.md             # You are here
```

## Troubleshooting

**Display stays blank:**
- Verify pin definitions in `pins.h` against the Waveshare schematic
- Ensure PSRAM is enabled in board settings (OPI PSRAM)
- Try `ROTATION` values 0-3 in `config.h`

**Weather shows "loading...":**
- Check your OWM API key is valid (test in a browser first)
- Free tier keys can take a few hours to activate after signup
- Check serial output for HTTP error codes

**Touch not responding:**
- The AXS15231B touch address may differ — run an I2C scanner sketch to verify
- Touch data format may need adjustment for your firmware version

**WiFi won't connect:**
- The ESP32-S3 only supports 2.4GHz networks
- Check SSID/password in `config.h` (they're case-sensitive)

## What's Next (iteration ideas)

After you get this running, here are natural next features to add:

- **GitHub notifications** — poll the GitHub API for unread notifications, show count in the status panel
- **Calendar events** — fetch next event from a Google Calendar JSON feed
- **Kubernetes pod health** — hit a lightweight status endpoint from your cluster
- **CI/CD pipeline status** — poll Vela or GitHub Actions for build status
- **LVGL upgrade** — swap Arduino_GFX primitives for LVGL widgets for smoother UI and animations
- **OTA updates** — add ArduinoOTA so you can push updates over WiFi
- **Deep sleep** — use the RTC to wake periodically and save battery
- **IMU gestures** — use the onboard QMI8658 to detect tilt/tap gestures

## License

Do whatever you want with this. It's your hardware, have fun.
