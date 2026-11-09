#pragma once

#include <stdint.h>

// What has to outlive deep sleep without touching flash. RTC memory keeps it
// while the board has power; a cold start finds the magic number gone and
// begins again from NVS.
namespace sys {

struct RtcState {
  uint32_t magic;

  // The clock: true UTC = raw system time + offset, stretched by the drift
  // measured between the last two syncs.
  bool clockSet;
  int64_t clockOffset;      // UTC seconds minus raw seconds, at the last sync
  int64_t lastSyncRaw;      // raw seconds at the last sync
  int64_t lastSyncUtc;      // UTC seconds at the last sync
  int32_t driftPpm;
  int32_t utcOffsetSeconds; // the owner's zone
  bool utcOffsetKnown;

  // When the API last confirmed the day on record, and when to ask again
  // after a failure. Raw seconds, so they work before the clock is set.
  int64_t lastAnswerRaw;    // last 200 or 304 from /device/today; 0: never
  int64_t nextTryRaw;       // no sync before this (rate limit, repeated failure)
  // 0: fine; 1: the owner's subscription lapsed; 2: this client was disabled.
  uint8_t blocked;

  // What already rang today, so a wake a little early or late cannot ring twice.
  int32_t firedDay;         // local date, days since 1970
  uint8_t remindersFired;   // bit per office
  uint8_t bellsFired;       // bit 0: 6h, 1: 12h, 2: 18h

  // A time of silence under way.
  int64_t silenceEndsRaw;   // raw seconds; 0: none

  // Wi-Fi fast reconnect.
  char ssid[33];
  uint8_t bssid[6];
  uint8_t channel;

  // The screens.
  uint16_t beads;

  // For the "Sobre" screen.
  int32_t countersDay;
  uint32_t wakesToday;
  uint32_t syncsToday;
};

constexpr uint32_t kRtcMagic = 0x4F52444F;  // "ORDO"

RtcState& rtc();
// True when the state survived (a wake from sleep), false on a cold start.
bool rtcBegin();

}  // namespace sys
