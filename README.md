# Desk Status Bar — ESP32-S3-Touch-LCD-3.49

A WiFi-connected always-on desk HUD built for the Waveshare ESP32-S3-Touch-LCD-3.49's unique 640x172 bar display.

## What It Does

```
+--------------+-----------------+----------------+-------------+
|              |                 |                |             |
|   12:34      |   Monday        |   72°F         |  WiFi OK    |
|      :56 PM  |   Mar 2         |   Clear sky    |  -42 dBm    |
|              |   2026          |   Feels 69°    |             |
|              |   W10 Day 66    |                |  85% ⚡     |
|              |                 |                |  Up: 3h 24m |
|              |                 |                |             |
+--------------+-----------------+----------------+-------------+
  TIME (160px)   DATE (172px)     WEATHER (152px)  STATUS (122px)
```

Each column has multiple tiles you can cycle through by tapping:

| Column | Tiles |
|--------|-------|
| Time | Clock (HH:MM:SS AM/PM), World Clocks (3 configurable) |
| Date | Day/Month/Year/Week, Sun Arc (sunrise/sunset), Moon Phase |
| Weather | Temp + Description, Air Quality (AQI), Wind + Conditions |
| Status | WiFi/Battery/Uptime, System Info (IP, heap, PSRAM, CPU) |

A tiny animated critter pet roams the bottom of the screen, reacting to device tilt via the onboard accelerometer. Touch near it to make it wave.

## Features

- **NTP time sync** with PCF85063 RTC backup (instant time on boot, no WiFi wait)
- **Weather** via OpenWeatherMap (temp, feels-like, AQI, wind, humidity, pressure)
- **Sun arc** showing sunrise/sunset timeline with day/night progress dot
- **Moon phase** with illumination percentage
- **Touch navigation** — tap columns to cycle tiles, edge zones for page navigation
- **Audio feedback** — click sound on touch, startup chime (ES8311 codec + I2S)
- **USB power detection** — lightning bolt when charging, checkmark when full
- **Auto-dim** — dims backlight after inactivity (disabled on USB power)
- **Auto-return** — returns to home tiles after configurable inactivity
- **Power button** — 3-second hold to shut down (battery mode)
- **NVS weather cache** — shows last weather data immediately on boot

## Hardware

- **Board**: Waveshare ESP32-S3-Touch-LCD-3.49 (Case A or B)
- **Display**: 3.49" IPS, 172x640, AXS15231B QSPI
- **Touch**: AXS15231B integrated (I2C at 0x3B)
- **Audio**: ES8311 codec + I2S speaker
- **IMU**: QMI8658 accelerometer/gyroscope
- **RTC**: PCF85063 (battery-backed time)
- **I/O Expander**: TCA9554 (power latch, battery ADC, charger status, speaker amp)
- **Battery**: 18650 (Case A) or 3.7V LiPo (Case B)

## Quick Start

### 1. Install Board Support

Install [arduino-cli](https://arduino.github.io/arduino-cli/) or [Arduino IDE](https://www.arduino.cc/en/software), then add ESP32 board support:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### 2. Install Libraries

| Library | Author | Purpose |
|---------|--------|---------|
| GFX Library for Arduino | moononournation | Display driver (AXS15231B QSPI) |
| ArduinoJson | Benoit Blanchon | Weather API JSON parsing |
| SensorLib | Lewis He | QMI8658 IMU + PCF85063 RTC |

### 3. Configure

Copy `.env.local.example` to `.env.local` (if using Makefile) or edit `config.h` directly:

```cpp
#define WIFI_SSID     "YourNetwork"
#define WIFI_PASS     "YourPassword"

#define OWM_API_KEY   "abc123..."       // free at openweathermap.org/api
#define OWM_CITY      "New York"
#define OWM_UNITS     "imperial"        // or "metric"

#define UTC_OFFSET    -5                // your timezone
#define DST_OFFSET    1                 // 1 during daylight saving, 0 otherwise
```

See `config.h` for all options: brightness, critter enable, touch sound, auto-dim timeout, world clock labels/offsets, and more.

### 4. Build & Upload

**With Makefile (recommended):**

```bash
make build          # compile
make upload         # compile + upload
make monitor        # serial monitor (115200 baud)
make clean          # remove build artifacts
```

**With Arduino IDE:**

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Default 4MB with spiffs |

If upload fails: hold **BOOT**, press **RESET**, release **BOOT**, then retry.

## Project Structure

```
desk-status-bar/
  desk-status-bar.ino  — Setup, loop, globals, includes
  config.h             — User-editable settings (WiFi, API keys, display, timing)
  pins.h               — GPIO pin definitions and I2C addresses
  types.h              — Structs, enums, layout constants
  colors.h             — Color palette macros
  hal.h                — TCA9554, power latch, battery, backlight, auto-dim, USB detection
  imu.h                — IMU reading (QMI8658)
  networking.h         — WiFi, NTP, weather/AQI fetch, NVS cache
  audio.h              — ES8311 codec, I2S, click sound, startup tune
  drawing.h            — Panel drawing, weather icons, WiFi bars, sun arc, icon helpers
  critter.h            — Animated pet state machine + drawing
  tiles.h              — All 11 tile draw functions
  ui.h                 — Main panel dispatch, splash screen, indicators
  touch.h              — Touch polling + navigation
```

## Pin Mapping

> These pins match the Waveshare AXS15231B family. See `pins.h` for full details including I2C addresses and TCA9554 pin assignments.

| Function | Pin |
|----------|-----|
| QSPI CS | GPIO 9 |
| QSPI CLK | GPIO 10 |
| QSPI D0-D3 | GPIO 11-14 |
| LCD RST | GPIO 21 |
| Backlight | GPIO 8 (active-low PWM) |
| Touch SDA/SCL | GPIO 17/18 |
| Peripheral I2C SDA/SCL | GPIO 47/48 |
| I2S MCLK/BCLK/LRCK | GPIO 7/15/46 |
| Battery ADC | GPIO 4 (3:1 divider) |
| Power Button | GPIO 16 (also USB power detect) |

## Troubleshooting

**Display stays blank:**
- Verify PSRAM is enabled in board settings (OPI PSRAM)
- Check pin definitions in `pins.h` against the Waveshare schematic
- Try `ROTATION` values 0-3 in `config.h`

**Weather shows "loading...":**
- Check your OWM API key (test in browser first)
- Free tier keys can take a few hours to activate
- Check serial output for HTTP error codes

**Touch not responding:**
- Run the `i2c_scanner` sketch to verify the touch address
- Touch is polled via I2C, not interrupt-driven

**WiFi won't connect:**
- ESP32-S3 only supports 2.4GHz networks
- SSID/password are case-sensitive

**No sound:**
- Check `TOUCH_SOUND_ENABLED` is 1 in `config.h`
- The ES8311 codec needs I2S running before init (handled automatically)

## Architecture Diagrams

### Main Loop

Each `loop()` iteration runs these steps sequentially. The draw gate (`drawInterval`) controls frame rate, which varies by power mode.

```mermaid
flowchart TD
    A[loop] --> B[readIMU]
    B --> C[checkMotion]
    C --> D[resolvePowerMode]
    D --> E{Mode changed?}
    E -- Yes --> F[applyPowerMode]
    F --> G{Waking from\nDEEP/AWAY?}
    G -- Yes --> H[reconnectWiFi]
    G -- No --> I
    H --> I
    E -- No --> I

    I{now - lastClockUpdate\n>= drawInterval?}
    I -- Yes --> J[drawMainPanel + flush]
    I -- No --> K

    J --> K{now - lastBatteryRead\n>= 60s?}
    K -- Yes --> L[sampleBatteryOnce\nupdatePowerState]
    K -- No --> M

    L --> M[WiFi management block\nmode-dependent]
    M --> N{Power button\nheld >= 3s?}
    N -- Yes --> O[powerOff]
    N -- No --> P

    P{Touch pressed\n& debounce ok?}
    P -- Yes --> Q[handleTouch]
    P -- No --> R

    Q --> R{Idle >=\nAUTO_RETURN_MIN?}
    R -- Yes --> S[Reset all tiles\nto home]
    R -- No --> T[delay 50ms]
    S --> T
```

### Power Management Tiers

The device transitions between 7 power modes based on USB power, motion idle time, WiFi availability, and time of day. USB power always forces full active mode.

```mermaid
stateDiagram-v2
    [*] --> PWR_ACTIVE : USB powered\n(always)

    PWR_ACTIVE --> PWR_ACTIVE_NIGHT : Night hours\n(22:00–06:00)
    PWR_ACTIVE_NIGHT --> PWR_ACTIVE : Day hours

    PWR_ACTIVE --> PWR_IDLE_DAY : Idle > 5 min
    PWR_ACTIVE_NIGHT --> PWR_IDLE_NIGHT : Idle > 5 min

    PWR_IDLE_DAY --> PWR_ACTIVE : Motion detected
    PWR_IDLE_NIGHT --> PWR_ACTIVE_NIGHT : Motion detected

    PWR_IDLE_DAY --> PWR_DEEP_DAY : Idle > 10 min
    PWR_IDLE_NIGHT --> PWR_DEEP_NIGHT : Idle > 10 min

    PWR_DEEP_DAY --> PWR_ACTIVE : Motion detected
    PWR_DEEP_NIGHT --> PWR_ACTIVE_NIGHT : Motion detected

    PWR_DEEP_DAY --> PWR_AWAY : WiFi unavailable\n> 5 min
    PWR_DEEP_NIGHT --> PWR_AWAY : WiFi unavailable\n> 5 min

    PWR_AWAY --> PWR_ACTIVE : Motion + WiFi\nreconnects
    PWR_AWAY --> PWR_ACTIVE_NIGHT : Motion + WiFi\nreconnects (night)
```

### WiFi Cycling in Deep & Away Modes

Deep modes disconnect WiFi and periodically reconnect just long enough to fetch weather, then disconnect again. This is the primary battery conservation mechanism.

```mermaid
flowchart TD
    A[Enter DEEP mode] --> B[disconnectWiFi\nWiFi.mode WIFI_OFF]
    B --> C[Wait reconnect interval\nDay: 2 min / Night: 10 min + jitter]
    C --> D[reconnectWiFi]
    D --> E{Connected?}
    E -- Yes --> F[fetchWeather + fetchAQI\nsaveWeatherCache]
    F --> G[disconnectWiFi]
    G --> C
    E -- No --> H{WiFi unavailable\n> 5 min?}
    H -- No --> C
    H -- Yes --> I[Enter PWR_AWAY\nRetry every 5 min]
    I --> J[reconnectWiFi]
    J --> K{Connected?}
    K -- Yes --> L[Fetch weather\nClear away mode]
    L --> G
    K -- No --> I
```

### Touch Navigation

Touch events are dispatched by screen position: critter hit area first, then edge zones for page navigation, then column zones for tile cycling.

```mermaid
flowchart TD
    A[Touch detected] --> B{Debounce\n> 300ms since last?}
    B -- No --> Z[Ignore]
    B -- Yes --> C[Transform coords\nlx = 639 - tp.x]
    C --> D{Hit critter?\ndist < 20px}
    D -- Yes --> E{Sleeping?}
    E -- Yes --> F[Wake → IDLE]
    E -- No --> G[→ WAVING]
    D -- No --> H{lx < 30?\nLeft edge}
    H -- Yes --> I[Previous page]
    H -- No --> J{lx > 610?\nRight edge}
    J -- Yes --> K[Next page]
    J -- No --> L{Page 0?}
    L -- No --> Z
    L -- Yes --> M[Find column\nfrom COL_GEOM]
    M --> N{numTiles > 1?}
    N -- No --> Z
    N -- Yes --> O{ly < 86?\nTop half}
    O -- Yes --> P[Previous tile]
    O -- No --> Q[Next tile]
    P --> R[Set focusCol\nfor ring animation]
    Q --> R
```

### Weather Data Pipeline

Weather data flows from OpenWeatherMap through an NVS cache for instant display on boot. The cache is checked for freshness before making network calls.

```mermaid
flowchart TD
    subgraph Boot
        A[setup] --> B[loadWeatherCache\nfrom NVS]
        B --> C{WiFi connected?}
        C -- No --> D[Display cached data]
        C -- Yes --> E{Cache fresh?\nage < 10 min}
        E -- Yes --> F[Use cache\nskip fetch]
        E -- No --> G[fetchWeather]
        G --> H[fetchAQI]
        H --> I[saveWeatherCache]
    end

    subgraph Runtime
        J{Active/Idle mode\n& 10 min elapsed?} -- Yes --> K[fetchWeather]
        K --> L[fetchAQI]
        L --> M[saveWeatherCache]
        J -- No --> N[Use current data]
    end

    subgraph Deep/Away
        O[WiFi cycle reconnect] --> P{Connected?}
        P -- Yes --> Q[fetchWeather + fetchAQI\nsaveWeatherCache]
        Q --> R[disconnectWiFi]
        P -- No --> S[Use cached data]
    end
```

### Tile Rendering Dispatch

`drawMainPanel()` iterates over 4 columns, drawing the currently selected tile in each. A focus ring animates briefly after touch.

```mermaid
flowchart TD
    A[drawMainPanel] --> B[fillScreen BG_COLOR]
    B --> C{currentPage == 0?}
    C -- No --> D[drawPage1\nplaceholder]
    C -- Yes --> E[getLocalTime]
    E --> F[For each column 0–3]
    F --> G[drawPanel background]
    G --> H{Which column?}

    H -- "0: Time" --> I0{currentTile 0}
    I0 -- 0 --> T0[Digital Clock\nHH:MM:SS AM/PM]
    I0 -- 1 --> T1[World Clocks\n3 time zones]
    I0 -- 2 --> T2[Tix Clock\nanimated grid]

    H -- "1: Date" --> I1{currentTile 1}
    I1 -- 0 --> T3[Date Display\nDay/Month/Year]
    I1 -- 1 --> T4[Sun Arc\nsunrise/sunset]
    I1 -- 2 --> T5[Moon Phase\nillumination %]

    H -- "2: Weather" --> I2{currentTile 2}
    I2 -- 0 --> T6[Temp + Desc\nfeels-like]
    I2 -- 1 --> T7[Air Quality\nAQI + PM2.5]
    I2 -- 2 --> T8[Wind/Humidity\npressure/clouds]

    H -- "3: Status" --> I3{currentTile 3}
    I3 -- 0 --> T9[WiFi + Battery\nuptime]
    I3 -- 1 --> T10[System Info\nIP/heap/PSRAM]

    T0 & T1 & T2 & T3 & T4 & T5 & T6 & T7 & T8 & T9 & T10 --> K[drawTileIndicator dots]
    K --> L[Draw focus ring\nif recently tapped]
    L --> F

    D --> M[drawPageIndicator]
    F --> N[updateCritter + drawCritter]
    N --> M
    M --> O[gfx->flush\nQSPI to panel]
```

### Critter State Machine

The animated pet has 6 states driven by tilt, touch, idle time, and randomness.

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> WALKING : Tilt detected OR\nrandom chance
    IDLE --> JUMPING : Random chance\n(VY = 3.0)
    IDLE --> FLYING : Random chance\n(VY = 2.5)
    IDLE --> WAVING : Random chance
    IDLE --> SLEEPING : Idle > 30s\n(210 ticks)

    WALKING --> IDLE : After 5–15s
    WALKING --> JUMPING : Random chance
    WALKING --> FLYING : Random chance

    JUMPING --> IDLE : Landed\n(no tilt)
    JUMPING --> WALKING : Landed\n(tilting)

    FLYING --> IDLE : Landed\n(no tilt)
    FLYING --> WALKING : Landed\n(tilting)

    WAVING --> IDLE : After ~5s\n(35 ticks)

    SLEEPING --> IDLE : Tilt or touch\nor rare random

    note right of WALKING : Responds to device tilt\nFlips direction every ~2 min
    note right of FLYING : 4s flapping phase\nthen glide down
    note right of SLEEPING : Touch to wake\nor shake device
```

## License

Do whatever you want with this. It's your hardware, have fun.
