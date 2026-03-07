#pragma once

// =============================================================
// Critter Pet — animated blob that roams the bottom of the screen
// =============================================================
#if CRITTER_ENABLED

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
        critterDirTicks = 0;
      } else {
        critterDirTicks++;
        critterVX += critterDir * 0.4f;
        critterVX = constrain(critterVX, -2.0f, 2.0f);
        // After ~120 seconds (840 ticks at 7fps), 50% chance to flip direction
        if (critterDirTicks > 840 && random(2) == 0) {
          critterDir = -critterDir;
          critterDirTicks = 0;
        }
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
  int groundY = SCREEN_H - 15;
  int cy = groundY - (int)critterY;
  int dir = critterDir;  // 1=right, -1=left
  bool sleeping = (critterState == CRIT_SLEEPING);

  // Body — oval torso (two overlapping circles, radius 8)
  gfx->fillCircle(cx, cy, 8, CRITTER_BODY);
  gfx->fillCircle(cx + dir * 2, cy, 7, CRITTER_BODY);
  // Belly highlight — lighter circle shifted down
  gfx->fillCircle(cx, cy + 2, 6, CRITTER_HIGHLIGHT);

  // Head — overlapping circle at top-front of body
  int hx = cx + dir * 6;
  int hy = cy - 6;
  if (sleeping) hy = cy - 4;  // head droops when sleeping
  gfx->fillCircle(hx, hy, 5, CRITTER_BODY);

  // Beak — small orange triangle pointing in walk direction
  int bx = hx + dir * 5;
  int by = hy;
  gfx->fillTriangle(bx, by, bx + dir * 5, by + 1, bx, by + 3, CRITTER_BEAK);

  // Eye — single eye on the visible side
  int ex = hx + dir * 2;
  int ey = hy - 1;
  if (sleeping || critterBlink) {
    // Closed eye — horizontal line
    gfx->drawFastHLine(ex - 1, ey, 3, CRITTER_EYE);
  } else {
    gfx->fillCircle(ex, ey, 1, CRITTER_EYE);
  }

  // Cheek blush — small pink spot below eye
  gfx->fillRect(hx + dir * 1, hy + 2, 2, 2, CRITTER_CHEEK);

  // Wing — overlapping circle on body, deeper blue
  int wx = cx - dir * 2;
  int wy = cy - 1;
  if (critterState == CRIT_JUMPING || critterState == CRIT_WAVING) {
    // Wing raised — shifted up
    int wingWiggle = ((critterAnimTick / 4) % 2 == 0) ? -3 : 0;
    gfx->fillCircle(wx, wy - 5 + wingWiggle, 4, CRITTER_WING);
  } else if (critterState == CRIT_WALKING) {
    // Wing bobs slightly while walking
    int wingBob = ((critterAnimTick / 4) % 2 == 0) ? -1 : 0;
    gfx->fillCircle(wx, wy + wingBob, 4, CRITTER_WING);
  } else {
    // Idle/sleeping — wing resting at side
    gfx->fillCircle(wx, wy, 4, CRITTER_WING);
  }

  // Tail — triangle at back of body, opposite to direction
  int tx = cx - dir * 8;
  int ty = cy - 2;
  gfx->fillTriangle(tx, ty, tx - dir * 6, ty - 5, tx - dir * 4, ty + 1, CRITTER_WING);

  // Legs — two thin rectangles below body with small feet
  if (critterState == CRIT_JUMPING) {
    // Legs tucked up during jump
    gfx->fillRect(cx - 3, cy + 6, 2, 3, CRITTER_BEAK);
    gfx->fillRect(cx + 2, cy + 6, 2, 3, CRITTER_BEAK);
  } else {
    // Walking: alternate leg positions
    int legOff = (critterState == CRIT_WALKING && (critterAnimTick / 4) % 2 == 0) ? 2 : 0;
    // Left leg
    gfx->fillRect(cx - 3, cy + 7 - legOff, 2, 4, CRITTER_BEAK);
    gfx->fillRect(cx - 4, cy + 10 - legOff, 3, 1, CRITTER_BEAK);  // foot
    // Right leg
    gfx->fillRect(cx + 2, cy + 7 + legOff, 2, 4, CRITTER_BEAK);
    gfx->fillRect(cx + 1, cy + 10 + legOff, 3, 1, CRITTER_BEAK);  // foot
  }

  // Sleeping ZZZ
  if (sleeping) {
    int zy = hy - 8 - ((critterAnimTick / 7) % 3) * 5;
    gfx->setTextColor(TEXT_SECONDARY);
    gfx->setTextSize(1);
    gfx->setCursor(hx + 6, zy);
    gfx->print("z");
    if ((critterAnimTick / 7) % 6 < 3) {
      gfx->setCursor(hx + 13, zy - 9);
      gfx->print("z");
    }
  }
}

#endif // CRITTER_ENABLED
