#pragma once
#include <Arduino.h>

// Power + mount the microSD card on the display's shared SPI instance.
// Call displayInit() first. Returns true on success.
bool sdInit();

// Remove a file if it exists (safe no-op when SD is not mounted).
void sdDelete(const char* path);
