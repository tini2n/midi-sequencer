# Exec Plan 03 — Step Sequencer (Digitakt-style)

**Status:** ✓ COMPLETE
**Goal:** 16 keyboard buttons = 16 step slots; toggle notes on/off; serial monitor for hardware-free testing.
**Touches:** `src/model/pattern.hpp`, `src/engine/playback_engine.hpp`, `src/core/runloop.hpp`, `src/io/`, `src/app.*`

---

## Steps Completed

### 1. Reduce to 2 tracks + fix buffer sizes
- `src/model/pattern.hpp`: `MAX_TRACKS = 16` → `MAX_TRACKS = 2` (RAM1: ~113KB → ~33KB)
- `src/engine/playback_engine.hpp`: `active_[32]` → `active_[64]`, `MaxActive = 64`
- `src/core/runloop.hpp`: `MaxEvents = 32` → `MaxEvents = 64`

### 2. `src/io/pcf8575.hpp`
PCF8575 I2C driver. All `Serial.printf` behind `#ifdef SEQUENCER_DEBUG`.

### 3. `src/io/matrix_kb_mode.hpp`
`IMatrixKBMode` interface + `IModeConfig` struct. Methods: `onButtonDown`, `onButtonUp`, `onControl`, `update`, `onActivate`, `onDeactivate`, `configure`, `getConfig`.

### 4. `src/io/cursor_mode.hpp` + `src/io/cursor_mode.cpp`
Digitakt-style step editor implementing `IMatrixKBMode`:
- `pageOffset_` (uint8_t): base step = btn + pageOffset × 16. Encoder 0 controlled.
- `trackIdx_` (uint8_t): which of the 2 tracks is active (0 or 1).
- `editPitch_` (uint8_t): MIDI note assigned to new steps. Encoder 1 controlled.
- `toggleStep()`: finds note at step tick → `removeAt(i)` if exists, else `push()` + `sortByOnTick()`.
- `printTrackState()`: ASCII grid like `[X][ ][X]` with pitch labels below — printed after every edit.
- No `std::algorithm`, no heap. Uses `NotePool` API only.
- Control buttons: CTL 0 = toggle track, CTL 1 = play, CTL 2 = stop, CTL 7 = shift modifier.
- Shift combos: Shift+CTL 0 = clear step, Shift+CTL 1 = copy step, Shift+CTL 2 = paste step.

### 5. `src/io/matrix_kb.hpp` + `src/io/matrix_kb.cpp`
- `attach(RunLoop*, Transport*)` — no RecordEngine
- Default mode: `CursorMode` (step sequencer mode)
- `attachViewManager()` is no-op stub (TODO: Phase 5)
- All `Serial.printf` behind `#ifdef SEQUENCER_DEBUG`

### 6. `src/io/encoder.hpp` + `src/io/encoder_manager.hpp`
- `Encoder`: quadrature decoding (Gray-code state table) + debounced switch. `poll()` → int8_t delta.
- `EncoderManager`: manages 8 encoders, dispatches `IEncoderHandler` events.

### 7. `src/io/serial_monitor.hpp`
No heap, no `String`. `char buf_[48]`. Commands:
| Input | Action |
|-------|--------|
| `p` | Play / stop toggle |
| `T<bpm>` | Set tempo (e.g. `T140`) |
| `G<steps>` | Set pattern steps (e.g. `G32`) |
| `A<tr>,<pitch>,<step>` | Add note at step |
| `X<tr>,<step>` | Remove note at step |
| `C` / `C<tr>` | Clear track |
| `L` | Print all tracks as step grid |
| `?` | Print help |

### 8. Wire into `src/app.hpp/.cpp`
- `App` implements `IEncoderHandler`
- Enc 0 rotation → `cursor_.setPage()`, Enc 1 rotation → `cursor_.setEditPitch()`
- Enc 0 button → reset page to 0, Enc 1 button → reset pitch to C4
- Pattern starts empty (no hardcoded test notes)

---

## Verification

1. `pio run -e teensy41` — compiles clean (58KB flash, 33KB RAM1), no heap types
2. Flash → serial monitor 115200 baud
3. `?` → help printed; `L` → empty 2-track grid
4. `A0,60,0` → step 0 C4; `A0,64,4` → step 4 E4; `L` → grid shows both
5. `p` → notes play via MIDI out, loop cleanly
6. `T140` → tempo changes; `G32` → pattern extends
7. Hardware: press step button → note toggles, grid prints to serial
8. Hardware: encoder 0 → page shifts; encoder 1 → edit pitch changes

---

## Key Design Decisions

- **2 tracks only**: `MAX_TRACKS=2` until UI phase — saves 80KB RAM1 vs 16-track design.
- **Freeze fix**: Legacy froze at 8-12 notes due to `allNotesOff()` sending 384 bytes to 64-byte UART TX buffer. Fixed via `sendAllNotesOffCC()` (CC 123, 3 bytes per channel) + `active_[64]` + `MaxEvents=64`.
- **No real-time recording**: Step editing is the primary goal; `RecordEngine` deferred to future phase.
- **Serial as UI**: `printTrackState()` after every edit gives visual feedback until OLED is wired (Phase 5).
