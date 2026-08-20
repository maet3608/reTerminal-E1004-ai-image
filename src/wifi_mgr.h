#pragma once
#include <Arduino.h>

// Connect to Wi-Fi (STA). Returns true when connected within timeoutMs.
bool wifiConnect(int timeoutMs);

// Disconnect and switch the radio off (for deep sleep).
void wifiDisconnect();
