#include "palette_dither.h"
#include <climits>
#include <cmath>
#include <cstring>

const PaletteColor SPECTRA6[6] = {
    {25, 30, 33},     // 0: Black   -> nibble 0xF
    {232, 232, 232},  // 1: White   -> nibble 0x0
    {239, 222, 68},   // 2: Yellow  -> nibble 0xB
    {178, 19, 24},    // 3: Red     -> nibble 0x6
    {33, 87, 186},    // 4: Blue    -> nibble 0xD
    {18, 95, 32},     // 5: Green   -> nibble 0x2
};

const uint8_t PALETTE_NIBBLE[6] = {0xF, 0x0, 0xB, 0x6, 0xD, 0x2};

uint8_t closestPalette(int r, int g, int b) {
  uint8_t best = 0;
  long bestDist = LONG_MAX;
  for (uint8_t i = 0; i < 6; i++) {
    long dr = (long)r - SPECTRA6[i].r;
    long dg = (long)g - SPECTRA6[i].g;
    long db = (long)b - SPECTRA6[i].b;
    long dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

FSDither::FSDither(int width)
    : width_(width), errR_(nullptr), errG_(nullptr), errB_(nullptr), row_(0) {}

FSDither::~FSDither() {
  if (errR_) free(errR_);
  if (errG_) free(errG_);
  if (errB_) free(errB_);
}

bool FSDither::alloc() {
  if (width_ <= 0) return false;
  const size_t n = (size_t)width_ * 2 * sizeof(float);

  errR_ = (float*)ps_malloc(n);
  if (!errR_) errR_ = (float*)malloc(n);
  errG_ = (float*)ps_malloc(n);
  if (!errG_) errG_ = (float*)malloc(n);
  errB_ = (float*)ps_malloc(n);
  if (!errB_) errB_ = (float*)malloc(n);

  if (!errR_ || !errG_ || !errB_) {
    if (errR_) free(errR_);
    if (errG_) free(errG_);
    if (errB_) free(errB_);
    errR_ = errG_ = errB_ = nullptr;
    return false;
  }
  reset();
  return true;
}

void FSDither::reset() {
  memset(errR_, 0, (size_t)width_ * 2 * sizeof(float));
  memset(errG_, 0, (size_t)width_ * 2 * sizeof(float));
  memset(errB_, 0, (size_t)width_ * 2 * sizeof(float));
  row_ = 0;
}

bool FSDither::processRow(const uint8_t* rgb888, uint8_t* out4bpp) {
  if (!errR_ || !rgb888 || !out4bpp) return false;

  float* curR = errR_ + (size_t)row_ * width_;
  float* curG = errG_ + (size_t)row_ * width_;
  float* curB = errB_ + (size_t)row_ * width_;
  float* nxtR = errR_ + (size_t)(1 - row_) * width_;
  float* nxtG = errG_ + (size_t)(1 - row_) * width_;
  float* nxtB = errB_ + (size_t)(1 - row_) * width_;

  int outIdx = 0;
  for (int x = 0; x < width_; x++) {
    const uint8_t* px = rgb888 + (size_t)x * 3;
    float r = (float)px[0] + curR[x];
    float g = (float)px[1] + curG[x];
    float b = (float)px[2] + curB[x];
    r = fmaxf(0.0f, fminf(255.0f, r));
    g = fmaxf(0.0f, fminf(255.0f, g));
    b = fmaxf(0.0f, fminf(255.0f, b));

    uint8_t idx = closestPalette((int)roundf(r), (int)roundf(g), (int)roundf(b));
    const PaletteColor& pc = SPECTRA6[idx];
    float er = r - (float)pc.r;
    float eg = g - (float)pc.g;
    float eb = b - (float)pc.b;

    if (x + 1 < width_) {
      curR[x + 1] += er * (7.0f / 16.0f);
      curG[x + 1] += eg * (7.0f / 16.0f);
      curB[x + 1] += eb * (7.0f / 16.0f);
    }
    if (x > 0) {
      nxtR[x - 1] += er * (3.0f / 16.0f);
      nxtG[x - 1] += eg * (3.0f / 16.0f);
      nxtB[x - 1] += eb * (3.0f / 16.0f);
    }
    nxtR[x] += er * (5.0f / 16.0f);
    nxtG[x] += eg * (5.0f / 16.0f);
    nxtB[x] += eb * (5.0f / 16.0f);
    if (x + 1 < width_) {
      nxtR[x + 1] += er * (1.0f / 16.0f);
      nxtG[x + 1] += eg * (1.0f / 16.0f);
      nxtB[x + 1] += eb * (1.0f / 16.0f);
    }

    if ((x & 1) == 0) {
      out4bpp[outIdx] = PALETTE_NIBBLE[idx] << 4;
    } else {
      out4bpp[outIdx] |= PALETTE_NIBBLE[idx];
      outIdx++;
    }
  }

  memset(curR, 0, (size_t)width_ * sizeof(float));
  memset(curG, 0, (size_t)width_ * sizeof(float));
  memset(curB, 0, (size_t)width_ * sizeof(float));
  row_ = 1 - row_;
  return true;
}
