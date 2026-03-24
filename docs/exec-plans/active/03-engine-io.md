# Exec Plan 03 — Engine & IO Fixes

**Status:** Not started (depends on 01 and 02)
**Goal:** O(1) playback scan, ring-buffer MIDI queue, no Serial in hot paths.
**Touches:** `src/engine/playback_engine.hpp`, `src/core/midi_io.hpp`

---

## Steps

### 1. Cursor-based `PlaybackEngine`

After notes are sorted (NotePool::sortByOnTick), advance a cursor instead of scanning all:

```cpp
struct PlaybackEngine {
    void reset() { recCursor_ = 0; genCursor_ = 0; }

    void processTick(uint32_t prev, uint32_t curr, const Pattern& p, /* event out */) {
        bool wrapped = curr < prev;
        if (wrapped) { recCursor_ = 0; genCursor_ = 0; }

        processLayer(p.track.recorded, recCursor_, prev, curr, p, out);
        processLayer(p.track.generative, genCursor_, prev, curr, p, out);
    }

private:
    uint16_t recCursor_{0};
    uint16_t genCursor_{0};

    template <size_t N>
    void processLayer(const NotePool<N>& pool, uint16_t& cursor, ...);
};
```

`processLayer` advances cursor while `notes[cursor].on <= curr`, emits events, stops.
On wrap, note-offs for any still-active notes must be sent — track them with a small
`ActiveNote active_[16]` array (pitch + layer tag).

### 2. Ring-buffer MIDI delayed queue

Replace `Q q_[32]` + `pop()` O(n) shift in `MidiIO` with:
```cpp
RingBufferSPSC<Q, 32> dq_;
```
`enqueue` → `dq_.push()`. `update()` peeks head, pops if due.
Note: `RingBufferSPSC` already exists in `core/ring_buffer.hpp`.

### 3. Remove Serial from hot paths

Files to audit:
- `engine/generator_manager.cpp` — all `Serial.printf` → `#ifdef SEQUENCER_DEBUG`
- `engine/euclidean_generator.cpp:151` — same
- `core/midi_io.hpp:48` — remove `bool debug_{true}` and its print
- `io/matrix_kb.hpp:261,278` — `Serial.printf` on every note on/off → debug gate

---

## Notes

`MidiIO::allNotesOff` sends 128 note-offs. This is correct per MIDI spec for an
all-notes-off panic. The CC 123 message is preferred; the explicit loop is a belt-and-suspenders
safety net. Keep it, but document why.
