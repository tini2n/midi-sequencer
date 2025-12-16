#pragma once
#include <stdint.h>

#include "step_track.hpp"

// Encapsulates track set for step-based cursor mode. Allows future expansion
// to multiple tracks while keeping call sites stable.
namespace step {

struct Pattern {
    Track tracks[1];
    uint8_t active{0};

    inline Track& track() { return tracks[active]; }
    inline const Track& track() const { return tracks[active]; }
};

} // namespace step
