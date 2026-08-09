#include "app.hpp"
#include "types.hpp"
#include "config.hpp"

// Pin assignments for 8 encoders.
// {pinA, pinB, pinSW, reversed}
//
// reversed=true: A and B are physically swapped on the PCB; the delta is
// negated in software so L always gives −1, R always gives +1.
//
// Known hardware issues (do not reorder wires — fix here in software):
//   Avoid pins 11/12/13 for pinSW — Teensy 4.1 hardware SPI0 (MOSI/MISO/SCK).
//     oled_.begin() calls SPI.begin(), which reclaims those pins for the SPI
//     peripheral and overrides any INPUT_PULLUP set on them.
//   K4 SW (pin 27): switch not triggering — pinSW likely miswired or floating.
//
// Normal mode:  K1=page  K2=pitch  K3=velocity  K4=length  K5=step count
// Held-step:    K1=nudge K2=pitch  K3=vel        K4=duration
static const EncoderManager::PinConfig kEncoderPins[EncoderManager::NUM_ENCODERS] = {
    {2,  3,  0,  false},  // K1 — page offset     | held: tick nudge
    {4,  5,  32, false},  // K2 — edit pitch       | held: pitch
    {6,  7,  26, true },  // K3 — edit velocity    | held: velocity  (A/B swapped)
    {14, 15, 27, true },  // K4 — edit note length | held: duration  (A/B swapped; SW broken)
    {16, 17, 28, false},  // K5 — step count
    {20, 21, 29, true },  // K6 — reserved                           (A/B swapped)
    {22, 23, 30, true },  // K7 — reserved                           (A/B swapped)
    {24, 25, 31, true },  // K8 — reserved                           (A/B swapped)
};

void App::setup() {
    Serial.begin(115200);
    delay(500); // let serial port settle

    // Pattern: empty, 64 steps @ 1/16, 120 BPM (max track length is 128 steps).
    // pat_.steps is the interim single playback loop length (Phase A); per-track
    // lenTicks (default 1536 = 64 steps) drives the editor and, in Phase B, playback.
    pat_.tempo = 120.f;
    pat_.grid  = 16;
    pat_.steps = 64;
    pat_.tracks[0].channel = 1;
    pat_.tracks[1].channel = 2;

    // Core sequencer pipeline
    midi_.begin();
    sched_.begin();
    tx_.setTempo(pat_.tempo);
    tx_.setLoopLen(pat_.ticks());
    eng_.reset();
    loop_.begin(&sched_, &tx_, &eng_, &midi_, &pat_);

    // Step editor — default: track 0, cursor at 0, C5
    sequencer_.setTrack(0);
    sequencer_.setEditPitch(60);

    // Matrix keyboard — PCF8575 needs time to stabilise on shared 3.3V rail.
    // Diagnostic showed it takes ~450ms from boot; pad to 600ms total here.
    while (millis() < 600) {}
    MatrixKB::Config kbCfg;
    kbCfg.address = cfg::PCF_ADDRESS;
    kb_.begin(kbCfg);
    kb_.attach(&loop_, &tx_);
    kb_.setMode(&sequencer_);
    kb_.setModeContext(&pat_);

    // Encoders
    enc_.begin(kEncoderPins, cfg::ENCODER_DEBOUNCE_US);
    enc_.setHandler(this);

    // Serial monitor
    serial_.attach(&loop_, &tx_, &pat_, &midi_, &sequencer_);

    // OLED display
    oled_.begin();
    screenMgr_.begin(&oled_);

    Serial.println("=== MIDI Sequencer ready ===");
    Serial.println("Type ? for serial commands.");
    sequencer_.onActivate();
}

void App::update() {
    serial_.poll();
    kb_.poll(midi_, pat_.tracks[sequencer_.getTrack()].channel);
    screenMgr_.markDirty();  // keyboard may have toggled a note
    enc_.poll();
    loop_.service();

    // Handle settings mode toggle (posted from CTL 6 via RunLoop event)
    if (loop_.consumeSettingsToggle()) {
        settingsMode_ = !settingsMode_;
        screenMgr_.setScreen(settingsMode_ ? ScreenId::Settings : ScreenId::PianoRoll);
        if (settingsMode_) Serial.printf("[SET] on   BPM=%.1f\n", pat_.tempo);
        else               Serial.println("[SET] off");
    }

    // Draw display (rate-capped internally; dirty flag set by encoder/key handlers)
    UICtx ctx{pat_, tx_, sequencer_, micros(), settingsMode_};
    screenMgr_.draw(ctx);
}

// ─── Encoder routing ──────────────────────────────────────────────────────────

void App::onEncoderRotation(const EncoderRotationEvent& e) {
#ifdef SEQUENCER_DEBUG
    Serial.printf("[ENC] K%u %+d\n", e.encoderId + 1, e.delta);
#endif
    const bool     held  = sequencer_.getHeldTick() >= 0;
    const bool     shift = sequencer_.isShiftPressed();   // CTL7
    const uint32_t g     = seq::gridTicks(pat_.grid);

    switch (e.encoderId) {
    case 0: // E1 — move cursor by grid (Shift = ×16) | BPM (settings) | tick nudge (held)
        if (held) {
            sequencer_.editHeld(2, e.delta, pat_);
        } else if (settingsMode_) {
            float bpm = pat_.tempo + e.delta * 0.5f;
            if (bpm < 20.f)  bpm = 20.f;
            if (bpm > 300.f) bpm = 300.f;
            pat_.tempo = bpm;
            tx_.setTempo(bpm);
            Serial.printf("[SET] BPM=%.1f\n", bpm);
        } else {
            sequencer_.moveCursor(e.delta * int(g) * (shift ? 16 : 1), pat_);
        }
        break;
    case 1: // E2 — cursor pitch lane (held: edit note pitch)
        if (held) {
            sequencer_.editHeld(0, e.delta, pat_);
        } else {
            int p = (int)sequencer_.getEditPitch() + e.delta;
            if (p < 0)   p = 0;
            if (p > 127) p = 127;
            sequencer_.setEditPitch((uint8_t)p);
        }
        break;
    case 2: // E3 — velocity (held: edit note velocity)
        if (held) {
            sequencer_.editHeld(1, e.delta, pat_);
        } else {
            int v = (int)sequencer_.getEditVelocity() + e.delta;
            if (v < 1)   v = 1;
            if (v > 127) v = 127;
            sequencer_.setEditVelocity((uint8_t)v);
        }
        break;
    case 3: // E4 — grid / zoom (held: edit note duration in grid cells)
        if (held) {
            sequencer_.editHeld(3, e.delta, pat_);
        } else {
            pat_.grid = seq::cycleGrid(pat_.grid, e.delta);
            sequencer_.onGridChanged(pat_);
            Serial.printf("[Sequencer] grid -> 1/%u\n", pat_.grid);
        }
        break;
    case 4: // E5 — focused track length by grid (Shift = ×16)
        sequencer_.editTrackLen(e.delta * int(g) * (shift ? 16 : 1), pat_);
        break;
    default:
        break;
    }
    screenMgr_.markDirty();
}

void App::onEncoderButton(const EncoderButtonEvent& e) {
#ifdef SEQUENCER_DEBUG
    Serial.printf("[ENC] K%u %s\n", e.encoderId + 1, e.pressed ? "press" : "release");
#endif
    switch (e.encoderId) {
    case 0: // E1 press — reset BPM (settings) | cursor to start
        if (!e.pressed) break;
        if (settingsMode_) { pat_.tempo = 120.f; tx_.setTempo(120.f); Serial.println("[SET] BPM reset to 120"); }
        else               { sequencer_.moveCursor(-(int)sequencer_.getCursorTick(), pat_); }
        break;
    case 1: // E2 press — reset pitch to C5
        if (e.pressed) sequencer_.setEditPitch(60);
        break;
    case 2: // E3 press — reset velocity to 100
        if (e.pressed) sequencer_.setEditVelocity(100);
        break;
    case 3: // E4 press — reset grid to 1/16
        if (e.pressed) { pat_.grid = 16; sequencer_.onGridChanged(pat_); Serial.println("[Sequencer] grid -> 1/16"); }
        break;
    case 4: // E5 press — reset focused track length to default (128 steps)
        if (e.pressed) sequencer_.editTrackLen(int(seq::TRACK_LEN_MAX_TICKS), pat_);
        break;
    default:
        break;
    }
    screenMgr_.markDirty();
}
