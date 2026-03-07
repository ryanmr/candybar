#pragma once

// =============================================================
// Tile Draw Functions — per-column, per-tile content
// Each takes panel coordinates (px, py, pw, ph)
// =============================================================

// ── TIME tile 0: Clock (HH:MM:SS + AM/PM) ──────────────────
void drawTimeTile0(int px, int py, int pw, int ph, struct tm* t) {
  if (t) {
    int hour12 = formatHour12(t->tm_hour);

    char timeBuf[10];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d:%02d", hour12, t->tm_min, t->tm_sec);
    int timeW = strlen(timeBuf) * 18;
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(3);
    gfx->setCursor(px + (pw - timeW) / 2, py + 40);
    gfx->print(timeBuf);

    const char* ampm = t->tm_hour >= 12 ? "PM" : "AM";
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(2);
    gfx->setCursor(px + (pw - 24) / 2, py + 74);
    gfx->print(ampm);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(3);
    gfx->setCursor(px + (pw - 90) / 2, py + 52);
    gfx->print("--:--");
  }
}

// ── TIME tile 1: World Clocks ───────────────────────────────
void drawTimeTile1(int px, int py, int pw, int ph) {
  // time(nullptr) returns UTC epoch on ESP32
  time_t utcEpoch = time(nullptr);

  const char* labels[] = { WORLD_CLOCK_1_LABEL, WORLD_CLOCK_2_LABEL, WORLD_CLOCK_3_LABEL };
  const int offsets[] = { WORLD_CLOCK_1_OFFSET, WORLD_CLOCK_2_OFFSET, WORLD_CLOCK_3_OFFSET };

  for (int i = 0; i < 3; i++) {
    int rowY = py + 12 + i * 46;

    // Label
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(1);
    gfx->setCursor(px + 12, rowY);
    gfx->print(labels[i]);

    // Time (offsets are in minutes from UTC)
    time_t clockEpoch = utcEpoch + (long)offsets[i] * 60;
    struct tm ct;
    gmtime_r(&clockEpoch, &ct);
    int h12 = formatHour12(ct.tm_hour);
    char buf[10];
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, ct.tm_min, ct.tm_hour >= 12 ? "PM" : "AM");
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(2);
    gfx->setCursor(px + 12, rowY + 12);
    gfx->print(buf);
  }
}

// ── DATE tile 0: Day/Month/Year ─────────────────────────────
void drawDateTile0(int px, int py, int pw, int ph, struct tm* t) {
  if (!t) return;

  const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                        "Thursday", "Friday", "Saturday"};
  gfx->setTextColor(ACCENT_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(px + 12, py + 12);
  gfx->print(days[t->tm_wday]);

  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateBuf[16];
  snprintf(dateBuf, sizeof(dateBuf), "%s %d", months[t->tm_mon], t->tm_mday);
  gfx->setTextColor(TEXT_PRIMARY);
  gfx->setTextSize(2);
  gfx->setCursor(px + 12, py + 36);
  gfx->print(dateBuf);

  char yearBuf[6];
  snprintf(yearBuf, sizeof(yearBuf), "%d", t->tm_year + 1900);
  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(2);
  gfx->setCursor(px + 12, py + 60);
  gfx->print(yearBuf);

  // Day of year / days remaining
  char doyBuf[16];
  int doy = t->tm_yday + 1;
  int daysLeft = (((t->tm_year + 1900) % 4 == 0) ? 366 : 365) - doy;
  snprintf(doyBuf, sizeof(doyBuf), "Day %d (-%d)", doy, daysLeft);
  gfx->setTextColor(TEXT_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(px + 12, py + 88);
  gfx->print(doyBuf);
}

// ── DATE tile 1: Sun Arc ────────────────────────────────────
void drawDateTile1(int px, int py, int pw, int ph) {
  if (weather.sunrise == 0 || weather.sunset == 0) {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    gfx->setCursor(px + 12, py + 40);
    gfx->print("No sun data");
    return;
  }

  long tzOffset = (long)UTC_OFFSET * 3600 + (long)DST_OFFSET * 3600;
  unsigned long rise = weather.sunrise + tzOffset;
  unsigned long set  = weather.sunset  + tzOffset;

  int riseH = (rise % 86400) / 3600;
  int riseM = ((rise % 86400) % 3600) / 60;
  int setH  = (set % 86400) / 3600;
  int setM  = ((set % 86400) % 3600) / 60;
  int rH = formatHour12(riseH);
  int sH = formatHour12(setH);

  // Sunrise/sunset times — orange, textSize 2
  char buf[24];
  gfx->setTextColor(WARN_COLOR);
  gfx->setTextSize(2);
  snprintf(buf, sizeof(buf), "Rise %d:%02d%s", rH, riseM, riseH >= 12 ? "p" : "a");
  gfx->setCursor(px + 12, py + 10);
  gfx->print(buf);

  snprintf(buf, sizeof(buf), "Set  %d:%02d%s", sH, setM, setH >= 12 ? "p" : "a");
  gfx->setCursor(px + 12, py + 32);
  gfx->print(buf);

  // Sun arc — below the text
  drawSunArc(px, py + 78);

  // Day + night length — textSize 2
  unsigned long dayLen = weather.sunset - weather.sunrise;
  unsigned long nightLen = 86400 - dayLen;
  int dlH = dayLen / 3600;
  int dlM = (dayLen % 3600) / 60;
  int nlH = nightLen / 3600;
  int nlM = (nightLen % 3600) / 60;

  gfx->setTextColor(WARN_COLOR);
  gfx->setTextSize(2);
  snprintf(buf, sizeof(buf), "%dh%dm", dlH, dlM);
  gfx->setCursor(px + 12, py + 110);
  gfx->print(buf);

  gfx->setTextColor(TEXT_SECONDARY);
  snprintf(buf, sizeof(buf), "%dh%dm", nlH, nlM);
  int nw = strlen(buf) * 12;
  gfx->setCursor(px + pw - nw - 12, py + 110);
  gfx->print(buf);
}

// ── DATE tile 2: Moon Phase ─────────────────────────────────
float moonAge(int year, int month, int day) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  long jdn = day + (153L*m + 2)/5 + 365L*y + y/4 - y/100 + y/400 - 32045;
  float age = fmod((float)(jdn - 2451550) + 0.5f, 29.53059f);
  return age < 0 ? age + 29.53059f : age;
}

const char* moonPhaseName(float age) {
  if (age < 1.85f)  return "New Moon";
  if (age < 5.53f)  return "Wax Crescent";
  if (age < 9.22f)  return "First Quarter";
  if (age < 12.91f) return "Wax Gibbous";
  if (age < 16.61f) return "Full Moon";
  if (age < 20.30f) return "Wan Gibbous";
  if (age < 23.99f) return "Last Quarter";
  if (age < 27.68f) return "Wan Crescent";
  return "New Moon";
}

void drawDateTile2(int px, int py, int pw, int ph, struct tm* t) {
  if (!t) return;

  float age = moonAge(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
  float phase = age / 29.53059f;  // 0-1
  float illum = 0.5f * (1.0f - cosf(phase * 2.0f * PI));
  bool waxing = (age < 14.765f);

  // Draw moon circle
  int mcx = px + pw / 2;
  int mcy = py + 50;
  int mr = 25;

  // Shadow (dark circle)
  gfx->fillCircle(mcx, mcy, mr, gfx->color565(30, 30, 40));

  // Illuminated portion — scanline by scanline
  uint16_t litColor = gfx->color565(220, 215, 190);
  for (int row = -mr; row <= mr; row++) {
    float halfW = sqrtf((float)(mr * mr - row * row));
    float litW = halfW * illum * 2.0f;
    int x1, x2;
    if (waxing) {
      // Illuminated on right side
      x1 = mcx + (int)(halfW - litW);
      x2 = mcx + (int)halfW;
    } else {
      // Illuminated on left side
      x1 = mcx - (int)halfW;
      x2 = mcx - (int)(halfW - litW);
    }
    if (x2 > x1) {
      gfx->drawFastHLine(x1, mcy + row, x2 - x1, litColor);
    }
  }

  // Phase name — textSize 2
  const char* name = moonPhaseName(age);
  int nameW = strlen(name) * 12;
  gfx->setTextColor(TEXT_PRIMARY);
  gfx->setTextSize(2);
  gfx->setCursor(px + (pw - nameW) / 2, py + 86);
  gfx->print(name);

  // Illumination percentage — textSize 2
  char illBuf[10];
  snprintf(illBuf, sizeof(illBuf), "%.0f%% lit", illum * 100.0f);
  int illW = strlen(illBuf) * 12;
  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(2);
  gfx->setCursor(px + (pw - illW) / 2, py + 108);
  gfx->print(illBuf);
}

// ── WEATHER tile 0: Temp + Icon + Description ───────────────
void drawWeatherTile0(int px, int py, int pw, int ph) {
  if (weather.valid) {
    // Weather icon — top right area
    drawWeatherIcon(px + pw - 30, py + 28, weather.icon);

    // Temperature — large
    char tempBuf[10];
    const char* unit = (strcmp(OWM_UNITS, "imperial") == 0) ? "F" : "C";
    snprintf(tempBuf, sizeof(tempBuf), "%.0f\xF8%s", weather.temp, unit);
    gfx->setTextColor(TEXT_PRIMARY);
    gfx->setTextSize(4);
    gfx->setCursor(px + 12, py + 14);
    gfx->print(tempBuf);

    // Description — word wrap if > 12 chars
    gfx->setTextColor(ACCENT_COLOR);
    gfx->setTextSize(2);
    int descLen = strlen(weather.description);
    int maxChars = 12;
    if (descLen <= maxChars) {
      gfx->setCursor(px + 12, py + 60);
      gfx->print(weather.description);
    } else {
      int split = maxChars;
      for (int i = maxChars - 1; i > 0; i--) {
        if (weather.description[i] == ' ') { split = i; break; }
      }
      char line1[32];
      strlcpy(line1, weather.description, split + 1);
      gfx->setCursor(px + 12, py + 54);
      gfx->print(line1);
      gfx->setCursor(px + 12, py + 74);
      gfx->print(&weather.description[split + (weather.description[split] == ' ' ? 1 : 0)]);
    }

    // Feels like
    char detailBuf[20];
    snprintf(detailBuf, sizeof(detailBuf), "Feels %.0f\xF8%s", weather.feels_like, unit);
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(1);
    gfx->setCursor(px + 12, py + 98);
    gfx->print(detailBuf);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(2);
    gfx->setCursor(px + 20, py + 42);
    gfx->print("Weather");
    gfx->setCursor(px + 20, py + 70);
    gfx->print("loading...");
  }
}

// ── WEATHER tile 1: AQI ─────────────────────────────────────
void drawWeatherTile1(int px, int py, int pw, int ph) {
  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  gfx->setCursor(px + 12, py + 12);
  gfx->print("Air Quality");

  if (weather.aqi > 0) {
    // Large AQI number
    char aqiNum[4];
    snprintf(aqiNum, sizeof(aqiNum), "%d", weather.aqi);
    gfx->setTextColor(aqiColor(weather.aqi));
    gfx->setTextSize(5);
    int numW = strlen(aqiNum) * 30;
    gfx->setCursor(px + (pw - numW) / 2, py + 30);
    gfx->print(aqiNum);

    // Label
    const char* label = aqiLabel(weather.aqi);
    int labelW = strlen(label) * 12;
    gfx->setTextSize(2);
    gfx->setCursor(px + (pw - labelW) / 2, py + 78);
    gfx->print(label);

    // PM2.5
    char pmBuf[20];
    snprintf(pmBuf, sizeof(pmBuf), "PM2.5: %.1f", weather.pm2_5);
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    int pmW = strlen(pmBuf) * 6;
    gfx->setCursor(px + (pw - pmW) / 2, py + 100);
    gfx->print(pmBuf);
  } else {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(2);
    gfx->setCursor(px + 12, py + 50);
    gfx->print("No data");
  }
}

// ── WEATHER tile 2: Wind + Conditions ───────────────────────
void drawWeatherTile2(int px, int py, int pw, int ph) {
  if (!weather.valid) {
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    gfx->setCursor(px + 12, py + 50);
    gfx->print("No data");
    return;
  }

  char buf[24];
  const char* speedUnit = (strcmp(OWM_UNITS, "imperial") == 0) ? "mph" : "m/s";

  // Wind speed — large
  gfx->setTextColor(TEXT_PRIMARY);
  gfx->setTextSize(3);
  snprintf(buf, sizeof(buf), "%.0f", weather.wind_speed);
  gfx->setCursor(px + 12, py + 10);
  gfx->print(buf);

  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  gfx->setCursor(px + 12, py + 36);
  snprintf(buf, sizeof(buf), "%s %s", speedUnit, windDir(weather.wind_deg));
  gfx->print(buf);

  // Humidity
  gfx->setTextColor(ACCENT_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(px + 12, py + 54);
  snprintf(buf, sizeof(buf), "%d%% rh", weather.humidity);
  gfx->print(buf);

  // Pressure
  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  gfx->setCursor(px + 12, py + 80);
  snprintf(buf, sizeof(buf), "%dhPa", weather.pressure);
  gfx->print(buf);

  // Visibility
  gfx->setCursor(px + 12, py + 96);
  if (weather.visibility >= 1000) {
    snprintf(buf, sizeof(buf), "Vis: %dkm", weather.visibility / 1000);
  } else {
    snprintf(buf, sizeof(buf), "Vis: %dm", weather.visibility);
  }
  gfx->print(buf);

  // Clouds
  gfx->setCursor(px + 12, py + 112);
  snprintf(buf, sizeof(buf), "Clouds: %d%%", weather.clouds);
  gfx->print(buf);
}

// ── STATUS tile 0: WiFi + Battery + Uptime ──────────────────
void drawStatusTile0(int px, int py, int pw, int ph) {
  // WiFi signal bars
  if (wifiConnected) {
    int rssi = WiFi.RSSI();
    drawWifiSignalBars(px + 10, py + 8, rssi);
    // Small dBm label below bars
    gfx->setTextColor(TEXT_DIM);
    gfx->setTextSize(1);
    char rssiBuf[12];
    snprintf(rssiBuf, sizeof(rssiBuf), "%ddBm", rssi);
    gfx->setCursor(px + 10, py + 32);
    gfx->print(rssiBuf);
  } else {
    gfx->setTextColor(ERR_COLOR);
    gfx->setTextSize(2);
    gfx->setCursor(px + 10, py + 8);
    gfx->print("No WiFi");
  }

  // Battery
  float batV = getBatteryVoltage();
  gfx->setTextSize(2);
  gfx->setCursor(px + 10, py + 50);
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
  char uptBuf[12];
  formatUptime(uptBuf, sizeof(uptBuf));
  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM);
  gfx->setCursor(px + 10, py + 92);
  gfx->print(uptBuf);
}

// ── STATUS tile 1: System Info ──────────────────────────────
void drawStatusTile1(int px, int py, int pw, int ph) {
  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  int rowY = py + 10;
  int rowH = 14;
  char buf[20];

  // IP
  gfx->setCursor(px + 6, rowY);
  if (wifiConnected) {
    gfx->print(WiFi.localIP().toString().c_str());
  } else {
    gfx->print("No IP");
  }

  // RSSI
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  snprintf(buf, sizeof(buf), "RSSI:%ddBm", wifiConnected ? WiFi.RSSI() : 0);
  gfx->print(buf);

  // Heap
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  snprintf(buf, sizeof(buf), "Heap:%dK", ESP.getFreeHeap() / 1024);
  gfx->print(buf);

  // PSRAM
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  snprintf(buf, sizeof(buf), "PSRAM:%dK", ESP.getFreePsram() / 1024);
  gfx->print(buf);

  // CPU MHz
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  snprintf(buf, sizeof(buf), "CPU:%dMHz", ESP.getCpuFreqMHz());
  gfx->print(buf);

  // Flash
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  snprintf(buf, sizeof(buf), "Flash:%dMB", ESP.getFlashChipSize() / (1024*1024));
  gfx->print(buf);

  // Uptime
  rowY += rowH;
  gfx->setCursor(px + 6, rowY);
  char uptBuf[12];
  formatUptime(uptBuf, sizeof(uptBuf));
  snprintf(buf, sizeof(buf), "Up:%s", uptBuf);
  gfx->print(buf);
}
