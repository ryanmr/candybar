#pragma once

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

      weather.lat        = doc["coord"]["lat"] | 0.0f;
      weather.lon        = doc["coord"]["lon"] | 0.0f;
      weather.sunrise    = doc["sys"]["sunrise"] | 0UL;
      weather.sunset     = doc["sys"]["sunset"]  | 0UL;
      weather.wind_speed = doc["wind"]["speed"] | 0.0f;
      weather.wind_deg   = doc["wind"]["deg"] | 0;
      weather.clouds     = doc["clouds"]["all"] | 0;
      weather.pressure   = doc["main"]["pressure"] | 0;
      weather.visibility = doc["visibility"] | 0;

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
// NVS Cache — persist weather data across restarts
// =============================================================
void saveWeatherCache() {
  prefs.begin("weather", false);  // read-write
  prefs.putFloat("temp", weather.temp);
  prefs.putFloat("feels", weather.feels_like);
  prefs.putInt("humidity", weather.humidity);
  prefs.putString("desc", weather.description);
  prefs.putString("icon", weather.icon);
  prefs.putFloat("lat", weather.lat);
  prefs.putFloat("lon", weather.lon);
  prefs.putULong("sunrise", weather.sunrise);
  prefs.putULong("sunset", weather.sunset);
  prefs.putInt("aqi", weather.aqi);
  prefs.putFloat("pm25", weather.pm2_5);
  prefs.putFloat("wind_spd", weather.wind_speed);
  prefs.putInt("wind_deg", weather.wind_deg);
  prefs.putInt("clouds", weather.clouds);
  prefs.putInt("pressure", weather.pressure);
  prefs.putInt("vis", weather.visibility);
  prefs.putBool("valid", weather.valid);
  prefs.putULong("epoch", (unsigned long)time(nullptr));
  prefs.end();
  Serial.println("[Cache] Weather data saved to NVS");
}

bool loadWeatherCache() {
  prefs.begin("weather", true);  // read-only
  bool valid = prefs.getBool("valid", false);
  if (!valid) {
    prefs.end();
    Serial.println("[Cache] No cached weather data");
    return false;
  }

  weather.temp       = prefs.getFloat("temp", 0.0f);
  weather.feels_like = prefs.getFloat("feels", 0.0f);
  weather.humidity   = prefs.getInt("humidity", 0);
  String desc = prefs.getString("desc", "unknown");
  strlcpy(weather.description, desc.c_str(), sizeof(weather.description));
  String icon = prefs.getString("icon", "01d");
  strlcpy(weather.icon, icon.c_str(), sizeof(weather.icon));
  weather.lat        = prefs.getFloat("lat", 0.0f);
  weather.lon        = prefs.getFloat("lon", 0.0f);
  weather.sunrise    = prefs.getULong("sunrise", 0);
  weather.sunset     = prefs.getULong("sunset", 0);
  weather.aqi        = prefs.getInt("aqi", 0);
  weather.pm2_5      = prefs.getFloat("pm25", 0.0f);
  weather.wind_speed = prefs.getFloat("wind_spd", 0.0f);
  weather.wind_deg   = prefs.getInt("wind_deg", 0);
  weather.clouds     = prefs.getInt("clouds", 0);
  weather.pressure   = prefs.getInt("pressure", 0);
  weather.visibility = prefs.getInt("vis", 0);
  weather.valid      = true;
  prefs.end();

  Serial.printf("[Cache] Loaded cached weather: %.0f°, %s\n", weather.temp, weather.description);

  return true;
}

bool isWeatherCacheFresh() {
  prefs.begin("weather", true);
  unsigned long savedEpoch = prefs.getULong("epoch", 0);
  prefs.end();
  if (savedEpoch == 0) return false;

  unsigned long nowEpoch = (unsigned long)time(nullptr);
  // time(nullptr) returns 0 or small values before NTP sync
  if (nowEpoch < 1000000) return false;

  unsigned long age = nowEpoch - savedEpoch;
  unsigned long maxAge = WEATHER_INTERVAL / 1000;  // convert ms to seconds
  bool fresh = (age < maxAge);
  Serial.printf("[Cache] Weather age: %lus, max: %lus → %s\n", age, maxAge, fresh ? "fresh" : "stale");
  return fresh;
}
