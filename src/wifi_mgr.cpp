#include "wifi_mgr.h"
#include "logging.h"
#include "credentials.h"
#include <WiFi.h>

bool wifiConnect(int timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < (unsigned long)timeoutMs) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    LOG.printf("[WIFI] connected, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  LOG.println("[WIFI] connect failed");
  return false;
}

void wifiDisconnect() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
