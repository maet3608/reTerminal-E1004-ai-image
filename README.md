# reTerminal E1004 AI image

A battery-powered ePaper wall/desk display built around the **Seeed Studio
reTerminal E1004** — a 13.3" 1200×1600 **six-color Spectra 6** ePaper panel
driven by an ESP32-S3 with OPI PSRAM. 

Once a day it wakes from deep sleep, fetches the weather **forecast for around 12:00
(noon)**, and asks OpenAI to generate themed artwork in which the weekday,
date and weather are integrated as small objects *inside* the scene. The image
is resized, dithered to the panel's six colors, displayed — where it stays
visible for the whole day **without consuming any power** — and the device
returns to deep sleep until the next morning.

The result is a "generative wall art" that looks different.

### How it works (the daily cycle)

Each run follows a fixed sequence. On any failure a full-screen error message
is drawn on the panel and the device goes back to sleep to retry at the next
wake:

1. **Wake** — the deep-sleep timer fires at `REFRESH_HOUR` (01:00 local time
   by default). The front-bezel **Circle** button wakes the device instantly to
   re-run the same cycle (confirmed with a short beep).
2. **Time** — the PCF8563 hardware RTC (I²C) seeds the ESP32 system clock and
   the POSIX timezone is applied. Once Wi-Fi is up, NTP corrects RTC drift and
   rewrites the RTC chip, so the time survives even if the network is down on
   the next wake.
3. **Display init** — the ePaper controller is brought up *early* so error
   screens can be rendered at any later failure point.
4. **Battery gate** — below `BAT_LOW_PERCENT` charge the cycle stops with a
   "please charge" screen instead of draining the battery on a doomed network
   run.
5. **SD card** — the microSD card is powered and mounted; it carries the temp
   files of the image pipeline (`/daydream/` is created automatically).
6. **Weather** — OpenWeather's 5-day/3-hour forecast is fetched and the entry
   closest to today's `WX_TARGET_HOUR` (noon, local time) is selected; if none
   is within ±`WX_MAX_NOON_DELTA`, the current weather is used instead.
7. **Prompt** — a prompt is assembled from the theme, the weekday/date
   (`DATE_FORMAT`), the weather description and temperature, and the battery
   level — all of which must appear as *objects drawn into* the scene, not as
   text overlays.
8. **AI image** — OpenAI `gpt-image-1` renders the artwork; the JSON response
   is streamed to the SD card and the base64 PNG payload is decoded to a file
   (no large RAM buffer).
9. **Pipeline** — the PNG is decoded into an RGB565 framebuffer in PSRAM,
   fitted to the panel per `RESIZE_MODE`/`ORIENTATION`, and Floyd–Steinberg
   dithered to the six Spectra 6 colors.
10. **Display & sleep** — the packed 4bpp image is pushed to the panel and a
    full-color refresh (~40 s) is triggered. Then the panel is powered down,
    temp files are deleted, Wi-Fi is switched off and the device returns to
    deep sleep for the next day.

### Features

- **Fully autonomous** — one daily run, deep sleep for the remaining ~23 hours.
- **Real weather, not a guess** — the noon forecast drives the artwork.
- **Fresh art every day** — a fixed style per weekday (Kandinsky, Ukiyo-e,
  anime, botanical, steampunk, Victorian, Mondrian) or a single custom theme.
- **Two wake paths** — RTC timer at 01:00, or the front-bezel KEY2 button.
- **Built to recover** — NTP-corrected RTC, API retries, graceful full-screen
  error screens and low-battery protection.
- **Nearly zero maintenance** — the ePaper image persists with no power, and
  the LiPo battery lasts weeks between charges.

## Links

[reTerminal E1004 Product](https://www.seeedstudio.com/reTerminal-E1004-p-6692.html)
[Getting Started with reTerminal E1004](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1004/)
[Schematic reTerminal E1004](https://files.seeedstudio.com/wiki/reterminal_e10xx/res/202004523_reTerminal%20E1004_V1.0_SCH_260105.pdf)
[Github](https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D)
[Github E1004](https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/blob/main/examples/base/GxEPD2_reTerminal_E1004/GxEPD2_reTerminal_E1004.ino)

## Display hardware

### The panel

| Property     | Value                                                   |
| ------------ | ------------------------------------------------------- |
| Size         | 13.3" diagonal                                          |
| Resolution   | 1200 × 1600 pixels (native portrait)                    |
| Technology   | E Ink **Spectra 6** color ePaper                        |
| Colors       | 6 — black, white, red, yellow, blue, green              |
| Controller   | Dual-chip T133A01 (two driver ICs)                      |
| Interface    | SPI, shared bus with the microSD slot                   |
| Framebuffer  | Packed 4 bpp → 1200 × 1600 / 2 ≈ **937 kB** in PSRAM    |
| Refresh      | Full-color update ≈ **40 s**                            |

Spectra 6 is E Ink's six-color "Gallery-type" ePaper: every pixel can only be
one of the six palette colors, so each generated image is reduced to exactly
those colors before it is sent to the panel. 

### Interface and pin map

The ESP32-S3 talks to the panel over SPI using two chip-selects (dual-chip
T133A01) and a power-enable rail. The complete pin assignment lives in
`include/pins.h`:

| Function         | GPIO        | Notes                                       |
| ---------------- | ----------- | ------------------------------------------- |
| EPD SCK          | 7           | SPI clock (shared with SD)                  |
| EPD MISO         | 8           | SPI MISO (shared with SD)                   |
| EPD MOSI         | 9           | SPI MOSI (shared with SD)                   |
| EPD CS           | 10          | Chip select, driver 1                       |
| EPD DC           | 11          | Data / command                              |
| EPD EN           | 12          | Panel power enable                          |
| EPD BUSY         | 13          | BUSY = HIGH means "ready" (T133A01)         |
| EPD CS2          | 2           | Chip select, driver 2                       |
| EPD RST          | 38          | Panel reset                                 |
| SD CS / DET / EN | 14 / 15 / 16| microSD card, shares SPI with the display   |
| I²C0 SDA / SCL   | 19 / 20     | PCF8563 RTC + SHT40 sensor                  |
| Battery ADC / EN | 1 / 21      | LiPo voltage through a ×2 divider           |
| KEY2 (wake)      | 5           | Front-bezel refresh button                  |
| Buzzer           | 45          | Confirmation beep on KEY2 wake              |
| Logging TX / RX  | 43 / 44     | Carrier USB-UART bridge, 115200 baud        |

### Onboard companions

- **PCF8563 real-time clock** (I²C, address `0x51`) keeps the time through
  deep sleep and is re-synced from NTP on every wake.
- **MicroSD slot** (shared SPI) buffers the streamed OpenAI response and the
  decoded PNG while the image moves through the pipeline.
- **Battery gauge** — LiPo voltage on GPIO1 through a ×2 divider, converted to
  a charge percentage via a lookup table; the network cycle is skipped below
  `BAT_LOW_PERCENT`.
- **KEY2 button + buzzer** on the front bezel for on-demand refresh.

### What the firmware needs from the hardware

- **OPI PSRAM is mandatory.** The ePaper framebuffer (~937 kB) and the image
  pipeline buffers (several MB) are allocated in PSRAM; the build forces
  `qio_opi` memory and the 8 MB partition table (`platformio.ini`).
- A **microSD card (FAT32)** must be inserted — the pipeline streams its temp
  files through `/daydream/` on the card.
- A **LiPo battery** (or 3.7–4.2 V supply).

## Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) (the **espressif32** platform and all
  libraries are resolved automatically on the first build).
- A 2.4 GHz Wi-Fi network — the ESP32-S3 has no 5 GHz radio.
- A **microSD card** formatted as FAT32.
- Two API keys: **OpenAI** and **OpenWeather** (see below).

### 1. Get the code

```bash
git clone https://github.com/maet3608/reTerminal-E1004-ai-image
cd reTerminal-E1004-ai-image
```

### 2. Configure credentials

Copy the template to a real (gitignored) file and fill in your values:

```bash
cp include/credentials.example.h include/credentials.h
```

On Windows: `copy include\credentials.example.h include\credentials.h`

`include/credentials.h` is listed in `.gitignore` and is **never committed**.
It holds exactly four values:

| Constant              | Where to get it                                                       |
| --------------------- | --------------------------------------------------------------------- |
| `WIFI_SSID`           | Your 2.4 GHz Wi-Fi network name                                       |
| `WIFI_PASSWORD`       | Your Wi-Fi password                                                   |
| `OPENAI_API_KEY`      | https://platform.openai.com/api-keys  (secret key, starts with `sk-`) |
| `OPENWEATHER_API_KEY` | https://home.openweathermap.org/api_keys  (free tier is sufficient)   |

> **OpenAI:** `gpt-image-1` is a *paid* model — the account needs a billing
> method and sufficient credits, otherwise the cycle fails with `ERR_AI`.
>
> **OpenWeather:** the free "Current weather and forecasts" subscription is
> enough; a freshly created key can take a few minutes to become active.

### 3. Adjust `include/config.h`

Set your location, timezone and wake hour. All constants are documented in the
[Configuration reference](#configuration-reference-includeconfigh) below; the
ones you will usually touch first are:

- `WX_LOCATION` — `"City,CC"` as understood by OpenWeather, e.g. `"Lohr am Main,DE"`.
- `TIMEZONE` — POSIX TZ string, e.g. `"CET-1CEST,M3.5.0,M10.5.0/3"`.
- `REFRESH_HOUR` — daily wake hour in local time (default `1` → 01:00).
- `THEME` / `THEMES[7]` / `USE_WEEKDAY_THEMES` — artwork style(s).
- `ORIENTATION` — `ORIENT_LANDSCAPE` (default) or `ORIENT_PORTRAIT`.
- `DATE_FORMAT` — `strftime` format for the date inside the artwork.

### 4. Build, upload, monitor

```bash
pio run                       # compile
pio run -t upload             # flash over USB
pio device monitor            # logs on the carrier USB-UART bridge, 115200 baud
```

After the first boot the panel should show an image within a few minutes
(NTP sync, weather and image generation dominate). All diagnostics appear on
the serial monitor with tags such as `[RTC]`, `[WIFI]`, `[WX]`, `[AI]`,
`[PIPE]`, `[EPD]`, `[BAT]`, `[OK]` and `[ERR]`.

### Troubleshooting

| Symptom                     | Likely cause / fix                                                |
| --------------------------- | ----------------------------------------------------------------- |
| `ERR_WIFI` screen           | Wrong SSID/password, 5 GHz-only network, or `WIFI_TIMEOUT_MS` too short. |
| `ERR_AI` screen             | OpenAI account has no billing/credits, or the API key is invalid. |
| `ERR_WEATHER` screen        | Wrong OpenWeather key or `WX_LOCATION`; key still activating.     |
| `ERR_SD` screen             | No microSD card inserted, or card is not FAT32.                   |
| `ERR_LOW_BATTERY` screen    | Charge the battery above `BAT_LOW_PERCENT`.                       |
| `ERR_TIME` screen           | RTC empty *and* NTP unreachable — check Wi-Fi and `NTP_SERVER`.   |
| Wrong image aspect / crop   | Check `ORIENTATION` vs. `AI_SIZE` and `RESIZE_MODE` in `config.h`.|

## Configuration reference (`include/config.h`)

`include/config.h` is the single place for all tunables; secrets stay in
`include/credentials.h`. Every value is a `static const` compiled into the
firmware, so a change requires a rebuild (`pio run -t upload`).

### Weather

| Constant            | Default              | Meaning                                                                 |
| ------------------- | -------------------- | ----------------------------------------------------------------------- |
| `WX_LOCATION`       | `"Lohr am Main,DE"`  | OpenWeather location query, format `"City,CC"` (city + 2-letter country code). |
| `WX_UNITS`          | `"metric"`           | `metric` → °C / m/s; `imperial` → °F / mph.                              |
| `WX_TARGET_HOUR`    | `12`                 | Hour (local time) whose 3-hourly forecast entry is used — noon by default. |
| `WX_MAX_NOON_DELTA` | `6L * 3600L` (21600 s) | Max allowed distance to the target hour; if no forecast entry is within ±6 h, the *current* weather is used as fallback. |

### Time / date

| Constant       | Default                        | Meaning                                                                   |
| -------------- | ------------------------------ | ------------------------------------------------------------------------- |
| `TIMEZONE`     | `"CET-1CEST,M3.5.0,M10.5.0/3"` | POSIX TZ string — CET (UTC+1) with DST from the last Sunday of March until the last Sunday of October at 03:00. |
| `NTP_SERVER`   | `"pool.ntp.org"`               | NTP server used to correct RTC drift once Wi-Fi is connected.             |
| `NTP_ENABLED`  | `true`                         | Set `false` to skip NTP entirely and rely on the RTC alone.                |
| `REFRESH_HOUR` | `1`                            | Daily wake-up hour, 0–23, local time.                                      |
| `DATE_FORMAT`  | `"%d %B %Y"`                   | `strftime` format for the date drawn into the artwork (e.g. `"1 June 2026"`). |

### Themes

| Constant             | Default        | Meaning                                                              |
| -------------------- | -------------- | -------------------------------------------------------------------- |
| `USE_WEEKDAY_THEMES` | `true`         | When `true`, the theme depends only on the weekday via `THEMES[7]`; when `false`, `THEME` is used every day. |
| `THEME`              | `"Vintage botanical illustration"` | Fixed style / fallback when weekday themes are disabled. |
| `THEMES[7]`          | (7 styles)     | One style per weekday, indexed by `tm_wday` (0 = Sunday … 6 = Saturday): Kandinsky, Ukiyo-e, anime, botanical, steampunk, Victorian city, Mondrian. |

### Battery

| Constant              | Default | Meaning                                                          |
| --------------------- | ------- | ---------------------------------------------------------------- |
| `BAT_VOLTAGE_DIVIDER` | `2.0f`  | Multiplier from raw ADC millivolts to battery voltage (hardware divider = 2). |
| `BAT_LOW_PERCENT`     | `5`     | Below this charge % the network cycle is skipped and a "charge me" screen is shown. |

### Orientation

| Constant      | Default             | Meaning                                                                 |
| ------------- | ------------------- | ----------------------------------------------------------------------- |
| `ORIENTATION` | `ORIENT_LANDSCAPE`  | `ORIENT_PORTRAIT` uses the panel natively (1200 × 1600); `ORIENT_LANDSCAPE` renders a 1600 × 1200 frame rotated 90° onto the physical panel. |

### AI image (OpenAI)

| Constant             | Default       | Meaning                                                                    |
| -------------------- | ------------- | -------------------------------------------------------------------------- |
| `AI_MODEL`           | `"gpt-image-1"` | OpenAI image model. `gpt-image-1` accepts non-square sizes; `gpt-image-1-mini` is 1024 × 1024 only. |
| `AI_SIZE`            | `"1536x1024"` | Chosen automatically from `ORIENTATION` (landscape 1536 × 1024, portrait 1024 × 1536) so less cropping is needed. |
| `AI_QUALITY`         | `""` (empty)  | Optional: `low` / `medium` / `high` for `gpt-image-1`; empty = model default (medium). |
| `AI_TIMEOUT_MS`      | `120000`      | Per-request read timeout — `gpt-image-1` can take a while.                 |
| `AI_MAX_ATTEMPTS`    | `3`           | Retry count for transient API failures.                                      |
| `AI_RETRY_DELAY_MS`  | `3000`        | Pause between attempts.                                                      |
| `AI_DEBUG_DUMP`      | `false`       | When `true`, logs the head of the OpenAI JSON response (for `b64_json` parsing issues). |

### Image pipeline / display

| Constant                  | Default        | Meaning                                                                 |
| ------------------------- | -------------- | ----------------------------------------------------------------------- |
| `RESIZE_MODE`             | `RESIZE_COVER` | `RESIZE_COVER` scales to fill and center-crops; `RESIZE_CONTAIN` fits the whole image with white letterbox bars; `RESIZE_ORIGINAL` renders 1:1 centered (crops edges if larger). |
| `PANEL_W` / `PANEL_H`     | `1200` / `1600`| Physical panel resolution, fixed by the Spectra 6 hardware.              |
| `RENDER_W` / `RENDER_H`   | derived        | Logical render resolution from `ORIENTATION` (landscape → 1600 × 1200).  |
| `DISPLAY_INIT_TIMEOUT_MS` | `2000`         | Max time to wait after reset for the T133A01 to deassert BUSY (BUSY = HIGH means ready). |

### SD card temp files

| Constant      | Default                        | Meaning                                                     |
| ------------- | ------------------------------ | ----------------------------------------------------------- |
| `SD_TMP_RESP` | `"/daydream/tmp_resp.json"`    | Streaming buffer for the raw OpenAI JSON response.          |
| `SD_TMP_PNG`  | `"/daydream/tmp_image.png"`    | Decoded base64 PNG payload, read back by the renderer.      |

Both files are deleted after each successful/failed run and before deep sleep.

### Misc

| Constant          | Default | Meaning                                    |
| ----------------- | ------- | ------------------------------------------ |
| `WIFI_TIMEOUT_MS` | `30000` | Max time to wait for Wi-Fi association.    |

