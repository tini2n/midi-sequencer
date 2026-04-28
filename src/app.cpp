#include "app.hpp"
#include "types.hpp"
#include "config.hpp"

// Pin assignments for 8 encoders (adjust to your hardware).
// Normal mode:  K1=page  K2=pitch  K3=velocity  K4=length  K5=step count
// Held-step:    K1=pitch K2=vel    K3=nudge      K4=duration
static const EncoderManager::PinConfig kEncoderPins[EncoderManager::NUM_ENCODERS] = {
    {2,  3,  4},   // K1 — page offset / held: pitch
    {5,  6,  7},   // K2 — edit pitch  / held: velocity
    {14, 15, 16},  // K3 — edit vel    / held: tick nudge
    {32, 33, 34},  // K4 — edit length / held: duration  (stub: pins 17/18/19 conflict with Wire SDA/SCL — update when physical wiring confirmed)
    {20, 21, 22},  // K5 — step count
    {23, 24, 25},  // K6 — reserved
    {26, 27, 28},  // K7 — reserved
    {29, 30, 31},  // K8 — reserved
};

void App::setup() {
    Serial.begin(115200);
    delay(500); // let serial port settle

    // Pattern: empty, 16 steps, 120 BPM
    pat_.tempo = 120.f;
    pat_.grid  = 16;
    pat_.steps = 16;
    pat_.tracks[0].channel = 1;
    pat_.tracks[1].channel = 2;

    // Core sequencer pipeline
    midi_.begin();
    sched_.begin();
    tx_.setTempo(pat_.tempo);
    tx_.setLoopLen(pat_.ticks());
    eng_.reset();
    loop_.begin(&sched_, &tx_, &eng_, &midi_, &pat_);

    // Step editor — default: track 0, page 0, C4
    cursor_.setTrack(0);
    cursor_.setPage(0, pat_.steps);
    cursor_.setEditPitch(60);

    // Matrix keyboard
    MatrixKB::Config kbCfg;
    kbCfg.address = cfg::PCF_ADDRESS;
    kb_.begin(kbCfg);
    kb_.attach(&loop_, &tx_);
    kb_.setMode(&cursor_);
    kb_.setModeContext(&pat_);

    // Encoders
    enc_.begin(kEncoderPins, cfg::ENCODER_DEBOUNCE_US);
    enc_.setHandler(this);

    // Serial monitor
    serial_.attach(&loop_, &tx_, &pat_, &midi_, &cursor_);

    Serial.println("=== MIDI Sequencer ready ===");
    Serial.println("Type ? for serial commands.");
    cursor_.onActivate();
}

void App::update() {
    serial_.poll();
    kb_.poll(midi_, pat_.tracks[cursor_.getTrack()].channel);
    enc_.poll();
    loop_.service();

    // Handle settings mode toggle (posted from CTL 6 via RunLoop event)
    if (loop_.consumeSettingsToggle()) {
        settingsMode_ = !settingsMode_;
        if (settingsMode_) Serial.printf("[SET] on   BPM=%.1f\n", pat_.tempo);
        else               Serial.println("[SET] off");
    }
}

// ─── Encoder routing ──────────────────────────────────────────────────────────

void App::onEncoderRotation(const EncoderRotationEvent& e) {
    bool stepHeld = cursor_.getHeldStep() >= 0;

    switch (e.encoderId) {
    case 0: // K1 — page offset (normal) | BPM (settings) | tick nudge (held)
        if (stepHeld) {
            cursor_.editHeld(2, e.delta, pat_);
        } else if (settingsMode_) {
            float bpm = pat_.tempo + e.delta * 0.5f;
            if (bpm < 20.f)  bpm = 20.f;
            if (bpm > 300.f) bpm = 300.f;
            pat_.tempo = bpm;
            tx_.setTempo(bpm);
            Serial.printf("[SET] BPM=%.1f\n", bpm);
        } else {
            int pg = (int)cursor_.getPage() + e.delta;
            cursor_.setPage((uint8_t)(pg < 0 ? 0 : pg), pat_.steps);
        }
        break;
    case 1: // K2 — edit pitch (normal) | pitch (held)
        if (stepHeld) {
            cursor_.editHeld(0, e.delta, pat_);
        } else {
            int p = (int)cursor_.getEditPitch() + e.delta;
            if (p < 0)   p = 0;
            if (p > 127) p = 127;
            cursor_.setEditPitch((uint8_t)p);
        }
        break;
    case 2: // K3 — edit velocity (normal) | velocity (held)
        if (stepHeld) {
            cursor_.editHeld(1, e.delta, pat_);
        } else {
            int v = (int)cursor_.getEditVelocity() + e.delta;
            if (v < 1)   v = 1;
            if (v > 127) v = 127;
            cursor_.setEditVelocity((uint8_t)v);
        }
        break;
    case 3: // K4 — edit note length in steps (normal) | duration (held)
        if (stepHeld) {
            cursor_.editHeld(3, e.delta, pat_);
        } else {
            int l = (int)cursor_.getEditLength() + e.delta;
            if (l < 1)   l = 1;
            if (l > 128) l = 128;
            cursor_.setEditLength((uint8_t)l);
        }
        break;
    case 4: { // K5 — step count (±1, or ±16 if K5 button held)
        int delta = k5Held_ ? e.delta * 16 : e.delta;
        int steps = (int)pat_.steps + delta;
        if (steps < 1)   steps = 1;
        if (steps > 255) steps = 255;
        pat_.steps = (uint8_t)steps;
        tx_.setLoopLen(pat_.ticks());
        Serial.printf("[K5] steps=%u  ticks=%lu\n", pat_.steps, (unsigned long)pat_.ticks());
        break;
    }
    default:
        break;
    }
}

void App::onEncoderButton(const EncoderButtonEvent& e) {
    switch (e.encoderId) {
    case 0:
        if (!e.pressed) break;
        if (settingsMode_) { pat_.tempo = 120.f; tx_.setTempo(120.f); Serial.println("[SET] BPM reset to 120"); }
        else               { cursor_.setPage(0, pat_.steps); }
        break;
    case 1:
        if (e.pressed) cursor_.setEditPitch(60);      // reset to C4
        break;
    case 2:
        if (e.pressed) cursor_.setEditVelocity(100);  // reset to 100
        break;
    case 3:
        if (e.pressed) cursor_.setEditLength(1);      // reset to 1 step
        break;
    case 4:
        k5Held_ = e.pressed;  // track hold state for ±16 step mode
        break;
    default:
        break;
    }
}
