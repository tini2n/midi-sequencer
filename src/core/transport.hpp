#pragma once
#include <stdint.h>
#include "timebase.hpp"
#include "../types.hpp"

// A tick window: the half-open interval (prev, curr] of pattern ticks processed this call.
// When curr < prev the pattern just wrapped — PlaybackEngine resets cursors on wrap.
struct TickWindow {
    uint32_t prev;
    uint32_t curr;
};

// Playhead state machine. Called once per scheduler tick (onTick()), advances an internal
// phase accumulator to produce play ticks at the correct BPM. Caller drains windows via next().
class Transport {
public:
    bool isRunning() const { return running_; }
    bool isPaused()  const { return paused_; }

    void setTempo(float bpm)      { tempo_.bpm = bpm; uptick_ = usPerTick(tempo_); }
    void setLoopLen(uint32_t ticks) { loopLen_ = ticks ? ticks : 1; play_ %= loopLen_; }

    void start()  { running_ = true;  paused_ = false; phase_ = 0; }
    void stop()   { running_ = false; paused_ = false; play_  = 0; phase_ = 0; pend_ = 0; }
    void pause()  { paused_  = true;  running_ = false; }
    void resume() { running_ = true;  paused_  = false; }
    void locate(uint32_t tick) { play_ = tick % loopLen_; }

    // Call once per scheduler fetch (i.e., once per 1 ms timer tick).
    // Accumulates fractional-tick phase to tolerate jitter without drift.
    void onTick() {
        if (!running_) return;
        phase_ += 1000; // 1000 µs per scheduler tick
        while (phase_ >= uptick_) {
            phase_ -= uptick_;
            pend_++;
        }
    }

    // Pop the next pending play tick. Returns false when no ticks are pending.
    // Call in a while loop until it returns false.
    bool next(TickWindow& w) {
        if (!pend_) return false;
        uint32_t prev = play_;
        play_ = (play_ + 1) % loopLen_;
        pend_--;
        w = {prev, play_};
        return true;
    }

    uint32_t playTick() const { return play_; }
    float    bpm()      const { return tempo_.bpm; }

    // Song position in bars:beats:sub-ticks (4/4, 1/16 grid).
    void songPos(uint32_t& bars, uint32_t& beats, uint32_t& ticks) const {
        const uint32_t step = timebase::ticksPerStep(16);
        uint32_t stepIdx = play_ / step;
        beats = stepIdx / 4;
        bars  = beats / 4;
        beats = beats % 4;
        ticks = play_ % step;
    }

private:
    Tempo    tempo_{};
    bool     running_{false};
    bool     paused_{false};
    uint32_t uptick_{usPerTick(tempo_)};
    uint32_t loopLen_{1};
    uint32_t play_{0};
    uint32_t pend_{0};
    uint32_t phase_{0};
};
