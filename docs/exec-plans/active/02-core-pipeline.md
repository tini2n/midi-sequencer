# Exec Plan 02 — Core MIDI Pipeline

**Status:** Not started (depends on 01-data-model)
**Goal:** Get a note playing end-to-end: hardcoded pattern → tick → MIDI byte out. First proof-of-life.
**Touches:** `src/core/`, `src/engine/playback_engine.hpp`, `src/engine/record_engine.hpp`, `src/app.cpp`

---

## Steps

### 1. Port `RingBufferSPSC<T,N>` → `src/core/ring_buffer.hpp`

Legacy version (`src/_legacy/core/ring_buffer.hpp`) is correct. Port as-is:
- Power-of-2 size `static_assert`
- Atomic head/tail (`std::atomic`, `memory_order_acquire/release`)
- `push(T)`, `pop(T&)`, `depth()` API

No changes needed to logic.

### 2. Port `Timebase` → `src/core/timebase.hpp`

Port `Tempo` struct and `usPerTick(Tempo)` from legacy. Ensure `PPQN = 96` constant lives here.
`TickEvent` and `TickWindow` structs go here too (or in `src/types.hpp` if already defined).

### 3. Port `TickScheduler` → `src/core/tick_scheduler.hpp` + `.cpp`

Legacy version is correct design. Port:
- `IntervalTimer` setup in `begin()`
- Static ISR thunk → pushes to internal `RingBufferSPSC<TickEvent, cfg::RB_CAP>`
- `fetch(TickEvent&)` pops from buffer
- `dropped()` counter for missed ticks

Verify ISR only pushes to ring buffer — no other work.

### 4. Port `Transport` → `src/core/transport.hpp`

Legacy version is clean. Port with minor cleanup:
- Rename `on1ms()` → `onTick()` (the "1ms" naming is misleading; it's called per scheduler fetch)
- Keep phase-accumulator approach for drift tolerance
- Methods: `start()`, `stop()`, `pause()`, `resume()`, `locate()`, `setTempo()`, `setLoopLen()`
- `next(TickWindow&)` → returns `{prev, curr}` tick pair if pending

### 5. Rewrite `MidiIO` → `src/core/midi_io.hpp`

Based on legacy `midi_io.hpp` but fix the O(n) queue:

```cpp
struct Q { MidiEvent ev; uint32_t dueUs; };

class MidiIO {
public:
    void begin();
    void send(const MidiEvent& ev);         // immediate or enqueue
    void update();                           // drain due delayed events
    void sendClock();
    void sendStart(); void sendStop(); void sendContinue();
    void allNotesOff(uint8_t channel);

private:
    void sendNow(const MidiEvent& ev);
    RingBufferSPSC<Q, 32> dq_;              // replaces Q q_[32] + O(n) pop
};
```

- `update()` peeks `dq_` head, pops if due — O(1)
- Remove `bool debug_{true}` and all `Serial.printf` (gate behind `SEQUENCER_DEBUG` if needed at all)
- `allNotesOff`: keep the 128 note-off loop; it's correct belt-and-suspenders. Add comment explaining why.

### 6. Rewrite `PlaybackEngine` → `src/engine/playback_engine.hpp`

Cursor-based, O(1) amortized per tick:

```cpp
struct PlaybackEngine {
    void reset() { recCursor_ = 0; genCursor_ = 0; }

    void processTick(uint32_t prev, uint32_t curr, const Pattern& p,
                     MidiEvent* out, uint8_t& outCount, uint8_t maxOut);

private:
    uint16_t recCursor_{0};
    uint16_t genCursor_{0};

    template <size_t N>
    void processLayer(const NotePool<N>& pool, uint16_t& cursor,
                      uint32_t prev, uint32_t curr, uint8_t channel,
                      MidiEvent* out, uint8_t& outCount, uint8_t maxOut);
};
```

- Notes must be sorted by `on` tick before playback starts (`NotePool::sortByOnTick()`)
- `processLayer`: advance cursor while `notes[cursor].on <= curr`, emit note-on events
- Track active notes for note-offs: `ActiveNote active_[32]` (pitch + due tick)
- On wrap (`curr < prev`): reset cursors, send note-off for any still-active notes
- **No `std::vector<MidiEvent>` output** — fixed-size output array passed by caller

### 7. Rewrite `RunLoop` → `src/core/runloop.hpp`

Wire everything together:

```cpp
class RunLoop {
public:
    void begin(TickScheduler*, Transport*, PlaybackEngine*, MidiIO*, Pattern*);
    void service();  // call from Arduino loop()

private:
    // Fixed-size event buffer — no std::vector
    MidiEvent evBuf_[32];
};
```

`service()` flow:
1. Fetch ticks from `TickScheduler`
2. Feed to `Transport::onTick()`
3. While `Transport::next(window)`: call `PlaybackEngine::processTick()`
4. Send accumulated events via `MidiIO::send()`
5. Call `MidiIO::update()` (drain delayed queue)

### 8. Wire into `App` and verify

In `src/app.cpp`:
- `setup()`: init all components, load a hardcoded test pattern (e.g., 4 notes on beats 1–4)
- `update()`: call `RunLoop::service()`
- Connect MIDI out on Serial1 to a DAW or hardware synth
- Verify notes play at correct tempo

---

## Verification

- Compile: `pio run -e teensy41` — zero errors, zero `std::vector`/`std::map`/`new` in new files
- Flash and connect MIDI out to DAW
- Observe hardcoded pattern playing at correct BPM
- Tap play/stop — transport responds correctly
- `SEQUENCER_DEBUG` off by default; no `Serial.printf` in hot path

---

## Out of Scope for This Step

- Recording (Phase 3)
- Generators (Phase 4)
- UI / display (Phase 5)
