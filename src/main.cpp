#include <Arduino.h>
#include "model/pattern.hpp"
#include "model/viewport.hpp"
#include "ui/renderer_oled.hpp"

static Pattern pattern;
static Viewport viewport;
static OledRenderer oled;

static uint32_t rng = 0xC0FFEEu;
static uint32_t rnd()
{
  rng = 1664525u * rng + 1013904223u;
  return rng;
}

static void fillDummy(Pattern &p)
{
  p.track.clear();
  for (int i = 0; i < 100; i++)
  {
    uint32_t t = (rnd() % p.ticks());
    uint32_t l = 40 + (rnd() % 400);
    uint8_t pitch = 36 + (rnd() % 32);
    uint8_t vel = 80 + (rnd() % 47);
    p.track.notes.push_back({t, l, pitch, vel, 0, 0});
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 1500)
  {
  }
  fillDummy(pattern);
  oled.begin();
  Serial.println("Viewport controls: A/D pan, W/S zoom, Z/X pitch base, R regen");
}

void loop()
{
  // controls (non-blocking)
  static bool uiDirty = false;
  static char hud[48];

  while (Serial.available())
  {
    char c = Serial.read();
    Serial.printf("key:%c\n", c);
    uiDirty = true;

    if (c == 'a' || c == 'A')
      viewport.pan_ticks(-(int32_t)(viewport.tickSpan / 8));
    if (c == 'd' || c == 'D')
      viewport.pan_ticks((int32_t)(viewport.tickSpan / 8));
    if (c == 'w' || c == 'W')
      viewport.zoom_ticks(1.25f);
    if (c == 's' || c == 'S')
      viewport.zoom_ticks(0.8f);
    if (c == 'z' || c == 'Z')
      viewport.pan_pitch(-1);
    if (c == 'x' || c == 'X')
      viewport.pan_pitch(+1);
    if (c == 'r' || c == 'R')
      fillDummy(pattern);
  }

  snprintf(hud, sizeof(hud), "t:%lu span:%lu pb:%u", viewport.tickStart, viewport.tickSpan, viewport.pitchBase);

  static uint32_t nextDraw = 0;
  uint32_t now = micros();
  if ((int32_t)(now - nextDraw) >= 0)
  {
    uint32_t frame = oled.drawFrame(pattern, viewport, now, hud);
    if (frame > 25000)
      Serial.printf("WARN frame=%lu us > 25ms\n", (unsigned long)frame);
    nextDraw = now + 50000; // 20 FPS cap (I2C)
    uiDirty = false;
  }
}
