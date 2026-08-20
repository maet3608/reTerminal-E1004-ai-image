#pragma once
#include <Arduino.h>

// Decode /daydream/tmp_image.png, fit it into the panel according to
// RESIZE_MODE / ORIENTATION (config.h), dither to the 4bpp Spectra 6 format
// and display it. Returns true on success.
bool pipelineRender();
