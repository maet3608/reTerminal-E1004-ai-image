#pragma once
#include <Arduino.h>

// Measure the battery voltage (millivolts) via GPIO1/GPIO21.
int batteryReadMillivolts();

// Battery charge in percent (LiPo discharge curve lookup).
int batteryReadPercent();
