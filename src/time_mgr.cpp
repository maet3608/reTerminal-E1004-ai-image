#include "time_mgr.h"
#include "config.h"
#include "logging.h"
#include "pins.h"
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

// sntp_get_sync_status() is exported by the IDF SNTP library, but its header
// (apps/esp_sntp.h) is not on the Arduino include path. Declare it directly;
// it returns the sntp_sync_status_t enum (SNTP_SYNC_STATUS_COMPLETED == 1).
extern "C" int sntp_get_sync_status(void);
static const int kSntpStatusCompleted = 1;

// ---------------- PCF8563 (port from examples/RTC_PCF8563) ----------------
#define PCF8563_ADDR 0x51
#define REG_CTRL1 0x00
#define REG_CTRL2 0x01
#define REG_SECONDS 0x02
#define REG_MINUTES 0x03
#define REG_HOURS 0x04
#define REG_DAYS 0x05
#define REG_WEEKDAYS 0x06
#define REG_MONTHS 0x07
#define REG_YEARS 0x08
#define REG_CLKOUT 0x0D

struct RtcTime {
  int year, month, day, weekday, hour, minute, second;
  bool voltageOK;
};

static inline uint8_t bcdToDec(uint8_t bcd) {
  return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}
static inline uint8_t decToBcd(uint8_t dec) {
  return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

static bool rtcReadRegs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(PCF8563_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false; // repeated START
  if (Wire.requestFrom((uint8_t)PCF8563_ADDR, (uint8_t)len) != len)
    return false;
  for (size_t i = 0; i < len; i++)
    buf[i] = (uint8_t)Wire.read();
  return true;
}

static bool rtcWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PCF8563_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool rtcProbe() {
  Wire.beginTransmission(PCF8563_ADDR);
  return Wire.endTransmission() == 0;
}

static bool rtcInit() {
  if (!rtcWriteReg(REG_CTRL1, 0x00))
    return false; // STOP=0 -> run
  if (!rtcWriteReg(REG_CTRL2, 0x00))
    return false; // clear alarm/timer flags
  if (!rtcWriteReg(REG_CLKOUT, 0x00))
    return false; // disable CLKOUT (power)
  return true;
}

static bool rtcGetTime(RtcTime &rt) {
  uint8_t raw[7];
  if (!rtcReadRegs(REG_SECONDS, raw, 7))
    return false;
  rt.voltageOK = (raw[0] & 0x80U) == 0U; // VL flag
  rt.second = bcdToDec(raw[0] & 0x7FU);
  rt.minute = bcdToDec(raw[1] & 0x7FU);
  rt.hour = bcdToDec(raw[2] & 0x3FU);
  rt.day = bcdToDec(raw[3] & 0x3FU);
  rt.weekday = bcdToDec(raw[4] & 0x07U);
  rt.month = bcdToDec(raw[5] & 0x1FU);
  const int yr = bcdToDec(raw[6]);
  rt.year = ((raw[5] & 0x80U) != 0U) ? (1900 + yr) : (2000 + yr);
  return true;
}

static bool rtcSetTime(int year, int month, int day,
                       int hour, int minute, int second) {
  if (year < 2000 || year > 2099)
    return false;
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  mktime(&t); // fills t.tm_wday

  Wire.beginTransmission(PCF8563_ADDR);
  Wire.write(REG_SECONDS);
  Wire.write(decToBcd((uint8_t)second));
  Wire.write(decToBcd((uint8_t)minute));
  Wire.write(decToBcd((uint8_t)hour));
  Wire.write(decToBcd((uint8_t)day));
  Wire.write((uint8_t)t.tm_wday);
  Wire.write(decToBcd((uint8_t)month));
  Wire.write(decToBcd((uint8_t)(year % 100)));
  return Wire.endTransmission() == 0;
}

static void syncSystemClock(const RtcTime &rt) {
  struct tm t = {};
  t.tm_year = rt.year - 1900;
  t.tm_mon = rt.month - 1;
  t.tm_mday = rt.day;
  t.tm_hour = rt.hour;
  t.tm_min = rt.minute;
  t.tm_sec = rt.second;
  const time_t epoch = mktime(&t);
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
}

// ---------------- Public API ----------------

static void timeSetTz() {
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

bool timeMgrInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000UL);
  timeSetTz();

  if (!rtcProbe()) {
    LOG.println("[RTC] PCF8563 not found - time via NTP only.");
    return false;
  }
  if (!rtcInit()) {
    LOG.println("[RTC] PCF8563 init failed - time via NTP only.");
    return false;
  }

  RtcTime rt;
  if (!rtcGetTime(rt)) {
    LOG.println("[RTC] read failed - time via NTP only.");
    return false;
  }
  if (!rt.voltageOK) {
    LOG.println("[RTC] VL flag set (time unreliable) - will set time via NTP.");
    return false;
  }
  syncSystemClock(rt);
  LOG.printf("[RTC] %04d-%02d-%02d %02d:%02d:%02d (from RTC)\n",
             rt.year, rt.month, rt.day, rt.hour, rt.minute, rt.second);
  return true;
}

bool timeSyncFromNtp() {
  if (!NTP_ENABLED)
    return false;
  configTzTime(TIMEZONE, NTP_SERVER);
  time_t now = 0;
  int tries = 0;
  while (tries < 50) {
    delay(200);
    time(&now);
    tries++;
    if (now > 1600000000L &&
        sntp_get_sync_status() == kSntpStatusCompleted) {
      break;
    }
  }
  if (now < 1600000000L) {
    LOG.println("[RTC] NTP sync failed.");
    return false;
  }
  // Also (re)write the hardware RTC so time survives even if NTP is down
  // during the next wake.
  struct tm t;
  localtime_r(&now, &t);
  rtcSetTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
  LOG.println("[RTC] NTP sync ok (RTC updated).");
  return true;
}

bool fillDayInfo(DayInfo &info) {
  time_t now = time(nullptr);
  if (now < 1600000000L)
    return false;
  struct tm t;
  localtime_r(&now, &t);
  info.year = t.tm_year + 1900;
  info.month = t.tm_mon + 1;
  info.day = t.tm_mday;
  info.weekday = t.tm_wday;
  strftime(info.weekdayFull, sizeof(info.weekdayFull), "%A", &t);
  strftime(info.weekdayShort, sizeof(info.weekdayShort), "%a", &t);
  strftime(info.dateFull, sizeof(info.dateFull), DATE_FORMAT, &t);
  return true;
}
