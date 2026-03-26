#pragma once
#include "model/pattern.hpp"
#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "core/runloop.hpp"
#include "engine/playback_engine.hpp"

class App {
public:
    void setup();
    void update();

private:
    Pattern        pat_;
    TickScheduler  sched_;
    Transport      tx_;
    MidiIO         midi_;
    PlaybackEngine eng_;
    RunLoop        loop_;
};
