#include "weather.h"
#include "config.h"
#include "credentials.h"
#include "logging.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

WeatherData weatherData = {false, "", "", "", 0.0f, 0, 0.0f, 0.0f};

static void urlEncode(const char* src, char* dst, size_t dstSize) {
  char* d = dst;
  while (*src && (size_t)(d - dst) < dstSize - 3) {
    if (*src == ' ') {
      *d++ = '%';
      *d++ = '2';
      *d++ = '0';
      src++;
    } else {
      *d++ = *src++;
    }
  }
  *d = '\0';
}

// Copy the weather fields of a JSON entry into weatherData.
// Rain is taken from "1h" or "3h" (whichever exists).
static bool fillFromJson(const JsonVariantConst& e) {
  const char* icon = e["weather"][0]["icon"];
  const char* cond = e["weather"][0]["main"];
  const char* desc = e["weather"][0]["description"];
  if (!icon || !desc) return false;

  strncpy(weatherData.icon, icon, sizeof(weatherData.icon) - 1);
  weatherData.icon[sizeof(weatherData.icon) - 1] = '\0';
  strncpy(weatherData.condition, cond ? cond : "", sizeof(weatherData.condition) - 1);
  weatherData.condition[sizeof(weatherData.condition) - 1] = '\0';
  strncpy(weatherData.description, desc, sizeof(weatherData.description) - 1);
  weatherData.description[sizeof(weatherData.description) - 1] = '\0';

  weatherData.tempC    = e["main"]["temp"] | 0.0f;
  weatherData.humidity = e["main"]["humidity"] | 0;
  weatherData.rainMm   = e["rain"]["1h"] | e["rain"]["3h"] | 0.0f;
  weatherData.windMs   = e["wind"]["speed"] | 0.0f;
  weatherData.valid    = true;
  return true;
}

static bool weatherFetchCurrent() {
  char enc[128];
  urlEncode(WX_LOCATION, enc, sizeof(enc));
  char url[300];
  snprintf(url, sizeof(url),
           "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s",
           enc, OPENWEATHER_API_KEY, WX_UNITS);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    LOG.printf("[WX] current weather HTTP %d\n", code);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  return fillFromJson(doc.as<JsonVariantConst>());
}

static bool weatherFetchForecast() {
  char enc[128];
  urlEncode(WX_LOCATION, enc, sizeof(enc));
  char url[300];
  snprintf(url, sizeof(url),
           "https://api.openweathermap.org/data/2.5/forecast?q=%s&appid=%s&units=%s",
           enc, OPENWEATHER_API_KEY, WX_UNITS);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    LOG.printf("[WX] forecast HTTP %d\n", code);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;

  // Target: today at WX_TARGET_HOUR (noon) in local time -> UTC epoch.
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  struct tm noon = {};
  noon.tm_year = lt.tm_year;
  noon.tm_mon  = lt.tm_mon;
  noon.tm_mday = lt.tm_mday;
  noon.tm_hour = WX_TARGET_HOUR;
  const long target = (long)mktime(&noon);

  // Pick the entry with the smallest |dt - target|.
  JsonArrayConst list = doc["list"];
  long bestDiff = WX_MAX_NOON_DELTA + 1;
  JsonVariantConst best;
  for (JsonVariantConst e : list) {
    const long dt = e["dt"] | 0L;
    const long d = (dt > target) ? (dt - target) : (target - dt);
    if (d < bestDiff) {
      bestDiff = d;
      best = e;
    }
  }

  if (best.isNull() || bestDiff > WX_MAX_NOON_DELTA) {
    LOG.printf("[WX] no forecast entry within +-%ld s of noon (best %ld s)\n",
               WX_MAX_NOON_DELTA, bestDiff);
    return false;
  }
  LOG.printf("[WX] noon entry %ld s from target\n", bestDiff);
  return fillFromJson(best);
}

bool weatherFetchNoon() {
  if (weatherFetchForecast()) {
    LOG.printf("[WX] %s, %.1f C\n", weatherData.description, weatherData.tempC);
    return true;
  }
  LOG.println("[WX] forecast failed - falling back to current weather");
  weatherData.valid = false;
  if (weatherFetchCurrent()) {
    LOG.printf("[WX] current: %s, %.1f C\n", weatherData.description, weatherData.tempC);
    return true;
  }
  return false;
}
