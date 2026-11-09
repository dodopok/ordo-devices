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
};

struct Feast {
  String name;
  lv_color_t color;
  float at = 0;  // fraction of the liturgical year
};

struct Office {
  bool done = false;
};

// GET /api/v1/device/today, as the screens use it. Strings stay in the
// book's own language; the API already localized them.
struct Today {
  bool valid = false;
  String date;       // "2026-11-09"
  String language;   // "pt-BR"
  String weekday;    // "Segunda-feira"
  String title;      // "Semana do Próprio 27"
  String season;     // "Tempo Comum"
  String colorKey;   // "green"
  lv_color_t color = lv_color_hex(0x66BB6A);
  bool hasColor = false;
  bool isSunday = false;
  bool fastDay = false;

  std::vector<Reading> daily;
  std::vector<Reading> morning;
  std::vector<Reading> evening;

  float progress = 0;
  std::vector<RingSegment> segments;
  std::vector<Feast> feasts;

  // morning, midday, evening, compline
  Office offices[4];
  int completedToday = 0;
  int streakCurrent = 0;
  int streakLongest = 0;

  uint32_t refreshIntervalSeconds = 300;
  uint32_t dayEndsInSeconds = 0;

  bool allDone() const { return completedToday >= 4 && offices[0].done && offices[1].done && offices[2].done && offices[3].done; }
  int nextOfficeIndex() const;
};

// Parses the API body. Returns false on malformed JSON; a payload missing
// optional parts still parses.
bool parseToday(const String& json, Today& out);

// "#66BB6A" -> color. Falls back to grey for anything it cannot read.
lv_color_t colorFromHex(const char* hex, bool* ok = nullptr);

// Days since 1970-01-01 for "YYYY-MM-DD"; -1 when the string is not a date.
long daysFromIsoDate(const char* iso);

}  // namespace model
