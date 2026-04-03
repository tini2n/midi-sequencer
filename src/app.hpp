#pragma once
#include "model/pattern.hpp"
#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "core/runloop.hpp"
#include "engine/playback_engine.hpp"
#include "io/cursor_mode.hpp"
#include "io/matrix_kb.hpp"
#include "io/encoder_manager.hpp"
#include "io/serial_monitor.hpp"

class App : public IEncoderHandler {
public:
    void setup();
    void update();

    // IEncoderHandler — routes encoder events to cursor mode
    void onEncoderRotation(const EncoderRotationEvent& e) override;
    void onEncoderButton(const EncoderButtonEvent& e) override;

private:
    Pattern        pat_;
    TickScheduler  sched_;
    Transport      tx_;
    MidiIO         midi_;
    PlaybackEngine eng_;
    RunLoop        loop_;

    CursorMode     cursor_;
    MatrixKB       kb_;
    EncoderManager enc_;
    SerialMonitor  serial_;

    bool    settingsMode_{false}; // true while CTL 6 (SETTINGS) is active
    bool    k5Held_{false};       // encoder 4 button held → ±16 step mode
};
