#include "display_mgr.h"
#include "config.h"
#include "logging.h"

// FreeSans fonts are already provided by TFT_eSPI.h (Seeed_GFX bundles them
// via Fonts/GFXFF/gfxfont.h), so no explicit font #includes are needed here.

EPaper epaper;
static bool sInited = false;

bool displayInit() {
  epaper.begin();

  // The EPaper constructor allocates the full 1200x1600 framebuffer in
  // PSRAM. If that allocation failed (PSRAM broken/disabled), every later
  // draw call would write through a null pointer - catch it here.
  if (!epaper.created()) {
    LOG.println("[EPD] init failed - framebuffer not allocated (PSRAM?).");
    return false;
  }

  // The T133A01 controller drives BUSY HIGH when it is ready (the driver's
  // CHECK_BUSY waits for exactly that). begin() just reset the panel, so a
  // healthy one deasserts BUSY within a short time; a stuck-low line means
  // the panel is unresponsive (power / ribbon / controller fault).
  pinMode(PIN_EPD_BUSY, INPUT);
  const unsigned long deadline =
      millis() + (unsigned long)DISPLAY_INIT_TIMEOUT_MS;
  while (millis() < deadline) {
    if (digitalRead(PIN_EPD_BUSY)) {
      sInited = true;
      LOG.println("[EPD] initialized (1200x1600)");
      return true;
    }
    delay(10);
  }

  LOG.println("[EPD] init failed - panel BUSY stuck low.");
  return false;
}

bool displayShowImage(const uint8_t* buf4bpp, int w, int h) {
  if (!sInited || !buf4bpp) return false;
  epaper.pushImage(0, 0, w, h, (uint16_t*)buf4bpp);
  epaper.update();  // full-color refresh, ~40 s
  LOG.println("[EPD] updated");
  return true;
}

void displaySleep() {
  if (sInited) {
    epaper.sleep();
  }
}

static const char* errorLabel(ErrorCode code) {
  switch (code) {
    case ERR_LOW_BATTERY: return "ERR_LOW_BATTERY";
    case ERR_WIFI:        return "ERR_WIFI";
    case ERR_WEATHER:     return "ERR_WEATHER";
    case ERR_TIME:        return "ERR_TIME";
    case ERR_AI:          return "ERR_AI";
    case ERR_DOWNLOAD:    return "ERR_DOWNLOAD";
    case ERR_DECODE_B64:  return "ERR_DECODE_B64";
    case ERR_DECODE:      return "ERR_DECODE";
    case ERR_SD:          return "ERR_SD";
    case ERR_DISPLAY:     return "ERR_DISPLAY";
    default:              return "ERR_UNKNOWN";
  }
}

void showError(ErrorCode code, const char* message) {
  LOG.printf("[ERR] %s: %s\n", errorLabel(code), message ? message : "");
  if (!sInited) return;

  epaper.fillScreen(TFT_WHITE);
  epaper.setTextDatum(TL_DATUM);

  epaper.setTextColor(TFT_BLACK);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString("reTerminal E1004 AI image", 40, 60);

  epaper.setTextColor(TFT_RED);
  epaper.setFreeFont(&FreeSansBold18pt7b);
  epaper.drawString(errorLabel(code), 40, 150);

  epaper.setTextColor(TFT_BLACK);
  epaper.setFreeFont(&FreeSans9pt7b);
  epaper.drawString(message ? message : "", 40, 220);
  epaper.drawString("Will retry tomorrow at 01:00.", 40, 260);

  epaper.update();
}

