#pragma once
#include <stdint.h>

// Bitflags stored in Note::flags
enum NoteFlags : uint8_t {
    NF_Mute = 1 << 0   // note exists but is silenced during playback
};

// A single musical event in a pattern.
// All timing is in ticks (see timebase::PPQN in types.hpp).
struct Note {
    uint32_t on;        // tick at which note-on fires (relative to pattern start)
    uint32_t duration;  // length in ticks; note-off fires at (on + duration)

    int16_t  micro_q8;  // sub-tick microtiming offset in 1/256 tick units
                        // positive = delay note-on slightly for humanization
                        // only applied at playback; 0 means no offset

    uint8_t  pitch;     // MIDI pitch 0–127
    uint8_t  vel;       // MIDI velocity 0–127
    uint8_t  flags;     // bitset of NoteFlags
};
