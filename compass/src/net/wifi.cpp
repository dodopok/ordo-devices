#include "wifi.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "../app_config.h"
#include "../sys/rtc_state.h"
#include "portal_page.h"

namespace net {
namespace {

enum class Phase { Idle, Fast, Scanning, Joining };

store::Settings* settings = nullptr;
WifiState state = WifiState::Off;
Phase phase = Phase::Idle;
uint32_t phaseStartedMs = 0;

PortalState portal = PortalState::Off;
String apSsid;
String targetSsid;
String targetPass;
uint32_t portalStartedMs = 0;
uint32_t portalJoinMs = 0;
DNSServer dns;
WebServer server(80);
bool serverRoutes = false;

const store::WifiNetwork* known(const String& ssid) {
  if (!settings) return nullptr;
  for (const auto& network : settings->networks()) {
    if (network.ssid == ssid) return &network;
  }
  return nullptr;
}

void rememberAccessPoint() {
  sys::RtcState& r = sys::rtc();
  const String ssid = WiFi.SSID();
  strlcpy(r.ssid, ssid.c_str(), sizeof(r.ssid));
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid) memcpy(r.bssid, bssid, 6);
  r.channel = static_cast<uint8_t>(WiFi.channel());
  const int rssi = WiFi.RSSI();
  r.rssi = static_cast<int8_t>(rssi < -127 ? -127 : (rssi >= 0 ? -1 : rssi));
}

void startScan() {
  WiFi.scanDelete();
  // 110 ms a channel: a little under two seconds for the 2.4 GHz band.
  WiFi.scanNetworks(true, false, false, 110);
  phase = Phase::Scanning;
  phaseStartedMs = millis();
}

void failed() {
  state = WifiState::Failed;
  phase = Phase::Idle;
  if (portal == PortalState::Off) WiFi.mode(WIFI_OFF);
}

void stepStation() {
  if (state != WifiState::Connecting) return;
  const uint32_t now = millis();
  switch (phase) {
    case Phase::Fast:
      if (WiFi.status() == WL_CONNECTED) {
        state = WifiState::Connected;
        phase = Phase::Idle;
      } else if (now - phaseStartedMs > cfg::kWifiFastConnectMs) {
        // The access point moved channel, or we are somewhere else.
        WiFi.disconnect();
        sys::rtc().channel = 0;
        startScan();
      }
      break;
    case Phase::Scanning: {
      const int16_t found = WiFi.scanComplete();
      if (found == WIFI_SCAN_RUNNING) {
        if (now - phaseStartedMs > 8000) failed();
        return;
      }
      int best = -1;
      for (int i = 0; i < found; i++) {
        if (!known(WiFi.SSID(i))) continue;
        if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
      }
      if (best < 0) {
        WiFi.scanDelete();
        failed();
        return;
      }
      const store::WifiNetwork* network = known(WiFi.SSID(best));
      WiFi.begin(network->ssid.c_str(), network->pass.c_str(), WiFi.channel(best), WiFi.BSSID(best));
      WiFi.scanDelete();
      phase = Phase::Joining;
      phaseStartedMs = now;
      break;
    }
    case Phase::Joining:
      if (WiFi.status() == WL_CONNECTED) {
        state = WifiState::Connected;
        phase = Phase::Idle;
        rememberAccessPoint();
      } else if (now - phaseStartedMs > cfg::kWifiConnectMs) {
        WiFi.disconnect();
        failed();
      }
      break;
    case Phase::Idle:
      break;
  }
}

// ----------------------------------------------------------- setup page

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

void sendJson(const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

void handleNetworks() {
  const bool joining = portal == PortalState::Connecting;
  int16_t found = WiFi.scanComplete();
  if (!joining && (found == WIFI_SCAN_FAILED || (server.hasArg("rescan") && found != WIFI_SCAN_RUNNING))) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 110);
    found = WIFI_SCAN_RUNNING;
  }
  String body = "{\"scanning\":";
  body += found == WIFI_SCAN_RUNNING ? "true" : "false";
  body += ",\"networks\":[";
  if (found > 0) {
    // Strongest first, each name once, hidden networks left out.
    std::vector<int> order;
    for (int i = 0; i < found; i++) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      bool seen = false;
      for (int j : order) {
        if (WiFi.SSID(j) == ssid) {
          seen = true;
          break;
        }
      }
      if (!seen) order.push_back(i);
    }
    for (size_t a = 0; a < order.size(); a++) {
      for (size_t b = a + 1; b < order.size(); b++) {
        if (WiFi.RSSI(order[b]) > WiFi.RSSI(order[a])) std::swap(order[a], order[b]);
      }
    }
    for (size_t k = 0; k < order.size() && k < 20; k++) {
      const int i = order[k];
      if (k) body += ',';
      body += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"secure\":";
      body += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
      body += '}';
    }
  }
  body += "]}";
  sendJson(body);
}

void handleConnect() {
  const String ssid = server.arg("ssid");
  const String pass = server.arg("pass");
  if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63 || (!pass.isEmpty() && pass.length() < 8)) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  targetSsid = ssid;
  targetPass = pass;
  WiFi.scanDelete();
  WiFi.disconnect();
  WiFi.begin(targetSsid.c_str(), targetPass.isEmpty() ? nullptr : targetPass.c_str());
  portal = PortalState::Connecting;
  portalJoinMs = millis();
  sendJson("{\"ok\":true}");
}

void handleStatus() {
  const char* name = "waiting";
  if (portal == PortalState::Connecting) name = "connecting";
  else if (portal == PortalState::Connected) name = "connected";
  else if (portal == PortalState::Failed) name = "failed";
  sendJson(String("{\"state\":\"") + name + "\",\"ssid\":\"" + jsonEscape(targetSsid) + "\"}");
}

void handlePage() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", ORDO_PORTAL_PAGE);
}

// Phones probe a handful of well-known URLs to decide whether a network is a
// captive portal; answering all of them with a redirect to the page is what
// makes it open by itself.
void handleAnythingElse() {
  if (server.uri() == "/favicon.ico") {
    server.send(404, "text/plain", "");
    return;
  }
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void stepPortal() {
  if (portal == PortalState::Off) return;
  dns.processNextRequest();
  server.handleClient();
  const uint32_t now = millis();

  if (portal == PortalState::WaitingPhone && WiFi.softAPgetStationNum() > 0) {
    portal = PortalState::PhoneConnected;
  } else if (portal == PortalState::PhoneConnected && WiFi.softAPgetStationNum() == 0) {
    portal = PortalState::WaitingPhone;
  } else if (portal == PortalState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      if (settings) settings->addNetwork(targetSsid, targetPass);
      targetPass = "";
      rememberAccessPoint();
      state = WifiState::Connected;
      phase = Phase::Idle;
      portal = PortalState::Connected;
    } else if (now - portalJoinMs > 20000) {
      WiFi.disconnect();
      targetPass = "";
      portal = PortalState::Failed;
    }
  }
  // Nobody came: close the open network rather than leave it up.
  if (portal != PortalState::Connected && now - portalStartedMs > cfg::kPortalTimeoutMs && WiFi.softAPgetStationNum() == 0) {
    portalStop();
  }
}

}  // namespace

void wifiBegin(store::Settings* s) {
  settings = s;
  // The network's name carries the last two bytes of the factory MAC, read
  // from eFuse: WiFi.macAddress() answers zeros until the radio is started,
  // which is how the first firmware came to call itself "Ordo-0000".
  const uint64_t mac = ESP.getEfuseMac();
  char buf[16];
  snprintf(buf, sizeof(buf), "Ordo-%02X%02X", static_cast<unsigned>((mac >> 32) & 0xFF), static_cast<unsigned>((mac >> 40) & 0xFF));
  apSsid = buf;

  if (settings && settings->networks().empty()) {
    // Firmware 0.1 let WiFiManager keep the network inside the Wi-Fi driver's
    // own storage. Adopt it, so an upgrade does not ask for the Wi-Fi again.
    WiFi.mode(WIFI_STA);
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK && conf.sta.ssid[0] != 0) {
      settings->addNetwork(String(reinterpret_cast<const char*>(conf.sta.ssid)), String(reinterpret_cast<const char*>(conf.sta.password)));
    }
    WiFi.mode(WIFI_OFF);
  }
}

bool wifiConfigured() { return settings && !settings->networks().empty(); }

void wifiConnect() {
  if (state == WifiState::Connecting || state == WifiState::Connected) return;
  if (!wifiConfigured()) {
    state = WifiState::Failed;
    return;
  }
  WiFi.mode(portal == PortalState::Off ? WIFI_STA : WIFI_AP_STA);
  WiFi.setHostname("ordo-compass");
  WiFi.setSleep(true);
  state = WifiState::Connecting;

  const sys::RtcState& r = sys::rtc();
  const store::WifiNetwork* last = r.channel ? known(String(r.ssid)) : nullptr;
  if (last) {
    WiFi.begin(last->ssid.c_str(), last->pass.c_str(), r.channel, r.bssid);
    phase = Phase::Fast;
    phaseStartedMs = millis();
  } else {
    startScan();
  }
}

void wifiOff() {
  if (portal != PortalState::Off) return;
  if (state == WifiState::Off && WiFi.getMode() == WIFI_OFF) return;
  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  state = WifiState::Off;
  phase = Phase::Idle;
}

WifiState wifiState() {
  if (state == WifiState::Connected && WiFi.status() != WL_CONNECTED) state = WifiState::Failed;
  return state;
}

bool wifiConnected() { return wifiState() == WifiState::Connected; }

void wifiForgetAll() {
  portalStop();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  state = WifiState::Off;
  sys::rtc().channel = 0;
}

void wifiLoop() {
  stepStation();
  stepPortal();
}

void portalStart() {
  if (portal != PortalState::Off) return;
  state = WifiState::Off;
  phase = Phase::Idle;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid.c_str());
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  if (!serverRoutes) {
    server.on("/", HTTP_GET, handlePage);
    server.on("/api/networks", HTTP_GET, handleNetworks);
    server.on("/api/connect", HTTP_POST, handleConnect);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.onNotFound(handleAnythingElse);
    serverRoutes = true;
  }
  server.begin();
  targetSsid = "";
  targetPass = "";
  portal = PortalState::WaitingPhone;
  portalStartedMs = millis();
  WiFi.scanNetworks(true, false, false, 110);
}

void portalStop() {
  if (portal == PortalState::Off) return;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.scanDelete();
  portal = PortalState::Off;
  targetPass = "";
  if (state == WifiState::Connected && WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
  } else {
    WiFi.mode(WIFI_OFF);
    state = WifiState::Off;
  }
}

PortalState portalState() { return portal; }

const String& portalSsid() { return apSsid; }

const String& portalNetwork() { return targetSsid; }

}  // namespace net
