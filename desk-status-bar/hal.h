#pragma once

// =============================================================
// TCA9554 I/O Expander Helpers
// Address 0x20 on peripheral I2C bus (Wire1).
// Pin 1: ADC voltage divider enable (LOW = enabled)
// Pin 6: Battery power latch (HIGH = on, LOW = off)
// =============================================================
static const uint8_t TCA_REG_INPUT  = 0x00;
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

// =============================================================
// Backlight Control (PWM, active-low)
// =============================================================
void setBacklight(uint8_t brightness) {
  // Active-low: 0 = full on, 255 = off
  uint8_t duty = 255 - brightness;
  ledcWrite(LCD_BL_PIN, duty);
  Serial.printf("[Backlight] brightness=%d duty=%d\n", brightness, duty);
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
// Power Source Detection
// GPIO 16 LOW = USB power, HIGH = battery power
// TCA9554 P5 (ETA6098 STAT): LOW = charging, HIGH = not charging
// =============================================================
// Configure TCA9554 P5 (charger STAT) as input — call once in setup()
void initChargerStatPin() {
  uint8_t config = tca9554ReadReg(TCA_REG_CONFIG);
  config |= (1 << TCA9554_CHRG_STAT);
  tca9554WriteReg(TCA_REG_CONFIG, config);
}

void updatePowerState() {
  usbPowered = (digitalRead(BTN_PWR_READ) == LOW);

  // Read charger STAT on TCA9554 P5 (configured as input in setup)
  uint8_t input = tca9554ReadReg(TCA_REG_INPUT);
  batteryCharging = !(input & (1 << TCA9554_CHRG_STAT));
}

// =============================================================
// Day/Night Detection
// =============================================================
bool isNighttime() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;  // fallback: daytime
  return (t.tm_hour < NIGHT_END_HOUR || t.tm_hour >= NIGHT_START_HOUR);
}

// =============================================================
// Motion Detection (extracted from former updateAutoDim)
// =============================================================
void checkMotion() {
  if (!qmiReady) return;

  float dx = lastAccX - prevAccX;
  float dy = lastAccY - prevAccY;
  float dz = lastAccZ - prevAccZ;
  float deltaSq = dx*dx + dy*dy + dz*dz;
  prevAccX = lastAccX;
  prevAccY = lastAccY;
  prevAccZ = lastAccZ;

  if (deltaSq > MOTION_THRESHOLD * MOTION_THRESHOLD) {
    lastMotionTime = millis();
  }
}

// =============================================================
// Power Mode Resolution — pure function
// =============================================================
PowerMode resolvePowerMode() {
  // USB power bypasses all power saving
  if (usbPowered) return PWR_ACTIVE;

  unsigned long now = millis();
  unsigned long idleMs = now - lastMotionTime;
  unsigned long dimMs = (unsigned long)DIM_TIMEOUT_MIN * 60UL * 1000UL;
  unsigned long deepMs = (unsigned long)DEEP_IDLE_TIMEOUT_MIN * 60UL * 1000UL;
  bool night = isNighttime();

  if (wifiAwayMode) return PWR_AWAY;

  if (idleMs < dimMs) {
    return night ? PWR_ACTIVE_NIGHT : PWR_ACTIVE;
  }

  if (idleMs < deepMs) {
    return night ? PWR_IDLE_NIGHT : PWR_IDLE_DAY;
  }

  return night ? PWR_DEEP_NIGHT : PWR_DEEP_DAY;
}

// =============================================================
// Power Mode Application — sets brightness, draw interval, logs
// =============================================================
const char* powerModeName(PowerMode m) {
  switch (m) {
    case PWR_ACTIVE:       return "ACTIVE";
    case PWR_ACTIVE_NIGHT: return "ACTIVE_NIGHT";
    case PWR_IDLE_DAY:     return "IDLE_DAY";
    case PWR_IDLE_NIGHT:   return "IDLE_NIGHT";
    case PWR_DEEP_DAY:     return "DEEP_DAY";
    case PWR_DEEP_NIGHT:   return "DEEP_NIGHT";
    case PWR_AWAY:         return "AWAY";
    default:               return "UNKNOWN";
  }
}

void applyPowerMode(PowerMode newMode, PowerMode prevMode) {
  if (newMode == prevMode) return;

  Serial.printf("[Power] %s -> %s\n", powerModeName(prevMode), powerModeName(newMode));

  switch (newMode) {
    case PWR_ACTIVE:
      setBacklight(BRIGHTNESS);
      drawInterval = 150;
      wifiReconnectInterval = 0;
      break;
    case PWR_ACTIVE_NIGHT:
      setBacklight(DIM_BRIGHTNESS);
      drawInterval = 150;
      wifiReconnectInterval = 0;
      break;
    case PWR_IDLE_DAY:
      setBacklight(DIM_BRIGHTNESS_DAY);
      drawInterval = 150;
      wifiReconnectInterval = 0;
      break;
    case PWR_IDLE_NIGHT:
      setBacklight(DIM_BRIGHTNESS);
      drawInterval = 150;
      wifiReconnectInterval = 0;
      break;
    case PWR_DEEP_DAY:
      setBacklight(DIM_BRIGHTNESS_DAY);
      drawInterval = DEEP_DAY_DRAW_INTERVAL;
      wifiReconnectInterval = WIFI_CYCLE_DAY_MS;
      break;
    case PWR_DEEP_NIGHT:
      setBacklight(DIM_BRIGHTNESS);
      drawInterval = DEEP_NIGHT_DRAW_INTERVAL;
      wifiReconnectInterval = WIFI_CYCLE_NIGHT_MS + (esp_random() % WIFI_CYCLE_JITTER_MS);
      break;
    case PWR_AWAY:
      setBacklight(DIM_BRIGHTNESS);
      drawInterval = DEEP_NIGHT_DRAW_INTERVAL;
      wifiReconnectInterval = WIFI_AWAY_INTERVAL_MS;
      break;
  }
}
