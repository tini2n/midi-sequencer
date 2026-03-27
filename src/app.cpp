#include "app.hpp"
#include "types.hpp"
#include "config.hpp"

// Pin assignments for 8 encoders (adjust to your hardware).
// Encoder 0: page offset.  Encoder 1: edit pitch.  Others reserved.
static const EncoderManager::PinConfig kEncoderPins[EncoderManager::NUM_ENCODERS] = {
    {2,  3,  4},   // Enc 0 — page offset
    {5,  6,  7},   // Enc 1 — edit pitch
    {14, 15, 16},  // Enc 2 — reserved
    {17, 18, 19},  // Enc 3 — reserved
    {20, 21, 22},  // Enc 4 — reserved
    {23, 24, 25},  // Enc 5 — reserved
    {26, 27, 28},  // Enc 6 — reserved
    {29, 30, 31},  // Enc 7 — reserved
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
}

// ─── Encoder routing ──────────────────────────────────────────────────────────

void App::onEncoderRotation(const EncoderRotationEvent& e) {
    switch (e.encoderId) {
    case 0: // Page offset
        cursor_.setPage(
            (uint8_t)((int)cursor_.getPage() + e.delta < 0
                ? 0
                : cursor_.getPage() + e.delta),
            pat_.steps);
        break;
    case 1: { // Edit pitch
        int p = (int)cursor_.getEditPitch() + e.delta;
        if (p < 0)   p = 0;
        if (p > 127) p = 127;
        cursor_.setEditPitch((uint8_t)p);
        break;
    }
    default:
        break;
    }
}

void App::onEncoderButton(const EncoderButtonEvent& e) {
    if (!e.pressed) return;
    switch (e.encoderId) {
    case 0: cursor_.setPage(0, pat_.steps); break;          // reset page to 0
    case 1: cursor_.setEditPitch(60); break;                // reset pitch to C4
    default: break;
    }
}
