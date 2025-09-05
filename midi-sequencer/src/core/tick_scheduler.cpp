#include "tick_scheduler.hpp"

TickScheduler *TickScheduler::self_ = nullptr;

bool TickScheduler::begin()
{
    self_ = this;
    tick_ = 0;
    dropped_ = 0;

    return timer_.begin(isrThunk, cfg::TICK_US);
}

void TickScheduler::end()
{
    timer_.end();
    self_ = nullptr;
}

void TickScheduler::isr()
{
    TickEvent e{tick_++, micros()};
    if (!rb_.push(e))
        dropped_++;
}

void TickScheduler::isrThunk()
{
    if (self_)
        self_->isr();
}
