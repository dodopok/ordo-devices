#include "clock.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "rtc_state.h"

namespace sys {
namespace {

constexpr int32_t kMaxDriftPpm = 60000;  // 6 %: more than that is a broken reading
constexpr int64_t kMinDriftSpan = 20 * 60;

long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

}  // namespace

int64_t rawSeconds() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec);
}

bool clockValid() { return rtc().clockSet; }

int64_t nowUtc() {
  const RtcState& s = rtc();
  const int64_t raw = rawSeconds();
  if (!s.clockSet) return raw;
  const int64_t elapsed = raw - s.lastSyncRaw;
  return s.lastSyncUtc + elapsed + (elapsed * s.driftPpm) / 1000000;
}

void clockSet(int64_t utcSeconds) {
  RtcState& s = rtc();
  const int64_t raw = rawSeconds();
  if (s.clockSet) {
    const int64_t rawSpan = raw - s.lastSyncRaw;
    const int64_t trueSpan = utcSeconds - s.lastSyncUtc;
    if (rawSpan >= kMinDriftSpan && trueSpan > 0) {
      int64_t ppm = ((trueSpan - rawSpan) * 1000000) / rawSpan;
      if (ppm > kMaxDriftPpm) ppm = kMaxDriftPpm;
      if (ppm < -kMaxDriftPpm) ppm = -kMaxDriftPpm;
      // Half of the old estimate, half of the new: one odd stretch (a hot
      // car, a long time awake) does not throw the next one off.
      s.driftPpm = s.driftPpm == 0 ? static_cast<int32_t>(ppm) : static_cast<int32_t>((s.driftPpm + ppm) / 2);
    }
  }
  s.clockSet = true;
  s.lastSyncRaw = raw;
  s.lastSyncUtc = utcSeconds;
  s.clockOffset = utcSeconds - raw;
}

int32_t clockDriftPpm() { return rtc().driftPpm; }

void setUtcOffset(int32_t seconds) {
  rtc().utcOffsetSeconds = seconds;
  rtc().utcOffsetKnown = true;
}

bool utcOffsetKnown() { return rtc().utcOffsetKnown; }

int32_t utcOffset() { return rtc().utcOffsetSeconds; }

int64_t localNow() { return nowUtc() + rtc().utcOffsetSeconds; }

long localDays() { return static_cast<long>(floorDiv(localNow(), 86400)); }

int secondOfDay() {
  const int64_t local = localNow();
  return static_cast<int>(local - floorDiv(local, 86400) * 86400);
}

int minuteOfDay() { return secondOfDay() / 60; }

int64_t nextLocalTime(int minute) {
  const int64_t now = nowUtc();
  const int64_t local = now + rtc().utcOffsetSeconds;
  const int64_t dayStart = floorDiv(local, 86400) * 86400;
  int64_t at = dayStart + static_cast<int64_t>(minute) * 60;
  if (at <= local) at += 86400;
  return at - rtc().utcOffsetSeconds;
}

int32_t offsetFromDayEnd(int64_t utc, uint32_t dayEndsInSeconds) {
  const int64_t localSecond = 86400 - static_cast<int64_t>(dayEndsInSeconds);
  int64_t offset = localSecond - (utc - floorDiv(utc, 86400) * 86400);
  while (offset > 14 * 3600) offset -= 86400;
  while (offset < -12 * 3600) offset += 86400;
  // Zones sit on quarter hours.
  const int64_t quarter = 900;
  offset = floorDiv(offset + quarter / 2, quarter) * quarter;
  return static_cast<int32_t>(offset);
}

uint64_t sleepSecondsFor(int64_t seconds) {
  if (seconds <= 0) return 0;
  // The timer runs fast or slow by driftPpm; ask it for correspondingly
  // fewer or more of its own seconds.
  const int64_t adjusted = seconds - (seconds * rtc().driftPpm) / 1000000;
  return static_cast<uint64_t>(adjusted > 1 ? adjusted : 1);
}

bool parseHttpDate(const char* text, int64_t& out) {
  if (!text) return false;
  const char* comma = strchr(text, ',');
  if (!comma) return false;
  int day = 0, year = 0, hour = 0, minute = 0, second = 0;
  char month[4] = {0};
  if (sscanf(comma + 1, " %d %3s %d %d:%d:%d", &day, month, &year, &hour, &minute, &second) != 6) return false;
  static const char* names = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* found = strstr(names, month);
  if (!found || (found - names) % 3 != 0) return false;
  const unsigned m = static_cast<unsigned>((found - names) / 3 + 1);
  if (year < 2024 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return false;
  out = static_cast<int64_t>(daysFromCivil(year, m, static_cast<unsigned>(day))) * 86400 + hour * 3600 + minute * 60 + second;
  return true;
}

}  // namespace sys
