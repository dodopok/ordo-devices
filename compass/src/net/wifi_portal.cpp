#include "wifi_portal.h"

#include <WiFi.h>
#include <WiFiManager.h>

namespace net {
namespace {

WiFiManager wm;
String portalSsid;
bool portalActive = false;

String makeSsid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[16];
  snprintf(buf, sizeof(buf), "Ordo-%02X%02X", mac[4], mac[5]);
  return String(buf);
}

}  // namespace

void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ordo-compass");
  portalSsid = makeSsid();

  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(0);  // stays open until a network is chosen
  wm.setConnectTimeout(20);
  wm.setConnectRetries(2);
  wm.setTitle("Ordo");
  wm.setDarkMode(true);

  // Returns false in non-blocking mode when the portal had to be opened.
  portalActive = !wm.autoConnect(portalSsid.c_str());
}

void wifiLoop() {
  if (portalActive) {
    wm.process();
    if (WiFi.status() == WL_CONNECTED) {
      portalActive = false;
    }
  }
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool wifiPortalActive() { return portalActive; }

String wifiPortalSsid() { return portalSsid; }

void wifiForget() { wm.resetSettings(); }

}  // namespace net
