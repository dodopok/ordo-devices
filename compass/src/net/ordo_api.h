#pragma once

#include <Arduino.h>

namespace net {

// The device side of the link flow (RFC 8628) and the one read a device may
// make. Every call is synchronous and returns within cfg::kHttpTimeoutMs.

struct LinkStart {
  String deviceCode;              // secret, polled with, never shown
  String userCode;                // "KQ7-MD4", shown on screen
  String verificationUriComplete; // encoded in the QR
  uint32_t expiresIn = 600;
  uint32_t interval = 5;
};

enum class TokenPoll { Success, Pending, SlowDown, Expired, Denied, Invalid, NetworkError };

struct TokenResult {
  TokenPoll status = TokenPoll::NetworkError;
  String deviceToken;
  String deviceName;
};

enum class TodayStatus {
  Updated,
  NotModified,
  Unauthorized,     // token unknown, revoked or idle: link again
  PremiumRequired,  // owner's subscription lapsed: keep the link, say so
  ClientDisabled,
  RateLimited,
  NetworkError,
  ServerError
};

struct TodayResult {
  TodayStatus status = TodayStatus::NetworkError;
  String etag;
  String body;
};

class OrdoApi {
 public:
  bool startLink(LinkStart& out);
  TokenResult pollToken(const String& deviceCode);
  TodayResult fetchToday(const String& deviceToken, const String& etag);

  // The server's clock (UTC seconds) as of the last exchange that got an
  // answer, read from its `Date` header; 0 when there was none. This is how
  // a compass without a clock chip knows the time.
  int64_t serverTime() const { return serverTime_; }

 private:
  // One HTTPS exchange. `code` receives the HTTP status, or a negative
  // HTTPClient error. Headers listed in `collect` are available afterwards.
  bool exchange(const String& baseUrl, const char* method, const String& path, const String& body,
                const String& deviceToken, const String& etag, int& code, String& responseBody,
                String& responseEtag);

  int64_t serverTime_ = 0;
};

}  // namespace net
