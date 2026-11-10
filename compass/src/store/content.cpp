#include "content.h"

#include <LittleFS.h>

#include "../model/content.h"

namespace store {
namespace {

const char* kDir = "/c";
bool mounted = false;

String pathFor(const String& slug, ContentKind kind) {
  return String(kDir) + "/" + model::contentKey(slug) + (kind == ContentKind::Art ? ".jpg" : ".json");
}

}  // namespace

bool contentBegin() {
  if (mounted) return true;
  // The partition is labelled "littlefs" in both partition tables.
  if (!LittleFS.begin(true, "/littlefs", 8, "littlefs")) return false;
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  mounted = true;
  return true;
}

bool contentHas(const String& slug, ContentKind kind) {
  if (slug.isEmpty() || !contentBegin()) return false;
  return LittleFS.exists(pathFor(slug, kind));
}

bool contentWrite(const String& slug, ContentKind kind, const uint8_t* data, size_t length) {
  if (slug.isEmpty() || !data || length == 0 || !contentBegin()) return false;
  const String path = pathFor(slug, kind);
  const String temp = path + ".tmp";
  File file = LittleFS.open(temp, "w");
  if (!file) return false;
  const size_t written = file.write(data, length);
  file.close();
  // A half-written file never takes the place of a whole one.
  if (written != length) {
    LittleFS.remove(temp);
    return false;
  }
  LittleFS.remove(path);
  return LittleFS.rename(temp, path);
}

uint8_t* contentRead(const String& slug, ContentKind kind, size_t& length) {
  length = 0;
  if (slug.isEmpty() || !contentBegin()) return nullptr;
  File file = LittleFS.open(pathFor(slug, kind), "r");
  if (!file) return nullptr;
  const size_t size = file.size();
  if (size == 0 || size > 512 * 1024) {
    file.close();
    return nullptr;
  }
  uint8_t* buffer = static_cast<uint8_t*>(malloc(size));
  if (!buffer) {
    file.close();
    return nullptr;
  }
  const size_t read = file.read(buffer, size);
  file.close();
  if (read != size) {
    free(buffer);
    return nullptr;
  }
  length = size;
  return buffer;
}

void contentRemove(const String& slug, ContentKind kind) {
  if (slug.isEmpty() || !contentBegin()) return;
  LittleFS.remove(pathFor(slug, kind));
}

void contentPrune(const std::vector<String>& keep) {
  if (!contentBegin()) return;
  std::vector<String> wanted;
  for (const String& slug : keep) wanted.push_back(model::contentKey(slug));
  std::vector<String> gone;
  File dir = LittleFS.open(kDir);
  if (!dir || !dir.isDirectory()) return;
  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    const String name = String(entry.name());
    // "0123456789abcdef.jpg": the key is everything before the first dot.
    const int dot = name.indexOf('.');
    const String key = dot > 0 ? name.substring(0, dot) : name;
    bool kept = false;
    for (const String& w : wanted) kept = kept || w == key;
    // A leftover ".tmp" from an interrupted write goes too.
    if (!kept || name.endsWith(".tmp")) gone.push_back(String(kDir) + "/" + name);
  }
  dir.close();
  for (const String& path : gone) LittleFS.remove(path);
}

void contentWipe() {
  contentPrune(std::vector<String>());
}

}  // namespace store
