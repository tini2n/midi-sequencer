#pragma once
#include <unordered_map>
#include <vector>
#include <Arduino.h>

#include "config.hpp"

#include "ui/performance_state.hpp"
#include "ui/views/base_view.hpp"
#include "ui/renderer_oled.hpp"

#include "core/midi_io.hpp"

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "model/scale.hpp"
#include "io/matrix_kb.hpp"
#include "io/matrix_kb_mode.hpp"
#include "io/encoder_manager.hpp"

class PerformanceView : public IView, public IEncoderHandler
{
public:
    // IView interface implementation
    void begin(uint8_t midiCh) override
    {
        st_.channel = midiCh;
    }
    
    void attach(RunLoop* rl, RecordEngine* re, Transport* tx, class ViewManager* vm = nullptr) override
    {
        rl_ = rl;
        re_ = re;
        tx_ = tx;
        vm_ = vm;
    }
    
    void setMatrixKB(MatrixKB* mkb) { mkb_ = mkb; }
    
    void draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick) override;
    
    void poll(MidiIO &midi) override
    {
        if (!mkb_) return;
        int last = -1;
        mkb_->poll(midi, st_.channel, &last);

        if (last >= 0)
            st_.lastPitch = last;
    }
    
    void onActivate() override
    {
        if (mkb_) mkb_->setMode(MatrixKB::Mode::Performance);
        Serial.println("=== PerformanceView Activated ===");
        Serial.println("Matrix KB: Piano keyboard mode");
        Serial.println("  ENC1 - Root, ENC2 - Octave, ENC3 - Scale");
    }
    
    void onDeactivate() override
    {
        Serial.println("PerformanceView deactivated");
    }
    
    const char* getName() const override { return "Performance"; }
    
    // Provide encoder handler interface
    IEncoderHandler* getEncoderHandler() override { return this; }

    // Performance-specific methods
    void setRoot(uint8_t semis) { if (mkb_) mkb_->setRoot(semis); }
    void setOctave(int8_t o) { if (mkb_) mkb_->setOctave(o); }
    void setScale(Scale s) { if (mkb_) mkb_->setScale(s); st_.scale = (uint8_t)s; }
    void setFold(bool f) { if (mkb_) mkb_->setFold(f); st_.fold = f; }
    
    // Getters that read from mode config
    uint8_t getRoot() const
    {
        if (!mkb_) return 0;
        IModeConfig cfg;
        mkb_->getModeConfig(cfg);
        return cfg.root;
    }
    
    int8_t getOctave() const
    {
        if (!mkb_) return 4;
        IModeConfig cfg;
        mkb_->getModeConfig(cfg);
        return cfg.octave;
    }

    PerformanceState &state() { return st_; }
    void setLastPitch(int p) { st_.lastPitch = p; }
    
    // IEncoderHandler interface
    void onEncoderRotation(const EncoderRotationEvent& event) override;
    void onEncoderButton(const EncoderButtonEvent& event) override;

private:
    PerformanceState st_{};
    MatrixKB* mkb_{nullptr};
    RunLoop* rl_{nullptr};
    RecordEngine* re_{nullptr};
    Transport* tx_{nullptr};
    class ViewManager* vm_{nullptr};
};
