#include <Arduino.h>
#include <Wire.h>

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "core/runloop.hpp"
#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "engine/playback_engine.hpp"
#include "engine/record_engine.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/views/view_manager.hpp"
#include "ui/views/performance_view.hpp"
#include "ui/views/generative_view.hpp"
#include "io/serial_monitor_input.hpp"
#include "io/encoder_manager.hpp"
#include "io/matrix_kb.hpp"

static constexpr uint16_t PPQN = 96;
static inline uint32_t ticksPerStep(uint16_t gridDiv) { return (uint32_t(PPQN) * 4u) / gridDiv; }

// Encoder pin configuration
// ENC1: A=2,  B=3,  SW=0
// ENC2: A=4,  B=5,  SW=12
// ENC3: A=6,  B=7,  SW=26
// ENC4: A=14, B=15, SW=27
// ENC5: A=16, B=17, SW=28
// ENC6: A=20, B=21, SW=29
// ENC7: A=22, B=23, SW=30
// ENC8: A=24, B=25, SW=31
const EncoderManager::PinConfig ENCODER_PINS[8] = {
    {2, 3, 0},    // ENC1
    {4, 5, 12},   // ENC2
    {6, 7, 26},   // ENC3
    {14, 15, 27}, // ENC4
    {16, 17, 28}, // ENC5
    {20, 21, 29}, // ENC6
    {22, 23, 30}, // ENC7
    {24, 25, 31}  // ENC8
};

TickScheduler sched;
Transport transport;
PlaybackEngine engine;
MidiIO midi;
RunLoop runner;
Pattern pat;
Viewport vp;
OledRenderer oled;
ViewManager viewManager;
PerformanceView performanceView;
GenerativeView generativeView;
RecordEngine recorder;
SerialMonitorInput serialIn;
MatrixKB matrixKB; // Shared hardware controller

static uint16_t visibleSteps = 16; // S ∈ {1,2,4,8,16,32,64}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 1500)
  {
  }

  Serial.println("MIDI Sequencer. Initing...");
  Serial.println("View system: Performance (default) and Generative modes");
  Serial.println("Switch views: Control button 6");

  // Initialize SPI devices first (OLED)
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
  // transport.start();

  vp.pitchBase = 36;
  vp.tickSpan = ticksPerStep(pat.grid) * visibleSteps;

  runner.begin(&sched, &transport, &engine, &midi, &pat);
  recorder.begin(&pat, &transport);
  
  // Initialize I2C bus for matrix keyboard (PCF8575)
  Wire.begin();
  Wire.setClock(400000); // 400kHz
  delay(50); // Let I2C bus stabilize
  
  // Initialize shared matrix keyboard
  MatrixKB::Config kbConfig;
  kbConfig.address = cfg::PCF_ADDRESS;
  matrixKB.begin(kbConfig, 0, 4, 100);
  matrixKB.attach(&runner, &recorder, &transport);
  matrixKB.attachViewManager(&viewManager);
  
  // Initialize view system
  viewManager.registerView(ViewType::Performance, &performanceView);
  viewManager.registerView(ViewType::Generative, &generativeView);
  
  // Inject shared MatrixKB and Pattern into views
  performanceView.setMatrixKB(&matrixKB);
  generativeView.setMatrixKB(&matrixKB);
  generativeView.setPattern(&pat);
  
  viewManager.initialize(&runner, &recorder, &transport, &pat, pat.track.channel,
                         ENCODER_PINS, cfg::ENCODER_DEBOUNCE_US);

  serialIn.attach(&runner, &transport, &pat, &vp, &viewManager, &performanceView);
}

void loop()
{
  // Run clock & transport and generate events
  runner.service();

  // UI input
  viewManager.pollKB(midi);
  viewManager.pollEncoders();
  serialIn.poll(midi); // Serial monitor controls

  // Draw
  static uint32_t nextDraw = 0;
  uint32_t now = micros();
  if ((int32_t)(now - nextDraw) >= 0)
  {
    viewManager.draw(pat, vp, oled, midi, now, transport.playTick());
    nextDraw = now + 33333; // ~30 FPS
  }
}