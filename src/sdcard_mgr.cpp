#include "sdcard_mgr.h"
#include "config.h"
#include "pins.h"
#include "logging.h"
#include "display_mgr.h"  // epaper (owns the shared SPI bus)
#include <SD.h>

bool sdInit() {
  pinMode(PIN_SD_EN, OUTPUT);
  digitalWrite(PIN_SD_EN, HIGH);  // power the card
  pinMode(PIN_SD_DET, INPUT_PULLUP);
  delay(50);

  // The SD card shares GPIO7/8/9 with the display; mount it on the display's
  // SPI instance (pattern from examples/SD_ImagePipeline_E1004).
  SPIClass& spi = epaper.getSPIinstance();
  spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, -1);

  if (!SD.begin(PIN_SD_CS, spi)) {
    LOG.println("[SD] SD.begin FAILED");
    return false;
  }
  if (!SD.exists("/daydream")) {
    SD.mkdir("/daydream");
  }
  LOG.println("[SD] mounted");
  return true;
}

void sdDelete(const char* path) {
  if (SD.exists(path)) {
    SD.remove(path);
  }
}
