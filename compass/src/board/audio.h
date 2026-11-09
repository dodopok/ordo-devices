#pragma once

#include <stdint.h>

namespace board {

// True on boards whose speaker path this firmware knows how to drive.
bool audioAvailable();

// Rings a bell: `strikes` soft strokes through the ES8311 and the speaker.
// Powers the codec and the amplifier for the duration and puts both back to
// sleep. Blocks for about a second and a half per stroke. `volume` 0..2.
void audioBell(int strikes, uint8_t volume);

}  // namespace board
