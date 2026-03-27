#pragma once
#include <stdint.h>
#include "track.hpp"
#include "../types.hpp"

// A pattern is one looping sequence: up to 16 independent tracks, a shared length, and a tempo.
// Inspired by Digitakt-style per-track sequencing: each track has its own MIDI channel,
// recorded layer (user notes), and generative layer (algorithm output).
//
// All tracks share the same steps/grid/tempo — they loop in lockstep.
// Per-track length override is available via LayeredTrack::steps (0 = use pattern length).
//
// steps × grid define the total length:
//   ticks() = ticksPerStep(grid) * steps
//
// Example: steps=64, grid=16, PPQN=96
//   ticksPerStep(16) = (96 * 4) / 16 = 24 ticks per 1/16 step
//   ticks()          = 24 * 64 = 1536 ticks = 4 bars in 4/4

static constexpr uint8_t MAX_TRACKS = 2; // 2 tracks for development (one per MIDI output channel)

struct Pattern {
    LayeredTrack tracks[MAX_TRACKS];  // tracks[0..15], each on its own MIDI channel

    uint8_t steps{64};      // number of grid steps (default: 4 bars of 1/16)
    uint8_t grid{16};       // grid division: 4=quarter, 8=eighth, 16=sixteenth, 32=thirty-second
    float   tempo{120.f};   // BPM

    // TODO: per-track step length (Digitakt-style track length mismatch for polyrhythms)
    // Each track could loop independently at a different step count while sharing the pattern tempo.
    // Implement when adding per-track length UI controls.

    // Total pattern length in ticks. PlaybackEngine wraps each track's playhead at this boundary
    // unless LayeredTrack::steps overrides it for that track.
    uint32_t ticks() const {
        return timebase::ticksPerStep(grid) * steps;
    }
};
