#pragma once

#include <stdint.h>

// How a new screen takes the place of the one on the glass.
//
// The new screen is drawn whole out of sight (the board keeps one frame in
// memory for that, the "stage"), and is then laid over the old one strip by
// strip, in the direction the finger went. The old screen stays lit until a
// strip passes over it, and every pixel is written once: the change costs
// the panel a single frame, however slow its bus.
//
// Only the order of the strips is decided here. It has no board and no LVGL
// in it, so the firmware (board/display.cpp) and the simulator lay down the
// very same strips.
namespace ui {

enum class Wipe : uint8_t {
  Up,     // the new screen comes in from the bottom edge: a swipe up
  Down,   // from the top edge
  Left,   // from the right edge: a swipe to the left
  Right,  // from the left edge
  Open,   // from the middle outwards, up and down: something was opened
  Close,  // from the top and the bottom towards the middle: it was closed
};

struct WipeStrip {
  int16_t x, y, w, h;
};

// Rows or columns per step. Even, and every strip starts on an even line:
// the panel addresses its pixels in pairs.
constexpr int kWipeLines = 16;
constexpr uint32_t kWipeMs = 170;

inline int wipeSteps(Wipe, int side) { return (side + kWipeLines - 1) / kWipeLines; }

// The strips of step `step`, 0 being the first laid down: one for the
// straight wipes, up to two for Open and Close. Returns how many.
inline int wipeStrips(Wipe wipe, int side, int step, WipeStrip out[2]) {
  const int steps = wipeSteps(wipe, side);
  if (step < 0 || step >= steps) return 0;
  auto clampLine = [side](int v) { return v < 0 ? 0 : (v > side ? side : v); };
  auto rows = [&](int from, int to, WipeStrip& strip) {
    from = clampLine(from);
    to = clampLine(to);
    if (to <= from) return 0;
    strip = {0, static_cast<int16_t>(from), static_cast<int16_t>(side), static_cast<int16_t>(to - from)};
    return 1;
  };
  auto columns = [&](int from, int to, WipeStrip& strip) {
    from = clampLine(from);
    to = clampLine(to);
    if (to <= from) return 0;
    strip = {static_cast<int16_t>(from), 0, static_cast<int16_t>(to - from), static_cast<int16_t>(side)};
    return 1;
  };
  switch (wipe) {
    case Wipe::Down: return rows(step * kWipeLines, (step + 1) * kWipeLines, out[0]);
    case Wipe::Up: return rows(side - (step + 1) * kWipeLines, side - step * kWipeLines, out[0]);
    case Wipe::Right: return columns(step * kWipeLines, (step + 1) * kWipeLines, out[0]);
    case Wipe::Left: return columns(side - (step + 1) * kWipeLines, side - step * kWipeLines, out[0]);
    case Wipe::Open:
    case Wipe::Close: {
      // Half a strip above the middle and half below it. The middle is taken
      // on an even line.
      const int half = kWipeLines / 2;
      const int middle = (side / 2) & ~1;
      const int k = wipe == Wipe::Open ? step : steps - 1 - step;
      int count = rows(middle - (k + 1) * half, middle - k * half, out[0]);
      count += rows(middle + k * half, middle + (k + 1) * half, out[count]);
      return count;
    }
  }
  return 0;
}

// How many steps should be on the glass `elapsedMs` into the wipe: quick
// off the mark, easing into place.
inline int wipeStepsDue(int steps, uint32_t elapsedMs, uint32_t totalMs = kWipeMs) {
  if (elapsedMs >= totalMs) return steps;
  const float p = static_cast<float>(elapsedMs) / static_cast<float>(totalMs);
  const float eased = 1.0f - (1.0f - p) * (1.0f - p);
  int due = static_cast<int>(eased * static_cast<float>(steps)) + 1;
  return due > steps ? steps : due;
}

}  // namespace ui
