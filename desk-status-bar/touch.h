#pragma once

// =============================================================
// Touch Handling — polling + navigation
// =============================================================

// The AXS15231B integrated touch uses I2C at address 0x3B.
// Simple point-read polling (no interrupt for MVP).

TouchPoint readTouch() {
  TouchPoint tp = { 0, 0, false };

  // AXS15231B requires this magic command sequence to request touch data
  static const uint8_t read_touchpad_cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x08};

  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(read_touchpad_cmd, 8);
  Wire.endTransmission();

  uint8_t buf[14] = {0};
  Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)14);
  for (int i = 0; i < 14 && Wire.available(); i++) {
    buf[i] = Wire.read();
  }

  // AXS15231B response format:
  // buf[0]: gesture type
  // buf[1]: number of touch points (1-4 = valid)
  // buf[2]: X high nibble (bits 3:0)
  // buf[3]: X low byte
  // buf[4]: Y high nibble (bits 3:0)
  // buf[5]: Y low byte
  uint8_t pointNum = buf[1];
  if (pointNum > 0 && pointNum < 5) {
    tp.x = ((uint16_t)(buf[2] & 0x0F) << 8) | (uint16_t)buf[3];
    tp.y = ((uint16_t)(buf[4] & 0x0F) << 8) | (uint16_t)buf[5];
    tp.pressed = true;
  }

  return tp;
}

// Touch Navigation
void handleTouch(int lx, int ly, unsigned long now) {
#if CRITTER_ENABLED
  // Check if tap is near the critter (~20px radius)
  int groundY = SCREEN_H - 15;
  int critterScreenY = groundY - (int)critterY;
  float cdx = (float)lx - critterX;
  float cdy = (float)ly - (float)critterScreenY;
  if (cdx * cdx + cdy * cdy < 20.0f * 20.0f) {
    if (critterState == CRIT_SLEEPING) {
      critterState = CRIT_IDLE;
      critterStateTicks = 0;
      critterIdleTicks = 0;
      Serial.println("[Critter] Pet detected! Waking up!");
    } else {
      critterState = CRIT_WAVING;
      critterStateTicks = 0;
      Serial.println("[Critter] Pet detected!");
    }
    return;  // consume the touch
  }
#endif

  // Edge zones: page navigation
  if (lx < 30) {
    if (currentPage > 0) {
      currentPage--;
      lastNavTime = now;
      Serial.printf("[Nav] Page prev → %d\n", currentPage);
    }
    return;
  }
  if (lx > 610) {
    if (currentPage < NUM_PAGES - 1) {
      currentPage++;
      lastNavTime = now;
      Serial.printf("[Nav] Page next → %d\n", currentPage);
    }
    return;
  }

  // Only handle column tile nav on page 0
  if (currentPage != 0) return;

  // Find which column was tapped
  for (int c = 0; c < NUM_COLUMNS; c++) {
    int cx = COL_GEOM[c].x;
    int cw = COL_GEOM[c].w;
    if (lx >= cx && lx < cx + cw) {
      // Trigger focus ring on this column
      focusCol = c;
      focusTouchTime = now;

      uint8_t numT = COL_GEOM[c].numTiles;
      if (numT <= 1) return;  // no tiles to cycle

      if (ly < 86) {
        // Top half: previous tile (wrap around)
        currentTile[c] = (currentTile[c] + numT - 1) % numT;
      } else {
        // Bottom half: next tile (wrap around)
        currentTile[c] = (currentTile[c] + 1) % numT;
      }
      lastNavTime = now;
      Serial.printf("[Nav] Col %d tile → %d\n", c, currentTile[c]);
      return;
    }
  }
}
