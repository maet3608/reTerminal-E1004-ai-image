#pragma once
#include <Arduino.h>
#include <stdint.h>

// Spectra 6 palette (calibrated RGB triplets), ported from
// xiao-ee02-e-paper-calendar/picture.h. Nibble codes match the E1004
// dither.h PAL_E6 encoding.
struct PaletteColor { int r, g, b; };

extern const PaletteColor SPECTRA6[6];
extern const uint8_t PALETTE_NIBBLE[6];  // 0x0=W 0x2=G 0x6=R 0xB=Y 0xD=B 0xF=BK

// Nearest of the six Spectra 6 colors (Euclidean distance in RGB space).
uint8_t closestPalette(int r, int g, int b);

// Floyd-Steinberg error-diffusion ditherer, streaming row by row.
// Feed rows top-to-bottom with processRow(); output is packed 4bpp
// (even-x pixel in the high nibble, odd-x in the low nibble) - the same
// format pushImage() expects on the E1004.
class FSDither {
 public:
  explicit FSDither(int width);
  ~FSDither();

  // Allocate the error buffers (PSRAM preferred). Call before processRow().
  bool alloc();
  void reset();

  // Dither one RGB888 row (width*3 bytes) into width/2 packed bytes.
  bool processRow(const uint8_t* rgb888, uint8_t* out4bpp);

  int width() const { return width_; }

 private:
  int width_;
  float* errR_;
  float* errG_;
  float* errB_;  // each 2*width floats (current row + next row)
  int row_;      // 0 or 1 - which half is the current row
};
