#pragma once
#include <unordered_map>
#include <vector>
#include <Arduino.h>

#include "ui/performance_state.hpp"
#include "ui/cursor/cursor_input.hpp"
#include "ui/cursor/keyboard_mapper.hpp"
#include "ui/cursor/chord_mapper.hpp"

#include "core/midi_io.hpp"

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "ui/renderer_oled.hpp"

class PerformanceView
{
public:
    void begin(uint8_t midiCh) { st_.channel = midiCh; }
    void tick(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick);
    void handleInput(MidiIO &midi);

    void octaveUp();
    void octaveDown();
    void cycleMode();
    void toggleHold();
    void panic(MidiIO &m);

private:
    PerformanceState st_{};
    CursorInput input_;
    KeyboardMapper km_;
    ChordMapper cm_;

    std::unordered_map<uint8_t, uint8_t> active_; // btnId->pitch
    int8_t lastPitch_ = -1;

    void sendOn(MidiIO &m, uint8_t p) { m.send(MidiEvent{st_.channel, p, 127, true, 0}); }
    void sendOff(MidiIO &m, uint8_t p) { m.send(MidiEvent{st_.channel, p, 0, false, 0}); }
    
    void handleFunction(uint8_t btn);
};
