#include <Arduino.h>
#include <Wire.h>

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "core/runloop.hpp"
#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "engine/playback_engine.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/views/performance_view.hpp"
#include "ui/cursor/serial_keyboard_input.hpp"

static constexpr uint16_t PPQN = 96;
static inline uint32_t ticksPerStep(uint16_t gridDiv) { return (uint32_t(PPQN) * 4u) / gridDiv; }

TickScheduler sched;
Transport transport;
PlaybackEngine engine;
MidiIO midi;
RunLoop runner;
Pattern pat;
Viewport vp;
OledRenderer oled;
PerformanceView perf;


static uint16_t visibleSteps = 16; // S ∈ {1,2,4,8,16,32,64}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 1500)
  {
  }

  Serial.println("MIDI Sequencer. Initing...");

  oled.begin();
  midi.begin();
  sched.begin();

  pat.grid = 16;
  pat.steps = 64;
  pat.tempo = 120.f;
  pat.track.channel = 13;

  transport.setLoopLen(pat.ticks());
  transport.setTempo(pat.tempo);
  transport.locate(0);
  transport.start();

  vp.pitchBase = 36;
  vp.tickSpan = ticksPerStep(pat.grid) * visibleSteps;

  runner.begin(&sched, &transport, &engine, &midi, &pat);
  perf.begin(pat.track.channel);
}

void loop()
{
  // Run clock & transport and generate events
  runner.service();

  // Keyboard input
  perf.poll(midi);

  // Draw
  static uint32_t nextDraw = 0;
  uint32_t now = micros();
  if ((int32_t)(now - nextDraw) >= 0)
  {

    perf.draw(pat, vp, oled, midi, now, transport.playTick());
    nextDraw = now + 50000; // 20 FPS
  }
}