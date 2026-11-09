#pragma once

#include <stdint.h>

// The compass has no clock chip (the 1.75C has neither an RTC nor a 32 kHz
// crystal), so the time is borrowed: every answer from the API carries a
// `Date` header, and between answers the ESP32's own timer keeps counting,
// asleep or awake. That timer drifts, mostly while asleep; the drift seen
// between two syncs is applied to the next stretch.
namespace sys {

// Seconds since power-on that keep counting through deep sleep. Not a time
// of day; good for intervals.
int64_t rawSeconds();

bool clockValid();
int64_t nowUtc();
// Sets the clock from a server's time and learns the drift since last time.
void clockSet(int64_t utcSeconds);
int32_t clockDriftPpm();

void setUtcOffset(int32_t seconds);
bool utcOffsetKnown();
int32_t utcOffset();

// The owner's local time. Only meaningful when clockValid().
int64_t localNow();
long localDays();    // days since 1970-01-01
int minuteOfDay();
int secondOfDay();
// UTC seconds of the next time the local clock shows `minute` of the day.
int64_t nextLocalTime(int minute);

// The owner's zone, worked out from an answer of an API that does not send
// `utc_offset_seconds`: the payload says how long until the owner's
// midnight, the `Date` header says what time it is in UTC.
int32_t offsetFromDayEnd(int64_t utc, uint32_t dayEndsInSeconds);

// How long the hardware timer must run for `seconds` of true time to pass.
uint64_t sleepSecondsFor(int64_t seconds);

// "Mon, 09 Nov 2026 19:46:46 GMT" -> UTC seconds. False when it is not a date.
bool parseHttpDate(const char* text, int64_t& out);

}  // namespace sys
