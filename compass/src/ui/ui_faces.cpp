// The two faces: "Agora" (the glance: time, season, next office) and the
// compass of the liturgical year.

#include "ui_internal.h"

namespace ui::detail {
namespace {

// The marker's way to today: it sets out a quarter of the year back and
// comes to rest on the day, so the eye is led to where the year stands.
struct Travel {
  float radius;
  float from;
  float to;
};

void travelStep(lv_anim_t* a, int32_t v) {
  const Travel* travel = static_cast<const Travel*>(lv_anim_get_user_data(a));
  placePolar(static_cast<lv_obj_t*>(a->var), travel->radius, travel->from + (travel->to - travel->from) * static_cast<float>(v) / 1000.0f);
}

void travelTo(lv_obj_t* marker, float radius, float progress) {
  if (!entrance()) return;
  const float from = progress > 0.22f ? progress - 0.22f : 0.0f;
  if (progress - from < 0.01f) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, marker);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, 750);
  lv_anim_set_delay(&a, 120);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_user_data(&a, new Travel{radius, from, progress});
  lv_anim_set_custom_exec_cb(&a, travelStep);
  // Freed when the journey ends or the marker is deleted under it.
  lv_anim_set_deleted_cb(&a, [](lv_anim_t* anim) { delete static_cast<Travel*>(lv_anim_get_user_data(anim)); });
  lv_anim_start(&a);
}

// The year as a ring: every season in its colour, the current one lit, the
// feasts as points and a marker on today.
void yearRing(lv_obj_t* scr, int32_t diameter, int32_t width, bool withFeasts) {
  const model::Today& t = S.day;
  const bool allDone = t.allDone();
  ringArc(scr, diameter, width + 2, 0.0f, 1.0f, theme::track());
  for (const auto& seg : t.segments) {
    const bool active = t.progress >= seg.from && t.progress < seg.to;
    const float gap = 0.004f;
    ringArc(scr, diameter, width, seg.from + gap, seg.to - gap, allDone ? theme::gold() : seg.color,
            active ? LV_OPA_COVER : LV_OPA_40);
  }
  const float centre = diameter / 2.0f - width / 2.0f - 1.0f;
  if (withFeasts) {
    // The feasts light up round the year, one after the other.
    uint32_t index = 0;
    for (const auto& feast : t.feasts) {
      lv_obj_t* d = dot(scr, 7, feast.color, feast.color, 0);
      placePolar(d, diameter / 2.0f + 6.0f, feast.at);
      fadeIn(d, 200 + index++ * 30, 200);
    }
  }
  const lv_color_t accent = accentOf(t);
  const int32_t size = width + 12;
  // The core rides inside the marker, so the two travel as one.
  lv_obj_t* marker = dot(scr, size, theme::bg(), accent, 3);
  const int32_t coreSize = size - 16 < 6 ? 6 : size - 16;
  lv_obj_t* core = dot(marker, coreSize, accent, accent, 0);
  lv_obj_center(core);
  placePolar(marker, centre, t.progress);
  travelTo(marker, centre, t.progress);
}

// The four offices as dots, the one the hour calls for breathing.
void officeDots(lv_obj_t* scr, int32_t y, int next, bool withLetters) {
  const model::Today& t = S.day;
  for (int i = 0; i < 4; i++) {
    const bool done = t.officesKnown && t.offices[i].done;
    const int32_t x = kCenter + (i - 2) * 40 + 20;
    const lv_color_t edge = !t.officesKnown ? theme::faint() : (done ? theme::gold() : theme::dim());
    lv_obj_t* d = dot(scr, 18, done ? theme::gold() : theme::bg(), edge, 2);
    lv_obj_set_pos(d, x - 9, y);
    if (!done && i == next) {
      lv_obj_set_style_border_color(d, theme::gold(), 0);
      breathe(d, LV_OPA_40, [](void* obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
      });
    } else {
      // The day's prayer so far, counted out one office at a time.
      fadeIn(d, 260 + static_cast<uint32_t>(i) * 90, 240);
    }
    if (withLetters) {
      lv_obj_t* letter = text(scr, officeShort(i), theme::tiny(), done ? theme::fg() : theme::dim(), y + 24, 1, 40);
      lv_obj_align(letter, LV_ALIGN_TOP_LEFT, x - 20, y + 24);
    }
  }
}

}  // namespace

void buildNow(lv_obj_t* scr) {
  const model::Today& t = S.day;
  const model::DayInfo& day = t.day;
  const lv_color_t accent = accentOf(t);

  yearRing(scr, 440, 6, false);

  // "SEG · 9 NOV"
  int y = 0, m = 0, d = 0;
  model::civilFromDays(day.days, y, m, d);
  char line[40];
  snprintf(line, sizeof(line), "%s  ·  %d %s", weekdayShort(model::weekdayOf(day.days)), d, upper(monthShort(m)).c_str());
  text(scr, line, theme::tiny(), theme::dim(), 76, 2, 260);

  if (S.status.clockValid) {
    text(scr, clockText(S.status.minuteOfDay).c_str(), theme::digits(), theme::fg(), 110, 0, 360);
  } else {
    // No clock yet (never synchronised since power-on): the date instead.
    text(scr, dateLine(day).c_str(), theme::display(), theme::fg(), 138, 0, 360);
  }

  if (!day.season.isEmpty()) {
    text(scr, upper(day.season).c_str(), theme::tiny(), accent, 244, 2, 330);
  }
  if (day.title != day.season) {
    lv_obj_t* title = text(scr, day.title.c_str(), theme::title(), theme::fg(), 268, 0, 320);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(title, 62);
  }

  // The office the hour calls for, with the time of its reminder if one is set.
  const int next = S.status.clockValid ? t.officeForMinute(S.status.minuteOfDay) : t.nextOfficeIndex();
  char office[64] = "";
  lv_color_t officeColor = theme::dim();
  if (t.allDone()) {
    snprintf(office, sizeof(office), "Laus Deo");
    officeColor = theme::gold();
  } else if (next >= 0) {
    const int16_t at = S.prefs.reminder[next];
    if (at >= 0) snprintf(office, sizeof(office), "%s  ·  %s", officeName(next), clockText(at).c_str());
    else snprintf(office, sizeof(office), "%s", officeName(next));
  }
  if (office[0]) text(scr, office, theme::small(), officeColor, 338, 0, 280);
  officeDots(scr, 370, next, false);
}

void buildCompass(lv_obj_t* scr) {
  const model::Today& t = S.day;
  const model::DayInfo& day = t.day;
  const bool allDone = t.allDone();
  const lv_color_t accent = accentOf(t);

  yearRing(scr, 412, 18, true);

  if (allDone) {
    text(scr, "LAUS DEO", theme::tiny(), theme::gold(), 138, 3, 260);
  } else if (t.officesKnown && t.streakCurrent > 0) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d %s", t.streakCurrent, tr("DIAS SEGUIDOS", "DAY STREAK", "DÍAS SEGUIDOS"));
    text(scr, buf, theme::tiny(), theme::gold(), 138, 2, 260);
  }
  text(scr, upper(day.weekday).c_str(), theme::tiny(), theme::dim(), 168, 3, 300);
  text(scr, dateLine(day).c_str(), theme::display(), theme::fg(), 186, 0, 320);
  lv_obj_t* title = text(scr, day.title.c_str(), theme::title(), accent, 248, 0, 300);
  lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(title, 62);
  if (!day.season.isEmpty() && day.season != day.title) {
    text(scr, day.season.c_str(), theme::tiny(), theme::dim(), 314, 1, 280);
  }

  const int next = S.status.clockValid ? t.officeForMinute(S.status.minuteOfDay) : t.nextOfficeIndex();
  officeDots(scr, 340, next, true);
}

}  // namespace ui::detail
