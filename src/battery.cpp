#include "battery.h"
#include "config.h"
#include "logging.h"
#include "pins.h"

// LiPo discharge curve: voltage (mV) -> charge (%).
struct VoltagePoint {
  int mv;
  int pct;
};
static const VoltagePoint DISCHARGE_CURVE[] = {
    {4200, 100},
    {4030, 90},
    {3950, 80},
    {3870, 70},
    {3800, 60},
    {3740, 50},
    {3680, 40},
    {3610, 30},
    {3520, 20},
    {3430, 10},
    {3300, 0},
};
static const int CURVE_POINTS =
    sizeof(DISCHARGE_CURVE) / sizeof(DISCHARGE_CURVE[0]);

static int voltageToPercent(int mv) {
  if (mv >= DISCHARGE_CURVE[0].mv)
    return 100;
  if (mv <= DISCHARGE_CURVE[CURVE_POINTS - 1].mv)
    return 0;
  for (int i = 0; i < CURVE_POINTS - 1; i++) {
    const int vHi = DISCHARGE_CURVE[i].mv;
    const int vLo = DISCHARGE_CURVE[i + 1].mv;
    if (mv <= vHi && mv >= vLo) {
      const int pctHi = DISCHARGE_CURVE[i].pct;
      const int pctLo = DISCHARGE_CURVE[i + 1].pct;
      const float t = (float)(vHi - mv) / (float)(vHi - vLo);
      return pctHi - (int)(t * (pctHi - pctLo) + 0.5f);
    }
  }
  return 0;
}

int batteryReadMillivolts() {
  static bool adcConfigured = false;
  if (!adcConfigured) {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
    adcConfigured = true;
  }
  pinMode(PIN_BATTERY_EN, OUTPUT);
  digitalWrite(PIN_BATTERY_EN, HIGH);
  delay(10); // E1004 docs: add ~10 ms settle before analogRead
  const int mv = analogReadMilliVolts(PIN_BATTERY_ADC);
  digitalWrite(PIN_BATTERY_EN, LOW);
  return (int)((float)mv * BAT_VOLTAGE_DIVIDER);
}

int batteryReadPercent() {
  const int mv = batteryReadMillivolts();
  const int pct = voltageToPercent(mv);
  LOG.printf("[BAT] %dmV -> %d%%\n", mv, pct);
  return pct;
}
