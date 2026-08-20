#include "prompt.h"
#include "config.h"

const char *selectTheme(int weekday) {
  if (USE_WEEKDAY_THEMES && weekday >= 0 && weekday < 7) {
    return THEMES[weekday];
  }
  return THEME;
}

String buildPrompt(const DayInfo &day, const WeatherData &wx, int batteryPct) {
  String p;
  p.reserve(1200);

  p += "Create an artwork in the style of ";
  p += selectTheme(day.weekday);
  p += ".\n";
  p += "The scene MUST contain these facts drawn as small objects INSIDE the scene, "
       "not as text overlays:\n";
  p += "- Weekday: ";
  p += day.weekdayFull;
  p += "\n- Date: ";
  p += day.dateFull;
  p += "\n- Weather: ";
  p += wx.valid ? wx.description : "unknown weather";
  p += ", ";
  p += String((int)(wx.tempC + 0.5f));
  p += " C";
  p += "\n- Battery: ";
  p += String(batteryPct);
  p += "%\n";
  p += "Integrate them naturally as tiny, almost hidden objects, e.g.:\n";
  p += "- date on a calendar page / clock / newspaper, battery within the scene,\n";
  p += "- weather shown through the sky and scene details (sun, clouds, rain, "
       "snow, wind...),\n";
  p += "Keep all important content in the central area (edges may be cropped). "
       "Text must be legible but stylized. No watermark, no border, no text "
       "outside the scene.";
  return p;
}
