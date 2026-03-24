# Exec Plan 01 — Data Model Redesign

**Status:** Not started
**Goal:** Replace heap-allocated model with fixed-size, embedded-safe types.
**Touches:** `src/model/`, `src/engine/playback_engine.hpp`, `src/engine/record_engine.hpp`

---

## Steps

### 1. Define `NotePool<N>` in `src/model/note_pool.hpp`

```cpp
template <size_t N>
struct NotePool {
    Note notes[N];
    uint16_t count{0};

    bool push(const Note& n) {
        if (count >= N) return false;
        notes[count++] = n;
        return true;
    }
    void clear() { count = 0; }
    void sortByOnTick(); // insertion sort, N <= 256 is fine
    Note* begin() { return notes; }
    Note* end()   { return notes + count; }
    const Note* begin() const { return notes; }
    const Note* end()   const { return notes + count; }
};
```

### 2. Define `LayeredTrack` in `src/model/track.hpp`

Replace `std::vector<Note> notes` with:
```cpp
struct LayeredTrack {
    NotePool<256> recorded;
    NotePool<128> generative;
    uint8_t channel{1};
};
```
Remove `sortByTime()` — `NotePool::sortByOnTick()` covers it.

### 3. Update `Pattern`

`Track track` → `LayeredTrack track`. No other change to `Pattern`.

### 4. Update `PlaybackEngine::processTick`

Iterate both `track.recorded` and `track.generative`.
Add cursor fields (`recCursor_`, `genCursor_`) for O(1) advance.
See `docs/design-docs/memory-model.md`.

### 5. Update `RecordEngine`

`pat_->track.notes.push_back(n)` → `pat_->track.recorded.push(n)`.
Replace `std::unordered_map<uint8_t, Pending>` with `Pending pending_[128]` + `bool active_[128]`.

### 6. Verify

- Compile with PlatformIO `teensy41` env.
- Run `test/legacy_main.cpp` wiring mentally: all accessors still valid.
- No `std::vector`, `std::map`, `new` in `model/` or `engine/`.

---

## Out of Scope for This Step

- Generator redesign (Phase 2)
- Scale changes (Phase 4)
