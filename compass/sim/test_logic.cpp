// Runs the arithmetic the battery depends on (the clock, the wake schedule,
// reminders, the day seen from another date) on a computer.
//   make -C sim test
// Every check prints nothing when it holds and a line when it does not.

#include <sys/time.h>

#include <fstream>
#include <sstream>
#include <string>

#include <vector>

#include "app/schedule.h"
#include "content/art.h"
#include "model/content.h"
#include "model/today.h"
#include "sys/clock.h"
#include "sys/rtc_state.h"
#include "sys/wake_trail.h"
#include "ui/wipe.h"

namespace {

int failures = 0;
int64_t fakeRaw = 1000;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      failures++;                                                      \
      printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                  \
  } while (0)

#define CHECK_EQ(a, b)                                                                          \
  do {                                                                                          \
    const long long va = static_cast<long long>(a), vb = static_cast<long long>(b);             \
    if (va != vb) {                                                                             \
      failures++;                                                                               \
      printf("FAILED %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb);       \
    }                                                                                           \
  } while (0)

// 2026-11-09 19:46:46 UTC, a Monday; 16:46:46 in São Paulo.
constexpr int64_t kNov9 = 1794253606;
constexpr int32_t kSaoPaulo = -3 * 3600;

app::Moment at(int64_t utc, int64_t raw) {
  app::Moment m;
  m.raw = raw;
  m.clockValid = true;
  m.utc = utc;
  m.utcOffset = kSaoPaulo;
  return m;
}

// UTC seconds of a São Paulo wall-clock time on 2026-11-09 (+days).
int64_t local(int hour, int minute, int second = 0, int dayOffset = 0) {
  const int64_t midnight = kNov9 - (16 * 3600 + 46 * 60 + 46);  // 2026-11-09 00:00 local
  return midnight + dayOffset * 86400LL + hour * 3600 + minute * 60 + second;
}

void testClock() {
  sys::rtcBegin();
  int64_t parsed = 0;
  CHECK(sys::parseHttpDate("Mon, 09 Nov 2026 19:46:46 GMT", parsed));
  CHECK_EQ(parsed, kNov9);
  CHECK(!sys::parseHttpDate("garbage", parsed));
  CHECK(!sys::parseHttpDate(nullptr, parsed));

  CHECK(!sys::clockValid());
  fakeRaw = 1000;
  sys::clockSet(kNov9);
  sys::setUtcOffset(kSaoPaulo);
  CHECK(sys::clockValid());
  CHECK_EQ(sys::nowUtc(), kNov9);
  CHECK_EQ(sys::minuteOfDay(), 16 * 60 + 46);
  CHECK_EQ(sys::localDays(), 20766);  // 2026-11-09

  // An hour on, the timer has counted 1 % too few seconds: the API says
  // 3636 s went by where we counted 3600.
  fakeRaw += 3600;
  CHECK_EQ(sys::nowUtc(), kNov9 + 3600);
  sys::clockSet(kNov9 + 3636);
  CHECK_EQ(sys::clockDriftPpm(), 10000);
  // The next stretch is stretched by the same 1 %.
  fakeRaw += 3600;
  CHECK_EQ(sys::nowUtc(), kNov9 + 3636 + 3636);
  // And a sleep of an hour of true time asks the slow timer for fewer ticks.
  CHECK_EQ(sys::sleepSecondsFor(3600), 3564);
  CHECK_EQ(sys::sleepSecondsFor(0), 0);

  // Zones from the midnight countdown, for an API without utc_offset_seconds.
  CHECK_EQ(sys::offsetFromDayEnd(kNov9, 86400 - (16 * 3600 + 46 * 60 + 46)), -10800);  // São Paulo
  CHECK_EQ(sys::offsetFromDayEnd(kNov9, 86400 - (19 * 3600 + 46 * 60 + 46)), 0);       // Lisbon
  CHECK_EQ(sys::offsetFromDayEnd(kNov9, 86400 - (4 * 3600 + 46 * 60 + 46)), 32400);    // Tokyo, already tomorrow
  CHECK_EQ(sys::offsetFromDayEnd(kNov9, 86400 - (1 * 3600 + 16 * 60 + 40)), 19800);    // Delhi, +5:30, a few seconds off
}

void testDue() {
  sys::RtcState rtc = {};
  rtc.firedDay = -1;
  app::Plan plan;
  plan.reminder[0] = 7 * 60;
  plan.reminder[2] = 18 * 60;
  plan.hourBell = true;

  // 06:59:58: nothing yet.
  app::Due due = app::takeDue(at(local(6, 59, 58), 100), plan, rtc);
  CHECK_EQ(due.reminder, -1);
  // The 6 o'clock bell is long past its ten minutes: let pass, not rung.
  CHECK_EQ(due.bellHour, -1);
  // 07:00:03: the morning reminder, once.
  due = app::takeDue(at(local(7, 0, 3), 105), plan, rtc);
  CHECK_EQ(due.reminder, 0);
  due = app::takeDue(at(local(7, 0, 40), 142), plan, rtc);
  CHECK_EQ(due.reminder, -1);
  // 12:00:01: the noon bell.
  due = app::takeDue(at(local(12, 0, 1), 18000), plan, rtc);
  CHECK_EQ(due.bellHour, 12);
  CHECK_EQ(due.reminder, -1);
  // Switched off over the evening reminder and back at 19:00: an hour late, it is let pass...
  due = app::takeDue(at(local(19, 0, 0), 40000), plan, rtc);
  CHECK_EQ(due.reminder, -1);
  // ...and the next day everything is armed again.
  due = app::takeDue(at(local(7, 5, 0, 1), 90000), plan, rtc);
  CHECK_EQ(due.reminder, 0);

  // A reminder moved to a later hour the same day (the owner changed it in
  // the app after it had rung) is a new appointment...
  plan.reminder[0] = 7 * 60 + 30;
  due = app::takeDue(at(local(7, 30, 5, 1), 91500), plan, rtc);
  CHECK_EQ(due.reminder, 0);
  due = app::takeDue(at(local(7, 31, 0, 1), 91560), plan, rtc);
  CHECK_EQ(due.reminder, -1);
  // ...and one moved to an hour already gone is let pass, not rung late.
  plan.reminder[0] = 5 * 60;
  due = app::takeDue(at(local(8, 0, 0, 1), 93300), plan, rtc);
  CHECK_EQ(due.reminder, -1);

  // Two offices at the same minute: one now, the other at the next look.
  app::Plan both;
  both.reminder[1] = 12 * 60;
  both.reminder[3] = 12 * 60;
  sys::RtcState twice = {};
  twice.firedDay = -1;
  CHECK_EQ(app::takeDue(at(local(12, 0, 1), 100), both, twice).reminder, 1);
  CHECK_EQ(app::takeDue(at(local(12, 0, 2), 101), both, twice).reminder, 3);
  CHECK_EQ(app::takeDue(at(local(12, 0, 3), 102), both, twice).reminder, -1);

  // The streak reminder: once, at its minute; with an office due at the same
  // minute both are reported and the caller shows the office.
  app::Plan evening;
  evening.streakReminder = 22 * 60;
  evening.reminder[3] = 22 * 60;
  sys::RtcState night = {};
  night.firedDay = -1;
  CHECK(!app::takeDue(at(local(21, 59, 58), 100), evening, night).streak);
  due = app::takeDue(at(local(22, 0, 1), 103), evening, night);
  CHECK(due.streak);
  CHECK_EQ(due.reminder, 3);
  CHECK(!app::takeDue(at(local(22, 0, 30), 132), evening, night).streak);
  // Found out an hour late: let pass.
  sys::RtcState late = {};
  late.firedDay = -1;
  CHECK(!app::takeDue(at(local(23, 0, 0), 100), evening, late).streak);
  CHECK(!app::takeDue(at(local(23, 0, 1), 101), evening, late).streak);

  // Silence ends by the raw clock, with or without a valid time.
  rtc.silenceEndsRaw = 500;
  app::Moment blind;
  blind.raw = 499;
  CHECK(!app::takeDue(blind, plan, rtc).silence);
  blind.raw = 500;
  CHECK(app::takeDue(blind, plan, rtc).silence);
  CHECK(!app::takeDue(blind, plan, rtc).silence);

  // So does the reminder asked for from the settings: once, a minute on.
  rtc.testAtRaw = 500 + 60;
  blind.raw = 559;
  CHECK(!app::takeDue(blind, plan, rtc).test);
  blind.raw = 561;
  due = app::takeDue(blind, plan, rtc);
  CHECK(due.test);
  CHECK(due.any());
  CHECK(!app::takeDue(blind, plan, rtc).test);
  CHECK(!app::Due().any());
}

void testSchedule() {
  sys::RtcState rtc = {};
  rtc.firedDay = -1;
  app::Plan plan;
  plan.syncHours = 0;

  // Nothing set, not linked: only a button wakes it.
  CHECK_EQ(app::secondsToNextWake(at(local(10, 0), 100), plan, rtc), 0);

  // A reminder at 18:00, looked at from 10:00: eight hours.
  plan.reminder[2] = 18 * 60;
  CHECK_EQ(app::secondsToNextWake(at(local(10, 0), 100), plan, rtc), 8 * 3600);
  // Woken two seconds early: sleep a moment more, do not spin and do not skip to tomorrow.
  CHECK_EQ(app::secondsToNextWake(at(local(17, 59, 58), 100), plan, rtc), 5);
  // Its minute has come and it has not rung yet: in a moment.
  CHECK_EQ(app::secondsToNextWake(at(local(18, 0, 2), 100), plan, rtc), 5);
  // Once rung, tomorrow.
  app::takeDue(at(local(18, 0, 2), 100), plan, rtc);
  CHECK_EQ(app::secondsToNextWake(at(local(18, 0, 2), 100), plan, rtc), 86400 - 2);

  // The streak reminder wakes it like an office's does.
  {
    sys::RtcState fresh = {};
    fresh.firedDay = -1;
    app::Plan nudge;
    nudge.syncHours = 0;
    nudge.streakReminder = 22 * 60;
    CHECK_EQ(app::secondsToNextWake(at(local(20, 0), 100), nudge, fresh), 2 * 3600);
    CHECK_EQ(app::secondsToNextWake(at(local(22, 0, 1), 100), nudge, fresh), 5);
    app::takeDue(at(local(22, 0, 1), 100), nudge, fresh);
    CHECK_EQ(app::secondsToNextWake(at(local(22, 0, 1), 100), nudge, fresh), 86400 - 1);
  }

  // The reminder asked for from the settings wakes it by the raw clock.
  {
    sys::RtcState fresh = {};
    fresh.firedDay = -1;
    app::Plan none;
    none.syncHours = 0;
    app::Moment blind;
    blind.raw = 1000;
    fresh.testAtRaw = 1060;
    CHECK_EQ(app::secondsToNextWake(blind, none, fresh), 60);
    CHECK_EQ(app::secondsToNextWake(at(local(10, 0), 1000), none, fresh), 60);
  }

  // Ten minutes before a reminder it wakes to synchronise, so that the
  // reminder itself rings without waiting for a network.
  {
    sys::RtcState fresh = {};
    fresh.firedDay = -1;
    fresh.lastAnswerRaw = 99000;  // just synced: the four-hourly one is far
    app::Plan night;
    night.canSync = true;
    night.syncHours = 4;
    night.reminder[3] = 22 * 60;
    CHECK_EQ(app::secondsToNextWake(at(local(20, 0), 99100), night, fresh), 110 * 60);
    CHECK(!app::reminderWithin(at(local(20, 0), 99100), night, app::kPrepMinutes + 2));
    // At that wake: the reminder is near, and next comes the reminder.
    CHECK(app::reminderWithin(at(local(21, 50, 2), 99100), night, app::kPrepMinutes + 2));
    CHECK_EQ(app::secondsToNextWake(at(local(21, 50, 2), 99100), night, fresh), 10 * 60 - 2);
    CHECK_EQ(app::secondsToNextWake(at(local(21, 55), 99100), night, fresh), 5 * 60);
    // A little early (the timer is not a clock): still near, still the reminder next.
    CHECK(app::reminderWithin(at(local(21, 49, 40), 99100), night, app::kPrepMinutes + 2));
    // At its minute it is no longer "ahead".
    CHECK(!app::reminderWithin(at(local(22, 0, 1), 99100), night, app::kPrepMinutes + 2));
    // The streak reminder counts, and so does one across midnight.
    app::Plan late;
    late.canSync = true;
    late.reminder[0] = 5;  // 00:05
    late.streakReminder = 22 * 60;
    CHECK(app::reminderWithin(at(local(23, 58), 99100), late, app::kPrepMinutes + 2));
    CHECK(app::reminderWithin(at(local(21, 51), 99100), late, app::kPrepMinutes + 2));
    CHECK(!app::reminderWithin(at(local(12, 0), 99100), late, app::kPrepMinutes + 2));
    CHECK_EQ(app::secondsToNextWake(at(local(23, 50), 99100), late, fresh), 5 * 60);  // 23:55, for the one at 00:05
    // Never syncing by itself, or with no network to sync on: no such wake.
    night.syncHours = 0;
    CHECK_EQ(app::secondsToNextWake(at(local(20, 0), 99100), night, fresh), 2 * 3600);
    night.syncHours = 4;
    night.canSync = false;
    CHECK_EQ(app::secondsToNextWake(at(local(20, 0), 99100), night, fresh), 2 * 3600);
    // No clock: nothing is "near".
    app::Moment blind;
    blind.raw = 5;
    CHECK(!app::reminderWithin(blind, late, 1440));
  }

  // Linked and syncing every four hours, last answer an hour ago: three hours,
  // sooner than midnight (13 h away) and the reminder (tomorrow).
  plan.canSync = true;
  plan.syncHours = 4;
  rtc.lastAnswerRaw = 10000;
  CHECK_EQ(app::secondsToNextWake(at(local(19, 0), 10000 + 3600), plan, rtc), 3 * 3600);
  // 23:00, synced a minute ago: midnight plus two minutes comes first.
  rtc.lastAnswerRaw = 50000;
  CHECK_EQ(app::secondsToNextWake(at(local(23, 0), 50060), plan, rtc), 3600 + 120);

  // Away from every known network: the tries thin out instead of repeating.
  rtc.syncFailures = 1;
  CHECK_EQ(app::secondsToNextWake(at(local(10, 0, 0, 1), 90000), plan, rtc), 1800);
  rtc.syncFailures = 3;
  CHECK_EQ(app::secondsToNextWake(at(local(10, 0, 0, 1), 90000), plan, rtc), 7200);
  rtc.syncFailures = 200;
  CHECK_EQ(app::secondsToNextWake(at(local(10, 0, 0, 1), 90000), plan, rtc), 6 * 3600);
  CHECK_EQ(app::retryDelay(0), 0);
  CHECK_EQ(app::retryDelay(1), 300);
  CHECK_EQ(app::retryDelay(4), 2400);
  CHECK_EQ(app::retryDelay(9), 3600);

  // No clock yet (never synced since power-on): reminders cannot be timed,
  // the sync ladder still runs.
  rtc = {};
  rtc.firedDay = -1;
  app::Moment blind;
  blind.raw = 77;
  CHECK_EQ(app::secondsToNextWake(blind, plan, rtc), 1800);
  plan.canSync = false;
  CHECK_EQ(app::secondsToNextWake(blind, plan, rtc), 0);
  // A time of silence needs no clock.
  rtc.silenceEndsRaw = 77 + 600;
  CHECK_EQ(app::secondsToNextWake(blind, plan, rtc), 600);

  // A day lived through: every wake the plan asks for is taken, and nothing
  // rings twice or wakes more often than it should.
  rtc = {};
  rtc.firedDay = -1;
  plan = app::Plan();
  plan.canSync = true;
  plan.syncHours = 4;
  plan.hourBell = true;
  plan.reminder[0] = 7 * 60;
  plan.reminder[3] = 21 * 60 + 30;
  int64_t utc = local(0, 0, 30);
  int64_t raw = 1000;
  rtc.lastAnswerRaw = raw;
  int wakes = 0, reminders = 0, bells = 0, syncs = 0;
  while (utc < local(0, 0, 30, 1)) {
    const int64_t sleep = app::secondsToNextWake(at(utc, raw), plan, rtc);
    CHECK(sleep >= 5);
    if (sleep < 5) break;
    utc += sleep;
    raw += sleep;
    wakes++;
    const app::Due due = app::takeDue(at(utc, raw), plan, rtc);
    if (due.bellHour >= 0) bells++;
    if (due.reminder >= 0) {
      reminders++;
      // It rings from what a sync a few minutes ago left on record.
      CHECK(raw - rtc.lastAnswerRaw <= (app::kPrepMinutes + 1) * 60);
      continue;
    }
    // As the controller does on a wake with nothing to ring (background()).
    const bool periodic = raw - rtc.lastAnswerRaw >= 4 * 3600 - 300;
    const bool beforeReminder = app::reminderWithin(at(utc, raw), plan, app::kPrepMinutes + 2) && raw - rtc.lastAnswerRaw >= 5 * 60;
    const bool dayTurned = app::localDay(at(utc, raw)) != app::localDay(at(utc - sleep, raw));
    if (!due.any() && (periodic || beforeReminder || dayTurned)) {
      rtc.lastAnswerRaw = raw;  // the sync answered
      syncs++;
    }
  }
  CHECK_EQ(reminders, 2);
  CHECK_EQ(bells, 3);
  CHECK(wakes <= 14);
  CHECK(syncs >= 5 && syncs <= 9);
}

// The lines the "Sobre" screen reads out about the last wakes.
void testTrail() {
  using sys::WakeCause, sys::WakeStep;
  sys::trailBegin();
  CHECK_EQ(sys::trailCount(), 0);

  sys::trailOpen(21 * 60 + 50, WakeCause::Timer, false).step = WakeStep::Slept;
  sys::trailSeal();
  sys::WakeNote& rang = sys::trailOpen(22 * 60, WakeCause::Timer, false);
  rang.due = static_cast<uint8_t>(sys::kDueMorning << 3);
  rang.step = WakeStep::Rang;
  sys::trailSeal();
  CHECK_EQ(sys::trailCount(), 2);
  CHECK_EQ(sys::trailAt(0).minute, 22 * 60);
  CHECK_EQ(sys::trailAt(1).minute, 21 * 60 + 50);

  // Glances by a button that found nothing to do share one line...
  sys::trailOpen(22 * 60 + 5, WakeCause::Button, false).step = WakeStep::Shown;
  sys::trailSeal();
  sys::trailOpen(22 * 60 + 9, WakeCause::Button, false).step = WakeStep::Shown;
  sys::trailSeal();
  sys::trailOpen(22 * 60 + 12, WakeCause::Touch, false).step = WakeStep::Shown;
  sys::trailSeal();
  CHECK_EQ(sys::trailCount(), 3);
  CHECK_EQ(sys::trailAt(0).minute, 22 * 60 + 12);
  CHECK_EQ(sys::trailAt(1).minute, 22 * 60);
  // ...but not the one after a restart nobody asked for.
  sys::trailOpen(22 * 60 + 20, WakeCause::Button, true).step = WakeStep::Shown;
  sys::trailSeal();
  CHECK_EQ(sys::trailCount(), 4);
  CHECK(sys::trailAt(0).afterCrash);
  // A wake that stopped half way keeps its line under the glances that follow.
  sys::trailOpen(23 * 60, WakeCause::Timer, false).step = WakeStep::Syncing;
  sys::trailSeal();
  sys::trailOpen(23 * 60 + 30, WakeCause::Button, false).step = WakeStep::Shown;
  sys::trailSeal();
  CHECK_EQ(sys::trailCount(), 6);
  CHECK(sys::trailAt(1).step == WakeStep::Syncing);

  // The oldest lines make room; the newest is always first.
  for (int i = 0; i < 2 * sys::kWakeNotes; i++) {
    sys::trailOpen(static_cast<int16_t>(i), WakeCause::Timer, false).step = WakeStep::Slept;
    sys::trailSeal();
  }
  CHECK_EQ(sys::trailCount(), sys::kWakeNotes);
  for (int back = 0; back < sys::kWakeNotes; back++) CHECK_EQ(sys::trailAt(back).minute, 2 * sys::kWakeNotes - 1 - back);

  // The memory survives a restart, and noise in it is thrown away whole.
  sys::trailBegin();
  CHECK_EQ(sys::trailCount(), sys::kWakeNotes);
  sys::trailNow().minute = 777;  // changed behind its back, not sealed
  sys::trailBegin();
  CHECK_EQ(sys::trailCount(), 0);
}

// A change of screen: the strips laid over the old screen, in order.
void testWipe() {
  const int side = 466;
  for (ui::Wipe wipe : {ui::Wipe::Up, ui::Wipe::Down, ui::Wipe::Left, ui::Wipe::Right, ui::Wipe::Open, ui::Wipe::Close}) {
    const bool upright = wipe == ui::Wipe::Left || wipe == ui::Wipe::Right;
    std::vector<int> covered(side, 0);
    const int steps = ui::wipeSteps(wipe, side);
    for (int step = 0; step < steps; step++) {
      ui::WipeStrip strips[2];
      const int count = ui::wipeStrips(wipe, side, step, strips);
      CHECK(count >= 1 && count <= 2);
      for (int i = 0; i < count; i++) {
        const ui::WipeStrip& strip = strips[i];
        // The panel takes windows that start on an even line and are an even number of lines.
        CHECK(strip.x % 2 == 0 && strip.y % 2 == 0 && strip.w % 2 == 0 && strip.h % 2 == 0);
        CHECK(strip.w > 0 && strip.h > 0 && strip.x + strip.w <= side && strip.y + strip.h <= side);
        CHECK_EQ(upright ? strip.h : strip.w, side);
        const int from = upright ? strip.x : strip.y;
        const int lines = upright ? strip.w : strip.h;
        for (int line = from; line < from + lines; line++) covered[line]++;
      }
    }
    // Every line of the glass, once.
    for (int line = 0; line < side; line++) CHECK_EQ(covered[line], 1);
    ui::WipeStrip none[2];
    CHECK_EQ(ui::wipeStrips(wipe, side, steps, none), 0);
    CHECK_EQ(ui::wipeStrips(wipe, side, -1, none), 0);
  }
  // Where each begins: under the finger.
  ui::WipeStrip first[2];
  ui::wipeStrips(ui::Wipe::Up, side, 0, first);
  CHECK_EQ(first[0].y + first[0].h, side);
  ui::wipeStrips(ui::Wipe::Down, side, 0, first);
  CHECK_EQ(first[0].y, 0);
  ui::wipeStrips(ui::Wipe::Left, side, 0, first);
  CHECK_EQ(first[0].x + first[0].w, side);
  ui::wipeStrips(ui::Wipe::Right, side, 0, first);
  CHECK_EQ(first[0].x, 0);
  CHECK_EQ(ui::wipeStrips(ui::Wipe::Open, side, 0, first), 2);
  CHECK_EQ(first[0].y + first[0].h, first[1].y);  // the two meet in the middle
  ui::wipeStrips(ui::Wipe::Close, side, 0, first);
  CHECK(first[0].y == 0 || first[0].y + first[0].h == side);

  // The pace: something at once, never backwards, all of it by the end.
  const int steps = ui::wipeSteps(ui::Wipe::Up, side);
  int before = 0;
  for (uint32_t ms = 0; ms <= ui::kWipeMs + 20; ms++) {
    const int due = ui::wipeStepsDue(steps, ms);
    CHECK(due >= before && due >= 1 && due <= steps);
    before = due;
  }
  CHECK_EQ(ui::wipeStepsDue(steps, ui::kWipeMs), steps);
  CHECK(ui::wipeStepsDue(steps, ui::kWipeMs / 2) > steps / 2);  // quick off the mark
}

void testModel(const char* path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  model::Today today;
  CHECK(model::parseToday(buffer.str().c_str(), today));
  CHECK_EQ(today.day.days, 20766);
  CHECK(today.day.title == "Próprio 27");
  CHECK_EQ(today.upcoming.size(), 7);
  CHECK(today.hasUtcOffset);
  CHECK_EQ(today.utcOffsetSeconds, -10800);
  CHECK(today.offices[0].done && today.offices[1].done && !today.offices[2].done);
  CHECK_EQ(model::weekdayOf(today.day.days), 1);  // Monday
  CHECK(model::isoDateFromDays(20766) == "2026-11-09");

  // The office the hour calls for.
  CHECK_EQ(today.officeForMinute(8 * 60), 2);    // morning and midday done: on to the evening
  CHECK_EQ(today.officeForMinute(22 * 60), 3);

  // Same day: untouched.
  CHECK(model::viewFor(today, today.day.days).officesKnown);
  // Two days on, no network: the feast from the window, offices unknown.
  model::Today wed = model::viewFor(today, today.day.days + 2);
  CHECK(wed.day.title == "Martinho de Tours");
  CHECK(wed.day.described);
  CHECK(!wed.officesKnown);
  CHECK(!wed.offices[0].done);
  CHECK_EQ(wed.upcoming.size(), 5);
  CHECK(wed.progress > today.progress);
  // Past the window: what the ring knows.
  model::Today feast = model::viewFor(today, today.day.days + 13);
  CHECK(feast.day.title == "Cristo Rei");
  CHECK(!feast.day.described);
  CHECK(feast.day.season == "Tempo Comum");
  CHECK(feast.day.weekday == "Domingo");
  CHECK(feast.day.daily.empty());
  // The clock went backwards (a bad reading): stay on the day on record.
  CHECK(model::viewFor(today, today.day.days - 1).day.title == "Próprio 27");

  const model::NextFeast next = model::nextFeast(today, today.day.days);
  CHECK(next.found);
  CHECK(next.name == "Cristo Rei");
  CHECK_EQ(next.inDays, 13);

  // ---- what the 0.4 payload adds
  // Who the day remembers, and where its picture and text are to be had.
  CHECK(!today.day.celebration.present());
  const model::Celebration& martin = wed.day.celebration;
  CHECK(martin.present());
  CHECK(martin.epithet == "Monge e Bispo");
  CHECK(martin.year == "397");
  CHECK(martin.byline() == "Monge e Bispo, 397");
  CHECK(martin.slug == "celebration-martin-of-tours");
  CHECK(martin.artPath.startsWith("/c/celebration-martin-of-tours.jpg?t="));
  CHECK(martin.textPath.startsWith("/c/celebration-martin-of-tours.pt-BR.json?t="));
  // The fast, in the short form.
  CHECK(today.day.observance.isEmpty());
  CHECK(model::viewFor(today, today.day.days + 4).day.observance == "Devoção e conversão");
  // The season in whole days, and how it moves when the day turns offline.
  CHECK(today.season.known);
  CHECK_EQ(today.season.day, 169);
  CHECK_EQ(today.season.days, 188);
  CHECK(today.season.nextName == "Advento");
  CHECK_EQ(today.season.nextInDays, 20);
  CHECK_EQ(wed.season.day, 171);
  CHECK_EQ(wed.season.nextInDays, 18);
  CHECK(!model::viewFor(today, today.day.days + 25).season.known);  // Advent by then
  // The app's reminders.
  CHECK(today.hasReminders);
  CHECK_EQ(today.reminders[0], 7 * 60);
  CHECK_EQ(today.reminders[1], -1);
  CHECK_EQ(today.reminders[2], 18 * 60);
  CHECK_EQ(today.streakRemindAt, 22 * 60);
  // The week: oldest first, today last; it slides when the day turns.
  CHECK(today.hasWeek);
  CHECK_EQ(today.week[0], 4);
  CHECK_EQ(today.week[6], 2);
  CHECK_EQ(wed.week[4], 2);   // Monday is now two days back
  CHECK_EQ(wed.week[5], -1);  // nobody has said what was prayed on Tuesday
  CHECK(!model::viewFor(today, today.day.days + 8).hasWeek);

  // Only signed paths on the content host are kept; anything else is dropped.
  model::Today odd;
  const char* oddDay =
      "{\"date\":\"2026-11-09\",\"day\":{\"celebration\":{\"name\":\"X\",\"slug\":\"celebration-x\",\"content\":{"
      "\"art\":\"https://elsewhere.example/c/celebration-x.jpg\",\"text\":\"/c/../firmware/manifest.json\"}}}}";
  CHECK(model::parseToday(oddDay, odd));
  CHECK(odd.day.celebration.present());
  CHECK(odd.day.celebration.artPath.isEmpty());
  CHECK(odd.day.celebration.textPath.isEmpty());

  // A payload from an API that predates the new fields still parses.
  model::Today old;
  CHECK(model::parseToday("{\"date\":\"2026-11-09\",\"language\":\"pt-BR\",\"day\":{\"title\":\"Dia\",\"celebration\":null}}", old));
  CHECK(!old.hasUtcOffset);
  CHECK(old.upcoming.empty());
  CHECK(!old.season.known && !old.hasReminders && !old.hasWeek && old.day.observance.isEmpty());
  CHECK_EQ(old.streakRemindAt, -1);
  CHECK(!model::parseToday("", old));
  CHECK(!model::parseToday("{not json", old));
}

std::string slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void testContent() {
  // The text the content host sends.
  const std::string json = slurp("content/celebration-martin-of-tours.pt-BR.json");
  model::CelebrationText text;
  CHECK(model::parseCelebrationText(json.c_str(), json.size(), text));
  CHECK(text.found && text.more);
  CHECK(text.summary.startsWith("Soldado romano"));
  CHECK(text.body.size() == 8);
  CHECK(text.body[0].heading && text.body[0].text == "O soldado e a capa");
  CHECK(!text.body[1].heading);
  CHECK(!model::parseCelebrationText("{\"v\":2,\"summary\":\"x\"}", 21, text));
  CHECK(!model::parseCelebrationText("<html>not found</html>", 22, text));
  CHECK(!text.found);

  // File names for the store: stable, sixteen hex digits, different per slug.
  const String key = model::contentKey("celebration-martin-of-tours");
  CHECK_EQ(key.length(), 16);
  CHECK(key == model::contentKey("celebration-martin-of-tours"));
  CHECK(key != model::contentKey("celebration-martin-of-tour"));

  // The picture, through the decoder the board uses.
  const std::string jpeg = slurp("content/celebration-martin-of-tours.jpg");
  CHECK(jpeg.size() > 1000);
  std::vector<uint16_t> pixels(static_cast<size_t>(content::kArtSide) * content::kArtSide, 0xFFFF);
  CHECK(content::decodeArt(reinterpret_cast<const uint8_t*>(jpeg.data()), jpeg.size(), pixels.data()));
  // Right down to the last row and column, which no whole MCU covers.
  size_t lit = 0;
  for (uint16_t p : pixels) lit += p != 0;
  CHECK(lit > pixels.size() * 9 / 10);
  CHECK(pixels[pixels.size() - 1] != 0);
  // A cut-off file and a file of another size are refused, and leave black.
  CHECK(!content::decodeArt(reinterpret_cast<const uint8_t*>(jpeg.data()), 40, pixels.data()));
  CHECK(!content::decodeArt(reinterpret_cast<const uint8_t*>(jpeg.data()), jpeg.size(), pixels.data(), 240));
  CHECK_EQ(pixels[0], 0);

  // Versions.
  CHECK(model::versionNewer("0.4.1", "0.4.0"));
  CHECK(model::versionNewer("0.10.0", "0.9.9"));
  CHECK(model::versionNewer("1.0.0", "0.99.99"));
  CHECK(!model::versionNewer("0.4.0", "0.4.0"));
  CHECK(!model::versionNewer("0.3.9", "0.4.0"));
  CHECK(!model::versionNewer("0.4.1-beta", "0.4.0"));
  CHECK(!model::versionNewer("", "0.4.0"));
  CHECK(!model::versionNewer("latest", "0.4.0"));

  // The firmware manifest: the build for this board, and nothing odd.
  const std::string hash(64, 'a');
  auto manifest = [&](const std::string& path, const std::string& size, const std::string& sha) {
    return std::string("{\"v\":1,\"version\":\"0.4.1\",\"notes\":{\"pt\":\"Mais rápido.\",\"en\":\"Faster.\"},\"boards\":{\"amoled175c\":{\"path\":\"") +
           path + "\",\"size\":" + size + ",\"sha256\":\"" + sha + "\",\"sig\":\"\"}}}";
  };
  model::FirmwareOffer offer;
  const std::string good = manifest("/firmware/0.4.1/amoled175c.bin", "1900000", hash);
  CHECK(model::parseManifest(good.c_str(), good.size(), "amoled175c", "pt-BR", offer));
  CHECK(offer.found && offer.version == "0.4.1" && offer.size == 1900000 && offer.notes == "Mais rápido.");
  CHECK(model::parseManifest(good.c_str(), good.size(), "amoled175c", "en-US", offer) && offer.notes == "Faster.");
  CHECK(model::parseManifest(good.c_str(), good.size(), "amoled175c", "es", offer) && offer.notes == "Mais rápido.");
  CHECK(!model::parseManifest(good.c_str(), good.size(), "amoled175", "pt-BR", offer));  // nothing for that board
  for (const std::string& bad : {
           manifest("/c/x.bin", "1900000", hash),                               // not the firmware folder
           manifest("/firmware/../c/x.bin", "1900000", hash),
           manifest("https://elsewhere.example/firmware/x.bin", "1900000", hash),
           manifest("/firmware/0.4.1/amoled175c.elf", "1900000", hash),
           manifest("/firmware/0.4.1/amoled175c.bin", "1000", hash),            // not the size of a firmware
           manifest("/firmware/0.4.1/amoled175c.bin", "99000000", hash),
           manifest("/firmware/0.4.1/amoled175c.bin", "1900000", "abc"),        // not a hash
           manifest("/firmware/0.4.1/amoled175c.bin", "1900000", std::string(64, 'Z')),
       }) {
    CHECK(!model::parseManifest(bad.c_str(), bad.size(), "amoled175c", "pt-BR", offer));
    CHECK(!offer.found);
  }
  CHECK(!model::parseManifest("{\"v\":2}", 7, "amoled175c", "pt-BR", offer));
}

}  // namespace

// The firmware's clock reads the system time; here the test moves it.
extern "C" int fakeGettimeofday(struct timeval* tv, void*) noexcept {
  tv->tv_sec = fakeRaw;
  tv->tv_usec = 0;
  return 0;
}

uint32_t millis() { return 0; }

int main(int argc, char** argv) {
  testClock();
  testDue();
  testSchedule();
  testTrail();
  testWipe();
  testModel(argc > 1 ? argv[1] : "sample_today.json");
  testContent();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("logic ok\n");
  return 0;
}
