#pragma once
#include <stdint.h>
#include "timebase.hpp"

struct TickWindow
{
    uint32_t prev, curr;
};

class Transport
{
public:
    bool isRunning() const { return running_; }
    void setLoopLen(uint32_t ticks)
    {
        loopLen_ = ticks ? ticks : 1;
        play_ = play_ % loopLen_;
    }
    void setTempo(float bpm)
    {
        tempo_.bpm = bpm;
        uptick_ = usPerTick(tempo_);
    }
    void start()
    {
        running_ = true;
        phase_ = 0;
    }
    void stop() { running_ = false; }
    void resume() { running_ = true; }
    void locate(uint32_t tick) { play_ = tick % loopLen_; }
    void on1ms()
    {
        if (!running_)
            return;
        phase_ += 1000;
        while (phase_ >= uptick_)
        {
            phase_ -= uptick_;
            pend_++;
        }
    }
    bool next(TickWindow &w)
    {
        if (!pend_)
            return false;
        uint32_t prev = play_;
        play_ = (play_ + 1) % loopLen_;
        pend_--;
        w = {prev, play_};
        return true;
    }
    uint32_t playTick() const { return play_; }

private:
    Tempo tempo_{};
    uint32_t uptick_{usPerTick(tempo_)};
    uint32_t loopLen_{1};
    uint32_t play_{0};
    bool running_{false};
    uint32_t pend_{0};
    uint32_t phase_{0};
};
