#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace step {

struct TickWindow { uint32_t prev, curr; };

class Transport {
public:
    void setTempo(float bpm, uint16_t tpqn) {
        bpm_ = bpm;
        tpqn_ = tpqn;
        uptick_ = 60000000.0f / (bpm_ * tpqn_);
    }
    void setGridDiv(uint16_t d) {
        grid_ = d;
        tps_ = (tpqn_ * 4) / grid_;
        gate_ = tps_ / 2;
    }
    void start() { run_ = true; }
    void stop() { run_ = false; }
    void toggle() { run_ ? stop() : start(); }
    void on1ms() {
        if (!run_) return;
        acc_ += 1000;
        while (acc_ >= uptick_) {
            prev_ = tick_;
            tick_++;
            acc_ -= uptick_;
            stepEdge_ = (tick_ % tps_) == 0;
            clkDiv_++;
            if (clkDiv_ >= tpqn_ / 24) {
                clkPulse_ = true;
                clkDiv_ = 0;
            }
        }
    }
    bool next(TickWindow& w) {
        if (stepEdge_) {
            stepEdge_ = false;
            w = {prev_, tick_};
            return true;
        }
        return false;
    }
    bool clockPulse() {
        bool f = clkPulse_;
        clkPulse_ = false;
        return f && run_;
    }
    bool running() const { return run_; }
    uint16_t tps() const { return tps_; }
    uint16_t gate() const { return gate_; }

private:
    float bpm_{120};
    uint16_t tpqn_{96}, grid_{16}, tps_{24}, gate_{12};
    bool run_{false};
    uint32_t tick_{0}, prev_{0};
    uint32_t acc_{0};
    uint32_t uptick_{5208};
    bool stepEdge_{false}, clkPulse_{false};
    uint16_t clkDiv_{0};
};

} // namespace step
