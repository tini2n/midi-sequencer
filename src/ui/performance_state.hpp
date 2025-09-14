#pragma once
#include <stdint.h>

enum class PerformanceMode : uint8_t
{
  Keyboard = 0,
  Scale = 1,
  Chromatic = 2,
  Chord = 3
};

struct PerformanceState
{
  PerformanceMode mode = PerformanceMode::Keyboard;
  uint8_t channel{1};                  // use Pattern.track.channel at init
  int8_t octave{0};                     // MIDI 0–10 (clamped)
  uint8_t root{0};                      // 0=C … 11=B
  uint8_t  velocity{127}; 
  uint8_t  lastPitch{255};
};
