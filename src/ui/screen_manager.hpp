#pragma once
#include <Arduino.h>
#include "oled_renderer.hpp"
#include "ui_ctx.hpp"
#include "views/piano_roll_view.hpp"
#include "views/settings_view.hpp"
#include "../types.hpp"   // timebase::ticksPerStep

enum class ScreenId : uint8_t { PianoRoll = 0, Settings = 1 };

// Owns all concrete views and manages the render loop.
//
// Dirty flag: draw() is a no-op unless dirty_ is set or the playhead has
// crossed a step boundary since the last frame.
// Frame cap: even when dirty, draws at most kFrameIntervalUs apart (~30 fps)
// so the sequencer CPU stays available for MIDI and input processing.
class ScreenManager {
public:
    void begin(OledRenderer* oled) {
        oled_  = oled;
        pianoRoll_.begin();
        dirty_ = true;
    }

    void markDirty() { dirty_ = true; }

    void setScreen(ScreenId id) {
        if (id != current_) { current_ = id; dirty_ = true; }
    }

    // Call every App::update(). Redraws only when needed.
    void draw(const UICtx& ctx) {
        // Playhead step-boundary detection drives dirty without external help.
        if (ctx.transport.isRunning()) {
            uint32_t stepTicks = timebase::ticksPerStep(ctx.pat.grid);
            uint8_t  curStep   = uint8_t(ctx.transport.playTick() / stepTicks);
            if (curStep != lastStep_) { lastStep_ = curStep; dirty_ = true; }
        }

        if (!dirty_) return;
        if ((ctx.now - lastDrawUs_) < kFrameIntervalUs) return;
        dirty_      = false;
        lastDrawUs_ = ctx.now;

        U8G2& gfx = oled_->gfx();
        oled_->clear();

        switch (current_) {
            case ScreenId::PianoRoll: pianoRoll_.draw(gfx, ctx); break;
            case ScreenId::Settings:  settings_ .draw(gfx, ctx); break;
        }

        oled_->send();
    }

    // Pan/zoom control for the piano roll (K6/K7/K8 can update this later).
    Viewport& pianoRollViewport() { return pianoRoll_.viewport(); }

private:
    OledRenderer*  oled_{nullptr};
    PianoRollView  pianoRoll_;
    SettingsView   settings_;
    ScreenId       current_{ScreenId::PianoRoll};
    bool           dirty_{true};
    uint32_t       lastDrawUs_{0};
    uint8_t        lastStep_{255};
    static constexpr uint32_t kFrameIntervalUs = 33333;  // ~30 fps
};
