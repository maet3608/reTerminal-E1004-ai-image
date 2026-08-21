#pragma once
#include <stdint.h>

// ============================================================
// reTerminal E1004 AI image - central configuration
// All tunables live here; secrets go into credentials.h.
// ============================================================

// ---------------- Weather ----------------
static const char *WX_LOCATION = "Lohr am Main,DE"; // OpenWeather q = "City,CC"
static const char *WX_UNITS = "metric";
static const int WX_TARGET_HOUR = 12;             // use the 3-hourly forecast entry
                                                  // closest to this hour (noon) local
static const long WX_MAX_NOON_DELTA = 6L * 3600L; // fall back to current
                                                  // weather if no entry within +-6 h

// ---------------- Time / date ----------------
static const char *TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3"; // POSIX TZ
static const char *NTP_SERVER = "pool.ntp.org";
static const bool NTP_ENABLED = true;        // correct RTC drift on each wake
static const int REFRESH_HOUR = 1;           // daily wake hour, 0-23
static const char *DATE_FORMAT = "%d %B %Y"; // e.g. "1 June 2026" (strftime)

// ---------------- Themes ----------------
// With USE_WEEKDAY_THEMES the theme depends only on the weekday (tm_wday,
// 0 = Sunday .. 6 = Saturday), so the same weekday always shows the same theme.
static const bool USE_WEEKDAY_THEMES = true;
static const char *THEME = "Vintage botanical illustration";
static const char *THEMES[7] = {
    "Wassily Kandinsky like style",       // Sunday
    "Japanese Ukiyo-e woodblock print",   // Monday
    "Japanese Anime, Totoro or Inuyasha", // Tuesday
    "Steampunk style",                    // Wednesday
    "Vintage botanical illustration",     // Thursday
    "Victorian city street",              // Friday
    "Piet Mondrian abstract art ",        // Saturday
};

// ---------------- Battery ----------------
static const float BAT_VOLTAGE_DIVIDER = 2.0f; // VBAT = ADC * 2
static const int BAT_LOW_PERCENT = 5;          // skip the network cycle below this

// ---------------- Orientation ----------------
// Global screen orientation. Portrait uses the panel natively; landscape
// renders the frame rotated 90 degrees onto the physical panel.
enum Orientation { ORIENT_PORTRAIT = 0,
                   ORIENT_LANDSCAPE = 1 };
static const Orientation ORIENTATION = ORIENT_LANDSCAPE;

// ---------------- AI image (OpenAI) ----------------
static const char *AI_MODEL = "gpt-image-1-mini"; // "gpt-image-1" or "gpt-image-1-mini"
// Match the panel orientation so less cropping is needed:
//   portrait  -> 1024x1536,   landscape -> 1536x1024
static const char *AI_SIZE = (ORIENTATION == ORIENT_LANDSCAPE) ? "1536x1024" : "1024x1536";
static const char *AI_QUALITY = "";             // optional; empty = model default (medium). gpt-image-1 accepts low/medium/high
static const uint32_t AI_TIMEOUT_MS = 120000;   // per-request read timeout
static const int AI_MAX_ATTEMPTS = 3;           // retries for transient API failures
static const uint32_t AI_RETRY_DELAY_MS = 3000; // pause between attempts
static const bool AI_DEBUG_DUMP = false;        // log the head of the OpenAI JSON
                                                // response (for b64_json debugging)

// ---------------- Image pipeline / display ----------------
// How the generated image is fitted into the render frame:
//   RESIZE_COVER    - scale so the image fills the frame, center-crop overflow
//   RESIZE_CONTAIN  - scale to fit the whole image, letterbox with white bars
//   RESIZE_ORIGINAL - no scaling: 1:1 centered, crop edges if larger
enum ResizeMode { RESIZE_COVER = 0,
                  RESIZE_CONTAIN,
                  RESIZE_ORIGINAL };

// Physical panel resolution (fixed by the Spectra 6 hardware).
static const int PANEL_W = 1200;
static const int PANEL_H = 1600;

// Logical render resolution derived from ORIENTATION.
static const int RENDER_W = (ORIENTATION == ORIENT_LANDSCAPE) ? PANEL_H : PANEL_W;
static const int RENDER_H = (ORIENTATION == ORIENT_LANDSCAPE) ? PANEL_W : PANEL_H;

static const ResizeMode RESIZE_MODE = RESIZE_COVER;

// Max time to wait during displayInit() for the panel to deassert BUSY
// (BUSY = HIGH means "ready" on the T133A01 controller).
static const uint32_t DISPLAY_INIT_TIMEOUT_MS = 2000;

// ---------------- SD card temp files ----------------
static const char *SD_TMP_RESP = "/daydream/tmp_resp.json";
static const char *SD_TMP_PNG = "/daydream/tmp_image.png";

// ---------------- Misc ----------------
static const int WIFI_TIMEOUT_MS = 30000;
