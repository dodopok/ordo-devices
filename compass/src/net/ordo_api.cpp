#include "ordo_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../app_config.h"
#include "../sys/clock.h"
#include "root_certs.h"

namespace net {
namespace {

const char* kEtagHeader = "ETag";
const char* kDateHeader = "Date";

TokenPoll pollStatusFor(const String& error) {
  if (error == "authorization_pending") return TokenPoll::Pending;
  if (error == "slow_down") return TokenPoll::SlowDown;
  if (error == "expired_token") return TokenPoll::Expired;
  if (error == "access_denied") return TokenPoll::Denied;
  return TokenPoll::Invalid;
}

}  // namespace

bool OrdoApi::exchange(const String& baseUrl, const char* method, const String& path, const String& body,
                       const String& deviceToken, const String& etag, int& code, String& responseBody,
                       String& responseEtag) {
  WiFiClientSecure client;
#ifdef ORDO_TLS_INSECURE
  client.setInsecure();
#else
  client.setCACert(ORDO_ROOT_CERTS);
#endif
  client.setTimeout(cfg::kHttpTimeoutMs / 1000);

  HTTPClient http;
  http.setConnectTimeout(cfg::kHttpTimeoutMs);
  http.setTimeout(cfg::kHttpTimeoutMs);
  http.setReuse(false);
  http.setUserAgent(ORDO_USER_AGENT);
  if (!http.begin(client, baseUrl + path)) {
    code = -1;
    return false;
  }
  http.addHeader("Accept", "application/json");
  if (!body.isEmpty()) http.addHeader("Content-Type", "application/json");
  if (!deviceToken.isEmpty()) {
    http.addHeader("X-Ordo-Device-Token", deviceToken);
    http.addHeader("X-Ordo-Device-Firmware", ORDO_FIRMWARE_VERSION);
  }
  if (!etag.isEmpty()) http.addHeader("If-None-Match", etag);
  const char* collect[] = {kEtagHeader, kDateHeader};
  http.collectHeaders(collect, 2);

  code = (strcmp(method, "POST") == 0) ? http.POST(body) : http.GET();
  if (code > 0) {
    int64_t serverTime = 0;
    if (sys::parseHttpDate(http.header(kDateHeader).c_str(), serverTime)) serverTime_ = serverTime;
    responseEtag = http.header(kEtagHeader);
    responseBody = (code == HTTP_CODE_NOT_MODIFIED) ? String() : http.getString();
  }
  http.end();
  return code > 0;
}

bool OrdoApi::startLink(LinkStart& out) {
  int code = 0;
  String body;
  String etag;
  JsonDocument req;
  req["client_id"] = ORDO_DEVICE_CLIENT_ID;
  String payload;
  serializeJson(req, payload);

  if (!exchange(ORDO_API_BASE_URL, "POST", "/device_links", payload, "", "", code, body, etag)) {
    return false;
  }
  if (code != 201) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  out.deviceCode = doc["device_code"].as<String>();
  out.userCode = doc["user_code"].as<String>();
  out.verificationUriComplete = doc["verification_uri_complete"].as<String>();
  out.expiresIn = doc["expires_in"] | 600;
  out.interval = doc["interval"] | 5;
  return !out.deviceCode.isEmpty() && !out.userCode.isEmpty();
}

TokenResult OrdoApi::pollToken(const String& deviceCode) {
  TokenResult result;
  int code = 0;
  String body;
  String etag;
  JsonDocument req;
  req["device_code"] = deviceCode;
  String payload;
  serializeJson(req, payload);

  if (!exchange(ORDO_API_BASE_URL, "POST", "/device_links/token", payload, "", "", code, body, etag)) {
    return result;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    result.status = TokenPoll::Invalid;
    return result;
  }
  if (code == 200) {
    result.status = TokenPoll::Success;
    result.deviceToken = doc["device_token"].as<String>();
    result.deviceName = doc["device"]["name"].as<String>();
    if (result.deviceToken.isEmpty()) result.status = TokenPoll::Invalid;
    return result;
  }
  result.status = pollStatusFor(doc["error"].as<String>());
  return result;
}

TodayResult OrdoApi::fetchToday(const String& deviceToken, const String& etag) {
  TodayResult result;
  int code = 0;
  String body;
  String newEtag;

  bool reached = exchange(ORDO_API_BASE_URL, "GET", "/device/today", "", deviceToken, etag, code, body, newEtag);
  if (!reached) {
    // The API has a second hostname for exactly this moment.
    reached = exchange(ORDO_API_FALLBACK_URL, "GET", "/device/today", "", deviceToken, etag, code, body, newEtag);
  }
  if (!reached) return result;

  switch (code) {
    case 200:
      result.status = TodayStatus::Updated;
      result.etag = newEtag;
      result.body = body;
      break;
    case 304:
      result.status = TodayStatus::NotModified;
      break;
    case 401:
      result.status = TodayStatus::Unauthorized;
      break;
    case 403: {
      JsonDocument doc;
      deserializeJson(doc, body);
      const String errorCode = doc["code"].as<String>();
      result.status = (errorCode == "CLIENT_DISABLED") ? TodayStatus::ClientDisabled : TodayStatus::PremiumRequired;
      break;
    }
    case 429:
      result.status = TodayStatus::RateLimited;
      break;
    default:
      result.status = TodayStatus::ServerError;
  }
  return result;
}

}  // namespace net
