#pragma once
#include <Arduino.h>

// Filled from the 3-hourly forecast entry closest to 12:00 local time
// (fallback: current weather).
struct WeatherData {
  bool   valid;
  char   icon[4];          // OWM icon code, e.g. "02d"
  char   condition[24];    // weather[0].main, e.g. "Clouds"
  char   description[64];  // weather[0].description, e.g. "partly cloudy"
  float  tempC;            // deg C (metric)
  int    humidity;         // %
  float  rainMm;           // 0.0 if none
  float  windMs;           // m/s
};

extern WeatherData weatherData;

// Fetch the 3-hourly forecast and pick the entry closest to today's
// WX_TARGET_HOUR (noon) local time; fall back to current weather when the
// forecast is unavailable. Fills weatherData.
bool weatherFetchNoon();
