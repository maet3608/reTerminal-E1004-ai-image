#pragma once
#include <Arduino.h>
#include "time_mgr.h"
#include "weather.h"

// Theme for a weekday (0 = Sunday .. 6 = Saturday). Honors USE_WEEKDAY_THEMES.
const char* selectTheme(int weekday);

// Build the image-generation prompt from theme + day + weather + battery.
String buildPrompt(const DayInfo& day, const WeatherData& wx, int batteryPct);
