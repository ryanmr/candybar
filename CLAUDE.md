# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Candybar is an Arduino/ESP32 project — a WiFi-connected always-on desk HUD for the **Waveshare ESP32-S3-Touch-LCD-3.49** board. It displays time (NTP), date, weather (OpenWeatherMap API), WiFi signal, battery level, and uptime on a 640x172 bar-shaped display.

## Build & Upload

This is an Arduino IDE project, not a standard build-system project. There are no CLI build/test/lint commands.

- **IDE**: Arduino IDE 2.x
- **Board**: ESP32S3 Dev Module (via Espressif ESP32 board package)
- **Required board settings**: Flash 16MB, OPI PSRAM, USB CDC On Boot enabled, Default 4MB with spiffs partition
- **Required libraries** (Arduino Library Manager): `GFX Library for Arduino` (moononournation), `ArduinoJson` (Benoit Blanchon)
- **Upload**: USB-C; if upload fails, hold BOOT → press RESET → release BOOT → retry

## Architecture

Single Arduino sketch with header-only configuration files — no classes, no build abstraction.

- `desk-status-bar/desk-status-bar.ino` — Entire application: WiFi connection, NTP sync, OpenWeatherMap HTTP fetch + JSON parse, touch input polling (I2C to AXS15231B at 0x3B), battery ADC reading, and all display rendering via Arduino_GFX canvas (double-buffered). The main loop runs a 1-second clock redraw and 10-minute weather refresh.
- `desk-status-bar/config.h` — User-editable settings: WiFi credentials, OWM API key/city/units, timezone offsets, display rotation/brightness, update intervals. **Contains secrets — never commit real values.**
- `desk-status-bar/pins.h` — GPIO pin definitions and I2C device addresses for the Waveshare board. Also defines display dimensions (172x640 native, 640x172 after rotation).
- `desk-status-bar/i2c_scanner/` — Standalone diagnostic sketch to verify I2C bus wiring and detect devices.

## Display Layout

The 640x172 screen is divided into four panels drawn left-to-right: Time (0–170px), Date (170–350px), Weather (350–510px), Status (510–632px). All rendering uses Arduino_GFX primitives with a dark color palette defined as macros in the .ino file.

## Key Constraints

- ESP32-S3 with 16MB flash and 8MB PSRAM — memory-constrained embedded target
- Display driver is AXS15231B via QSPI — pin mapping is board-specific (see `pins.h`)
- WiFi is 2.4GHz only
- OpenWeatherMap free tier: 1000 API calls/day
- Touch is polled via I2C (no interrupt-driven touch library yet)
