#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <functional>
#include <vector>

// bussola.oficio.app as the compass sees it: one HTTPS connection, kept open
// for the few requests of a sync (a picture, a text, the firmware manifest),
// since the handshake costs more radio time than any of them.
namespace net {

// What get() and stream() return when the site answered but the body was too
// large or was turned away half way (HTTPClient's own errors are -1 to -11).
constexpr int kSiteBodyRefused = -100;

class Site {
 public:
  Site();
  // GET `path` on the site. The body lands in `out`, which is emptied first;
  // more than `maxBytes` of it is a failure. Returns the HTTP status, or a
  // negative number when the site could not be reached.
  int get(const String& path, std::vector<uint8_t>& out, size_t maxBytes);
  // GET `path`, handing the body to `sink` as it arrives; for the firmware,
  // which does not fit in memory. `sink` returning false stops the transfer.
  // `received` is what was handed over.
  int stream(const String& path, const std::function<bool(const uint8_t*, size_t)>& sink, size_t& received);
  void close();

 private:
  bool open(const String& path);

  WiFiClientSecure client_;
  HTTPClient http_;
};

// True for the paths the compass may ask the site for: the firmware folder
// and the signed content paths the API hands out. Nothing else is requested,
// whatever a payload or a manifest says.
bool sitePathAllowed(const String& path);

}  // namespace net
