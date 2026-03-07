#pragma once

// =============================================================
// Types, structs, enums, and layout constants
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
  float wind_speed;
  int   wind_deg;
  int   clouds;       // cloudiness %
  int   pressure;     // hPa
  int   visibility;   // meters
};

// Navigation — tile-per-column + page switching
#define NUM_COLUMNS  4
#define NUM_PAGES    2

struct ColGeom { int16_t x; int16_t w; uint8_t numTiles; };
static const ColGeom COL_GEOM[NUM_COLUMNS] = {
  {   8, 170, 2 },  // Time:    tile 0=clock, tile 1=world clocks
  { 186, 176, 3 },  // Date:    tile 0=date, tile 1=sun arc, tile 2=moon phase
  { 370, 156, 3 },  // Weather: tile 0=temp/desc, tile 1=AQI, tile 2=weather detail
  { 534,  98, 2 },  // Status:  tile 0=wifi/bat, tile 1=system info
};

enum CritterState { CRIT_IDLE, CRIT_WALKING, CRIT_JUMPING, CRIT_WAVING, CRIT_SLEEPING };

// Touch focus ring
#define FOCUS_DURATION_MS 600

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};
