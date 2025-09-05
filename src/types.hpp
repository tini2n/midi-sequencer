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

struct TickEvent
{
    uint32_t tick;
    uint32_t tmicros;
};
