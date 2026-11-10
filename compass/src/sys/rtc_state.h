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
  uint8_t syncFailures;     // syncs in a row that got no answer
  // 0: fine; 1: the owner's subscription lapsed; 2: this client was disabled.
  uint8_t blocked;

  // What already rang today, so a wake a little early or late cannot ring twice.
  int32_t firedDay;         // local date, days since 1970
  // The minute each reminder was settled for today (rung, or let pass because
  // the compass only learnt of it later); -1: not yet. Four offices and the
  // streak reminder. Keeping the minute, not a flag, lets a reminder moved to
  // a later hour the same day ring again.
  int16_t firedAt[5];
  uint8_t bellsFired;       // bit 0: 6h, 1: 12h, 2: 18h

  // A time of silence under way.
  int64_t silenceEndsRaw;   // raw seconds; 0: none
  // A reminder asked for from the settings, to see the compass wake by
  // itself and ring: when it is due.
  int64_t testAtRaw;        // raw seconds; 0: none

  // Wi-Fi fast reconnect.
  char ssid[33];
  uint8_t bssid[6];
  uint8_t channel;
  int8_t rssi;              // of the network last joined, dBm; 0: none yet

  // The screens.
  uint16_t beads;

  // Firmware updates: when the site was last asked, and what it offered.
  int64_t updateCheckedRaw;  // raw seconds; 0: never since power-on
  char updateVersion[16];    // "0.4.1"; empty: nothing newer was on offer

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
