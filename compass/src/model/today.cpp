#include "today.h"

#include <ArduinoJson.h>

namespace model {
namespace {

const char* kOfficeKeys[4] = {"morning", "midday", "evening", "compline"};

// Howard Hinnant's days-from-civil and its inverse: dates without <time.h>.
long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

String str(JsonVariantConst v) {
  const char* s = v | "";
  return String(s);
}

void readList(JsonArrayConst array, std::vector<Reading>& out) {
  out.clear();
  for (JsonObjectConst item : array) {
    Reading reading;
    reading.slot = str(item["slot"]);
    reading.reference = str(item["reference"]);
    if (!reading.reference.isEmpty()) out.push_back(reading);
  }
}

// A path on the content host, as the API signs them. Anything else (another
// host, another folder) is dropped here: the device asks that host for these
// paths and nothing more.
String contentPath(JsonVariantConst v) {
  const String path = str(v);
  if (!path.startsWith("/c/") || path.length() > 200) return String();
  for (size_t i = 0; i < path.length(); i++) {
    const char c = path[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' ||
                    c == '.' || c == '_' || c == '?' || c == '=' || c == '&';
    if (!ok) return String();
  }
  if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0) return String();
  return path;
}

void readCelebration(JsonObjectConst item, Celebration& out) {
  out = Celebration();
  if (item.isNull()) return;
  out.name = str(item["name"]);
  out.type = str(item["type"]);
  out.epithet = str(item["epithet"]);
  out.year = str(item["year"]);
  out.slug = str(item["slug"]);
  if (out.slug.isEmpty()) return;
  out.artPath = contentPath(item["content"]["art"]);
  out.textPath = contentPath(item["content"]["text"]);
}

void readDay(JsonObjectConst day, JsonObjectConst readings, DayInfo& out) {
  out.weekday = str(day["weekday"]);
  // `celebration` is null or an object: a feast or a commemoration names the
  // day; otherwise the week does ("Próprio 27").
  readCelebration(day["celebration"], out.celebration);
  out.title = out.celebration.present() ? out.celebration.name : str(day["title"]);
  // The short form fits under a day's name ("Jejum"); the full label is the
  // book's own sentence and can be a long one.
  out.observance = str(day["observance"]["short"]);
  if (out.observance.isEmpty()) out.observance = str(day["observance"]["label"]);
  out.season = str(day["season"]);
  out.color = colorFromHex(day["color_hex"] | "", &out.hasColor);
  out.isSunday = day["is_sunday"] | false;
  out.fastDay = day["fast_day"] | false;
  out.holyDay = day["is_holy_day"] | false;
  out.described = true;
  readList(readings["daily"], out.daily);
  readList(readings["morning"], out.morning);
  readList(readings["evening"], out.evening);
}

float fractionOf(long day, long from, long span) {
  if (span <= 0) return 0;
  float f = static_cast<float>(day - from) / static_cast<float>(span);
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return f;
}

const char* weekdayName(int weekday, const String& language) {
  static const char* pt[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
  static const char* en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char* es[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
  if (weekday < 0 || weekday > 6) return "";
  if (language.startsWith("en")) return en[weekday];
  if (language.startsWith("es")) return es[weekday];
  return pt[weekday];
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

void civilFromDays(long z, int& year, int& month, int& day) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long y = static_cast<long>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(y + (month <= 2));
}

String isoDateFromDays(long days) {
  int y = 0, m = 0, d = 0;
  civilFromDays(days, y, m, d);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
  return String(buf);
}

int weekdayOf(long days) {
  // 1970-01-01 was a Thursday.
  return static_cast<int>(((days % 7) + 11) % 7);
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

int Today::officeForMinute(int minuteOfDay) const {
  int index = 3;
  if (minuteOfDay < 11 * 60) index = 0;
  else if (minuteOfDay < 15 * 60) index = 1;
  else if (minuteOfDay < 21 * 60) index = 2;
  if (!officesKnown) return index;
  for (int i = index; i < 4; i++) {
    if (!offices[i].done) return i;
  }
  return -1;
}

bool parseToday(const char* json, Today& out) {
  if (!json || !*json) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

  Today today;
  today.language = str(doc["language"]);
  today.day.date = str(doc["date"]);
  today.day.days = daysFromIsoDate(today.day.date.c_str());
  readDay(doc["day"], doc["readings"], today.day);

  for (JsonObjectConst item : doc["upcoming"].as<JsonArrayConst>()) {
    DayInfo next;
    next.date = str(item["date"]);
    next.days = daysFromIsoDate(next.date.c_str());
    if (next.days < 0) continue;
    readDay(item["day"], item["readings"], next);
    today.upcoming.push_back(next);
  }

  JsonObjectConst ring = doc["year_ring"];
  today.progress = ring["progress"] | 0.0f;
  today.ringFrom = daysFromIsoDate(ring["from"] | "");
  today.ringTo = daysFromIsoDate(ring["to"] | "");
  const long span = (today.ringFrom >= 0 && today.ringTo >= today.ringFrom) ? (today.ringTo - today.ringFrom + 1) : 0;
  for (JsonObjectConst item : ring["segments"].as<JsonArrayConst>()) {
    RingSegment segment;
    segment.season = str(item["season"]);
    segment.name = str(item["name"]);
    segment.color = colorFromHex(item["color_hex"] | "");
    segment.fromDays = daysFromIsoDate(item["from"] | "");
    segment.toDays = daysFromIsoDate(item["to"] | "");
    if (segment.fromDays < 0 || segment.toDays < 0 || span == 0) continue;
    segment.from = fractionOf(segment.fromDays, today.ringFrom, span);
    segment.to = fractionOf(segment.toDays + 1, today.ringFrom, span);
    today.segments.push_back(segment);
  }
  for (JsonObjectConst item : ring["feasts"].as<JsonArrayConst>()) {
    Feast feast;
    feast.name = str(item["name"]);
    feast.color = colorFromHex(item["color_hex"] | "");
    feast.days = daysFromIsoDate(item["date"] | "");
    if (feast.days < 0 || span == 0) continue;
    feast.at = fractionOf(feast.days, today.ringFrom, span) + 0.5f / span;
    today.feasts.push_back(feast);
  }

  JsonObjectConst offices = doc["offices"];
  today.completedToday = 0;
  for (int i = 0; i < 4; i++) {
    today.offices[i].done = offices[kOfficeKeys[i]]["done"] | false;
    if (today.offices[i].done) today.completedToday++;
  }
  today.officesKnown = true;

  JsonObjectConst season = doc["season_progress"];
  if (!season.isNull()) {
    today.season.day = season["day"] | 0;
    today.season.days = season["days"] | 0;
    today.season.endsDays = daysFromIsoDate(season["ends_on"] | "");
    today.season.nextName = str(season["next"]["name"]);
    today.season.nextInDays = season["next"]["in_days"] | 0;
    today.season.known = today.season.day > 0 && today.season.days >= today.season.day;
  }

  for (JsonObjectConst item : doc["reminders"].as<JsonArrayConst>()) {
    const char* office = item["office"] | "";
    const char* time = item["time"] | "";
    int hour = -1, minute = -1;
    if (sscanf(time, "%2d:%2d", &hour, &minute) != 2 || hour < 0 || hour > 23 || minute < 0 || minute > 59) continue;
    for (int i = 0; i < 4; i++) {
      if (strcmp(office, kOfficeKeys[i]) != 0) continue;
      today.reminders[i] = static_cast<int16_t>(hour * 60 + minute);
      today.hasReminders = true;
    }
  }

  // Seven entries ending today; anything else is placed by its date.
  for (JsonObjectConst item : doc["week"].as<JsonArrayConst>()) {
    const long days = daysFromIsoDate(item["date"] | "");
    const long back = today.day.days - days;
    if (days < 0 || back < 0 || back > 6) continue;
    int completed = item["completed"] | 0;
    if (completed < 0) completed = 0;
    if (completed > 4) completed = 4;
    today.week[6 - back] = static_cast<int8_t>(completed);
    today.hasWeek = true;
  }

  today.streakCurrent = doc["streak"]["current"] | 0;
  today.streakLongest = doc["streak"]["longest"] | 0;
  today.refreshIntervalSeconds = doc["refresh"]["interval_seconds"] | 300;
  today.dayEndsInSeconds = doc["refresh"]["day_ends_in_seconds"] | 0;
  today.hasUtcOffset = !doc["utc_offset_seconds"].isNull();
  today.utcOffsetSeconds = doc["utc_offset_seconds"] | 0;

  today.valid = today.day.days >= 0;
  out = today;
  return today.valid;
}

Today viewFor(const Today& base, long days) {
  if (!base.valid || days == base.day.days || days < base.day.days) return base;

  Today view = base;
  view.officesKnown = false;
  view.completedToday = 0;
  for (auto& office : view.offices) office.done = false;

  // The week slides with the day: what was today is now some days back, and
  // nobody has said yet what was prayed since.
  const long ahead = days - base.day.days;
  for (int i = 0; i < 7; i++) {
    const long from = i + ahead;
    view.week[i] = from <= 6 ? base.week[from] : static_cast<int8_t>(-1);
  }
  view.hasWeek = base.hasWeek && ahead < 7;

  // So does the place in the season, while the season lasts.
  if (base.season.known && days <= base.season.endsDays) {
    view.season.day = base.season.day + static_cast<int>(ahead);
    view.season.nextInDays = base.season.nextInDays - static_cast<int>(ahead);
  } else {
    view.season = SeasonProgress();
  }
  const long span = (base.ringFrom >= 0 && base.ringTo >= base.ringFrom) ? (base.ringTo - base.ringFrom + 1) : 0;
  view.progress = fractionOf(days, base.ringFrom, span);

  // Described by the API: one of the upcoming days.
  for (size_t i = 0; i < base.upcoming.size(); i++) {
    if (base.upcoming[i].days != days) continue;
    view.day = base.upcoming[i];
    view.upcoming.assign(base.upcoming.begin() + static_cast<long>(i) + 1, base.upcoming.end());
    return view;
  }

  // Past the window: the ring still knows the season and its colour.
  view.upcoming.clear();
  DayInfo day;
  day.days = days;
  day.date = isoDateFromDays(days);
  day.weekday = weekdayName(weekdayOf(days), base.language);
  day.isSunday = weekdayOf(days) == 0;
  day.described = false;
  for (const auto& segment : base.segments) {
    if (days < segment.fromDays || days > segment.toDays) continue;
    day.season = segment.name;
    day.title = segment.name;
    day.color = segment.color;
    day.hasColor = true;
    break;
  }
  for (const auto& feast : base.feasts) {
    if (feast.days != days) continue;
    day.title = feast.name;
    day.color = feast.color;
    day.hasColor = true;
    break;
  }
  view.day = day;
  return view;
}

NextFeast nextFeast(const Today& today, long days) {
  NextFeast next;
  for (const auto& feast : today.feasts) {
    if (feast.days <= days) continue;
    if (next.found && feast.days - days >= next.inDays) continue;
    next.found = true;
    next.name = feast.name;
    next.color = feast.color;
    next.inDays = static_cast<int>(feast.days - days);
  }
  return next;
}

}  // namespace model
