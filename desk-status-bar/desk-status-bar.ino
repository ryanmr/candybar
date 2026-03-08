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
#include <Preferences.h>
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

#include "types.h"
#include "colors.h"

// =============================================================
// State
// =============================================================
WeatherData weather = {};

unsigned long lastWeatherFetch  = 0;
unsigned long lastClockUpdate   = 0;
unsigned long lastTouchTime     = 0;
unsigned long lastBatteryRead   = 0;

uint8_t currentTile[NUM_COLUMNS] = {0};
uint8_t currentPage = 0;
unsigned long lastNavTime = 0;
bool wifiConnected = false;
bool timeSynced    = false;
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
int critterDirTicks = 0;           // ticks spent walking in current direction

// -- Power source state --
bool usbPowered = false;             // true when running on USB/AC power
bool batteryCharging = false;         // true when ETA6098 is actively charging

// -- Touch focus ring state --
int8_t focusCol = -1;                // which column was last tapped (-1 = none)
unsigned long focusTouchTime = 0;    // when the focus ring was triggered

#include "hal.h"

Preferences prefs;

#include "networking.h"
#include "imu.h"

#include "audio.h"

#include "drawing.h"

#include "critter.h"

#include "tiles.h"
#include "ui.h"
#include "touch.h"

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
  updatePowerState();  // Detect USB vs battery power

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
  // 1000Hz works better than 5000Hz for partial duty-cycle dimming
  ledcAttach(LCD_BL_PIN, 1000, 8);
  setBacklight(BRIGHTNESS);
  lastMotionTime = millis();
  Serial.println("[Backlight] On (PWM)");

  // Load cached weather from NVS (shows data immediately while WiFi connects)
  loadWeatherCache();

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

  // Initial weather fetch — skip if cache is still fresh (saves API hits during dev)
  if (wifiConnected) {
    if (isWeatherCacheFresh()) {
      Serial.println("[Boot] Weather cache is fresh, skipping fetch");
      lastWeatherFetch = millis();
    } else {
      drawSplash("Fetching weather...");
      fetchWeather();
      fetchAQI();
      saveWeatherCache();
      lastWeatherFetch = millis();
    }
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

#if TOUCH_SOUND_ENABLED
  drawSplash("Setting up audio...");
  audioSetup();
  playStartupTune();
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
    saveWeatherCache();
    lastWeatherFetch = now;
  }

  // Refresh battery voltage + power state periodically
  if (now - lastBatteryRead >= 60000) {
    sampleBatteryOnce();
    updatePowerState();
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

    // Touch coords: tp.x = long axis (0-639, inverted), tp.y = short axis (0-171)
    int lx = 639 - tp.x;
    int ly = tp.y;
    Serial.printf("[Touch] lx=%d ly=%d\n", lx, ly);

#if TOUCH_SOUND_ENABLED
    playClick();
#endif

    handleTouch(lx, ly, now);
  }

  // Auto-return to home tiles after inactivity
  if (AUTO_RETURN_MIN > 0 && lastNavTime > 0) {
    unsigned long autoReturnMs = (unsigned long)AUTO_RETURN_MIN * 60UL * 1000UL;
    if (now - lastNavTime >= autoReturnMs) {
      bool wasAway = (currentPage != 0);
      for (int c = 0; c < NUM_COLUMNS; c++) {
        if (currentTile[c] != 0) wasAway = true;
        currentTile[c] = 0;
      }
      currentPage = 0;
      lastNavTime = 0;
      if (wasAway) Serial.println("[Nav] Auto-return to home");
    }
  }

  // Small yield to keep watchdog happy
  delay(50);
}
