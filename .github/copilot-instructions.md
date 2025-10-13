# Copilot Instructions for midi-sequencer

Purpose: Give AI coding agents the essentials to work productively in this Teensy 4.1 + Arduino MIDI sequencer. Keep changes aligned with the architecture and patterns below.

## Big picture
- Platform: Teensy 4.1 (Arduino framework). Config in `platformio.ini` (env `teensy41`, serial monitor 115200, OLED lib `U8g2`).
- Real-time clocking: `TickScheduler` (hardware IntervalTimer ISR) enqueues 1kHz tick events into a lock-free SPSC ring buffer. `RunLoop` consumes them.
- Transport and scheduling: `Transport` converts 1ms service ticks into musical ticks per current tempo (TPQN=96 by default), maintains loop length and playhead, and yields contiguous `TickWindow{prev,curr}` steps.
- Playback: `PlaybackEngine` scans the current `Pattern` and emits `MidiEvent`s on note on/off boundaries within each tick window. Note microtiming uses `micro_q8` (1/256 tick) as positive delay in microseconds.
- MIDI I/O: `MidiIO` writes raw bytes to `Serial1` at 31,250 baud. Supports immediate send, delayed queue (by `delay_us`), MIDI clock/start/continue/stop.
- UI/Rendering: `OledRenderer` (U8g2) draws a compact piano roll via `ui/widgets/piano_roll.*`. `PerformanceView` renders HUD and polls input.
- Input: Two sources exist:
  - Matrix keyboard via PCF8575 I/O expander (`ui/cursor/matrix_kb.hpp`, `io/pcf8575.hpp`) with debouncing and musical mapping (root/octave/velocity controls).
  - Optional serial keyboard (`ui/cursor/serial_keyboard_input.hpp`) for testing.

## Data model
- `model/note.hpp`: Note has `on`, `duration` (ticks), `micro`, `micro_q8`, `pitch`, `vel`, `flags`.
- `model/track.hpp`: Holds `std::vector<Note> notes`, `channel`.
- `model/pattern.hpp`: Wraps a `Track`, `steps` and `grid` (e.g., 16 for 1/16 notes). Total length `ticks()` = `timebase::ticksPerStep(grid) * steps`.
- `model/viewport.hpp`: Visual window over time/pitch for rendering (tickStart/tickSpan, pitchBase), with pan/zoom helpers.

## Control flow (main loop)
- `setup()` in `src/main.cpp` initializes OLED, MIDI, `TickScheduler`, sets pattern defaults, configures `Transport`, and starts playback.
- `loop()` runs:
  1) `runner.service()` → drains 1ms ticks, advances `Transport`, runs `PlaybackEngine`, sends MIDI and clocks.
  2) `perf.poll(midi)` → scans keyboard input and toggles notes (sends immediate MIDI).
  3) Renders at ~20 FPS via `perf.draw(..., oled, ...)`.

## Key timing/clocking facts
- Base PPQN is 96 (`timebase::PPQN`). Ticks per 1/16 step = 24. MIDI clock is sent every 24 ticks (see `RunLoop::clkDiv_`).
- `Transport::on1ms()` accumulates microseconds and generates whole musical ticks based on `usPerTick(Tempo{bpm,tpqn})`.
- `PlaybackEngine::microDelayUs(micro_q8,bpm)` converts sub-tick positive offsets to `delay_us` for `MidiIO`.

## Conventions & patterns
- Single-producer/single-consumer ring buffer (`core/ring_buffer.hpp`) is used by the ISR producer and the main thread consumer; capacity set via `cfg::RB_CAP` in `src/config.hpp`.
- Avoid dynamic allocation or heavy work in ISRs; ISR only pushes `TickEvent`.
- All times use `micros()`. Compare with signed deltas: `(int32_t)(now - next) >= 0`.
- MIDI channels are 1–16 in code, encoded as 0–15 in status byte inside `MidiIO::emit()`.
- UI drawing uses U8g2 page loop; keep per-frame allocations zero and rendering tight.
- Input mapping in MatrixKB: top row (0..7) are black-key gaps, bottom row (8..15) naturals derived from `root_` using C-major intervals; control row handles octave/root/velocity changes.

## How to build, run, and debug
- Use PlatformIO (VS Code) with the `teensy41` environment. Typical actions:
  - Build and upload to Teensy.
  - Open serial monitor at 115200 for logs (setup prints and MatrixKB/SerialKB debug).
- Hardware expectations:
  - MIDI out on `Serial1` (Teensy UART). Ensure DIN/adapter wired.
  - OLED SSD1306 128x64 on I2C 0x3C.
  - PCF8575 matrix at I2C 0x20 (configurable via `cfg::PCF_ADDRESS`).
- Quick simulation: Without hardware, you can still build; SerialKB path in `PerformanceView` shows how to test via USB serial if you enable it.

## Extension tips for agents
- When adding features that depend on tempo or loop length, update both `Pattern` (steps/grid) and `Transport` (`setLoopLen`, `setTempo`, `locate`). Keep PPQN assumptions consistent with `timebase`.
- To add new rendering, extend `OledRenderer` and/or `PianoRoll` while preserving the page loop and avoiding heap churn.
- For new input devices, follow `MatrixKB`'s debounced scan pattern; do not block in `poll()`.
- For scheduling micro-timed events, set `Note.micro_q8 > 0` to push delayed NoteOn via `MidiIO` and emit NoteOff on exact tick without delay.

## Reference map
- Core: `src/core/{tick_scheduler.*, transport.hpp, runloop.hpp, midi_io.hpp, ring_buffer.hpp, timebase.hpp}`
- Engine: `src/engine/playback_engine.hpp`
- Model: `src/model/{note.hpp, track.hpp, pattern.hpp, viewport.hpp}`
- UI: `src/ui/{renderer_oled.*, views/performance_view.*, widgets/piano_roll.*}`
- Input: `src/ui/cursor/{matrix_kb.hpp, serial_keyboard_input.hpp}`, I2C expander `src/io/pcf8575.hpp`
- Entrypoint: `src/main.cpp`
