#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include <vector>

namespace model {

struct Reading {
  String slot;       // first_reading, psalm, psalm_alternative, second_reading, gospel
  String reference;  // "Sl 106.1-18"
};

struct RingSegment {
  String season;  // slug: advent, christmas, ...
  String name;
  lv_color_t color;
  float from = 0;  // fraction of the liturgical year, 0..1
  float to = 0;
  long fromDays = -1;  // days since 1970-01-01, inclusive
  long toDays = -1;
};

struct Feast {
  String name;
  lv_color_t color;
  float at = 0;  // fraction of the liturgical year
  long days = -1;
};

struct Office {
  bool done = false;
};

// One liturgical day as the API describes it: today, or one of the days of
// the `upcoming` window. Strings stay in the book's own language.
struct DayInfo {
  String date;     // "2026-11-09"
  long days = -1;  // days since 1970-01-01
  String weekday;  // "Segunda-feira"
  String title;    // "Semana do Próprio 27", or the feast's name
  String season;   // "Tempo Comum"
  lv_color_t color = lv_color_hex(0x66BB6A);
  bool hasColor = false;
  bool isSunday = false;
  bool fastDay = false;
  bool holyDay = false;
  // False when the API never described this day and it was worked out from
  // the year ring alone: season and colour are right, title and readings are
  // not known.
  bool described = true;

  std::vector<Reading> daily;
  std::vector<Reading> morning;
  std::vector<Reading> evening;
};

// GET /api/v1/device/today, as the screens use it.
struct Today {
  bool valid = false;
  String language;  // "pt-BR"
  DayInfo day;
  std::vector<DayInfo> upcoming;  // the next days, oldest first; empty on an older API

  long ringFrom = -1;  // days since 1970-01-01
  long ringTo = -1;
  float progress = 0;
  std::vector<RingSegment> segments;
  std::vector<Feast> feasts;

  // morning, midday, evening, compline
  Office offices[4];
  int completedToday = 0;
  int streakCurrent = 0;
  int streakLongest = 0;
  // False on a day the device rolled to by itself: nobody has told it yet
  // which offices were prayed.
  bool officesKnown = true;

  uint32_t refreshIntervalSeconds = 300;
  uint32_t dayEndsInSeconds = 0;
  bool hasUtcOffset = false;
  int32_t utcOffsetSeconds = 0;

  bool allDone() const {
    return officesKnown && offices[0].done && offices[1].done && offices[2].done && offices[3].done;
  }
  // The first office not prayed yet, or -1.
  int nextOfficeIndex() const;
  // The office the hour calls for (morning before 11h, midday until 15h,
  // evening until 21h, then compline), moved on to the next one not prayed.
  int officeForMinute(int minuteOfDay) const;
};

struct NextFeast {
  bool found = false;
  String name;
  lv_color_t color;
  int inDays = 0;
};

// Parses the API body. Returns false on malformed JSON; a payload missing
// optional parts still parses.
bool parseToday(const char* json, Today& out);

// The same payload seen from another local date: the device kept the time
// and the day turned without a network. Uses the `upcoming` window when the
// date is in it, the year ring otherwise.
Today viewFor(const Today& base, long days);

// The next feast marked on the ring after `days`.
NextFeast nextFeast(const Today& today, long days);

// "#66BB6A" -> color. Falls back to grey for anything it cannot read.
lv_color_t colorFromHex(const char* hex, bool* ok = nullptr);

// Days since 1970-01-01 for "YYYY-MM-DD"; -1 when the string is not a date.
long daysFromIsoDate(const char* iso);
// And back: writes "YYYY-MM-DD".
String isoDateFromDays(long days);
void civilFromDays(long days, int& year, int& month, int& day);
// 0 = Sunday.
int weekdayOf(long days);

}  // namespace model
