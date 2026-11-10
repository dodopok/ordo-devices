#include "content.h"

#include <ArduinoJson.h>

namespace model {

bool parseCelebrationText(const char* json, size_t length, CelebrationText& out) {
  out = CelebrationText();
  if (!json || length == 0) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json, length) != DeserializationError::Ok) return false;
  if ((doc["v"] | 0) != 1) return false;
  out.title = String(doc["title"] | "");
  out.summary = String(doc["summary"] | "");
  out.more = doc["more"] | false;
  for (JsonObjectConst item : doc["body"].as<JsonArrayConst>()) {
    TextBlock block;
    const char* heading = item["h"] | "";
    const char* paragraph = item["p"] | "";
    block.heading = heading[0] != '\0';
    block.text = String(block.heading ? heading : paragraph);
    if (!block.text.isEmpty()) out.body.push_back(block);
  }
  out.found = !out.summary.isEmpty() || !out.body.empty();
  return out.found;
}

String contentKey(const String& slug) {
  // FNV-1a, 64 bits.
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (size_t i = 0; i < slug.length(); i++) {
    hash ^= static_cast<uint8_t>(slug[i]);
    hash *= 0x100000001b3ULL;
  }
  char buf[20];
  snprintf(buf, sizeof(buf), "%08lx%08lx", static_cast<unsigned long>(hash >> 32), static_cast<unsigned long>(hash & 0xffffffffULL));
  return String(buf);
}

namespace {

bool isHex(const String& text, size_t length) {
  if (text.length() != length) return false;
  for (size_t i = 0; i < length; i++) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool isVersion(const String& text) {
  int a = 0, b = 0, c = 0;
  char tail = 0;
  return text.length() <= 14 && sscanf(text.c_str(), "%d.%d.%d%c", &a, &b, &c, &tail) == 3 && a >= 0 && b >= 0 && c >= 0;
}

bool isFirmwarePath(const String& path) {
  if (!path.startsWith("/firmware/") || path.length() > 96 || path.length() < 16) return false;
  if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0) return false;
  for (size_t i = 0; i < path.length(); i++) {
    const char c = path[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '.' || c == '_')) return false;
  }
  return path.c_str()[path.length() - 4] == '.' && path.c_str()[path.length() - 3] == 'b' && path.c_str()[path.length() - 2] == 'i' &&
         path.c_str()[path.length() - 1] == 'n';
}

}  // namespace

bool parseManifest(const char* json, size_t length, const char* board, const String& language, FirmwareOffer& out) {
  out = FirmwareOffer();
  if (!json || length == 0 || !board) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json, length) != DeserializationError::Ok) return false;
  if ((doc["v"] | 0) != 1) return false;
  JsonObjectConst build = doc["boards"][board];
  if (build.isNull()) return false;

  FirmwareOffer offer;
  offer.version = String(doc["version"] | "");
  offer.path = String(build["path"] | "");
  offer.size = build["size"] | 0u;
  offer.sha256 = String(build["sha256"] | "");
  offer.signature = String(build["sig"] | "");
  const char* key = language.startsWith("en") ? "en" : (language.startsWith("es") ? "es" : "pt");
  const char* notes = doc["notes"][key] | "";
  if (!notes[0]) notes = doc["notes"]["pt"] | "";
  offer.notes = String(notes);

  if (!isVersion(offer.version) || !isFirmwarePath(offer.path) || !isHex(offer.sha256, 64)) return false;
  if (offer.size < 256u * 1024u || offer.size > 6u * 1024u * 1024u) return false;
  if (offer.signature.length() > 200 || offer.notes.length() > 400) return false;
  offer.found = true;
  out = offer;
  return true;
}

bool versionNewer(const char* candidate, const char* current) {
  int a[3] = {0, 0, 0};
  int b[3] = {0, 0, 0};
  char tail = 0;
  if (!candidate || !current) return false;
  if (sscanf(candidate, "%d.%d.%d%c", &a[0], &a[1], &a[2], &tail) != 3) return false;
  if (sscanf(current, "%d.%d.%d", &b[0], &b[1], &b[2]) != 3) return false;
  for (int i = 0; i < 3; i++) {
    if (a[i] < 0 || a[i] > 9999) return false;
    if (a[i] != b[i]) return a[i] > b[i];
  }
  return false;
}

}  // namespace model
