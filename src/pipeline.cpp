#include "pipeline.h"
#include "config.h"
#include "logging.h"
#include "display_mgr.h"
#include "palette_dither.h"
#include <PNGdec.h>
#include <SD.h>
#include <cmath>

static PNG sPng;
static uint16_t* sRgb = nullptr;  // RGB565 framebuffer (PSRAM)
static int sW = 0;
static int sH = 0;

// PNGdec draw callback: fill the row of the RGB565 framebuffer.
static int pngRowCallback(PNGDRAW* pDraw) {
  if (!sRgb) return 0;
  uint16_t* row = sRgb + (size_t)pDraw->y * sW;
  // LITTLE_ENDIAN stores native uint16_t values, so sRgb can be read back
  // directly. BIG_ENDIAN byte-swaps (bswap16) every pixel, which corrupts
  // the R/G/B channels when the buffer is treated as little-endian uint16_t.
  sPng.getLineAsRGB565(pDraw, row, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFF);
  return 1;
}

// ---- PNGdec file callbacks: stream the PNG from the SD card ----

static void* pngOpen(const char* szFilename, int32_t* pFileSize) {
  (void)szFilename;  // we always read SD_TMP_PNG
  fs::File* f = new fs::File(SD.open(SD_TMP_PNG, FILE_READ));
  if (!*f) {
    delete f;
    return nullptr;
  }
  *pFileSize = (int32_t)f->size();
  return f;
}

static int32_t pngRead(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  fs::File* f = static_cast<fs::File*>(pFile->fHandle);
  return f->read(pBuf, iLen);
}

static int32_t pngSeek(PNGFILE* pFile, int32_t iPosition) {
  fs::File* f = static_cast<fs::File*>(pFile->fHandle);
  f->seek(iPosition);
  return 0;
}

static void pngClose(void* pHandle) {
  fs::File* f = static_cast<fs::File*>(pHandle);
  if (f) {
    f->close();
    delete f;
  }
}

// ---------------------------------------------------------------------------

bool pipelineRender() {
  if (!SD.exists(SD_TMP_PNG)) {
    LOG.println("[PIPE] PNG file missing");
    return false;
  }

  const int openRc = sPng.open(SD_TMP_PNG, pngOpen, pngClose, pngRead, pngSeek,
                               pngRowCallback);
  if (openRc != PNG_SUCCESS) {
    LOG.printf("[PIPE] PNG open failed (rc=%d)\n", openRc);
    return false;
  }
  sW = sPng.getWidth();
  sH = sPng.getHeight();
  if (sW <= 0 || sH <= 0 || sW > PNG_MAX_BUFFERED_PIXELS || sH > 2048) {
    LOG.printf("[PIPE] bad PNG size %dx%d\n", sW, sH);
    sPng.close();
    return false;
  }

  sRgb = (uint16_t*)ps_malloc((size_t)sW * sH * 2);
  if (!sRgb) {
    LOG.println("[PIPE] PSRAM alloc failed (RGB565)");
    sPng.close();
    return false;
  }

  const int rc = sPng.decode(NULL, 0);
  sPng.close();
  if (rc != PNG_SUCCESS) {
    LOG.printf("[PIPE] PNG decode failed (%d)\n", rc);
    free(sRgb);
    sRgb = nullptr;
    return false;
  }
  LOG.printf("[PIPE] decoded %dx%d -> render %dx%d (%s)\n", sW, sH,
             RENDER_W, RENDER_H,
             ORIENTATION == ORIENT_LANDSCAPE ? "landscape" : "portrait");

  uint8_t* out4bpp = (uint8_t*)ps_malloc((size_t)PANEL_W * PANEL_H / 2);
  uint8_t* rowRgb = (uint8_t*)ps_malloc((size_t)PANEL_W * 3);
  FSDither dither(PANEL_W);
  const bool okAlloc = out4bpp && rowRgb && dither.alloc();
  if (!okAlloc) {
    LOG.println("[PIPE] PSRAM alloc failed (pipeline)");
    if (out4bpp) free(out4bpp);
    if (rowRgb) free(rowRgb);
    free(sRgb);
    sRgb = nullptr;
    return false;
  }

  // Fit policy (RESIZE_MODE in config.h), computed in the logical render
  // frame (RENDER_W x RENDER_H, derived from ORIENTATION):
  //   cover    - scale so the image fills the frame, then center-crop
  //   contain  - fit the whole image, letterbox with white bars
  //   original - no scaling: 1:1 in the center, crop edges if larger
  int srcX0 = 0, srcY0 = 0, srcW = sW, srcH = sH;  // source sampling rect
  int fx = 0, fy = 0, fw = RENDER_W, fh = RENDER_H;  // fitted rect (logical)
  if (RESIZE_MODE == RESIZE_CONTAIN) {
    const double s = fmin((double)RENDER_W / sW, (double)RENDER_H / sH);
    fw = (int)(sW * s);
    fh = (int)(sH * s);
    fx = (RENDER_W - fw) / 2;
    fy = (RENDER_H - fh) / 2;
  } else if (RESIZE_MODE == RESIZE_COVER) {
    const double s = fmax((double)RENDER_W / sW, (double)RENDER_H / sH);
    srcW = (int)(RENDER_W / s);
    srcH = (int)(RENDER_H / s);
    srcX0 = (sW - srcW) / 2;
    srcY0 = (sH - srcH) / 2;
  } else {  // RESIZE_ORIGINAL: 1:1, centered, crop edges if larger
    srcW = fmin(sW, RENDER_W);
    srcH = fmin(sH, RENDER_H);
    srcX0 = (sW - srcW) / 2;
    srcY0 = (sH - srcH) / 2;
    fw = srcW;
    fh = srcH;
    fx = (RENDER_W - fw) / 2;
    fy = (RENDER_H - fh) / 2;
  }

  // Render in physical scan order (the panel is always PANEL_W x PANEL_H).
  // In landscape mode the logical frame is rotated 90 degrees clockwise, so
  // each physical pixel is mapped back to its logical source before sampling.
  // The ditherer therefore sees rows top-to-bottom in physical order and error
  // diffusion stays correct across the rotated image.
  for (int py = 0; py < PANEL_H; py++) {
    // Default row = white (letterbox bars in contain / original mode).
    memset(rowRgb, 0xFF, (size_t)PANEL_W * 3);

    for (int px = 0; px < PANEL_W; px++) {
      int lx = px, ly = py;
      if (ORIENTATION == ORIENT_LANDSCAPE) {  // 90 deg clockwise
        lx = py;
        ly = RENDER_H - 1 - px;
      }
      if (lx < fx || lx >= fx + fw || ly < fy || ly >= fy + fh) continue;
      const int sy = srcY0 + (int)(((int64_t)(ly - fy) * srcH) / fh);
      const int sx = srcX0 + (int)(((int64_t)(lx - fx) * srcW) / fw);
      const uint16_t pc = sRgb[(size_t)sy * sW + sx];
      // 565 -> 888 conversion in float with rounding, identical to
      // picture.h's ditherRow(). Integer division would truncate instead.
      rowRgb[px * 3 + 0] = (uint8_t)roundf((float)((pc >> 11) & 0x1F) * 255.0f / 31.0f);
      rowRgb[px * 3 + 1] = (uint8_t)roundf((float)((pc >> 5) & 0x3F) * 255.0f / 63.0f);
      rowRgb[px * 3 + 2] = (uint8_t)roundf((float)(pc & 0x1F) * 255.0f / 31.0f);
    }
    dither.processRow(rowRgb, out4bpp + (size_t)py * (PANEL_W / 2));
  }

  free(rowRgb);
  free(sRgb);
  sRgb = nullptr;

  const bool ok = displayShowImage(out4bpp, PANEL_W, PANEL_H);
  free(out4bpp);
  return ok;
}

