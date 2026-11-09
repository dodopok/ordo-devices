#include "today.h"

#include <ArduinoJson.h>

namespace model {
namespace {

const char* kOfficeKeys[4] = {"morning", "midday", "evening", "compline"};

// Howard Hinnant's days-from-civil, so the ring needs no RTC and no <time.h>.
long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

void readList(JsonArrayConst array, std::vector<Reading>& out) {
  out.clear();
  for (JsonObjectConst item : array) {
    Reading reading;
    reading.slot = item["slot"].as<String>();
    reading.reference = item["reference"].as<String>();
    if (!reading.reference.isEmpty()) out.push_back(reading);
  }
}

float fractionOf(long day, long from, long span) {
  if (span <= 0) return 0;
  float f = static_cast<float>(day - from) / static_cast<float>(span);
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return f;
}

}  // namespace

long daysFromIsoDate(const char* iso) {
  if (!iso || strlen(iso) < 10) return -1;
  int y = 0;
  unsigned m = 0;
  unsigned d = 0;
  if (sscanf(iso, "%4d-%2u-%2u", &y, &m, &d) != 3) return -1;
  if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
  return daysFromCivil(y, m, d);
}

lv_color_t colorFromHex(const char* hex, bool* ok) {
  if (ok) *ok = false;
  if (!hex || hex[0] != '#' || strlen(hex) != 7) return lv_color_hex(0x8C8F8A);
  char* end = nullptr;
  const unsigned long value = strtoul(hex + 1, &end, 16);
  if (!end || *end != '\0') return lv_color_hex(0x8C8F8A);
  if (ok) *ok = true;
  return lv_color_hex(static_cast<uint32_t>(value));
}

int Today::nextOfficeIndex() const {
  for (int i = 0; i < 4; i++) {
    if (!offices[i].done) return i;
  }
  return -1;
}

bool parseToday(const String& json, Today& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

  Today today;
  today.date = doc["date"].as<String>();
  today.language = doc["language"].as<String>();

  JsonObjectConst day = doc["day"];
  today.weekday = day["weekday"].as<String>();
  today.title = day["title"].as<String>();
  today.season = day["season"].as<String>();
  today.colorKey = day["color"].as<String>();
  today.color = colorFromHex(day["color_hex"], &today.hasColor);
  today.isSunday = day["is_sunday"] | false;
  today.fastDay = day["fast_day"] | false;

  JsonObjectConst readings = doc["readings"];
  readList(readings["daily"], today.daily);
  readList(readings["morning"], today.morning);
  readList(readings["evening"], today.evening);

  JsonObjectConst ring = doc["year_ring"];
  today.progress = ring["progress"] | 0.0f;
  const long ringFrom = daysFromIsoDate(ring["from"]);
  const long ringTo = daysFromIsoDate(ring["to"]);
  const long span = (ringFrom >= 0 && ringTo >= ringFrom) ? (ringTo - ringFrom + 1) : 0;
  for (JsonObjectConst item : ring["segments"].as<JsonArrayConst>()) {
    RingSegment segment;
    segment.season = item["season"].as<String>();
    segment.name = item["name"].as<String>();
    segment.color = colorFromHex(item["color_hex"]);
    const long from = daysFromIsoDate(item["from"]);
    const long to = daysFromIsoDate(item["to"]);
    if (from < 0 || to < 0 || span == 0) continue;
    segment.from = fractionOf(from, ringFrom, span);
    segment.to = fractionOf(to + 1, ringFrom, span);
    today.segments.push_back(segment);
  }
  for (JsonObjectConst item : ring["feasts"].as<JsonArrayConst>()) {
    Feast feast;
    feast.name = item["name"].as<String>();
    feast.color = colorFromHex(item["color_hex"]);
    const long at = daysFromIsoDate(item["date"]);
    if (at < 0 || span == 0) continue;
    feast.at = fractionOf(at, ringFrom, span) + 0.5f / span;
    today.feasts.push_back(feast);
  }

  JsonObjectConst offices = doc["offices"];
  today.completedToday = 0;
  for (int i = 0; i < 4; i++) {
    today.offices[i].done = offices[kOfficeKeys[i]]["done"] | false;
    if (today.offices[i].done) today.completedToday++;
  }

  today.streakCurrent = doc["streak"]["current"] | 0;
  today.streakLongest = doc["streak"]["longest"] | 0;
  today.refreshIntervalSeconds = doc["refresh"]["interval_seconds"] | 300;
  today.dayEndsInSeconds = doc["refresh"]["day_ends_in_seconds"] | 0;

  today.valid = !today.date.isEmpty();
  out = today;
  return today.valid;
}

}  // namespace model
