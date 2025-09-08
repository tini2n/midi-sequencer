#include <Arduino.h>
#include "model/pattern.hpp"
#include "model/viewport.hpp"
#include "ui/renderer_oled.hpp"

static constexpr uint16_t PPQN = 96;
static inline uint32_t ticksPerStep(uint16_t gridDiv){ return (uint32_t(PPQN)*4u)/gridDiv; }

static Pattern pat;
static Viewport vp;
static OledRenderer oled;

static uint32_t rng=0xC0FFEEu; static inline uint32_t rnd(){ rng=1664525u*rng+1013904223u; return rng; }

static uint16_t visSteps = 16;    // S ∈ {1,2,4,8,16,32,64}
static uint32_t playTick = 0;
static uint32_t t0ms = 0;

static void regenNotes(){
  pat.track.clear();
  // 64 steps over ~10 octaves (24..127), ~140 notes
  const uint32_t stepT = ticksPerStep(pat.grid);
  for(uint16_t s=0;s<pat.steps;s+= (1+(rnd()%4))){        // sparse
    uint8_t reps = 1 + (rnd()%3);
    for(uint8_t r=0;r<reps;r++){
      uint8_t pitch = 24 + (rnd()%104);                  // 24..127
      uint32_t on   = s*stepT + (rnd()% (stepT/2));      // slight offset
      uint32_t dur  = stepT * (1 + (rnd()%2));           // 1–2 steps
      uint8_t  vel  = 60 + (rnd()%68);
      pat.track.notes.push_back({on,dur,0,pitch,vel,0});
    }
  }
}

static void updatePlayTick(){
  // simple transport from millis (wrap to pattern)
  float bpm = pat.tempo;
  uint32_t ticksPerSec = (uint32_t)(PPQN * (bpm/60.0f));
  uint32_t elapsed = millis() - t0ms;
  playTick = (uint64_t)elapsed * ticksPerSec / 1000u;
  const uint32_t len = pat.ticks();
  if(len) playTick %= len;
}

void setup(){
  Serial.begin(115200);
  while(!Serial && millis()<1500){}
  // pattern setup
  pat.grid = 16;  // 1/16
  pat.steps   = 64;  // 4 bars @ 16th grid
  pat.tempo   = 120.f;

  regenNotes();
  oled.begin();
  t0ms = millis();
  Serial.println("Controls: A/D pan (A/D×4 = shift: use uppercase), W/S zoom steps, Z/X pitch, R regen");
}

void loop(){
  // input
  bool uiDirty=false;
  while(Serial.available()){
    char c=Serial.read();
    Serial.printf("key:%c\n", c);
    uint32_t stepT = ticksPerStep(pat.grid);
    switch(c){
      case 'a': vp.pan_ticks(-(int32_t)stepT); uiDirty=true; break;
      case 'A': vp.pan_ticks(-(int32_t)(stepT*4)); uiDirty=true; break;
      case 'd': vp.pan_ticks( (int32_t)stepT ); uiDirty=true; break;
      case 'D': vp.pan_ticks( (int32_t)(stepT*4)); uiDirty=true; break;
      case 'w': if(visSteps>1){ visSteps/=2; vp.zoom_ticks(2.0f, pat.ticks()); } uiDirty=true; break;
      case 's': if(visSteps<64){ visSteps*=2; vp.zoom_ticks(0.5f, pat.ticks()); } uiDirty=true; break;
      case 'z': vp.pan_pitch(-1); uiDirty=true; break;
      case 'x': vp.pan_pitch(+1); uiDirty=true; break;
      case 'Z': vp.pan_pitch(-12); uiDirty=true; break;
      case 'X': vp.pan_pitch(+12); uiDirty=true; break;
      case 'r': regenNotes(); uiDirty=true; break;
    }
    vp.clamp(pat.ticks());
  }

  // draw (20 FPS cap) + HUD
  static uint32_t nextDraw=0;
  uint32_t now=micros();
  updatePlayTick();
  if(uiDirty || (int32_t)(now-nextDraw)>=0){
    char hud[64];
    snprintf(hud,sizeof(hud),"t:%lu span:%lu S:%u pb:%u",
             (unsigned long)vp.tickStart,(unsigned long)vp.tickSpan, visSteps, vp.pitchBase);
    uint32_t frame=oled.drawFrame(pat, vp, now, hud); // playhead rendered inside widget
    if(frame>25000) Serial.printf("WARN frame=%lu us > 25ms\n",(unsigned long)frame);
    nextDraw = now + 50000; // ≈20 FPS
  }
}
