# MIDI Sequencer

A polyphonic, performance-first MIDI step sequencer built on the Teensy 4.1. Digitakt-inspired: 16 pad buttons act as step slots, toggling notes on/off per track. No audio synthesis — it only produces MIDI, driven out over hardware MIDI DIN with sample-accurate timing from a dedicated ISR clock.

## Features

- **2 active tracks**, each routed to its own MIDI output channel
- **16-step pad grid** for direct step entry, with page paging beyond 16 steps
- **Layered patterns** — a recorded layer (what you play) and a generative layer (generator output), mixed at playback time without destroying either
- **8 rotary encoders** for live parameter editing: pitch, velocity, note length, step count, track length, tempo
- **Hold-step editing** — hold a step button and turn an encoder to nudge/tune that note live
- **SSD1322 OLED (256×64)** piano-roll display with header, grid, and playhead
- **Serial monitor interface** for debugging and manual control over USB serial
- **Zero heap allocation** after boot — safe for long-running embedded operation

## Hardware

| Module | Spec | Role |
|---|---|---|
| MCU | Teensy 4.1 | Core + USB + hardware timers |
| Pad grid | 16 buttons via PCF8575 I/O expander (I2C) | Step entry / time cursor |
| Control buttons | 8 (PLAY, STOP, REC, SHIFT, MODE, TRACK, SET, PG+) | Transport & mode switching |
| Encoders | 8× EC11 rotary w/ push switch | Navigation + parameter editing |
| Display | SSD1322 256×64 OLED, 4-wire SPI | Piano roll UI |
| MIDI | 1× IN, 1× OUT | Clock + notes |

See [`.claude/design-docs/control-map.md`](.claude/design-docs/control-map.md) for the full button/encoder mapping and known hardware quirks, and [`.claude/references/midi-protocol.md`](.claude/references/midi-protocol.md) for MIDI wiring/protocol notes.

## Building

Built with [PlatformIO](https://platformio.org/).

```bash
pio run -e teensy41          # build
pio run -e teensy41 -t upload    # build + flash
pio device monitor               # serial monitor (115200 baud)
```

Other environments in `platformio.ini` (`teensy41_i2c_test`, `teensy41_oled_test`, `teensy41_combined_test`) build standalone hardware diagnostics from `test/` for bringing up the I2C bus and OLED independently of the main firmware.

## Architecture

```
IntervalTimer ISR → TickScheduler → RunLoop → Transport → PlaybackEngine → MidiIO
```

- The 1 kHz ISR only pushes ticks to a ring buffer — no I2C/SPI/Serial/allocation ever runs inside it.
- `PlaybackEngine` walks a sorted, fixed-size `NotePool` with an O(1) cursor per tick rather than rescanning all notes.
- Generators write to a staging buffer and are swapped into a track's generative layer atomically from the run loop, never mutating live playback state mid-generation.
- All tick math funnels through `timebase::ticksPerStep()` (`src/types.hpp`) — the single source of truth for grid/tick conversion.

Source layout:

```
src/
  core/     tick scheduling, run loop, MIDI I/O, transport
  engine/   playback engine, note pool, generators
  model/    Pattern, Track, Scale, data types
  io/       encoders, matrix keyboard (PCF8575), serial monitor
  ui/       OLED renderer, screen manager, views
  app.cpp   wiring/composition root, encoder & pin assignments
```

## Project status

Mid-refactor, actively developed. See [`.claude/PLANS.md`](.claude/PLANS.md) for the phased roadmap and [`.claude/exec-plans/`](.claude/exec-plans/) for in-flight design docs. Data model, core MIDI pipeline, and the step sequencer control surface are done; the generator subsystem and OLED UI are in progress.

## Known issues

- K4 encoder's push switch isn't registering presses — suspected wiring/solder fault, rotation is unaffected. See `.claude/design-docs/control-map.md`.

## License

No license specified yet — all rights reserved by default.