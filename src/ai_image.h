#pragma once
#include <Arduino.h>

// Ask OpenAI for an image, stream the JSON response to SD_TMP_RESP, extract
// the base64 PNG payload and decode it to SD_TMP_PNG.
// Returns true when /daydream/tmp_image.png contains a valid PNG.
bool aiGenerateImage(const String& prompt);
