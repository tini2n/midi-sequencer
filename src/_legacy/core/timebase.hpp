#pragma once
#include <stdint.h>
struct Tempo
{
    float bpm{120.f};
    uint16_t tpqn{96};
};

inline uint32_t usPerTick(const Tempo &t)
{
    return (uint32_t)(60000000.0f / (t.bpm * t.tpqn));
}