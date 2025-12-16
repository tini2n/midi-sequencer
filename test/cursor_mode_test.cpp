/**
 * Cursor Sequencer Test Program
 *
 * Standalone sketch for debugging the cursor-mode step editor without any OLED
 * rendering. It mirrors the optimized 2x8 matrix workflow:
 *   - 1 kHz ISR drives a transport that emits 128 step edges.
 *   - 2x8 keyboard via PCF8575: bottom row toggles steps on the current page,
 *     top row moves the cursor, and the control row provides start/stop + page.
 *   - Playback fires DIN MIDI NoteOn at step edges and NoteOff after a fixed
 *     gate (50% of the step).
 *
 * Hardware pins (default):
 *   - PCF8575 rows: P0, P1, P2
 *   - PCF8575 cols: P8..P15
 *   - BTN16 (row2,col0): transport start/stop
 *   - BTN17 (row2,col1): page advance (wraps 0..7)
 *
 * Usage:
 *   - Open in PlatformIO and build/upload (or temporarily rename to main.cpp).
 *   - Watch the serial monitor @115200 for step toggle/page/run state logs.
 *   - Rotate through pages with BTN17; toggle steps with the bottom row buttons.
 */

#include <Arduino.h>
#include <bitset>

#include "core/midi_io.hpp"
#include "io/pcf8575.hpp"
#include "step/step_pattern.hpp"
#include "step/step_playback.hpp"
#include "step/step_tick_scheduler.hpp"
#include "step/step_transport.hpp"
#include "ui/cursor/step_cursor.hpp"

// Core objects
step::TickScheduler sched;
step::Transport transport;
MidiIO midi;
PCF8575 pcf;
step::Pattern pattern;
step::Playback playback;
step::Cursor cursor;

static inline step::Track& activeTrack() { return pattern.track(); }

// Debug state trackers (serial logs only)
static std::bitset<128> prevSteps;
static uint8_t prevPage = 0;
static bool prevRunning = false;

static void logChanges()
{
    step::Track& track = activeTrack();
    // Detect step toggles
    std::bitset<128> diff = track.steps ^ prevSteps;
    if (diff.any())
    {
        for (size_t i = 0; i < diff.size(); ++i)
        {
            if (!diff.test(i))
                continue;
            Serial.printf("[step] %3zu %s\n", i, track.steps.test(i) ? "ON " : "off");
        }
        prevSteps = track.steps;
    }

    // Page changes
    if (track.page != prevPage)
    {
        Serial.printf("[ui] page -> %u\n", track.page);
        prevPage = track.page;
    }

    // Transport state changes
    bool running = transport.running();
    if (running != prevRunning)
    {
        Serial.printf("[transport] %s\n", running ? "START" : "STOP");
        prevRunning = running;
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 1500) {}

    step::Track& track = activeTrack();
    midi.begin();
    pcf.begin(0x20);
    pcf.write(0xFFFF); // inputs high

    // Basic track defaults
    track.channel = 1;
    track.pitch = 60; // C4

    // Transport/grid configuration
    transport.setTempo(120, 96);
    transport.setGridDiv(16); // 16th notes
    transport.start();

    sched.begin();

    step::Cursor::Pins pins; // defaults rows 0..2, cols 8..15
    cursor.begin(&pcf, pins, &pattern, &transport);

    Serial.println("[setup] Cursor-mode sequencer test ready (128 steps / no OLED)");
}

void loop()
{
    // Drain ISR ticks into transport
    step::TickEvent e;
    while (sched.fetch(e))
        transport.on1ms();

    // Process step edges + MIDI clock
    step::TickWindow w;
    while (transport.next(w))
    {
        playback.processStep(transport, pattern, midi, transport.tps());
        if (transport.clockPulse())
            midi.sendClock();
    }

    // Poll keyboard matrix (non-blocking)
    cursor.poll();

    // Flush queued MIDI events
    midi.update();

    // Emit debug logs for state changes
    logChanges();
}
