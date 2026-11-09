#include "settings.h"

#include <Preferences.h>

namespace store {
namespace {

const char* kNamespace = "ordo";
Preferences prefs;

String readString(const char* key) {
  if (!prefs.isKey(key)) return String();
  return prefs.getString(key, "");
}

}  // namespace

void Settings::begin() {
  prefs.begin(kNamespace, false);
  token_ = readString("token");
  name_ = readString("name");
  etag_ = readString("etag");
  payload_ = readString("payload");
}

void Settings::saveLink(const String& token, const String& name) {
  token_ = token;
  name_ = name;
  prefs.putString("token", token_);
  prefs.putString("name", name_);
}

void Settings::saveToday(const String& etag, const String& payload) {
  etag_ = etag;
  payload_ = payload;
  prefs.putString("etag", etag_);
  prefs.putString("payload", payload_);
}

void Settings::clearLink() {
  token_ = "";
  name_ = "";
  etag_ = "";
  payload_ = "";
  prefs.remove("token");
  prefs.remove("name");
  prefs.remove("etag");
  prefs.remove("payload");
}

void Settings::factoryReset() {
  clearLink();
  prefs.clear();
}

}  // namespace store
