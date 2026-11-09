#include "site.h"

#include "../app_config.h"
#include "root_certs.h"

namespace net {
namespace {

// A Stream that only receives: HTTPClient::writeToStream undoes chunked
// transfer encoding and hands the body over through this.
class Sink : public Stream {
 public:
  explicit Sink(const std::function<bool(const uint8_t*, size_t)>& take) : take_(take) {}
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* buffer, size_t size) override {
    if (stopped_ || !take_(buffer, size)) {
      stopped_ = true;
      return 0;
    }
    total_ += size;
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t total() const { return total_; }
  bool stopped() const { return stopped_; }

 private:
  const std::function<bool(const uint8_t*, size_t)>& take_;
  size_t total_ = 0;
  bool stopped_ = false;
};

}  // namespace

bool sitePathAllowed(const String& path) {
  if (!(path.startsWith("/firmware/") || path.startsWith("/c/")) || path.length() > 220) return false;
  if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0) return false;
  for (size_t i = 0; i < path.length(); i++) {
    const char c = path[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '.' ||
                    c == '_' || c == '?' || c == '=' || c == '&';
    if (!ok) return false;
  }
  return true;
}

Site::Site() {
#ifdef ORDO_TLS_INSECURE
  client_.setInsecure();
#else
  client_.setCACert(ORDO_ROOT_CERTS);
#endif
  client_.setTimeout(cfg::kHttpTimeoutMs / 1000);
  http_.setConnectTimeout(cfg::kHttpTimeoutMs);
  http_.setTimeout(cfg::kHttpTimeoutMs);
  http_.setReuse(true);
  http_.setUserAgent(ORDO_USER_AGENT);
}

bool Site::open(const String& path) {
  if (!sitePathAllowed(path)) return false;
  return http_.begin(client_, String(ORDO_SITE_BASE_URL) + path);
}

int Site::get(const String& path, std::vector<uint8_t>& out, size_t maxBytes) {
  out.clear();
  if (!open(path)) return -1;
  const int code = http_.GET();
  if (code == HTTP_CODE_OK) {
    const int announced = http_.getSize();
    if (announced > 0 && static_cast<size_t>(announced) > maxBytes) {
      http_.end();
      return kSiteBodyRefused;
    }
    if (announced > 0) out.reserve(static_cast<size_t>(announced));
    const std::function<bool(const uint8_t*, size_t)> take = [&out, maxBytes](const uint8_t* data, size_t size) {
      if (out.size() + size > maxBytes) return false;
      out.insert(out.end(), data, data + size);
      return true;
    };
    Sink sink(take);
    const int written = http_.writeToStream(&sink);
    if (written < 0 || sink.stopped()) {
      out.clear();
      http_.end();
      return kSiteBodyRefused;
    }
  } else if (code > 0) {
    // Read the (short) error body away, so the connection can be used again.
    http_.getString();
  }
  http_.end();
  return code;
}

int Site::stream(const String& path, const std::function<bool(const uint8_t*, size_t)>& sink, size_t& received) {
  received = 0;
  if (!open(path)) return -1;
  const int code = http_.GET();
  if (code == HTTP_CODE_OK) {
    Sink into(sink);
    const int written = http_.writeToStream(&into);
    received = into.total();
    if (written < 0 || into.stopped()) {
      http_.end();
      client_.stop();
      return kSiteBodyRefused;
    }
  }
  http_.end();
  return code;
}

void Site::close() {
  http_.end();
  client_.stop();
}

}  // namespace net
