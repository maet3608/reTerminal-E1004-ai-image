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
  p.reserve(1400);

  p += "Create an artwork in the style of ";
  p += selectTheme(day.weekday);
  p += ".\n\n";

  p += "REQUIRED ELEMENTS (must be visible, drawn as objects inside the scene, "
       "never as UI-style text overlays):\n";
  p += "- Weekday: ";
  p += day.weekdayFull;
  p += "\n- Date: ";
  p += day.dateFull;
  p += "\n- Weather: ";
  p += wx.valid ? wx.description : "neutral";
  p += ", ";
  p += String((int)roundf(wx.tempC));
  p += " C";
  p += "\n- Battery: ";
  p += String(batteryPct);
  p += "%\n\n";

  p += "HOW TO INTEGRATE THEM:\n";
  p += "- Date/weekday/Weather: pick objects that fits the theme and show the "
       "date/day/weather on them but don't show the same data twice. Ideas to choose from:\n";
  p += "  * a torn-off or circled page on a wall/desk calendar\n";
  p += "  * a newspaper stand or front-page dateline\n";
  p += "  * a street sign, shop sign, or milestone marker\n";
  p += "  * a sundial, or train/departure board\n";
  p += "  * a chalkboard, price tag, ticket stub, or receipt\n";
  p += "  * a character/person carrying a notebook or letter or newspaper with the date/day/weather on it\n";
  p += "  * a market stall, or flag/banner with the date/day/weather on it\n";
  p += "- Use symbols (e.g. sun, cloud, rain, snowflake, wind swirl, moon, ";
  p += "or day-specific icon) rather than spelled out, if that suits the "
       "scene better than text\n";
  p += "- Battery: print the percentage as a label (e.g. 75%) on an object. "
       "Do not show a literal battery icon, unless it fits the theme.\n";
  p += "- Weather: also express through the sky, lighting, and scene details (sun, "
       "clouds, rain, snow, wind-blown elements).\n";
  p += "- Blend all elements naturally into the composition - clever and on-theme.\n\n";

  p += "FORMAT CONSTRAINTS:\n";
  p += "- Keep all important content centered (edges may be cropped).\n";
  p += "- All text must be small and styled to match the scene.\n";
  p += "- No watermark, no border, no frame, no text outside the scene.\n";

  return p;
}