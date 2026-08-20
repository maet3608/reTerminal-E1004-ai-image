#pragma once
#include <Arduino.h>

struct DayInfo {
  int  year, month, day;     // e.g. 2026, 6, 1
  int  weekday;              // 0 = Sunday .. 6 = Saturday
  char weekdayFull[16];      // "Monday"
  char weekdayShort[8];      // "Mon"
  char dateFull[32];         // "1 June 2026" (DATE_FORMAT)
};

// Read the PCF8563 and sync the ESP32 system clock; also sets the TZ.
// Returns true if the RTC time is trusted (VL flag not set).
bool timeMgrInit();

// Correct RTC drift (or set the time when the RTC was invalid). Best effort.
bool timeSyncFromNtp();

// Fill DayInfo from the (TZ-adjusted) system clock. Returns false if the
// system clock is not set yet (epoch before ~2020).
bool fillDayInfo(DayInfo& info);
