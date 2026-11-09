#pragma once

#include <stddef.h>
#include <stdint.h>

// The celebration's picture, from the file the content host sends to pixels
// LVGL can draw. Nothing here knows about flash or the network: the firmware
// and the simulator both hand in bytes and get pixels back.
namespace content {

// The side of the square the host renders (the glass is 466 px across).
constexpr int kArtSide = 466;

// Decodes a baseline JPEG of exactly `side` x `side` into `out`, RGB565 in
// the processor's byte order, one row after the other. False when the bytes
// are not such a picture; `out` is then left black.
bool decodeArt(const uint8_t* jpeg, size_t length, uint16_t* out, int side = kArtSide);

}  // namespace content
