#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>

#include "config.hpp"
#include "core/ring_buffer.hpp"

namespace step {

struct TickEvent { uint32_t tick; uint32_t us; };

// 1 kHz ISR producing timestamps into a lock-free SPSC ring buffer.
class TickScheduler {
public:
    bool begin(uint32_t tick_us = 1000) {
        self_ = this;
        tick_ = 0;
        dropped_ = 0;
        return timer_.begin(isrThunk, tick_us);
    }

    void end() {
        timer_.end();
        self_ = nullptr;
    }

    bool fetch(TickEvent& e) { return rb_.pop(e); }
    uint32_t dropped() const { return dropped_; }

private:
    static void isrThunk() { if (self_) self_->isr(); }
    void isr() {
        TickEvent e{tick_++, micros()};
        if (!rb_.push(e)) {
            dropped_++;
        }
    }

    IntervalTimer timer_;
    RingBufferSPSC<TickEvent, cfg::RB_CAP> rb_{};
    volatile uint32_t tick_{0};
    volatile uint32_t dropped_{0};
    static TickScheduler* self_;
};

inline TickScheduler* TickScheduler::self_ = nullptr;

} // namespace step
