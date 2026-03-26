#pragma once
#include <stdint.h>
#include "note.hpp"

// Fixed-size pool of Note values.
//
// Replaces std::vector<Note> with a statically-allocated array.
// No heap, no fragmentation. Capacity N is a compile-time constant.
//
// Usage:
//   NotePool<256> pool;
//   pool.push({.on=0, .duration=24, .pitch=60, .vel=100});
//   pool.sortByOnTick();  // required before playback
//
// Typical sizes:
//   NotePool<256>  — recorded layer (user-played notes)
//   NotePool<128>  — generative layer (algorithm output)

template <size_t N>
struct NotePool {
    Note     notes[N];
    uint16_t count{0};

    // --- Capacity -----------------------------------------------------------

    static constexpr uint16_t capacity() { return static_cast<uint16_t>(N); }
    bool isFull()  const { return count >= N; }
    bool isEmpty() const { return count == 0; }

    // --- Add / Remove -------------------------------------------------------

    // Append a note to the end. Returns false and does nothing if pool is full.
    bool push(const Note& n) {
        if (count >= N) return false;
        notes[count++] = n;
        return true;
    }

    // Remove the note at position `index` by shifting elements left.
    // Returns false if index is out of range.
    // O(n) — not for use in the ISR or RunLoop tick drain.
    bool removeAt(uint16_t index) {
        if (index >= count) return false;
        for (uint16_t i = index; i < count - 1u; ++i)
            notes[i] = notes[i + 1];
        --count;
        return true;
    }

    // Reset pool to empty. Does not zero the array — O(1).
    void clear() { count = 0; }

    // --- Indexed access -----------------------------------------------------

    Note&       operator[](uint16_t i)       { return notes[i]; }
    const Note& operator[](uint16_t i) const { return notes[i]; }

    // --- Iterators (enables range-for loops) --------------------------------

    Note*       begin()       { return notes; }
    Note*       end()         { return notes + count; }
    const Note* begin() const { return notes; }
    const Note* end()   const { return notes + count; }

    // --- Sorting ------------------------------------------------------------

    // Sort all notes by ascending on-tick using insertion sort.
    // Must be called after adding notes and before starting playback,
    // because PlaybackEngine uses a cursor that assumes sorted order.
    // O(n²) — only call from outside the RunLoop (e.g. after record/generate).
    void sortByOnTick() {
        for (uint16_t i = 1; i < count; ++i) {
            Note key = notes[i];
            int16_t j = static_cast<int16_t>(i) - 1;
            while (j >= 0 && notes[j].on > key.on) {
                notes[j + 1] = notes[j];
                --j;
            }
            notes[j + 1] = key;
        }
    }
};
