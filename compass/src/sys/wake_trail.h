#pragma once

#include <stdint.h>

// The last few times the compass woke, and how far each wake got. It is what
// the "Sobre" screen reads out when a reminder did not ring: whether the
// compass woke at all, what it thought was due, and where it stopped.
//
// The notes live in memory that survives a crash as well as sleep, so the
// wake after a crash can tell of the one that crashed. Nothing here touches
// the board: the screens and the tests on a computer include it as it is.
namespace sys {

// Why it woke.
enum class WakeCause : uint8_t { PowerOn, Button, Touch, Timer };

// What was due at that wake, a bit each.
constexpr uint8_t kDueMorning = 1 << 0;   // bits 0..3: the four offices, in order
constexpr uint8_t kDueStreak = 1 << 4;
constexpr uint8_t kDueBell = 1 << 5;
constexpr uint8_t kDueSilence = 1 << 6;
constexpr uint8_t kDueTest = 1 << 7;

// How far the wake got. A wake that ends well ends in Slept, Skipped, Rang
// or Shown; any other step left standing is where it stopped.
enum class WakeStep : uint8_t {
  Woke,      // started, nothing more yet
  Syncing,   // talking to the network with the screen dark
  Slept,     // nothing to ring: back to sleep
  Skipped,   // a reminder for an office already prayed: back to sleep
  Lighting,  // bringing the screen up to ring
  Rang,      // the notice was shown and the bell rang
  Shown,     // a button woke it: the day is on the glass
};

// What the network said, when it was asked.
enum class WakeNet : uint8_t {
  NotAsked,
  Answered,   // the API answered
  NoWifi,     // no known network joined
  NoAnswer,   // joined, but the request failed
  TimedOut,   // gave up waiting
};

struct WakeNote {
  int16_t minute;   // local minute of the day; -1: the clock was not set
  WakeCause cause;
  // True when the processor had restarted by itself before this wake (a
  // crash, a watchdog, a dip in the supply) rather than slept or been
  // switched on.
  bool afterCrash;
  uint8_t due;      // kDue* bits
  WakeStep step;
  WakeNet net;
  uint8_t seconds;  // from the wake to the last step, 255 at most
};

constexpr int kWakeNotes = 10;

// Checks the memory (after power-on it holds noise) and empties it if it
// does not check out. Call once per boot, before the others.
void trailBegin();
// Starts the note for this wake and returns it. Button wakes that found
// nothing to do are not worth a line each: one that follows another takes
// its place.
WakeNote& trailOpen(int16_t minute, WakeCause cause, bool afterCrash);
// The note of this wake (the one trailOpen returned).
WakeNote& trailNow();
// Call after changing a note: the memory is checked by a sum.
void trailSeal();
int trailCount();
// 0: this wake; 1: the one before; and so on.
const WakeNote& trailAt(int back);

}  // namespace sys
