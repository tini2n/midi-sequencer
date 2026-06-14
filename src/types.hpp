#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace timebase {
  constexpr uint16_t PPQN = 96;           // Teensy-friendly
  inline uint32_t ticksPerStep(uint16_t gridDiv){ // 4/4 only for now
    // ticks/step = (PPQN * 4) / gridDiv; e.g., 96*4/16 = 24
    return (uint32_t(PPQN) * 4u) / gridDiv;
  }
}

// Sequencer grid / position helpers. All positions & lengths are absolute ticks.
// GRID is a 4/4 division (divisor): 4=1/4, 8=1/8, 16=1/16, 32=1/32, 64=1/64.
// gridTicks(div) = ticks per one grid cell = 96, 48, 24, 12, 6.
namespace seq {
  constexpr int      TICKS_PER_BEAT      = 96;    // one 1/4 note
  constexpr int      TICKS_PER_BAR       = 384;   // 16 steps / 4 beats
  constexpr int      BUCKETS             = 16;    // the 2x8 pads = 16 time buckets
  constexpr uint32_t TRACK_LEN_MIN_TICKS = 24;    // 1 step @ 1/16
  constexpr uint32_t TRACK_LEN_MAX_TICKS = 3072;  // 128 steps @ 1/16 = 8 bars

  // Grid divisors, coarse -> fine. Index used for clamped (non-wrapping) cycling.
  constexpr uint8_t GRID_DIVS[5] = {4, 8, 16, 32, 64};

  inline uint32_t gridTicks(uint8_t div)   { return timebase::ticksPerStep(div); }
  inline uint32_t visibleSpanTicks(uint8_t div) { return uint32_t(BUCKETS) * gridTicks(div); }

  // Cycle the grid divisor by dir. Convention: dir>0 = finer (smaller cell, zoom in).
  // Clamps at 1/4 and 1/64 — does not wrap.
  inline uint8_t cycleGrid(uint8_t div, int dir) {
    int idx = 2;  // default 1/16
    for (int i = 0; i < 5; ++i) if (GRID_DIVS[i] == div) { idx = i; break; }
    idx += (dir > 0) ? 1 : -1;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return GRID_DIVS[idx];
  }

  inline uint32_t alignDown(uint32_t v, uint32_t step) { return step ? v - (v % step) : v; }

  // 1-based bar / beat for a position (e.g. "2:3" = bar 2, beat 3).
  inline int barOf (uint32_t tick) { return int(tick / TICKS_PER_BAR) + 1; }
  inline int beatOf(uint32_t tick) { return int((tick % TICKS_PER_BAR) / TICKS_PER_BEAT) + 1; }
}

struct TickEvent
{
    uint32_t tick;
    uint32_t tmicros;
};
