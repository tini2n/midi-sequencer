#include "app.hpp"
#include "types.hpp"

// Hardcoded test pattern: 4 notes on beats 1–4 of track 0, channel 1, 120 BPM.
// Replace with real pattern loading once RecordEngine and UI are in place (Phase 3+).
static void loadTestPattern(Pattern& pat) {
    pat.tempo = 120.f;
    pat.grid  = 16;
    pat.steps = 16; // 1 bar of 1/16 steps

    const uint32_t step = timebase::ticksPerStep(pat.grid); // 24 ticks per 1/16

    // Track 0, MIDI channel 1: kick on every beat (steps 0, 4, 8, 12)
    LayeredTrack& t0 = pat.tracks[0];
    t0.channel = 1;
    t0.recorded.push({0 * step,  step - 2, 0, 36, 100, 0}); // beat 1
    t0.recorded.push({4 * step,  step - 2, 0, 36, 100, 0}); // beat 2
    t0.recorded.push({8 * step,  step - 2, 0, 36, 100, 0}); // beat 3
    t0.recorded.push({12 * step, step - 2, 0, 36, 100, 0}); // beat 4
    t0.recorded.sortByOnTick();
}

void App::setup() {
    loadTestPattern(pat_);

    midi_.begin();
    sched_.begin();
    tx_.setTempo(pat_.tempo);
    tx_.setLoopLen(pat_.ticks());
    tx_.start();
    eng_.reset();

    loop_.begin(&sched_, &tx_, &eng_, &midi_, &pat_);
}

void App::update() {
    loop_.service();
}
