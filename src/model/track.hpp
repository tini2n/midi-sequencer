#pragma once
#include "note_pool.hpp"

// A track holds two independent note layers on one MIDI channel.
//
// recorded   — notes the user played or entered manually.
//              Survives a generator run; never overwritten by algorithms.
//
// generative — notes produced by the active generator.
//              Replaced atomically on each generate call (see GeneratorManager).
//
// Both layers play back simultaneously. PlaybackEngine iterates each with its
// own cursor so neither layer affects the other's timing.

struct LayeredTrack {
    NotePool<128> recorded;    // up to 128 user-edited steps (cursor mode: 8 pages × 16 steps)
    NotePool<128> generative;  // up to 128 generator-produced notes

    uint8_t  channel{1};       // MIDI channel 1–16
    uint32_t steps{0};         // per-track length override in ticks; 0 = use Pattern::steps

    // Clear both layers. Does not change channel or steps.
    void clear() {
        recorded.clear();
        generative.clear();
    }

    // Sort both layers by on-tick. Call after recording or generation,
    // before starting or resuming playback.
    void sortByOnTick() {
        recorded.sortByOnTick();
        generative.sortByOnTick();
    }
};
