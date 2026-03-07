#pragma once

// =============================================================
// Configuration — values come from build flags (-D) when using
// the Makefile, or edit the defaults below for Arduino IDE use.
// =============================================================

// -- WiFi --
#ifndef WIFI_SSID
#define WIFI_SSID     "YourNetwork"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS     "YourPassword"
#endif

// -- Time --
#ifndef NTP_SERVER
#define NTP_SERVER    "pool.ntp.org"
#endif
#ifndef UTC_OFFSET
#define UTC_OFFSET    -6          // EST = -5, CST = -6, PST = -8, etc.
#endif
#ifndef DST_OFFSET
#define DST_OFFSET    1           // 1 if DST active, 0 if not
#endif

// -- Weather (OpenWeatherMap — free tier, 1000 calls/day) --
// Sign up at https://openweathermap.org/api and grab an API key
#ifndef OWM_API_KEY
#define OWM_API_KEY   "your-api-key-here"
#endif
#ifndef OWM_CITY
#define OWM_CITY      "Chicago"
#endif
#ifndef OWM_UNITS
#define OWM_UNITS     "imperial"  // "imperial" for °F, "metric" for °C
#endif

// -- Display --
#ifndef ROTATION
#define ROTATION      1           // 0=portrait, 1=landscape (bar mode)
#endif
#ifndef BRIGHTNESS
#define BRIGHTNESS    200         // 0-255 backlight brightness
#endif

// -- Critter Pet --
#ifndef CRITTER_ENABLED
#define CRITTER_ENABLED  1          // 1=show critter, 0=disable
#endif

// -- Touch Sound --
#ifndef TOUCH_SOUND_ENABLED
#define TOUCH_SOUND_ENABLED  1    // 1=click on touch, 0=silent
#endif

// -- Auto-Dim --
#ifndef DIM_TIMEOUT_MIN
#define DIM_TIMEOUT_MIN  5        // minutes of no motion before dimming
#endif
#ifndef DIM_BRIGHTNESS
#define DIM_BRIGHTNESS   100      // 100–150 works; below ~100 the backlight goes fully black
#endif

// -- Update Intervals (milliseconds) --
#ifndef WEATHER_INTERVAL
#define WEATHER_INTERVAL  (10 * 60 * 1000)  // 10 minutes
#endif
#ifndef CLOCK_INTERVAL
#define CLOCK_INTERVAL    (1000)             // 1 second
#endif
#ifndef TOUCH_DEBOUNCE
#define TOUCH_DEBOUNCE    (300)              // touch debounce ms
#endif

// -- Navigation --
#ifndef AUTO_RETURN_MIN
#define AUTO_RETURN_MIN   5       // minutes before auto-return to home (0 = disabled)
#endif

// -- World Clocks (Time tile 1) --
// Offsets are in minutes from UTC (e.g. 330 = UTC+5:30)
#ifndef WORLD_CLOCK_1_LABEL
#define WORLD_CLOCK_1_LABEL  "London"
#endif
#ifndef WORLD_CLOCK_1_OFFSET
#define WORLD_CLOCK_1_OFFSET  0
#endif
#ifndef WORLD_CLOCK_2_LABEL
#define WORLD_CLOCK_2_LABEL  "India"
#endif
#ifndef WORLD_CLOCK_2_OFFSET
#define WORLD_CLOCK_2_OFFSET  330
#endif
#ifndef WORLD_CLOCK_3_LABEL
#define WORLD_CLOCK_3_LABEL  "Tokyo"
#endif
#ifndef WORLD_CLOCK_3_OFFSET
#define WORLD_CLOCK_3_OFFSET  540
#endif
