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
    bool isPaused() const { return paused_; }
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
        paused_ = false;
        phase_ = 0;
    }
    void stop() { running_ = false; paused_ = false; play_ = 0; phase_ = 0; pend_ = 0; }
    void pause() { paused_ = true; running_ = false; }
    void resume() { running_ = true; paused_ = false; }
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
    uint16_t tpqn() const { return tempo_.tpqn; }
    uint8_t  clockDivisor() const { return 96 / (tempo_.tpqn / 24); } // e.g. TPQN=96 → 4
    // Song position in bars:beats:ticks assuming 4/4 and step=1/16 → 4 steps per beat
    void songPos(uint32_t &bars, uint32_t &beats, uint32_t &ticks) const {
        const uint32_t step = timebase::ticksPerStep(16); // 1/16 grid
        const uint32_t stepsPerBeat = 4; // 4 sixteenths per beat in 4/4
        uint32_t stepIdx = play_ / step;
        beats = stepIdx / stepsPerBeat;
        bars = beats / 4;
        beats = beats % 4;
        ticks = play_ % step;
    }
private:
    Tempo tempo_{};
    
    bool running_{false};
    bool paused_{false};

    uint32_t uptick_{usPerTick(tempo_)};
    uint32_t loopLen_{1};
    uint32_t play_{0};
    uint32_t pend_{0};
    uint32_t phase_{0};
};
