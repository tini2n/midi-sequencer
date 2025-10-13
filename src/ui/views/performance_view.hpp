#pragma once
#include <unordered_map>
#include <vector>
#include <Arduino.h>

#include "config.hpp"

#include "ui/performance_state.hpp"

#include "core/midi_io.hpp"

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/cursor/matrix_kb.hpp"

class PerformanceView
{
public:
    void begin(uint8_t midiCh)
    {
        st_.channel = midiCh;
        st_.root = 0;
        st_.octave = 4;
        MatrixKB::Config config;
        config.address = cfg::PCF_ADDRESS;
        mkb_.begin(config, 0, 4, 100);
    }
    void attach(class RunLoop* rl, class RecordEngine* re, class Transport* tx)
    {
        mkb_.attach(rl, re, tx);
    }
    void draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick);
    void poll(MidiIO &midi)
    {
        int last = -1;
        mkb_.poll(midi, st_.channel, &last);

        if (last >= 0)
            st_.lastPitch = last;
    }

    void setRoot(uint8_t semis) { mkb_.setRoot(semis); st_.root = semis % 12; }
    void setOctave(int8_t o) { mkb_.setOctave(o); st_.octave = o; }

    PerformanceState &state() { return st_; }
    void setLastPitch(int p) { st_.lastPitch = p; }

private:
    PerformanceState st_{};
    MatrixKB mkb_{};
};
