#pragma once

// =============================================================
// Drawing Helpers
// =============================================================

// Draws a rounded rectangle panel background
void drawPanel(int x, int y, int w, int h, uint16_t color) {
  gfx->fillRoundRect(x, y, w, h, 8, color);
}

// Convert 24h hour to 12h format
int formatHour12(int hour24) {
  int h = hour24 % 12;
  return (h == 0) ? 12 : h;
}

// Format uptime into "Xd Yh" or "Xh Ym" string
void formatUptime(char* buf, size_t len) {
  unsigned long uptimeSec = millis() / 1000;
  unsigned long hrs = uptimeSec / 3600;
  unsigned long mins = (uptimeSec % 3600) / 60;
  if (hrs >= 24) {
    snprintf(buf, len, "%lud%luh", hrs / 24, hrs % 24);
  } else {
    snprintf(buf, len, "%luh%lum", hrs, mins);
  }
}

// ── Wind direction label ────────────────────────────────────
const char* windDir(int deg) {
  if (deg < 23)  return "N";
  if (deg < 68)  return "NE";
  if (deg < 113) return "E";
  if (deg < 158) return "SE";
  if (deg < 203) return "S";
  if (deg < 248) return "SW";
  if (deg < 293) return "W";
  if (deg < 338) return "NW";
  return "N";
}

// ── Weather Icon (drawn with GFX primitives) ───────────────
void drawWeatherIcon(int cx, int cy, const char* icon) {
  bool night = (icon[2] == 'n');
  // Sun / Moon base
  if (strncmp(icon, "01", 2) == 0) {
    // Clear sky
    if (night) {
      // Moon crescent
      gfx->fillCircle(cx, cy, 12, gfx->color565(220, 215, 180));
      gfx->fillCircle(cx + 6, cy - 4, 10, PANEL_COLOR);
    } else {
      // Sun
      gfx->fillCircle(cx, cy, 10, WARN_COLOR);
      for (int a = 0; a < 8; a++) {
        float angle = a * PI / 4.0f;
        int rx = cx + (int)(cosf(angle) * 15);
        int ry = cy - (int)(sinf(angle) * 15);
        gfx->fillCircle(rx, ry, 2, WARN_COLOR);
      }
    }
  } else if (strncmp(icon, "02", 2) == 0) {
    // Few clouds — sun/moon peeking out
    if (!night) {
      gfx->fillCircle(cx - 4, cy - 6, 8, WARN_COLOR);
    } else {
      gfx->fillCircle(cx - 4, cy - 6, 6, gfx->color565(200, 195, 170));
    }
    // Small cloud in front
    gfx->fillCircle(cx + 2, cy + 2, 8, gfx->color565(180, 180, 195));
    gfx->fillCircle(cx - 6, cy + 4, 6, gfx->color565(180, 180, 195));
    gfx->fillCircle(cx + 8, cy + 5, 5, gfx->color565(180, 180, 195));
  } else if (strncmp(icon, "03", 2) == 0 || strncmp(icon, "04", 2) == 0) {
    // Clouds
    gfx->fillCircle(cx, cy - 2, 9, gfx->color565(160, 160, 175));
    gfx->fillCircle(cx - 8, cy + 3, 7, gfx->color565(150, 150, 165));
    gfx->fillCircle(cx + 8, cy + 3, 6, gfx->color565(150, 150, 165));
    gfx->fillRoundRect(cx - 12, cy + 2, 24, 8, 3, gfx->color565(150, 150, 165));
  } else if (strncmp(icon, "09", 2) == 0 || strncmp(icon, "10", 2) == 0) {
    // Rain
    gfx->fillCircle(cx, cy - 4, 8, gfx->color565(130, 130, 150));
    gfx->fillCircle(cx - 7, cy, 6, gfx->color565(130, 130, 150));
    gfx->fillCircle(cx + 7, cy, 5, gfx->color565(130, 130, 150));
    // Raindrops
    for (int d = -1; d <= 1; d++) {
      gfx->fillRect(cx + d * 7, cy + 8, 2, 5, ACCENT_COLOR);
    }
  } else if (strncmp(icon, "11", 2) == 0) {
    // Thunderstorm
    gfx->fillCircle(cx, cy - 4, 8, gfx->color565(100, 100, 120));
    gfx->fillCircle(cx - 7, cy, 6, gfx->color565(100, 100, 120));
    gfx->fillCircle(cx + 7, cy, 5, gfx->color565(100, 100, 120));
    // Lightning bolt
    gfx->fillTriangle(cx, cy + 6, cx + 4, cy + 6, cx - 2, cy + 14, WARN_COLOR);
  } else if (strncmp(icon, "13", 2) == 0) {
    // Snow
    gfx->fillCircle(cx, cy - 4, 8, gfx->color565(170, 170, 185));
    // Snowflakes
    for (int d = -1; d <= 1; d++) {
      gfx->fillCircle(cx + d * 7, cy + 10, 2, TEXT_PRIMARY);
    }
  } else if (strncmp(icon, "50", 2) == 0) {
    // Fog — horizontal lines
    for (int l = 0; l < 4; l++) {
      gfx->drawFastHLine(cx - 12, cy - 6 + l * 5, 24, gfx->color565(150, 150, 165));
    }
  }
}

// ── WiFi Signal Bars ────────────────────────────────────────
void drawWifiSignalBars(int x, int y, int rssi) {
  // 4 bars of increasing height, 5px wide, 3px gap
  // Thresholds: >-50=4 bars, >-60=3, >-70=2, >-80=1, else 0
  int bars = 0;
  if      (rssi > -50) bars = 4;
  else if (rssi > -60) bars = 3;
  else if (rssi > -70) bars = 2;
  else if (rssi > -80) bars = 1;

  int barW = 5;
  int gap = 3;
  int heights[] = { 5, 10, 15, 20 };

  for (int i = 0; i < 4; i++) {
    int bx = x + i * (barW + gap);
    int bh = heights[i];
    int by = y + 20 - bh;  // align bottoms
    uint16_t color = (i < bars) ? GOOD_COLOR : TEXT_DIM;
    gfx->fillRect(bx, by, barW, bh, color);
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
  // Night arc is shifted up so its lowest point sits at arcCY (same baseline as day)
  int nightBaseY = arcCY - arcH + 3;
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
      y1 = nightBaseY + (int)(sinf(a1) * arcH);
      y2 = nightBaseY + (int)(sinf(a2) * arcH);
    }
    gfx->drawLine(x1, y1, x2, y2, TEXT_SECONDARY);
  }

  // Dot position along arc
  float dotAngle = PI * progress;
  int dotX = arcX + (int)(dotAngle / PI * arcW);
  int dotY;
  if (isDaytime) {
    dotY = arcCY - (int)(sinf(dotAngle) * arcH);
  } else {
    dotY = nightBaseY + (int)(sinf(dotAngle) * arcH);
  }
  uint16_t dotColor = isDaytime ? WARN_COLOR : TEXT_SECONDARY;
  gfx->fillCircle(dotX, dotY, 3, dotColor);

  // Time labels at endpoints
  int riseH = (rise % 86400) / 3600;
  int riseM = ((rise % 86400) % 3600) / 60;
  int setH  = (set % 86400) / 3600;
  int setM  = ((set % 86400) % 3600) / 60;

  // Convert to 12h
  int rH = formatHour12(riseH);
  int sH = formatHour12(setH);

  char rBuf[8], sBuf[8];
  snprintf(rBuf, sizeof(rBuf), "%d:%02d", rH, riseM);
  snprintf(sBuf, sizeof(sBuf), "%d:%02d", sH, setM);

  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(1);
  gfx->setCursor(arcX, arcCY + 6);
  gfx->print(rBuf);
  int sLen = strlen(sBuf) * 6;
  gfx->setCursor(arcX + arcW - sLen, arcCY + 6);
  gfx->print(sBuf);
}
