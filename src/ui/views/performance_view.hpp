#pragma once
#include <unordered_map>
#include <vector>
#include <Arduino.h>

#include "config.hpp"

#include "ui/performance_state.hpp"
#include "ui/views/base_view.hpp"

#include "core/midi_io.hpp"

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/cursor/matrix_kb.hpp"
#include "model/scale.hpp"
#include "io/encoder_manager.hpp"

class PerformanceView : public IView, public IEncoderHandler
{
public:
    // IView interface implementation
    void begin(uint8_t midiCh) override
    {
        st_.channel = midiCh;
        st_.root = 0;
        st_.octave = 4;
        MatrixKB::Config config;
        config.address = cfg::PCF_ADDRESS;
        mkb_.begin(config, 0, 4, 100);
    }
    
    void attach(RunLoop* rl, RecordEngine* re, Transport* tx, class ViewManager* vm = nullptr) override
    {
        mkb_.attach(rl, re, tx);
        if (vm != nullptr) {
            mkb_.attachViewManager(vm);
        }
    }
    
    void draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick) override;
    
    void poll(MidiIO &midi) override
    {
        int last = -1;
        mkb_.poll(midi, st_.channel, &last);

        if (last >= 0)
            st_.lastPitch = last;
    }
    
    const char* getName() const override { return "Performance"; }

    // Performance-specific methods
    void setRoot(uint8_t semis) { mkb_.setRoot(semis); st_.root = semis % 12; }
    void setOctave(int8_t o) { mkb_.setOctave(o); st_.octave = o; }
    void setScale(Scale s) { mkb_.setScale(s); st_.scale = (uint8_t)s; }
    void setFold(bool f) { mkb_.setFold(f); st_.fold = f; }

    PerformanceState &state() { return st_; }
    void setLastPitch(int p) { st_.lastPitch = p; }
    
    // IEncoderHandler interface
    void onEncoderRotation(const EncoderRotationEvent& event) override;
    void onEncoderButton(const EncoderButtonEvent& event) override;

private:
    PerformanceState st_{};
    MatrixKB mkb_{};
};
