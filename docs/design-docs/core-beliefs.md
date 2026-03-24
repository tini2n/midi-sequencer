# Core Beliefs

## 1. Embedded is not a desktop

Teensy 4.1 has 1 MB RAM and no MMU. `malloc`/`free` cause heap fragmentation.
A sequencer that runs for an hour should behave identically to one that ran for 10 seconds.
This means: **no runtime allocation after `setup()`**.

Every container that stores notes, events, or parameters must have a compile-time maximum size.
If a size limit is hit, the system degrades gracefully (drops the excess), not crashes.

## 2. The ISR is sacred

The `IntervalTimer` ISR fires every 1 ms and must return in microseconds.
It does exactly one thing: push a `TickEvent` into a lock-free ring buffer.
No MIDI, no note lookup, no Serial, no allocation — ever.

## 3. Hot path = main loop tick drain

`RunLoop::service()` is the only place where ticks are consumed and MIDI events are emitted.
It runs as fast as possible inside `loop()`. Any O(n) operation here multiplied by the note count
is a latency hazard. Target: O(1) per tick for playback.

## 4. Generators are offline

Generation (computing a new rhythm/melody) is a user-triggered batch operation.
It is NOT real-time. It can take a few milliseconds. But it must:
- Not corrupt the currently playing pattern mid-playback.
- Not destroy user-recorded notes.

Solution: generators write to a staging buffer; the result is atomically swapped in
between ticks (when `RunLoop` is not mid-scan).

## 5. Parameters are data, not objects

The legacy `std::map<const char*, GeneratorParameter>` is wrong for two reasons:
- `std::map` allocates heap nodes.
- `const char*` keys compare pointers, not strings — `find("density")` may return `end()`
  even if a "density" key was inserted, if the string literals live at different addresses.

Parameters are a fixed array of `{name, value, min, max, step}` structs, indexed by enum.
No map, no heap, no pointer aliasing bugs.
