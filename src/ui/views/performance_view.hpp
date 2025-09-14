#pragma once
#include <unordered_map>
#include <vector>
#include <Arduino.h>

#include "ui/performance_state.hpp"

#include "core/midi_io.hpp"

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/cursor/serial_keyboard_input.hpp"

class PerformanceView
{
public:
    void begin(uint8_t midiCh)
    {
        st_.channel = midiCh;
        kb_.begin(0, 4, 100); // root=C, octave=4, vel=100
    }
    void draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick);
    void poll(MidiIO &midi)
    {
        int last = -1;
        kb_.poll(midi, st_.channel, &last);
        if (last >= 0)
            st_.lastPitch = last;
    }

    void setRoot(uint8_t semis) { kb_.setRoot(semis); }
    void setOctave(int8_t o) { kb_.setOctave(o); }

    PerformanceState &state() { return st_; }
    void setLastPitch(int p) { st_.lastPitch = p; }

private:
    PerformanceState st_{};
    SerialKB kb_;
};
