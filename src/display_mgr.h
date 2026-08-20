#pragma once
#include "TFT_eSPI.h"
#include "pins.h"
#include <Arduino.h>

// Global display instance (Seeed_GFX EPaper for the E1004, selected via the
// BOARD_SCREEN_COMBO=523 build flag).
extern EPaper epaper;

enum ErrorCode {
  ERR_LOW_BATTERY,
  ERR_WIFI,
  ERR_WEATHER,
  ERR_TIME,
  ERR_AI,
  ERR_DOWNLOAD,
  ERR_DECODE_B64,
  ERR_DECODE,
  ERR_SD,
  ERR_DISPLAY,
};

// Called early in setup() so error screens can render.
bool displayInit();

// Push a packed 4bpp image (w*h/2 bytes) and refresh the panel.
bool displayShowImage(const uint8_t *buf4bpp, int w, int h);

// Power the panel down before deep sleep.
void displaySleep();

// Full-screen error message.
void showError(ErrorCode code, const char *message);
