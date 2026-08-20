#pragma once
// reTerminal E1004 pin map (see spec.md section 3.1)

// Logging (carrier USB-UART bridge)
#define PIN_SERIAL_TX 43
#define PIN_SERIAL_RX 44

// ePaper display (SPI, dual-chip T133A01)
#define PIN_EPD_SCK   7
#define PIN_EPD_MISO  8
#define PIN_EPD_MOSI  9
#define PIN_EPD_CS   10
#define PIN_EPD_DC   11
#define PIN_EPD_EN   12
#define PIN_EPD_BUSY 13
#define PIN_EPD_CS2   2
#define PIN_EPD_RST  38

// microSD (shares SPI bus with the display)
#define PIN_SD_SCK   7
#define PIN_SD_MISO  8
#define PIN_SD_MOSI  9
#define PIN_SD_CS   14
#define PIN_SD_DET  15
#define PIN_SD_EN   16

// I2C0 (PCF8563 RTC, SHT40)
#define PIN_I2C_SDA 19
#define PIN_I2C_SCL 20

// Battery monitoring
#define PIN_BATTERY_ADC  1
#define PIN_BATTERY_EN  21

// Deep-sleep wake button (front-bezel refresh, KEY2)
#define PIN_WAKE_BTN 5

// Onboard buzzer (audio confirmation feedback)
#define PIN_BUZZER 45
