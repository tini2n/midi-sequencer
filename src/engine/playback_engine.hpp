#pragma once
#include <vector>

#include "model/pattern.hpp"
#include "core/midi_io.hpp"
#include "core/timebase.hpp"

struct PlaybackEngine
{
    static inline uint32_t microDelayUs(int16_t micro_q8, float bpm)
    {
        if (micro_q8 <= 0)
            return 0;
        uint32_t upt = usPerTick(Tempo{bpm, 96}); // TPQN=96
        return (uint32_t)((int32_t)micro_q8 * (int32_t)upt / 256);
    }

    void processTick(uint32_t prev, uint32_t curr, const Pattern &p, std::vector<MidiEvent> &out)
    {
        const uint32_t L = p.ticks();
        const auto &trk = p.track;
        const uint8_t ch = trk.channel;

        auto inside = [&](uint32_t x) -> bool
        { return (prev < curr) ? (x > prev && x <= curr) : (x > prev || x <= curr); };

        for (const auto &n : trk.notes)
        {
            uint32_t on = n.on % L, off = (n.on + n.duration) % L;
            if (inside(on))
                out.push_back(MidiEvent{ch, n.pitch, n.vel, true, microDelayUs(n.micro_q8, p.tempo)});
            if (inside(off))
                out.push_back(MidiEvent{ch, n.pitch, 0, false, 0});
        }
    }
};
