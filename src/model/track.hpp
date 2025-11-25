#pragma once
#include <vector>
#include <algorithm>
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
    
    // Sort notes by time for faster rendering culling
    void sortByTime()
    {
        std::sort(notes.begin(), notes.end(), 
                  [](const Note& a, const Note& b) { return a.on < b.on; });
    }
};
