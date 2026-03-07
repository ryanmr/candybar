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
//   - SensorLib                              by Lewis He
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
#include <SensorQMI8658.hpp>

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
#define CRITTER_BODY      gfx->color565(130, 185, 230)  // soft sky blue
#define CRITTER_HIGHLIGHT gfx->color565(180, 215, 245)  // lighter blue belly
#define CRITTER_EYE       gfx->color565(30, 30, 40)     // near-black
#define CRITTER_CHEEK     gfx->color565(255, 160, 160)  // pink blush
#define CRITTER_BEAK      gfx->color565(240, 180, 70)   // warm orange
#define CRITTER_WING      gfx->color565(100, 155, 210)  // deeper blue

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
  float lat;
  float lon;
  unsigned long sunrise;
  unsigned long sunset;
  int   aqi;          // 1-5 scale from OWM
  float pm2_5;
};

WeatherData weather = { 0, 0, 0, "loading...", "01d", false, 0, 0, 0, 0, 0, 0 };

unsigned long lastWeatherFetch  = 0;
unsigned long lastClockUpdate   = 0;
unsigned long lastTouchTime     = 0;
unsigned long lastBatteryRead   = 0;
int  currentPanel = 0;   // 0 = main, 1 = detail, 2 = system info
bool wifiConnected = false;
bool timeSynced    = false;
bool onBattery     = false;    // true if booted from power button (battery)
unsigned long pwrBtnDownAt = 0;  // millis when power button was first pressed
bool pwrBtnWasDown = false;
bool pwrBtnReady   = false;     // true once we've seen a LOW reading (button released)

// -- IMU (QMI8658 accelerometer — used for critter tilt + auto-dim) --
SensorQMI8658 qmi;
bool qmiReady = false;
float lastAccX = 0.0f, lastAccY = 0.0f, lastAccZ = 0.0f;
bool  accelFresh = false;
#if CRITTER_ENABLED
float accelBaseX = 0.0f;       // resting tilt offset (calibrated at boot)
#endif

// -- Auto-dim state --
unsigned long lastMotionTime = 0;
float prevAccX = 0.0f, prevAccY = 0.0f, prevAccZ = 0.0f;
bool  backlightDimmed = false;
uint8_t currentBrightness = BRIGHTNESS;

// -- Critter pet state --
enum CritterState { CRIT_IDLE, CRIT_WALKING, CRIT_JUMPING, CRIT_WAVING, CRIT_SLEEPING };
CritterState critterState = CRIT_IDLE;
float critterX = 320.0f;
float critterY = 0.0f;       // jump offset (positive = up)
float critterVX = 0.0f;
float critterVY = 0.0f;
int critterDir = 1;           // 1=right, -1=left
int critterAnimTick = 0;
int critterIdleTicks = 0;
int critterStateTicks = 0;
unsigned long critterBlinkTime = 0;
bool critterBlink = false;
unsigned long critterEdgeTime = 0;  // when critter first entered edge zone
bool critterInEdge = false;

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};

// =============================================================
// TCA9554 I/O Expander Helpers
// Address 0x20 on peripheral I2C bus (Wire1).
// Pin 1: ADC voltage divider enable (LOW = enabled)
// Pin 6: Battery power latch (HIGH = on, LOW = off)
// =============================================================
static const uint8_t TCA_REG_OUTPUT = 0x01;
static const uint8_t TCA_REG_CONFIG = 0x03;

uint8_t tca9554ReadReg(uint8_t reg) {
  Wire1.beginTransmission(TCA9554_ADDR);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom((uint8_t)TCA9554_ADDR, (uint8_t)1);
  return Wire1.available() ? Wire1.read() : 0xFF;
}

void tca9554WriteReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(TCA9554_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

void tca9554SetPin(uint8_t pin, bool high) {
  // Set pin as output
  uint8_t config = tca9554ReadReg(TCA_REG_CONFIG);
  config &= ~(1 << pin);
  tca9554WriteReg(TCA_REG_CONFIG, config);

  // Set output level
  uint8_t output = tca9554ReadReg(TCA_REG_OUTPUT);
  if (high) output |= (1 << pin);
  else      output &= ~(1 << pin);
  tca9554WriteReg(TCA_REG_OUTPUT, output);
}

void latchPowerOn() {
  Wire1.beginTransmission(TCA9554_ADDR);
  uint8_t err = Wire1.endTransmission();
  if (err != 0) {
    Serial.printf("[Power] TCA9554 not found at 0x%02X (err=%d)\n", TCA9554_ADDR, err);
    return;
  }

  tca9554SetPin(TCA9554_PWR_PIN, true);   // Keep power on
  Serial.println("[Power] Latch ON");
}

void powerOff() {
  Serial.println("[Power] Shutting down...");
  gfx->fillScreen(0x0000);
  gfx->setTextColor(gfx->color565(255, 80, 80));
  gfx->setTextSize(3);
  gfx->setCursor(220, 60);
  gfx->print("Powering off...");
  gfx->flush();
  delay(500);

  // Turn off backlight
  setBacklight(0);

  // Release power latch — board will lose power
  tca9554SetPin(TCA9554_PWR_PIN, false);

  // If still alive (USB power), wait briefly then reboot
  // instead of sitting dark forever (which requires a power cycle to fix)
  delay(2000);
  ESP.restart();
}

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

      weather.lat     = doc["coord"]["lat"] | 0.0f;
      weather.lon     = doc["coord"]["lon"] | 0.0f;
      weather.sunrise = doc["sys"]["sunrise"] | 0UL;
      weather.sunset  = doc["sys"]["sunset"]  | 0UL;

      // Capitalize first letter
      if (weather.description[0] >= 'a' && weather.description[0] <= 'z') {
        weather.description[0] -= 32;
      }

      Serial.printf("[Weather] %.1f\xF8, %s\n", weather.temp, weather.description);
    } else {
      Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[Weather] HTTP error: %d\n", code);
  }

  http.end();
}

// =============================================================
// AQI (Air Quality Index)
// =============================================================
const char* aqiLabel(int aqi) {
  switch (aqi) {
    case 1: return "Good";
    case 2: return "Fair";
    case 3: return "Moderate";
    case 4: return "Poor";
    case 5: return "V.Poor";
    default: return "??";
  }
}

uint16_t aqiColor(int aqi) {
  switch (aqi) {
    case 1: return GOOD_COLOR;
    case 2: return GOOD_COLOR;
    case 3: return WARN_COLOR;
    case 4: return ERR_COLOR;
    case 5: return ERR_COLOR;
    default: return TEXT_DIM;
  }
}

void fetchAQI() {
  if (!wifiConnected || weather.lat == 0.0f) return;

  char url[160];
  snprintf(url, sizeof(url),
    "http://api.openweathermap.org/data/2.5/air_pollution?lat=%.4f&lon=%.4f&appid=%s",
    weather.lat, weather.lon, OWM_API_KEY);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      weather.aqi   = doc["list"][0]["main"]["aqi"] | 0;
      weather.pm2_5 = doc["list"][0]["components"]["pm2_5"] | 0.0f;
      Serial.printf("[AQI] %d (%s), PM2.5: %.1f\n", weather.aqi, aqiLabel(weather.aqi), weather.pm2_5);
    } else {
      Serial.printf("[AQI] JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[AQI] HTTP error: %d\n", code);
  }

  http.end();
}

// =============================================================
// Battery Voltage
// Sampled at boot and refreshed every 60 seconds in loop().
// =============================================================
float cachedBatteryVoltage = 0.0f;

void sampleBatteryOnce() {
  // Enable voltage divider
  tca9554SetPin(TCA9554_ADC_EN, false);
  delayMicroseconds(500); // let divider settle (500µs vs 10ms to avoid display flicker)

  // Read raw ADC — using analogReadMilliVolts on GPIO 4 only
  int mv = analogReadMilliVolts(BAT_ADC_PIN);
  cachedBatteryVoltage = (mv * BAT_DIVIDER) / 1000.0f;

  // Disable divider to reduce power drain
  tca9554SetPin(TCA9554_ADC_EN, true);

  Serial.printf("[Battery] Read: %dmV raw, %.2fV battery\n", mv, cachedBatteryVoltage);
}

float getBatteryVoltage() {
  return cachedBatteryVoltage;
}

int getBatteryPercent(float voltage) {
  // LiPo curve: 3.2V = 0%, 4.15V = 100% (conservative)
  int pct = (int)((voltage - 3.2f) / 0.95f * 100.0f);
  return constrain(pct, 0, 100);
}

// =============================================================
// IMU Reading — shared between critter and auto-dim
// =============================================================
void readIMU() {
  accelFresh = false;
  if (!qmiReady) return;
  if (qmi.getDataReady()) {
    IMUdata acc, gyro;
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
      lastAccX = acc.x;
      lastAccY = acc.y;
      lastAccZ = acc.z;
      accelFresh = true;
    }
    qmi.getGyroscope(gyro.x, gyro.y, gyro.z); // must read both
  }
}

// =============================================================
// Backlight Control (PWM, active-low)
// =============================================================
void setBacklight(uint8_t brightness) {
  currentBrightness = brightness;
  // Active-low: 0 = full on, 255 = off
  ledcWrite(LCD_BL_PIN, 255 - brightness);
}

void updateAutoDim() {
  if (!qmiReady) return;
  unsigned long now = millis();

  // Check for motion: delta against previous reading
  float dx = lastAccX - prevAccX;
  float dy = lastAccY - prevAccY;
  float dz = lastAccZ - prevAccZ;
  float delta = sqrtf(dx*dx + dy*dy + dz*dz);
  prevAccX = lastAccX;
  prevAccY = lastAccY;
  prevAccZ = lastAccZ;

  if (delta > 0.15f) {
    lastMotionTime = now;
    if (backlightDimmed) {
      backlightDimmed = false;
      setBacklight(BRIGHTNESS);
      Serial.println("[Dim] Motion detected, restoring brightness");
    }
  }

  // Dim after timeout
  unsigned long dimTimeoutMs = (unsigned long)DIM_TIMEOUT_MIN * 60UL * 1000UL;
  if (!backlightDimmed && (now - lastMotionTime >= dimTimeoutMs)) {
    backlightDimmed = true;
    setBacklight(DIM_BRIGHTNESS);
    Serial.println("[Dim] No motion, dimming backlight");
  }
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
// Critter Pet — animated blob that roams the bottom of the screen
// =============================================================
void updateCritter() {
  critterAnimTick++;

  // Use shared IMU data (read by readIMU() in loop)
  float tiltX = qmiReady ? (lastAccX - accelBaseX) : 0.0f;

  // Log accel every ~10 seconds (70 ticks at 7fps)
  if (qmiReady && critterAnimTick % 70 == 0) {
    Serial.printf("[Critter] accel x=%.2f state=%d pos=%.0f\n", tiltX, critterState, critterX);
  }

  // Dead zone ±1.0, map to ±3 px/tick max (at 7fps, 3*7=21 px/sec)
  float tiltForce = 0.0f;
  if (tiltX > 1.0f) tiltForce = min((tiltX - 1.0f) * 1.0f, 3.0f);
  else if (tiltX < -1.0f) tiltForce = max((tiltX + 1.0f) * 1.0f, -3.0f);
  bool tilting = (fabsf(tiltForce) > 0.1f);

  // (Corner escape removed — world wraps around)

  // Blink timer
  if (millis() - critterBlinkTime > 3000 + random(2000)) {
    critterBlink = true;
    critterBlinkTime = millis();
  }
  if (critterBlink && millis() - critterBlinkTime > 150) {
    critterBlink = false;
  }

  // State machine (probabilities scaled for ~7fps)
  critterStateTicks++;

  switch (critterState) {
    case CRIT_IDLE:
      critterVX *= 0.9f;
      critterIdleTicks++;
      if (tilting || random(700) < 10) {
        critterState = CRIT_WALKING;
        critterStateTicks = 0;
        critterIdleTicks = 0;
      } else if (random(700) < 5) {
        critterState = CRIT_JUMPING;
        critterStateTicks = 0;
        critterVY = 1.2f;
        critterIdleTicks = 0;
      } else if (random(700) < 3) {
        critterState = CRIT_WAVING;
        critterStateTicks = 0;
        critterIdleTicks = 0;
      } else if (critterIdleTicks > 210) {  // ~30 seconds at 7fps
        critterState = CRIT_SLEEPING;
        critterStateTicks = 0;
      }
      break;

    case CRIT_WALKING:
      if (tilting) {
        critterVX = tiltForce;
        critterDir = (tiltForce > 0) ? 1 : -1;
      } else {
        critterVX += critterDir * 0.4f;
        critterVX = constrain(critterVX, -2.0f, 2.0f);
        if (critterStateTicks > 35 + (int)random(70)) {
          critterState = CRIT_IDLE;
          critterStateTicks = 0;
        }
      }
      if (random(700) < 8) {
        critterState = CRIT_JUMPING;
        critterStateTicks = 0;
        critterVY = 1.2f;
      }
      break;

    case CRIT_JUMPING:
      critterY += critterVY;
      critterVY -= 0.3f;  // gravity scaled for 7fps
      if (critterY <= 0) {
        critterY = 0;
        critterVY = 0;
        critterState = tilting ? CRIT_WALKING : CRIT_IDLE;
        critterStateTicks = 0;
      }
      break;

    case CRIT_WAVING:
      if (critterStateTicks > 35) {  // ~5 seconds
        critterState = CRIT_IDLE;
        critterStateTicks = 0;
      }
      break;

    case CRIT_SLEEPING:
      critterVX = 0;
      if (tilting || random(700) < 2) {
        critterState = CRIT_IDLE;
        critterStateTicks = 0;
        critterIdleTicks = 0;
      }
      break;
  }

  // Apply velocity, wrap around edges (toroidal world)
  critterX += critterVX;
  if (critterX < -15.0f) {
    critterX = 655.0f;
  } else if (critterX > 655.0f) {
    critterX = -15.0f;
  }
}

void drawCritter() {
  int cx = (int)critterX;
  int cy = SCREEN_H - 18 - (int)critterY;
  int dir = critterDir;  // 1=right, -1=left
  bool sleeping = (critterState == CRIT_SLEEPING);

  // Body — round torso (radius 11)
  gfx->fillCircle(cx, cy, 11, CRITTER_BODY);
  // Belly highlight — lighter circle shifted down
  gfx->fillCircle(cx, cy + 3, 9, CRITTER_HIGHLIGHT);

  // Head — overlapping circle at top-front of body
  int hx = cx + dir * 8;
  int hy = cy - 8;
  if (sleeping) hy = cy - 5;  // head droops when sleeping
  gfx->fillCircle(hx, hy, 6, CRITTER_BODY);

  // Beak — small orange triangle pointing in walk direction
  int bx = hx + dir * 6;
  int by = hy;
  gfx->fillTriangle(bx, by, bx + dir * 6, by + 1, bx, by + 4, CRITTER_BEAK);

  // Eye — single eye on the visible side
  int ex = hx + dir * 3;
  int ey = hy - 1;
  if (sleeping || critterBlink) {
    // Closed eye — horizontal line
    gfx->drawFastHLine(ex - 1, ey, 4, CRITTER_EYE);
  } else {
    gfx->fillCircle(ex, ey, 1, CRITTER_EYE);
  }

  // Cheek blush — small pink spot below eye
  gfx->fillRect(hx + dir * 1, hy + 3, 3, 3, CRITTER_CHEEK);

  // Wing — overlapping circle on body, deeper blue
  int wx = cx - dir * 3;
  int wy = cy - 1;
  if (critterState == CRIT_JUMPING || critterState == CRIT_WAVING) {
    // Wing raised — shifted up
    int wingWiggle = ((critterAnimTick / 4) % 2 == 0) ? -3 : 0;
    gfx->fillCircle(wx, wy - 6 + wingWiggle, 6, CRITTER_WING);
  } else if (critterState == CRIT_WALKING) {
    // Wing bobs slightly while walking
    int wingBob = ((critterAnimTick / 4) % 2 == 0) ? -1 : 0;
    gfx->fillCircle(wx, wy + wingBob, 6, CRITTER_WING);
  } else {
    // Idle/sleeping — wing resting at side
    gfx->fillCircle(wx, wy, 6, CRITTER_WING);
  }

  // Tail — triangle at back of body, opposite to direction
  int tx = cx - dir * 11;
  int ty = cy - 3;
  gfx->fillTriangle(tx, ty, tx - dir * 8, ty - 6, tx - dir * 5, ty + 1, CRITTER_WING);

  // Legs — two thin rectangles below body with small feet
  if (critterState == CRIT_JUMPING) {
    // Legs tucked up during jump
    gfx->fillRect(cx - 4, cy + 9, 2, 4, CRITTER_BEAK);
    gfx->fillRect(cx + 3, cy + 9, 2, 4, CRITTER_BEAK);
  } else {
    // Walking: alternate leg positions
    int legOff = (critterState == CRIT_WALKING && (critterAnimTick / 4) % 2 == 0) ? 2 : 0;
    // Left leg
    gfx->fillRect(cx - 4, cy + 10 - legOff, 2, 5, CRITTER_BEAK);
    gfx->fillRect(cx - 5, cy + 14 - legOff, 4, 1, CRITTER_BEAK);  // foot
    // Right leg
    gfx->fillRect(cx + 3, cy + 10 + legOff, 2, 5, CRITTER_BEAK);
    gfx->fillRect(cx + 1, cy + 14 + legOff, 4, 1, CRITTER_BEAK);  // foot
  }

  // Sleeping ZZZ
  if (sleeping) {
    int zy = hy - 10 - ((critterAnimTick / 7) % 3) * 5;
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(1);
    gfx->setCursor(hx + 8, zy);
    gfx->print("z");
    if ((critterAnimTick / 7) % 6 < 3) {
      gfx->setCursor(hx + 15, zy - 9);
      gfx->print("z");
    }
  }
}

// =============================================================
// Sun Arc — visual sunrise/sunset timeline
// =============================================================
void drawSunArc(int panelX, int arcCY) {
  if (weather.sunrise == 0 || weather.sunset == 0) return;

  long tzOffset = (long)UTC_OFFSET * 3600 + (long)DST_OFFSET * 3600;
  unsigned long localNow = (unsigned long)time(nullptr) + tzOffset;
  unsigned long rise = weather.sunrise + tzOffset;
  unsigned long set  = weather.sunset  + tzOffset;

  int arcW = 150;
  int arcH = 28;
  int arcX = panelX + 13; // center arc in 176px panel
  int segments = 30;

  bool isDaytime = (localNow >= rise && localNow < set);
  float progress;

  if (isDaytime) {
    progress = (float)(localNow - rise) / (float)(set - rise);
  } else {
    // Nighttime: progress across the night
    unsigned long nightStart, nightEnd;
    if (localNow >= set) {
      nightStart = set;
      nightEnd = rise + 86400; // next sunrise
    } else {
      nightStart = set - 86400; // previous sunset
      nightEnd = rise;
    }
    progress = (float)(localNow - nightStart) / (float)(nightEnd - nightStart);
  }
  progress = constrain(progress, 0.0f, 1.0f);

  // Draw arc: upward dome for day, downward dome for night
  for (int i = 0; i < segments; i++) {
    float a1 = PI * (float)i / segments;
    float a2 = PI * (float)(i + 1) / segments;
    int x1 = arcX + (int)(a1 / PI * arcW);
    int y1, y2;
    int x2 = arcX + (int)(a2 / PI * arcW);
    if (isDaytime) {
      y1 = arcCY - (int)(sinf(a1) * arcH);
      y2 = arcCY - (int)(sinf(a2) * arcH);
    } else {
      y1 = arcCY + (int)(sinf(a1) * arcH);
      y2 = arcCY + (int)(sinf(a2) * arcH);
    }
    gfx->drawLine(x1, y1, x2, y2, TEXT_DIM);
  }

  // Dot position along arc
  float dotAngle = PI * progress;
  int dotX = arcX + (int)(dotAngle / PI * arcW);
  int dotY;
  if (isDaytime) {
    dotY = arcCY - (int)(sinf(dotAngle) * arcH);
  } else {
    dotY = arcCY + (int)(sinf(dotAngle) * arcH);
  }
  uint16_t dotColor = isDaytime ? WARN_COLOR : TEXT_SECONDARY;
  gfx->fillCircle(dotX, dotY, 3, dotColor);

  // Time labels at endpoints
  int riseH = (rise % 86400) / 3600;
  int riseM = ((rise % 86400) % 3600) / 60;
  int setH  = (set % 86400) / 3600;
  int setM  = ((set % 86400) % 3600) / 60;

  // Convert to 12h
  int rH = riseH % 12; if (rH == 0) rH = 12;
  int sH = setH % 12;  if (sH == 0) sH = 12;

  char rBuf[8], sBuf[8];
  snprintf(rBuf, sizeof(rBuf), "%d:%02d", rH, riseM);
  snprintf(sBuf, sizeof(sBuf), "%d:%02d", sH, setM);

  gfx->setTextColor(TEXT_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(arcX, arcCY + 6);
  gfx->print(rBuf);
  int sLen = strlen(sBuf) * 6;
  gfx->setCursor(arcX + arcW - sLen, arcCY + 6);
  gfx->print(sBuf);
}

// =============================================================
// Main Panel (0) — Time | Date | Weather | Battery
// =============================================================
void drawMainPanel() {
  gfx->fillScreen(BG_COLOR);

  struct tm t;
  bool hasTime = getLocalTime(&t, 0);

  // ── TIME SECTION (x: 0–178) ─────────────────────────────
  // Two rows centered: HH:MM:SS then AM/PM
  drawPanel(8, 8, 170, SCREEN_H - 16, PANEL_COLOR);

  if (hasTime) {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;

    // HH:MM:SS — centered
    char timeBuf[10];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d:%02d", hour12, t.tm_min, t.tm_sec);
    int timeW = strlen(timeBuf) * 18; // textSize 3: 6*3=18px per char
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(3);
    gfx->setCursor(8 + (170 - timeW) / 2, 48);
    gfx->print(timeBuf);

    // AM/PM — centered below
    const char* ampm = t.tm_hour >= 12 ? "PM" : "AM";
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(2);
    gfx->setCursor(8 + (170 - 24) / 2, 82); // 2 chars * 12px = 24px
    gfx->print(ampm);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(3);
    gfx->setCursor(8 + (170 - 90) / 2, 60); // "--:--" = 5 chars * 18px
    gfx->print("--:--");
  }

  // ── DATE SECTION (x: 186–362) ──────────────────────────
  drawPanel(186, 8, 176, SCREEN_H - 16, PANEL_COLOR);

  if (hasTime) {
    // Day of week
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                          "Thursday", "Friday", "Saturday"};
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setTextSize(2);
    gfx->setCursor(198, 20);
    gfx->print(days[t.tm_wday]);

    // Month Day
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d", months[t.tm_mon], t.tm_mday);
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(2);
    gfx->setCursor(198, 44);
    gfx->print(dateBuf);

    // Year
    char yearBuf[6];
    snprintf(yearBuf, sizeof(yearBuf), "%d", t.tm_year + 1900);
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(2);
    gfx->setCursor(198, 68);
    gfx->print(yearBuf);

    // Sun arc timeline
    drawSunArc(186, 115);
  }

  // ── WEATHER SECTION (x: 370–526) ──────────────────────
  drawPanel(370, 8, 156, SCREEN_H - 16, PANEL_COLOR);

  if (weather.valid) {
    // Temperature — large
    char tempBuf[10];
    const char* unit = (strcmp(OWM_UNITS, "imperial") == 0) ? "F" : "C";
    snprintf(tempBuf, sizeof(tempBuf), "%.0f\xF8%s", weather.temp, unit);
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(4);
    gfx->setCursor(382, 22);
    gfx->print(tempBuf);

    // Description — word wrap if > 12 chars (panel is ~144px at textSize 2)
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setTextSize(2);
    int descLen = strlen(weather.description);
    int maxChars = 12;
    if (descLen <= maxChars) {
      gfx->setCursor(382, 68);
      gfx->print(weather.description);
    } else {
      // Find last space within maxChars
      int split = maxChars;
      for (int i = maxChars - 1; i > 0; i--) {
        if (weather.description[i] == ' ') { split = i; break; }
      }
      char line1[32];
      strlcpy(line1, weather.description, split + 1);
      gfx->setCursor(382, 62);
      gfx->print(line1);
      gfx->setCursor(382, 82);
      gfx->print(&weather.description[split + (weather.description[split] == ' ' ? 1 : 0)]);
    }

    // Feels like + humidity
    char detailBuf[28];
    snprintf(detailBuf, sizeof(detailBuf), "Feels %.0f\xF8 | %d%%", weather.feels_like, weather.humidity);
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(1);
    gfx->setCursor(382, 106);
    gfx->print(detailBuf);

    // AQI
    if (weather.aqi > 0) {
      char aqiBuf[28];
      snprintf(aqiBuf, sizeof(aqiBuf), "AQI: %s (PM2.5: %.0f)", aqiLabel(weather.aqi), weather.pm2_5);
      gfx->setTextColor(aqiColor(weather.aqi));
      gfx->setTextSize(1);
      gfx->setCursor(382, 120);
      gfx->print(aqiBuf);
    }
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(2);
    gfx->setCursor(390, 50);
    gfx->print("Weather");
    gfx->setCursor(390, 78);
    gfx->print("loading...");
  }

  // ── STATUS SECTION (x: 534–632) ───────────────────────
  drawPanel(534, 8, 98, SCREEN_H - 16, PANEL_COLOR);

  // WiFi status
  gfx->setTextSize(2);
  gfx->setCursor(544, 16);
  if (wifiConnected) {
    gfx->setTextColor(GOOD_COLOR);
    gfx->print("WiFi");
    gfx->setTextSize(1);
    gfx->setTextColor(TEXT_DIM);
    gfx->setCursor(544, 38);
    int rssi = WiFi.RSSI();
    char rssiBuf[12];
    snprintf(rssiBuf, sizeof(rssiBuf), "%ddBm", rssi);
    gfx->print(rssiBuf);
  } else {
    gfx->setTextColor(ERR_COLOR);
    gfx->print("No WiFi");
  }

  // Battery (sampled once at boot)
  float batV = getBatteryVoltage();
  gfx->setTextSize(2);
  gfx->setCursor(544, 58);
  if (batV > 0.5f) {
    int batPct = getBatteryPercent(batV);
    if (batPct > 50)      gfx->setTextColor(GOOD_COLOR);
    else if (batPct > 20) gfx->setTextColor(WARN_COLOR);
    else                   gfx->setTextColor(ERR_COLOR);
    char batBuf[8];
    snprintf(batBuf, sizeof(batBuf), "%d%%", batPct);
    gfx->print(batBuf);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->print("Bat --");
  }

  // Uptime
  unsigned long uptimeSec = millis() / 1000;
  unsigned long hrs = uptimeSec / 3600;
  unsigned long mins = (uptimeSec % 3600) / 60;
  char uptBuf[12];
  if (hrs >= 24) {
    snprintf(uptBuf, sizeof(uptBuf), "%lud%luh", hrs / 24, hrs % 24);
  } else {
    snprintf(uptBuf, sizeof(uptBuf), "%luh%lum", hrs, mins);
  }
  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM);
  gfx->setCursor(544, 100);
  gfx->print(uptBuf);

  // Free heap
  char heapBuf[8];
  snprintf(heapBuf, sizeof(heapBuf), "%dK", ESP.getFreeHeap() / 1024);
  gfx->setCursor(544, 130);
  gfx->print(heapBuf);

#if CRITTER_ENABLED
  // Critter pet — draw after panels so it overlaps bottom edges
  updateCritter();
  drawCritter();
#endif

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
  // CRITICAL: Latch power ASAP — on battery, the board dies when
  // the power button is released unless we grab the latch first.
  Wire.begin(TOUCH_SDA, TOUCH_SCL);   // Bus 0: touch controller
  Wire1.begin(I2C_SDA, I2C_SCL);      // Bus 1: peripherals (has TCA9554)
  delay(20);                           // Let TCA9554 stabilize after power-on
  latchPowerOn();
  sampleBatteryOnce(); // Initial battery read at boot

  // Init IMU (QMI8658 — used for auto-dim motion detection + critter tilt)
  qmiReady = qmi.begin(Wire1, QMI8658_ADDR, I2C_SDA, I2C_SCL);
  if (qmiReady) {
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_62_5Hz);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS, SensorQMI8658::GYR_ODR_56_05Hz);
    qmi.enableAccelerometer();
    qmi.enableGyroscope();
  }

  Serial.begin(115200);
  delay(300);
  Serial.println("\n=============================");
  Serial.println("  Desk Status Bar — Booting");
  Serial.println("=============================\n");

  if (qmiReady) {
    Serial.println("[IMU] QMI8658 initialized");
  } else {
    Serial.println("[IMU] QMI8658 not found — auto-dim disabled");
  }

  // Power button read pin (GPIO 16) — HIGH when pressed
  // INPUT_PULLDOWN ensures it reads LOW when not pressed
  pinMode(BTN_PWR_READ, INPUT_PULLDOWN);

  // Init display + canvas
  if (!gfx->begin()) {
    Serial.println("[Display] FATAL: gfx->begin() failed!");
    while (1) delay(100);
  }
  Serial.println("[Display] Initialized");

  // Rotate drawing coordinates to landscape (640x172)
  // Canvas buffer stays portrait (172x640) so flush works with QSPI
  gfx->setRotation(ROTATION);

  // Backlight on via PWM (active-low: 255 = off, 0 = full on)
  ledcAttach(LCD_BL_PIN, 5000, 8);
  setBacklight(BRIGHTNESS);
  lastMotionTime = millis();
  Serial.println("[Backlight] On (PWM)");

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
    fetchAQI();
    lastWeatherFetch = millis();
  }

#if CRITTER_ENABLED
  // Calibrate resting tilt now that device has been sitting still
  if (qmiReady) {
    float sum = 0.0f;
    int samples = 0;
    for (int i = 0; i < 20; i++) {
      IMUdata acc, gyro;
      if (qmi.getDataReady()) {
        qmi.getAccelerometer(acc.x, acc.y, acc.z);
        qmi.getGyroscope(gyro.x, gyro.y, gyro.z);
        sum += acc.x;
        samples++;
      }
      delay(20);
    }
    if (samples > 0) accelBaseX = sum / samples;
    Serial.printf("[IMU] Calibrated baseline x=%.2f (%d samples)\n", accelBaseX, samples);
  }
#endif

  drawSplash("Ready!");
  delay(400);

  Serial.println("[Boot] Setup complete, entering main loop\n");
}

// =============================================================
// Main Loop
// =============================================================
void loop() {
  unsigned long now = millis();

  // Read IMU + update auto-dim (before draw so motion restores brightness immediately)
  readIMU();
  updateAutoDim();

  // Redraw display — faster when critter is enabled for smooth animation
#if CRITTER_ENABLED
  static const unsigned long drawInterval = 150;  // ~7fps
#else
  static const unsigned long drawInterval = CLOCK_INTERVAL;
#endif
  if (now - lastClockUpdate >= drawInterval) {
    lastClockUpdate = now;
    drawMainPanel();
  }

  // Refresh weather periodically
  if (wifiConnected && (now - lastWeatherFetch >= WEATHER_INTERVAL)) {
    Serial.println("[Loop] Refreshing weather...");
    fetchWeather();
    fetchAQI();
    lastWeatherFetch = now;
  }

  // Refresh battery voltage periodically
  if (now - lastBatteryRead >= 60000) {
    sampleBatteryOnce();
    lastBatteryRead = now;
  }

  // Reconnect WiFi if dropped
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    wifiConnected = (WiFi.status() == WL_CONNECTED);
  }

  // Handle power button long-press (3 seconds → power off)
  // GPIO 16 is active-HIGH: HIGH = pressed, LOW = released
  // Safety: we must see the button released (LOW) at least once after boot
  // before arming, so a held-down button at boot doesn't auto-shutdown.
  bool pwrDown = digitalRead(BTN_PWR_READ);
  if (!pwrBtnReady) {
    // Wait until button is released after boot
    if (!pwrDown) pwrBtnReady = true;
  } else {
    if (pwrDown && !pwrBtnWasDown) {
      pwrBtnDownAt = now;
      pwrBtnWasDown = true;
      lastMotionTime = now;
    } else if (!pwrDown) {
      pwrBtnWasDown = false;
    } else if (pwrDown && pwrBtnWasDown && (now - pwrBtnDownAt >= 3000)) {
      powerOff();
    }
  }

  // Handle touch input
  TouchPoint tp = readTouch();
  if (tp.pressed && (now - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = now;
    lastMotionTime = now;
    Serial.printf("[Touch] x=%d y=%d\n", tp.x, tp.y);

    // Tap anywhere → force weather refresh
    if (wifiConnected) {
      drawSplash("Refreshing...");
      fetchWeather();
      fetchAQI();
      lastWeatherFetch = now;
    }
  }

  // Small yield to keep watchdog happy
  delay(50);
}
