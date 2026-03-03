#pragma once

// =============================================================
// Configuration — Edit these values for your setup
// =============================================================

// -- WiFi --
#define WIFI_SSID     "YourNetwork"
#define WIFI_PASS     "YourPassword"

// -- Time --
#define NTP_SERVER    "pool.ntp.org"
#define UTC_OFFSET    -6          // EST = -5, CST = -6, PST = -8, etc.
#define DST_OFFSET    1           // 1 if DST active, 0 if not

// -- Weather (OpenWeatherMap — free tier, 1000 calls/day) --
// Sign up at https://openweathermap.org/api and grab an API key
#define OWM_API_KEY   "your-api-key-here"
#define OWM_CITY      "Chicago"
#define OWM_UNITS     "imperial"  // "imperial" for °F, "metric" for °C

// -- Display --
#define ROTATION      1           // 0=portrait, 1=landscape (bar mode)
#define BRIGHTNESS    200         // 0-255 backlight brightness

// -- Update Intervals (milliseconds) --
#define WEATHER_INTERVAL  (10 * 60 * 1000)  // 10 minutes
#define CLOCK_INTERVAL    (1000)             // 1 second
#define TOUCH_DEBOUNCE    (300)              // touch debounce ms
