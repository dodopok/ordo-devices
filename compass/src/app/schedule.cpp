#include "schedule.h"

namespace app {
namespace {

constexpr int kReminderGraceMinutes = 20;  // later than this, a reminder is let pass
constexpr int kStreakSlot = 4;             // in RtcState::firedAt, after the four offices
constexpr int kBellGraceMinutes = 10;
constexpr int64_t kAfterMidnightSeconds = 120;
const int kBellHours[3] = {6, 12, 18};

int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// Seconds from `now` until the local clock next shows `minute` of the day
// (always in the future: at that very minute it answers tomorrow).
int64_t untilLocalMinute(const Moment& now, int minute) {
  const int64_t local = now.utc + now.utcOffset;
  const int64_t dayStart = floorDiv(local, 86400) * 86400;
  int64_t at = dayStart + static_cast<int64_t>(minute) * 60;
  if (at <= local) at += 86400;
  return at - local;
}

}  // namespace

int32_t localDay(const Moment& now) { return static_cast<int32_t>(floorDiv(now.utc + now.utcOffset, 86400)); }

int minuteOfDay(const Moment& now) {
  const int64_t local = now.utc + now.utcOffset;
  return static_cast<int>((local - floorDiv(local, 86400) * 86400) / 60);
}

int64_t retryDelay(uint8_t failures) {
  if (failures == 0) return 0;
  const int shift = failures > 5 ? 4 : failures - 1;
  const int64_t delay = 300LL << shift;
  return delay > 3600 ? 3600 : delay;
}

int64_t backgroundRetryDelay(uint8_t failures) {
  const int shift = failures == 0 ? 0 : (failures > 5 ? 4 : failures - 1);
  const int64_t delay = 1800LL << shift;
  return delay > 6 * 3600 ? 6 * 3600 : delay;
}

Due takeDue(const Moment& now, const Plan& plan, sys::RtcState& rtc) {
  Due what;
  if (rtc.silenceEndsRaw != 0 && now.raw >= rtc.silenceEndsRaw) {
    rtc.silenceEndsRaw = 0;
    what.silence = true;
  }
  if (!now.clockValid) return what;

  const int32_t day = localDay(now);
  if (rtc.firedDay != day) {
    rtc.firedDay = day;
    for (int16_t& at : rtc.firedAt) at = -1;
    rtc.bellsFired = 0;
  }
  const int minute = minuteOfDay(now);
  for (int i = 0; i < 4; i++) {
    const int at = plan.reminder[i];
    if (at < 0 || minute < at || rtc.firedAt[i] == at) continue;
    const bool onTime = minute - at <= kReminderGraceMinutes;
    // One at a time: a second office due in the same minute rings at the
    // next look, not never.
    if (onTime && what.reminder >= 0) continue;
    rtc.firedAt[i] = static_cast<int16_t>(at);
    if (onTime) what.reminder = i;
  }
  if (plan.streakReminder >= 0 && minute >= plan.streakReminder && rtc.firedAt[kStreakSlot] != plan.streakReminder) {
    rtc.firedAt[kStreakSlot] = plan.streakReminder;
    what.streak = minute - plan.streakReminder <= kReminderGraceMinutes;
  }
  if (plan.hourBell) {
    for (int i = 0; i < 3; i++) {
      const int at = kBellHours[i] * 60;
      if ((rtc.bellsFired & (1 << i)) || minute < at) continue;
      rtc.bellsFired |= static_cast<uint8_t>(1 << i);
      if (minute - at <= kBellGraceMinutes && what.bellHour < 0) what.bellHour = kBellHours[i];
    }
  }
  return what;
}

int64_t secondsToNextWake(const Moment& now, const Plan& plan, const sys::RtcState& rtc) {
  int64_t best = INT64_MAX;
  // Never sooner than a few seconds: a wake that lands a moment early just
  // sleeps a moment more, it does not spin.
  auto consider = [&](int64_t seconds) {
    if (seconds < 5) seconds = 5;
    if (seconds < best) best = seconds;
  };

  if (rtc.silenceEndsRaw != 0) consider(rtc.silenceEndsRaw - now.raw);

  if (now.clockValid) {
    const int minute = minuteOfDay(now);
    const bool sameDay = rtc.firedDay == localDay(now);
    for (int i = 0; i <= kStreakSlot; i++) {
      const int at = i == kStreakSlot ? plan.streakReminder : plan.reminder[i];
      if (at < 0) continue;
      const bool rung = sameDay && rtc.firedAt[i] == at;
      // Its minute has come and it has not rung: in a moment, not tomorrow.
      if (minute >= at && minute - at <= kReminderGraceMinutes && !rung) consider(5);
      else consider(untilLocalMinute(now, at));
    }
    if (plan.hourBell) {
      for (int i = 0; i < 3; i++) {
        const int at = kBellHours[i] * 60;
        const bool rung = sameDay && (rtc.bellsFired & (1 << i));
        if (minute >= at && minute - at <= kBellGraceMinutes && !rung) consider(5);
        else consider(untilLocalMinute(now, at));
      }
    }
    // A fresh day right after the owner's midnight.
    if (plan.canSync) consider(untilLocalMinute(now, 0) + kAfterMidnightSeconds);
  }

  if (plan.canSync && plan.syncHours > 0) {
    int64_t at;
    if (rtc.syncFailures == 0 && rtc.lastAnswerRaw != 0) {
      at = rtc.lastAnswerRaw + static_cast<int64_t>(plan.syncHours) * 3600;
      if (at <= now.raw) at = now.raw + 60;
    } else {
      // Never answered, or failing (away from every known network): climb a
      // ladder instead of trying at every opportunity.
      at = now.raw + backgroundRetryDelay(rtc.syncFailures);
    }
    consider(at - now.raw);
  }
  return best == INT64_MAX ? 0 : best;
}

}  // namespace app
