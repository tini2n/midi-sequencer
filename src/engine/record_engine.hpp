#pragma once
#include <stdint.h>
#include <unordered_map>
#include "model/pattern.hpp"
#include "core/transport.hpp"
#include "core/timebase.hpp"

class RecordEngine
{
public:
    void begin(Pattern *pat, Transport *tx)
    {
        pat_ = pat;
        tx_ = tx;
    }

    void arm(bool on)
    {
        armed_ = on;
        if (!on)
            punching_ = false;
    }
    bool isArmed() const { return armed_; }
    bool isPunching() const { return punching_; }

    // Called on live performance events (already sent to MIDI)
    void onLiveNoteOn(uint8_t pitch, uint8_t vel, uint32_t tick)
    {
        if (!armed_ || !tx_ || !tx_->isRunning())
            return;
        if (!punching_)
            punching_ = true; // first note starts punching
        uint32_t on = quantize(tick);
        start_[pitch] = {on, vel};
    }
    void onLiveNoteOff(uint8_t pitch, uint32_t tick)
    {
        if (!armed_ || !tx_)
            return;
        auto it = start_.find(pitch);
        if (it == start_.end())
            return;
        uint32_t off = quantize(tick);
        uint32_t on = it->second.on;
        uint8_t vel = it->second.vel;
        start_.erase(it);
        if (!pat_)
            return;
        const uint32_t L = pat_->ticks() ? pat_->ticks() : 1u;
        // duration with wrap handling
        uint32_t dur = (off >= on) ? (off - on) : ((L - on) + off);
        if (dur == 0)
            dur = 1; // avoid zero-length
        Note n{};
        n.on = on % L;
        n.duration = dur;
        n.micro = 0;
        n.micro_q8 = 0;
        n.pitch = pitch;
        n.vel = vel;
        n.flags = 0;
        pat_->track.notes.push_back(n);
    }

private:
    struct Pending
    {
        uint32_t on;
        uint8_t vel;
    };
    Pattern *pat_{nullptr};
    Transport *tx_{nullptr};
    bool armed_{false};
    bool punching_{false};
    std::unordered_map<uint8_t, Pending> start_;

    inline uint32_t stepTicks() const
    {
        return pat_ ? timebase::ticksPerStep(pat_->grid) : 24u; // default 1/16 at PPQN=96
    }
    inline uint32_t loopLen() const { return pat_ ? pat_->ticks() : 1u; }
    // Round to nearest grid step and clamp to loop
    uint32_t quantize(uint32_t t) const
    {
        uint32_t s = stepTicks();
        uint32_t L = loopLen();
        uint32_t r = (t + s / 2) / s; // nearest step index
        uint32_t q = (r * s) % (L ? L : 1u);
        return q;
    }
};
