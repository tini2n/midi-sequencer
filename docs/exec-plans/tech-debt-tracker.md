# Tech Debt Tracker

Format: `[SEVERITY] location — problem — fix`
Severity: HIGH = crash/data-loss risk; MED = performance; LOW = code smell

---

## HIGH

- **[HIGH]** `app.cpp:12` — Encoder K4 pin conflict with I2C bus.
  `kEncoderPins[3]` is set to `{17, 18, 19}`. Pins 18 (SDA0) and 19 (SCL0) are the Teensy 4.1
  Wire/I2C0 bus lines shared with the PCF8575 keyboard driver. `enc_.begin()` reconfigures
  those pins as GPIO interrupt inputs, breaking all subsequent Wire transactions.
  Symptom: `pcf_.read()` silently returns false on every poll cycle → keyboard buttons never
  detected even though PCF initialises successfully.
  Fix: check physical wiring of encoder K4, then update `kEncoderPins[3]` to pins that avoid
  I2C0 (18, 19) and I2C1 (16, 17). Safe alternatives: any of pins 0–15, 20–23, 26–41.
  **TODO: confirm physical K4 encoder pin wiring before applying fix.**

- **[HIGH]** `engine/generator.hpp:73` — `std::map<const char*, GeneratorParameter>` uses pointer
  comparison for keys. `find("density")` may silently fail if string literals differ by TU.
  Fix: replace with fixed `GeneratorParam params[N]` array indexed by enum.

- **[HIGH]** `model/track.hpp:8` — `std::vector<Note> notes` allocates on heap.
  Long sessions cause fragmentation and eventual crash.
  Fix: replace with `NotePool<256>` (Phase 1).

- **[HIGH]** `engine/euclidean_generator.cpp:108` — `std::vector<bool> rhythm` allocated on heap
  every `generate()` call.
  Fix: use `bool rhythm[64]` stack array (max steps = 64).

- **[HIGH]** `engine/generator_manager.hpp:105` — `std::vector<std::unique_ptr<Generator>>`
  heap-allocates both the vector and each generator.
  Fix: static array of concrete generators, no heap.

## MED

- **[MED]** `engine/playback_engine.hpp:27` — O(n) scan of all notes every tick (1 kHz).
  With 64 notes this is 64k comparisons/sec.
  Fix: sort notes, use cursor index (Phase 3).

- **[MED]** `core/midi_io.hpp:87` — `MidiIO::pop()` is O(n) shift on every dequeue.
  Fix: replace linear array with ring buffer for the delayed-event queue.

- **[MED]** `engine/generator_manager.cpp:83` — `Serial.printf` on every `setParameter` call.
  Blocking UART in potential hot path.
  Fix: gate behind `#ifdef SEQUENCER_DEBUG`.

- **[MED]** `engine/euclidean_generator.cpp:151` — `Serial.printf` in `generate()`.
  Fix: gate behind `#ifdef SEQUENCER_DEBUG`.

## LOW

- **[LOW]** `core/midi_io.hpp:48` — `bool debug_{true}` hardcoded in `sendNoteNow()` body.
  Local variable allocated each call; debug always-on.
  Fix: remove variable, gate print behind compile flag.

- **[LOW]** `ui/views/generative_view.hpp:6,14` — `engine/generator_manager.hpp` and
  `io/matrix_kb.hpp` each included twice.
  Fix: remove duplicate includes.

- **[LOW]** `io/matrix_kb.hpp:308` — `void* modeContext_` is type-unsafe.
  Fix: template MatrixKB on context type, or use a typed union.

- **[LOW]** `model/note.hpp:14` — `micro` and `micro_q8` fields are redundant (same concept,
  two representations). Only `micro_q8` is used in playback.
  Fix: remove `micro`, keep `micro_q8`, document units clearly.

- **[LOW]** `engine/generator.cpp` — `Generator::printParameters()` iterates `std::map`.
  Fix: iterate fixed param array instead.

- **[LOW]** `core/transport.hpp:37` — `on1ms()` name is misleading; it's called once per
  `TickEvent` from a 1 kHz scheduler, which happens to be 1 ms intervals, but the name
  implies a wall-clock guarantee. Rename to `onTick()`.
