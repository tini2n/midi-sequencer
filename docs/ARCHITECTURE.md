# ARCHITECTURE.md

## Hardware Platform

| Component | Part | Interface |
|-----------|------|-----------|
| MCU | Teensy 4.1 (ARM Cortex-M7, 600 MHz, 1 MB RAM) | — |
| Display | SSD1322 256×64 OLED | 4-wire HW SPI (CS=10, DC=9, RST=8) |
| Key matrix | PCF8575 (3×8 = 16 keys + 8 control buttons) | I2C @ 400 kHz, addr 0x20 |
| Encoders | 8× rotary encoders with push switch | GPIO (debounce in SW) |
| MIDI out | Serial1 | 31250 baud (DIN-5 via optocoupler) |

## Tick System

```
IntervalTimer ISR (1 kHz)
    │ push TickEvent into RingBufferSPSC<1024>
    ▼
RunLoop::service()  ← called every Arduino loop()
    │ drain ring buffer → Transport::on1ms()
    │ Transport advances play_ tick counter (PPQN=96, tempo-derived)
    │ PlaybackEngine::processTick(prev, curr, pattern) → MidiEvents
    │ MidiIO::send() → immediate or time-tagged queue
    ▼
MidiIO::update()    ← end of service(), fires delayed events
```

**PPQN = 96.** One 1/16 step = 24 ticks. One bar (4/4) = 384 ticks.

## Subsystem Map

```
┌─────────────────────────────────────────────────────┐
│                     main loop                       │
│  RunLoop::service()  ViewManager::pollKB/Encoders   │
│  ViewManager::draw()                                │
└────────────┬────────────────────┬───────────────────┘
             │                    │
    ┌────────▼───────┐   ┌────────▼────────┐
    │   Core Layer   │   │    UI Layer     │
    │  TickScheduler │   │  ViewManager    │
    │  Transport     │   │  PerformanceView│
    │  RunLoop       │   │  GenerativeView │
    │  MidiIO        │   │  OledRenderer   │
    └────────┬───────┘   └────────┬────────┘
             │                    │
    ┌────────▼───────┐   ┌────────▼────────┐
    │  Engine Layer  │   │    IO Layer     │
    │  PlaybackEngine│   │  MatrixKB       │
    │  RecordEngine  │   │  EncoderManager │
    │  GeneratorMgr  │   │  PCF8575        │
    └────────┬───────┘   └─────────────────┘
             │
    ┌────────▼───────┐
    │  Model Layer   │
    │  Pattern       │
    │  Track         │
    │  Note          │
    │  Viewport      │
    └────────────────┘
```

## Data Flow — Playback

```
Pattern (Track → []Note)
    └─► PlaybackEngine::processTick()
            └─► note.on falls in (prev, curr] window?
                    yes → push MidiEvent to evs[]
RunLoop drains evs[] → MidiIO::send()
```

## Data Flow — Generative

```
GenerativeView (user triggers generate)
    └─► GeneratorManager::generatePattern(pattern)
            └─► Generator::generate(pattern)
                    └─► clears pattern.track.notes  ← PROBLEM: destructive
                        fills with new notes
```
See `docs/design-docs/memory-model.md` for the target design that fixes this.

## Known Problems (Legacy Code)

See `docs/exec-plans/tech-debt-tracker.md` for the full list.
The most critical:

1. `Track::notes` is `std::vector<Note>` — heap on embedded.
2. `Generator` uses `std::map<const char*, ...>` — pointer-keyed tree, breaks on string literals.
3. `PlaybackEngine::processTick` is O(n) per tick — scans all notes every 1 ms.
4. `MidiIO::pop` is O(n) shift — use a ring buffer.
5. Generators write directly to `Pattern::track.notes` — destructive, kills recorded notes.
6. `Serial.printf` in hot paths — blocking I/O.
