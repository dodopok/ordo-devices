// The day in detail: readings, the four offices, and the days ahead.

#include "ui_internal.h"

namespace ui::detail {
namespace {

void readingRow(lv_obj_t* parent, const char* label, const String& reference, int32_t y, uint32_t delayMs) {
  lv_obj_t* caption = text(parent, upper(label).c_str(), theme::tiny(), theme::dim(), y, 2, 300);
  lv_obj_t* ref = text(parent, reference.c_str(), theme::reading(), theme::fg(), y + 20, 0, 340);
  lv_label_set_long_mode(ref, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(ref, 30);
  riseIn(caption, delayMs);
  riseIn(ref, delayMs);
}

}  // namespace

void buildReadings(lv_obj_t* scr) {
  const model::DayInfo& day = S.day.day;
  const lv_color_t accent = day.hasColor ? day.color : theme::dim();

  ringArc(scr, 436, 4, 0.0f, 1.0f, accent, LV_OPA_90);
  int y = 0, m = 0, d = 0;
  model::civilFromDays(day.days, y, m, d);
  char header[48];
  snprintf(header, sizeof(header), "%s  ·  %d %s", tr("LEITURAS", "READINGS", "LECTURAS"), d, upper(monthShort(m)).c_str());
  text(scr, header, theme::tiny(), accent, 84, 3, 280);

  struct Row {
    const char* label;
    String ref;
  };
  std::vector<Row> rows;
  auto first = [](const std::vector<model::Reading>& list, const char* slot) -> String {
    for (const auto& r : list) {
      if (r.slot == slot) return r.reference;
    }
    return String();
  };
  // Books that give the morning and the evening their own psalm get a row
  // for each; where both offices share one, it is listed once with the rest.
  const String morningPsalm = first(day.morning, "psalm");
  const String eveningPsalm = first(day.evening, "psalm");
  const bool split = !morningPsalm.isEmpty() && !eveningPsalm.isEmpty() && morningPsalm != eveningPsalm;
  if (split) {
    rows.push_back({tr("Salmo da manhã", "Morning psalm", "Salmo de la mañana"), morningPsalm});
    rows.push_back({tr("Salmo da tarde", "Evening psalm", "Salmo de la tarde"), eveningPsalm});
  }
  const std::vector<model::Reading>& list = !day.daily.empty() ? day.daily : (!day.morning.empty() ? day.morning : day.evening);
  for (const auto& r : list) {
    if (split && r.slot == "psalm") continue;
    if (rows.size() >= 5) break;
    rows.push_back({slotName(r.slot), r.reference});
  }

  if (rows.empty()) {
    const char* why = day.described
                          ? tr("Sem leituras para hoje", "No readings today", "Sin lecturas hoy")
                          : tr("As leituras deste dia chegam na próxima sincronização",
                               "This day's readings arrive with the next sync",
                               "Las lecturas de este día llegan con la próxima sincronización");
    text(scr, why, theme::body(), theme::dim(), 200, 0, 300);
    return;
  }
  // Up to five rows, centred as a block.
  const int32_t rowHeight = 58;
  int32_t yPos = 233 - (static_cast<int32_t>(rows.size()) * rowHeight) / 2 + 12;
  if (yPos < 122) yPos = 122;
  uint32_t delayMs = 0;
  for (const auto& row : rows) {
    readingRow(scr, row.label, row.ref, yPos, delayMs);
    yPos += rowHeight;
    delayMs += 70;
  }
}

void buildOffices(lv_obj_t* scr) {
  const model::Today& t = S.day;
  const int next = S.status.clockValid ? t.officeForMinute(S.status.minuteOfDay) : t.nextOfficeIndex();
  const bool allDone = t.allDone();

  for (int i = 0; i < 4; i++) {
    const float gap = 0.02f;
    const float from = i * 0.25f + gap;
    const float to = (i + 1) * 0.25f - gap;
    const bool done = t.officesKnown && t.offices[i].done;
    const bool calling = !done && i == next && t.officesKnown;
    lv_obj_t* arc = ringArc(scr, 400, 22, from, to, done ? (allDone ? theme::gold() : theme::green()) : lv_color_hex(0x2A2D33));
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    if (calling) {
      // The office the hour calls for: its quarter in a quiet green, and a
      // bead on it that breathes. (Breathing the quarter itself would repaint
      // the whole glass thirty times a second.)
      lv_obj_set_style_arc_color(arc, theme::green(), LV_PART_MAIN);
      lv_obj_set_style_arc_opa(arc, LV_OPA_40, LV_PART_MAIN);
      lv_obj_t* bead = dot(scr, 12, theme::fg(), theme::bg(), 0);
      placePolar(bead, 189.0f, (from + to) / 2.0f);
      breathe(bead, LV_OPA_20, [](void* obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
      });
    }
    // The four quarters of the day close the circle one after the other.
    sweepIn(arc, from, to, static_cast<uint32_t>(i) * 120);
    lv_obj_t* name = text(scr, upper(officeName(i)).c_str(), theme::tiny(), done ? theme::fg() : theme::dim(), 0, 2, 120);
    placePolar(name, 138.0f, i * 0.25f + 0.125f);
  }

  if (!t.officesKnown) {
    // The day turned without a network: nobody has said yet what was prayed.
    text(scr, "—", theme::display(), theme::dim(), 176, 0, 200);
    text(scr, tr("os ofícios de hoje aparecem depois de sincronizar", "today's offices show after a sync",
                 "los oficios de hoy aparecen al sincronizar"),
         theme::small(), theme::dim(), 244, 0, 200);
    return;
  }
  char streak[16];
  snprintf(streak, sizeof(streak), "%d", t.streakCurrent);
  lv_obj_t* figure = text(scr, streak, theme::display(), theme::fg(), 172, 0, 200);
  countUp(figure, t.streakCurrent, 200);
  text(scr, tr("DIAS SEGUIDOS", "DAY STREAK", "DÍAS SEGUIDOS"), theme::tiny(), theme::gold(), 232, 3, 220);
  if (allDone) {
    text(scr, tr("dia completo", "day complete", "día completo"), theme::label(), theme::dim(), 262, 0, 220);
  } else if (next >= 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s", tr("próximo:", "next:", "siguiente:"), officeName(next));
    text(scr, buf, theme::label(), theme::dim(), 262, 0, 220);
  }

  // The last seven days, today on the right: a point per day, fuller the more
  // offices were prayed, gold on a day with all four.
  if (t.hasWeek) {
    const int32_t y = 306;
    for (int i = 0; i < 7; i++) {
      const int done = i == 6 ? t.completedToday : t.week[i];
      const int32_t x = kCenter + (i - 3) * 22;
      lv_obj_t* d;
      if (done < 0) {
        d = dot(scr, 4, theme::faint(), theme::bg(), 0);
      } else if (done == 0) {
        d = dot(scr, 12, theme::bg(), theme::faint(), 2);
      } else {
        d = dot(scr, 12, done >= 4 ? theme::gold() : theme::green(), theme::bg(), 0);
        if (done < 4) lv_obj_set_style_bg_opa(d, static_cast<lv_opa_t>(done == 1 ? 90 : (done == 2 ? 160 : 230)), 0);
      }
      lv_obj_update_layout(d);
      lv_obj_align(d, LV_ALIGN_TOP_LEFT, x - lv_obj_get_width(d) / 2, y - lv_obj_get_height(d) / 2);
      fadeIn(d, 300 + static_cast<uint32_t>(i) * 45, 200);
    }
    // A hair under today, so the row reads left to right into it.
    lv_obj_t* today = dot(scr, 4, theme::fg(), theme::bg(), 0);
    lv_obj_set_size(today, 12, 2);
    lv_obj_set_style_radius(today, 1, 0);
    lv_obj_align(today, LV_ALIGN_TOP_LEFT, kCenter + 3 * 22 - 6, y + 11);
    text(scr, tr("7 DIAS", "7 DAYS", "7 DÍAS"), theme::tiny(), theme::faint(), y + 20, 2, 160);
  }
}

void buildWeek(lv_obj_t* scr) {
  const model::Today& t = S.day;
  text(scr, tr("PRÓXIMOS DIAS", "DAYS AHEAD", "PRÓXIMOS DÍAS"), theme::tiny(), theme::dim(), 80, 3, 260);

  if (t.upcoming.empty()) {
    text(scr, tr("Os próximos dias chegam na próxima sincronização", "The days ahead arrive with the next sync",
                 "Los próximos días llegan con la próxima sincronización"),
         theme::body(), theme::dim(), 196, 0, 300);
  } else {
    // Six rows: a point in the day's colour, the weekday and the day's name.
    const size_t count = t.upcoming.size() < 6 ? t.upcoming.size() : 6;
    const int32_t rowHeight = 42;
    int32_t y = 244 - (static_cast<int32_t>(count) * rowHeight) / 2;
    for (size_t i = 0; i < count; i++) {
      const model::DayInfo& day = t.upcoming[i];
      const int32_t left = 76;
      lv_obj_t* d = dot(scr, 10, day.hasColor ? day.color : theme::faint(), theme::bg(), 0);
      lv_obj_set_pos(d, left, y + 8);
      lv_obj_t* wd = lv_label_create(scr);
      lv_label_set_text(wd, weekdayShort(model::weekdayOf(day.days)));
      lv_obj_set_style_text_font(wd, theme::tiny(), 0);
      lv_obj_set_style_text_color(wd, theme::dim(), 0);
      lv_obj_set_style_text_letter_space(wd, 1, 0);
      lv_obj_set_pos(wd, left + 22, y + 4);
      lv_obj_t* name = lv_label_create(scr);
      lv_label_set_text(name, day.title.c_str());
      lv_obj_set_style_text_font(name, day.isSunday || day.holyDay ? theme::label() : theme::body(), 0);
      lv_obj_set_style_text_color(name, theme::fg(), 0);
      lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_size(name, 236, 26);
      lv_obj_set_pos(name, left + 74, y);
      const uint32_t delayMs = static_cast<uint32_t>(i) * 55;
      fadeIn(d, delayMs);
      riseIn(wd, delayMs);
      riseIn(name, delayMs);
      y += rowHeight;
    }
  }

  // "Advento em 20 dias": the next feast on the ring, known without a network.
  const model::NextFeast feast = model::nextFeast(t, t.day.days);
  if (feast.found) {
    char buf[96];
    if (feast.inDays == 1) {
      snprintf(buf, sizeof(buf), "%s %s", feast.name.c_str(), tr("amanhã", "tomorrow", "mañana"));
    } else {
      snprintf(buf, sizeof(buf), tr("%s em %d dias", "%s in %d days", "%s en %d días"), feast.name.c_str(), feast.inDays);
    }
    lv_obj_t* l = text(scr, buf, theme::small(), feast.color, 384, 0, 260);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(l, 24);
  }
}

}  // namespace ui::detail
