#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <time.h>

#include "ai_image.h"
#include "battery.h"
#include "config.h"
#include "credentials.h"
#include "display_mgr.h"
#include "logging.h"
#include "pins.h"
#include "pipeline.h"
#include "prompt.h"
#include "sdcard_mgr.h"
#include "time_mgr.h"
#include "weather.h"
#include "wifi_mgr.h"

// ---------------------------------------------------------------------------
// Deep sleep
// ---------------------------------------------------------------------------

static uint64_t secondsUntilNextHour(int targetHour) {
  struct tm t;
  if (!getLocalTime(&t)) return 24ULL * 3600ULL;
  const int secSinceMidnight = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  int secTo = (targetHour * 3600 - secSinceMidnight + 86400) % 86400;
  if (secTo == 0) secTo = 86400;
  return (uint64_t)secTo;
}

static void goToDeepSleep() {
  displaySleep();
  wifiDisconnect();
  sdDelete(SD_TMP_RESP);
  sdDelete(SD_TMP_PNG);

  const uint64_t usec = secondsUntilNextHour(REFRESH_HOUR) * 1000000ULL;
  esp_sleep_enable_timer_wakeup(usec);
  // GPIO5 (KEY2, front-bezel refresh button) wakes the device too; it just
  // re-runs the same daily cycle.
  esp_sleep_enable_ext1_wakeup(1ULL << PIN_WAKE_BTN, ESP_EXT1_WAKEUP_ANY_LOW);
  rtc_gpio_pullup_en((gpio_num_t)PIN_WAKE_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_WAKE_BTN);

  LOG.printf("[SLEEP] deep sleep %llu s (next %02d:00), button GPIO%d\n",
             (unsigned long long)(usec / 1000000ULL), REFRESH_HOUR,
             PIN_WAKE_BTN);
  LOG.flush();
  delay(10);
  esp_deep_sleep_start();
}

static void failAndSleep(ErrorCode code, const char* message) {
  showError(code, message);
  goToDeepSleep();
}

// ---------------------------------------------------------------------------
// Buzzer (onboard, GPIO45)
// ---------------------------------------------------------------------------

// Short audible confirmation when KEY2 (front-bezel refresh) is pressed to
// wake the device, as opposed to the daily RTC-timer wake.
static void buzzerConfirmBeep() {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, 1000, 100);  // 1 kHz for 100 ms
  delay(120);                   // let the tone finish before continuing
  noTone(PIN_BUZZER);
}

// ---------------------------------------------------------------------------
// setup() - one daily cycle, then deep sleep
// ---------------------------------------------------------------------------

void setup() {
  logInit();
  delay(100);
  LOG.println("=======================================");
  LOG.println("  reTerminal E1004 AI image");
  LOG.println("=======================================");
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  LOG.printf("[WAKE] cause=%d\n", (int)wakeCause);

  // KEY2 (front-bezel refresh) was pressed to wake the device: confirm with a
  // short beep before re-running the daily cycle.
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT1) {
    buzzerConfirmBeep();
  }

  // Time first (RTC + TZ); NTP corrects drift once Wi-Fi is up.
  timeMgrInit();

  // The display is initialized early so error screens can render at any
  // later failure point.
  if (!displayInit()) {
    LOG.println("[FATAL] display init failed - cannot show error screens.");
    goToDeepSleep();
  }

  if (batteryReadPercent() < BAT_LOW_PERCENT) {
    failAndSleep(ERR_LOW_BATTERY, "Battery low - please charge the device.");
  }

  if (!sdInit()) {
    failAndSleep(ERR_SD, "SD card error.");
  }

  if (!wifiConnect(WIFI_TIMEOUT_MS)) {
    failAndSleep(ERR_WIFI, "Wi-Fi connection failed.");
  }
  timeSyncFromNtp();  // correct drift / set time when RTC was invalid

  // The noon forecast target (WX_TARGET_HOUR, local time) depends on the
  // system clock, so validate it BEFORE fetching weather. With an invalid
  // clock the forecast would be matched against a bogus epoch and silently
  // fall back to "current weather".
  DayInfo day;
  if (!fillDayInfo(day)) {
    failAndSleep(ERR_TIME, "System clock not set.");
  }

  if (!weatherFetchNoon()) {
    failAndSleep(ERR_WEATHER, "Weather data unavailable.");
  }

  const int batteryPct = batteryReadPercent();

  const String prompt = buildPrompt(day, weatherData, batteryPct);
  LOG.printf("[PROMPT]\n%s\n", prompt.c_str());

  if (!aiGenerateImage(prompt)) {
    failAndSleep(ERR_AI, "AI image generation failed.");
  }

  if (!pipelineRender()) {
    failAndSleep(ERR_DECODE, "Image processing failed.");
  }

  LOG.println("[OK] daily cycle complete.");
  goToDeepSleep();
}

void loop() {
  delay(1000);  // never reached (esp_deep_sleep_start() never returns)
}
