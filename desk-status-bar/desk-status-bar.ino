// =============================================================
// Desk Status Bar — ESP32-S3-Touch-LCD-3.49
// A WiFi-connected always-on HUD for your desk
//
// Features:
//   - Real-time clock via NTP
//   - Weather via OpenWeatherMap
//   - Battery voltage monitor
//   - Touch to cycle panels / force refresh
//
// Libraries needed (install via Arduino Library Manager):
//   - GFX Library for Arduino (Arduino_GFX)  by moononournation
//   - ArduinoJson                            by Benoit Blanchon
//
// Board settings in Arduino IDE:
//   Board:          ESP32S3 Dev Module
//   Flash Size:     16MB
//   PSRAM:          OPI PSRAM (8MB)
//   USB CDC:        Enabled (for serial output)
//   Partition:      Default 4MB with spiffs (or 16MB variant)
// =============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>

// Use SPI3_HOST for QSPI display (matches Waveshare reference design)
#define ESP32QSPI_SPI_HOST SPI3_HOST

#include <Arduino_GFX_Library.h>

#include "pins.h"
#include "config.h"

// =============================================================
// Display Setup
// AXS15231B in QSPI mode requires full-frame writes (no partial
// updates). We use a Canvas framebuffer + flush for every frame.
// Minimal init: sleep out + RGB565 + display on (the default
// Arduino_GFX init blob doesn't match this panel).
// =============================================================
static const uint8_t panel_init[] = {
  BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE,
  DELAY, 200,
  BEGIN_WRITE, WRITE_C8_D8, 0x3A, 0x55, END_WRITE,
  BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE,
  DELAY, 50,
};

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_QSPI_CS, LCD_QSPI_CLK,
  LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3
);

Arduino_AXS15231B *display = new Arduino_AXS15231B(
  bus, LCD_RST, 0 /* native portrait */, false /* IPS */,
  LCD_WIDTH, LCD_HEIGHT,
  0, 0, 0, 0, panel_init, sizeof(panel_init)
);

// Canvas in native portrait — flush sends full frame in correct order.
// setRotation(1) called in setup() remaps drawing coords to landscape.
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, display);

// =============================================================
// Color Palette — dark theme, easy on the eyes at a desk
// =============================================================
#define BG_COLOR       gfx->color565(10, 10, 14)      // near-black
#define PANEL_COLOR    gfx->color565(55, 55, 75)      // visible card
#define ACCENT_COLOR   gfx->color565(99, 140, 255)    // soft blue
#define TEXT_PRIMARY   gfx->color565(230, 230, 240)   // off-white
#define TEXT_SECONDARY gfx->color565(140, 140, 160)   // muted
#define TEXT_DIM       gfx->color565(100, 100, 120)   // muted but readable
#define GOOD_COLOR     gfx->color565(80, 200, 120)    // green
#define WARN_COLOR     gfx->color565(255, 180, 60)    // amber
#define ERR_COLOR      gfx->color565(255, 80, 80)     // red
#define DIVIDER_COLOR  gfx->color565(50, 50, 65)      // subtle line

// =============================================================
// State
// =============================================================
struct WeatherData {
  float temp;
  float feels_like;
  int   humidity;
  char  description[32];
  char  icon[8];
  bool  valid;
};

WeatherData weather = { 0, 0, 0, "loading...", "01d", false };

unsigned long lastWeatherFetch = 0;
unsigned long lastClockUpdate  = 0;
unsigned long lastTouchTime    = 0;
int  currentPanel = 0;   // 0 = main, 1 = detail, 2 = system info
bool wifiConnected = false;
bool timeSynced    = false;

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};

// =============================================================
// WiFi
// =============================================================
void setupWiFi() {
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Failed to connect");
  }
}

// =============================================================
// NTP Time
// =============================================================
void syncTime() {
  if (!wifiConnected) return;

  long utcOffsetSec = (long)UTC_OFFSET * 3600;
  long dstOffsetSec = (long)DST_OFFSET * 3600;
  configTime(utcOffsetSec, dstOffsetSec, NTP_SERVER);

  struct tm t;
  if (getLocalTime(&t, 10000)) {
    timeSynced = true;
    Serial.printf("[NTP] Time synced: %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    Serial.println("[NTP] Failed to sync time");
  }
}

// =============================================================
// Weather
// =============================================================
void fetchWeather() {
  if (!wifiConnected) return;

  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += OWM_CITY;
  url += "&appid=";
  url += OWM_API_KEY;
  url += "&units=";
  url += OWM_UNITS;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      weather.temp      = doc["main"]["temp"] | 0.0f;
      weather.feels_like = doc["main"]["feels_like"] | 0.0f;
      weather.humidity  = doc["main"]["humidity"] | 0;
      strlcpy(weather.description,
              doc["weather"][0]["description"] | "unknown",
              sizeof(weather.description));
      strlcpy(weather.icon,
              doc["weather"][0]["icon"] | "01d",
              sizeof(weather.icon));
      weather.valid = true;

      // Capitalize first letter
      if (weather.description[0] >= 'a' && weather.description[0] <= 'z') {
        weather.description[0] -= 32;
      }

      Serial.printf("[Weather] %.1f°, %s\n", weather.temp, weather.description);
    } else {
      Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[Weather] HTTP error: %d\n", code);
  }

  http.end();
}

// =============================================================
// Battery Voltage
// =============================================================
float getBatteryVoltage() {
  // TODO: read battery voltage via AXP2101 over I2C
  // No direct ADC pin on this board revision
  return 3.7f; // placeholder
}

int getBatteryPercent(float voltage) {
  // Rough LiPo estimate: 3.0V = 0%, 4.2V = 100%
  int pct = (int)((voltage - 3.0f) / 1.2f * 100.0f);
  return constrain(pct, 0, 100);
}

// =============================================================
// Drawing Helpers
// =============================================================

// Draws a rounded rectangle panel background
void drawPanel(int x, int y, int w, int h, uint16_t color) {
  gfx->fillRoundRect(x, y, w, h, 8, color);
}

// Draws a vertical divider
void drawDivider(int x, int y1, int y2) {
  gfx->drawFastVLine(x, y1, y2 - y1, DIVIDER_COLOR);
}

// Gets a weather emoji character for the icon code
// (Using text since we don't have icon fonts loaded)
const char* weatherLabel(const char* icon) {
  if (strncmp(icon, "01", 2) == 0) return "Clear";
  if (strncmp(icon, "02", 2) == 0) return "Partial";
  if (strncmp(icon, "03", 2) == 0) return "Cloudy";
  if (strncmp(icon, "04", 2) == 0) return "Overcast";
  if (strncmp(icon, "09", 2) == 0) return "Showers";
  if (strncmp(icon, "10", 2) == 0) return "Rain";
  if (strncmp(icon, "11", 2) == 0) return "Thunder";
  if (strncmp(icon, "13", 2) == 0) return "Snow";
  if (strncmp(icon, "50", 2) == 0) return "Fog";
  return "???";
}

// =============================================================
// Main Panel (0) — Time | Date | Weather | Battery
// =============================================================
void drawMainPanel() {
  gfx->fillScreen(BG_COLOR);

  struct tm t;
  bool hasTime = getLocalTime(&t, 0);

  // ── TIME SECTION (x: 0–170) ─────────────────────────────
  drawPanel(8, 8, 154, SCREEN_H - 16, PANEL_COLOR);

  if (hasTime) {
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);

    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(4);
    gfx->setCursor(20, 40);
    gfx->print(timeBuf);

    // Seconds — smaller, accent color
    char secBuf[4];
    snprintf(secBuf, sizeof(secBuf), ":%02d", t.tm_sec);
    gfx->setTextSize(2);
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setCursor(52, 95);
    gfx->print(secBuf);

    // AM/PM indicator
    gfx->setTextSize(1);
    gfx->setTextColor(TEXT_DIM);
    gfx->setCursor(100, 95);
    gfx->print(t.tm_hour >= 12 ? "PM" : "AM");
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(2);
    gfx->setCursor(30, 60);
    gfx->print("--:--");
  }

  // ── DATE SECTION (x: 170–350) ───────────────────────────
  drawPanel(170, 8, 172, SCREEN_H - 16, PANEL_COLOR);

  if (hasTime) {
    // Day of week
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                          "Thursday", "Friday", "Saturday"};
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setTextSize(2);
    gfx->setCursor(182, 30);
    gfx->print(days[t.tm_wday]);

    // Date
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d", months[t.tm_mon], t.tm_mday);
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(2);
    gfx->setCursor(182, 65);
    gfx->print(dateBuf);

    // Year
    char yearBuf[6];
    snprintf(yearBuf, sizeof(yearBuf), "%d", t.tm_year + 1900);
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    gfx->setCursor(182, 100);
    gfx->print(yearBuf);

    // Week number
    int weekNum = (t.tm_yday + 7 - ((t.tm_wday + 6) % 7)) / 7;
    char weekBuf[12];
    snprintf(weekBuf, sizeof(weekBuf), "Week %d", weekNum);
    gfx->setCursor(182, 120);
    gfx->print(weekBuf);
  }

  // ── WEATHER SECTION (x: 350–510) ────────────────────────
  drawPanel(350, 8, 152, SCREEN_H - 16, PANEL_COLOR);

  if (weather.valid) {
    // Temperature — big
    char tempBuf[10];
    const char* unit = (strcmp(OWM_UNITS, "imperial") == 0) ? "F" : "C";
    snprintf(tempBuf, sizeof(tempBuf), "%.0f°%s", weather.temp, unit);
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(3);
    gfx->setCursor(362, 30);
    gfx->print(tempBuf);

    // Description
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setTextSize(1);
    gfx->setCursor(362, 72);
    gfx->print(weather.description);

    // Feels like + humidity
    char detailBuf[28];
    snprintf(detailBuf, sizeof(detailBuf), "Feels %.0f° | %d%%", weather.feels_like, weather.humidity);
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setCursor(362, 92);
    gfx->print(detailBuf);

    // City name
    gfx->setTextColor(TEXT_DIM);
    gfx->setCursor(362, 112);
    gfx->print(OWM_CITY);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    gfx->setCursor(375, 60);
    gfx->print("Weather");
    gfx->setCursor(375, 78);
    gfx->print("loading...");
  }

  // ── STATUS SECTION (x: 510–632) ─────────────────────────
  drawPanel(510, 8, 122, SCREEN_H - 16, PANEL_COLOR);

  // WiFi status
  gfx->setTextSize(1);
  gfx->setCursor(522, 25);
  if (wifiConnected) {
    gfx->setTextColor(GOOD_COLOR);
    gfx->print("WiFi OK");
    gfx->setTextColor(TEXT_DIM);
    gfx->setCursor(522, 42);
    // Show signal strength
    int rssi = WiFi.RSSI();
    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", rssi);
    gfx->print(rssiBuf);
  } else {
    gfx->setTextColor(ERR_COLOR);
    gfx->print("WiFi OFF");
  }

  // Battery
  float batV = getBatteryVoltage();
  int batPct = getBatteryPercent(batV);
  gfx->setCursor(522, 68);
  if (batPct > 50)      gfx->setTextColor(GOOD_COLOR);
  else if (batPct > 20) gfx->setTextColor(WARN_COLOR);
  else                   gfx->setTextColor(ERR_COLOR);

  char batBuf[16];
  snprintf(batBuf, sizeof(batBuf), "Bat: %d%%", batPct);
  gfx->print(batBuf);

  char batVBuf[10];
  snprintf(batVBuf, sizeof(batVBuf), "%.2fV", batV);
  gfx->setTextColor(TEXT_DIM);
  gfx->setCursor(522, 85);
  gfx->print(batVBuf);

  // Uptime
  unsigned long uptimeSec = millis() / 1000;
  unsigned long hrs = uptimeSec / 3600;
  unsigned long mins = (uptimeSec % 3600) / 60;
  char uptBuf[16];
  snprintf(uptBuf, sizeof(uptBuf), "Up: %luh %lum", hrs, mins);
  gfx->setTextColor(TEXT_DIM);
  gfx->setCursor(522, 112);
  gfx->print(uptBuf);

  // Free heap
  char heapBuf[16];
  snprintf(heapBuf, sizeof(heapBuf), "%dK free", ESP.getFreeHeap() / 1024);
  gfx->setCursor(522, 130);
  gfx->print(heapBuf);

  // ── BOTTOM BAR — touch hint ──────────────────────────────
  gfx->setTextColor(TEXT_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(250, SCREEN_H - 14);
  gfx->print("[ tap to refresh ]");

  gfx->flush();
}

// =============================================================
// Splash Screen (shown during boot)
// =============================================================
void drawSplash(const char* status) {
  gfx->fillScreen(BG_COLOR);

  gfx->setTextColor(ACCENT_COLOR);
  gfx->setTextSize(3);
  gfx->setCursor(150, 40);
  gfx->print("Desk Status Bar");

  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  gfx->setCursor(240, 100);
  gfx->print(status);

  // Progress dots
  gfx->setTextColor(ACCENT_COLOR);
  gfx->setCursor(240, 130);
  static int dots = 0;
  for (int i = 0; i < (dots % 4); i++) gfx->print(". ");
  dots++;

  gfx->flush();
}

// =============================================================
// Touch Handling (basic polling — no interrupt for MVP)
// =============================================================
// The AXS15231B integrated touch uses I2C at address 0x3B.
// For the MVP we do simple point-read polling.
// A more robust approach would use the interrupt pin + a proper
// touch library, but this gets us moving fast.

TouchPoint readTouch() {
  TouchPoint tp = { 0, 0, false };
  uint8_t buf[6] = {0};

  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6);

  for (int i = 0; i < 6 && Wire.available(); i++) {
    buf[i] = Wire.read();
  }

  // AXS15231B touch data format:
  // buf[0]: event flag (bit 7-6: 0=down, 1=up, 2=contact)
  // buf[1]: touch point ID
  // buf[2-3]: X (12-bit)
  // buf[4-5]: Y (12-bit)
  uint8_t event = buf[0] >> 6;
  if (event == 0 || event == 2) { // down or contact
    tp.x = ((buf[0] & 0x0F) << 8) | buf[1];
    tp.y = ((buf[2] & 0x0F) << 8) | buf[3];
    // Filter out false positives where all bytes are zero
    tp.pressed = (tp.x != 0 || tp.y != 0);
  }

  return tp;
}

// =============================================================
// Setup
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=============================");
  Serial.println("  Desk Status Bar — Booting");
  Serial.println("=============================\n");

  // Init I2C buses — touch is on a separate bus from peripherals
  Wire.begin(TOUCH_SDA, TOUCH_SCL);   // Bus 0: touch controller
  Wire1.begin(I2C_SDA, I2C_SCL);      // Bus 1: IMU, RTC, etc.

  // Init display + canvas
  if (!gfx->begin()) {
    Serial.println("[Display] FATAL: gfx->begin() failed!");
    while (1) delay(100);
  }
  Serial.println("[Display] Initialized");

  // Rotate drawing coordinates to landscape (640x172)
  // Canvas buffer stays portrait (172x640) so flush works with QSPI
  gfx->setRotation(ROTATION);

  // Backlight on (active-low: LOW = full brightness)
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, LOW);
  Serial.println("[Backlight] On");

  // Splash
  drawSplash("Connecting to WiFi...");

  // WiFi
  setupWiFi();
  drawSplash(wifiConnected ? "WiFi connected!" : "WiFi failed — offline mode");
  delay(600);

  // Time sync
  if (wifiConnected) {
    drawSplash("Syncing time...");
    syncTime();
  }

  // Initial weather fetch
  if (wifiConnected) {
    drawSplash("Fetching weather...");
    fetchWeather();
    lastWeatherFetch = millis();
  }

  drawSplash("Ready!");
  delay(400);

  Serial.println("[Boot] Setup complete, entering main loop\n");
}

// =============================================================
// Main Loop
// =============================================================
void loop() {
  unsigned long now = millis();

  // Update clock display every second
  if (now - lastClockUpdate >= CLOCK_INTERVAL) {
    lastClockUpdate = now;
    drawMainPanel();
  }

  // Refresh weather periodically
  if (wifiConnected && (now - lastWeatherFetch >= WEATHER_INTERVAL)) {
    Serial.println("[Loop] Refreshing weather...");
    fetchWeather();
    lastWeatherFetch = now;
  }

  // Reconnect WiFi if dropped
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    wifiConnected = (WiFi.status() == WL_CONNECTED);
  }

  // Handle touch input
  TouchPoint tp = readTouch();
  if (tp.pressed && (now - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = now;
    Serial.printf("[Touch] x=%d y=%d\n", tp.x, tp.y);

    // Tap anywhere → force weather refresh
    if (wifiConnected) {
      drawSplash("Refreshing...");
      fetchWeather();
      lastWeatherFetch = now;
    }
  }

  // Small yield to keep watchdog happy
  delay(50);
}
