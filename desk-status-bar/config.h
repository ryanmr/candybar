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

// -- Auto-Dim --
#ifndef DIM_TIMEOUT_MIN
#define DIM_TIMEOUT_MIN  5        // minutes of no motion before dimming
#endif
#ifndef DIM_BRIGHTNESS
#define DIM_BRIGHTNESS   25       // ~10% brightness when dimmed
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
