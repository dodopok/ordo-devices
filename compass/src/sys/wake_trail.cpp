#include "wake_trail.h"

#include <esp_attr.h>
#include <stddef.h>
#include <string.h>

namespace sys {
namespace {

// Change it when the layout below changes: a firmware installed over the
// air finds the old firmware's notes in this memory.
constexpr uint32_t kMagic = 0x54524C31;  // "TRL1"

struct Trail {
  uint32_t magic;
  uint8_t count;  // notes held
  uint8_t head;   // where the newest is
  WakeNote notes[kWakeNotes];
  uint32_t sum;
};

// Not initialised at start-up: what was written before a restart is still
// there after it. After power-on it is noise, which the magic number and the
// sum tell apart.
RTC_NOINIT_ATTR Trail trail;

uint32_t sumOf(const Trail& t) {
  uint32_t sum = 0x9E3779B9u;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&t);
  for (size_t i = 0; i < offsetof(Trail, sum); i++) sum = (sum ^ bytes[i]) * 16777619u;
  return sum;
}

bool plainButtonWake(const WakeNote& note) {
  return (note.cause == WakeCause::Button || note.cause == WakeCause::Touch) && !note.afterCrash && note.due == 0 &&
         note.step == WakeStep::Shown;
}

}  // namespace

void trailBegin() {
  if (trail.magic == kMagic && trail.count <= kWakeNotes && trail.head < kWakeNotes && trail.sum == sumOf(trail)) return;
  memset(&trail, 0, sizeof(trail));
  trail.magic = kMagic;
  trailSeal();
}

WakeNote& trailOpen(int16_t minute, WakeCause cause, bool afterCrash) {
  const bool button = cause == WakeCause::Button || cause == WakeCause::Touch;
  const bool replace = trail.count > 0 && button && !afterCrash && plainButtonWake(trail.notes[trail.head]);
  if (!replace) {
    trail.head = trail.count == 0 ? 0 : static_cast<uint8_t>((trail.head + 1) % kWakeNotes);
    if (trail.count < kWakeNotes) trail.count++;
  }
  WakeNote& note = trail.notes[trail.head];
  memset(&note, 0, sizeof(note));
  note.minute = minute;
  note.cause = cause;
  note.afterCrash = afterCrash;
  note.step = WakeStep::Woke;
  note.net = WakeNet::NotAsked;
  trailSeal();
  return note;
}

WakeNote& trailNow() { return trail.notes[trail.head]; }

void trailSeal() { trail.sum = sumOf(trail); }

int trailCount() { return trail.count; }

const WakeNote& trailAt(int back) {
  if (back < 0) back = 0;
  if (back >= trail.count) back = trail.count > 0 ? trail.count - 1 : 0;
  return trail.notes[(trail.head + kWakeNotes - back) % kWakeNotes];
}

}  // namespace sys
