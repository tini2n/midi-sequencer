# Memory Model

## NotePool

Fixed-capacity container replacing `std::vector<Note>`.

```cpp
template <size_t N>
struct NotePool {
    Note notes[N];
    uint16_t count{0};

    bool push(const Note& n);        // returns false if full
    void clear();
    void sortByOnTick();             // insertion sort — N is small (<= 256)
    // iterator support for range-for
};
```

Sizes:
- Recorded layer: `NotePool<256>` (~5.5 KB)
- Generative layer: `NotePool<128>` (~2.8 KB)
- Staging buffer: `NotePool<128>` (generator writes here)

## LayeredTrack

```cpp
struct LayeredTrack {
    NotePool<256> recorded;    // user-recorded notes, survives generate
    NotePool<128> generative;  // last generator output
    uint8_t channel{1};

    void swapGenerative(NotePool<128>& staging); // atomic swap between ticks
};
```

## Pattern

```cpp
struct Pattern {
    LayeredTrack track;
    uint8_t steps{64};
    uint8_t grid{16};
    float tempo{120.f};

    uint32_t ticks() const;
};
```

## Staging Buffer

The `GeneratorManager` holds one `NotePool<128> staging_`.
On generate:
1. Generator fills `staging_` (clears it first).
2. After `RunLoop::service()` finishes a full pass (between ticks), `GeneratorManager`
   calls `pattern.track.swapGenerative(staging_)`.
3. Next tick sees the new generative layer. Old layer is gone.

This prevents mid-playback corruption without needing a mutex (single-core, cooperative).

## Playback Cursor

`PlaybackEngine` maintains one cursor index per layer:

```cpp
uint16_t recCursor_{0};
uint16_t genCursor_{0};
```

On loop wrap (`curr < prev`), cursors reset to 0.
On normal advance, cursor walks forward through sorted notes until `note.on > curr`.
Result: O(1) amortized per tick for typical patterns.
