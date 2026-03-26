#pragma once
#include <IntervalTimer.h>
#include "ring_buffer.hpp"
#include "../types.hpp"
#include "../config.hpp"

// Drives the 1 kHz timing backbone.
// ISR pushes TickEvents into a ring buffer; RunLoop drains them from the main loop.
// The ISR does NOTHING except push — no Serial, no I2C, no allocation.
class TickScheduler {
public:
    bool     begin();
    void     end();
    bool     fetch(TickEvent& e) { return rb_.pop(e); }
    uint32_t dropped() const { return dropped_; }

private:
    static void isrThunk();
    void isr();

    IntervalTimer timer_;
    RingBufferSPSC<TickEvent, cfg::RB_CAP> rb_;
    volatile uint32_t tick_{0};
    volatile uint32_t dropped_{0};
    static TickScheduler* self_;
};
