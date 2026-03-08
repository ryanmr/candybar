#pragma once

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

// =============================================================
// Backlight Control (PWM, active-low)
// =============================================================
void setBacklight(uint8_t brightness) {
  currentBrightness = brightness;
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
void updatePowerState() {
  usbPowered = (digitalRead(BTN_PWR_READ) == LOW);

  // Read charger STAT on TCA9554 P5 (ensure it's configured as input)
  uint8_t config = tca9554ReadReg(TCA_REG_CONFIG);
  if (!(config & (1 << TCA9554_CHRG_STAT))) {
    config |= (1 << TCA9554_CHRG_STAT);
    tca9554WriteReg(TCA_REG_CONFIG, config);
  }
  uint8_t input = tca9554ReadReg(0x00);  // TCA_REG_INPUT
  batteryCharging = !(input & (1 << TCA9554_CHRG_STAT));
}

// =============================================================
// Auto-Dim
// =============================================================
void updateAutoDim() {
  if (!qmiReady) return;
  // Never dim on USB power — not draining battery
  if (usbPowered) {
    if (backlightDimmed) {
      backlightDimmed = false;
      setBacklight(BRIGHTNESS);
      Serial.println("[Dim] USB power detected, restoring brightness");
    }
    lastMotionTime = millis();
    return;
  }
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
