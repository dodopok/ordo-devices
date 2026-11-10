#pragma once

#include <stdint.h>

#include "../sys/rtc_state.h"

// When to wake and what to ring: the part of the controller that decides
// how the battery is spent while nobody is looking. Pure arithmetic on
// purpose (no board, no clock of its own), so sim/test_logic.cpp can run it
// through a day on a computer.
namespace app {

struct Moment {
  int64_t raw = 0;          // seconds since power-on, through sleep
  bool clockValid = false;  // the fields below mean something
  int64_t utc = 0;
  int32_t utcOffset = 0;
};

struct Plan {
  int16_t reminder[4] = {-1, -1, -1, -1};  // minute of the day per office, -1: off
  // "Nothing prayed yet today": the app's evening nudge, minute of the day.
  int16_t streakReminder = -1;
  bool hourBell = false;                   // 6h, 12h, 18h
  uint8_t syncHours = 4;                   // 0: never by itself
  bool canSync = false;                    // linked, and at least one Wi-Fi network known
};

struct Due {
  int reminder = -1;   // office index
  bool streak = false; // the streak reminder's minute has come
  int bellHour = -1;   // 6, 12 or 18
  bool silence = false;
  bool test = false;   // the reminder asked for from the settings, to try it
  bool any() const { return reminder >= 0 || streak || bellHour >= 0 || silence || test; }
};

// A reminder rings from what the compass already knows: nothing at that
// wake waits for a network. So that what it knows is fresh, and its clock
// right to the second, it wakes this long before each reminder only to
// synchronise.
constexpr int kPrepMinutes = 10;

// What has come due at `now`. Marks it as rung in `rtc`, so a wake a little
// early or late, or two looks in the same minute, cannot ring twice.
Due takeDue(const Moment& now, const Plan& plan, sys::RtcState& rtc);

// True seconds until the next thing the compass must wake for by itself;
// 0 when there is nothing (only a button will wake it).
int64_t secondsToNextWake(const Moment& now, const Plan& plan, const sys::RtcState& rtc);

// True when a reminder (an office's or the streak's) comes within the next
// `minutes`: the moment for the sync that precedes it.
bool reminderWithin(const Moment& now, const Plan& plan, int minutes);

// After a sync that got no answer: how long to wait before the next try.
// Doubles from five minutes to an hour while awake; the scheduled wakes use
// a longer ladder, half an hour to six hours.
int64_t retryDelay(uint8_t failures);
int64_t backgroundRetryDelay(uint8_t failures);

// Local time helpers on a Moment.
int32_t localDay(const Moment& now);
int minuteOfDay(const Moment& now);

}  // namespace app
