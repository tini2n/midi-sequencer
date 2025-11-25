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
  uint8_t velocity{127}; 
  uint8_t lastPitch{255};
  // Scale filtering
  uint8_t scale{0};     // 0=off, 1=Dorian, 2=Lydian (maps to Scale enum)
  bool fold{false};     // When true and scale!=off → map all 16 keys to scale degrees
};
