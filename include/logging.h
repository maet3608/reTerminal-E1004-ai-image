#pragma once
#include <Arduino.h>
#include "pins.h"

// Logging goes to the carrier USB-UART bridge: Serial1 on GPIO44 (RX) / GPIO43
// (TX), 115200 baud. This is NOT the USB-CDC Serial.
#define LOG Serial1

inline void logInit() {
  Serial1.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
}
