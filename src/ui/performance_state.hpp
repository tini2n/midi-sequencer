#pragma once
#include <stdint.h>

enum class PerformanceMode : uint8_t { Keyboard=0, Scale=1, Chromatic=2, Chord=3 };

struct PerformanceState {
  PerformanceMode mode = PerformanceMode::Keyboard;
  int8_t  octave = 4;          // MIDI 0–10 (clamped)
  uint8_t root   = 0;          // 0=C … 11=B
  uint16_t scaleMask = 0xAD5;  // C major default (1010 1101 0101)b
  uint8_t channel = 13;        // use Pattern.track.channel at init
};
