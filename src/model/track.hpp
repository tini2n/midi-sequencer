#pragma once
#include <vector>
#include "note.hpp"

struct Track
{
    std::vector<Note> notes;

    uint8_t channel{13}; // 1-16
    uint32_t steps{0};  // 0 – use pattern length

    void clear()
    {
        notes.clear();
    }
};
