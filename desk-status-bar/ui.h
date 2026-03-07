#pragma once

// =============================================================
// UI — main panel dispatch, splash screen, indicators
// =============================================================

// Tile Indicator Dots — shows which tile is active
void drawTileIndicator(int px, int py, int pw, int ph, int numTiles, int active) {
  if (numTiles <= 1) return;

  int dotR = 3;
  int spacing = 12;
  int totalW = numTiles * (dotR * 2) + (numTiles - 1) * (spacing - dotR * 2);
  int startX = px + (pw - totalW) / 2;
  int dotY = py + ph - 10;

  for (int i = 0; i < numTiles; i++) {
    int dx = startX + i * spacing;
    uint16_t color = (i == active) ? ACCENT_COLOR : TEXT_DIM;
    gfx->fillCircle(dx, dotY, dotR, color);
  }
}

// Page 1 Placeholder
void drawPage1() {
  drawPanel(8, 8, SCREEN_W - 16, SCREEN_H - 16, PANEL_COLOR);

  gfx->setTextColor(TEXT_SECONDARY);
  gfx->setTextSize(3);
  int msgW = 14 * 18; // ~14 chars at textSize 3
  gfx->setCursor((SCREEN_W - msgW) / 2, 50);
  gfx->print("Page 2");

  gfx->setTextColor(TEXT_DIM);
  gfx->setTextSize(2);
  gfx->setCursor((SCREEN_W - 12 * 12) / 2, 100);
  gfx->print("Coming soon");
}

// Page Indicator — shows current page (e.g. "1/2")
void drawPageIndicator() {
  if (NUM_PAGES <= 1) return;

  char buf[6];
  snprintf(buf, sizeof(buf), "%d/%d", currentPage + 1, NUM_PAGES);
  gfx->setTextColor(TEXT_DIM);
  gfx->setTextSize(1);
  int bw = strlen(buf) * 6;
  gfx->setCursor(SCREEN_W - bw - 4, SCREEN_H - 10);
  gfx->print(buf);
}

// Main Panel — dispatches to tile draw functions
void drawMainPanel() {
  gfx->fillScreen(BG_COLOR);

  if (currentPage == 0) {
    struct tm t;
    bool hasTime = getLocalTime(&t, 0);

    // Draw each column panel + dispatch to active tile
    for (int c = 0; c < NUM_COLUMNS; c++) {
      int px = COL_GEOM[c].x;
      int pw = COL_GEOM[c].w;
      int py = 8;
      int ph = SCREEN_H - 16;

      drawPanel(px, py, pw, ph, PANEL_COLOR);

      switch (c) {
        case 0: // Time
          if (currentTile[0] == 0) drawTimeTile0(px, py, pw, ph, hasTime ? &t : nullptr);
          else                     drawTimeTile1(px, py, pw, ph);
          break;
        case 1: // Date
          if (currentTile[1] == 0)      drawDateTile0(px, py, pw, ph, hasTime ? &t : nullptr);
          else if (currentTile[1] == 1) drawDateTile1(px, py, pw, ph);
          else                           drawDateTile2(px, py, pw, ph, hasTime ? &t : nullptr);
          break;
        case 2: // Weather
          if (currentTile[2] == 0)      drawWeatherTile0(px, py, pw, ph);
          else if (currentTile[2] == 1) drawWeatherTile1(px, py, pw, ph);
          else                           drawWeatherTile2(px, py, pw, ph);
          break;
        case 3: // Status
          if (currentTile[3] == 0) drawStatusTile0(px, py, pw, ph);
          else                     drawStatusTile1(px, py, pw, ph);
          break;
      }

      drawTileIndicator(px, py, pw, ph, COL_GEOM[c].numTiles, currentTile[c]);

      // Focus ring — fading highlight border on recently-tapped column
      unsigned long focusAge = millis() - focusTouchTime;
      if (focusCol == c && focusAge < FOCUS_DURATION_MS) {
        float elapsed = (float)focusAge / (float)FOCUS_DURATION_MS;
        // Fade from bright accent to invisible
        uint8_t r = (uint8_t)(99 + (1.0f - elapsed) * 120);
        uint8_t g = (uint8_t)(140 + (1.0f - elapsed) * 80);
        uint8_t b = (uint8_t)(255);
        uint16_t ringColor = gfx->color565(r, g, b);
        gfx->drawRoundRect(px, py, pw, ph, 8, ringColor);
        gfx->drawRoundRect(px + 1, py + 1, pw - 2, ph - 2, 7, ringColor);
      }
    }

#if CRITTER_ENABLED
    updateCritter();
    drawCritter();
#endif
  } else {
    drawPage1();
  }

  drawPageIndicator();
  gfx->flush();
}

// Splash Screen (shown during boot)
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
