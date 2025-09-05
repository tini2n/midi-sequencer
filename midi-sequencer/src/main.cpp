#include <Arduino.h>

#include "core/tick_scheduler.hpp"
#include "config.hpp"

TickScheduler scheduler;

static uint32_t intervals[1000];
static uint16_t idx = 0;
static uint32_t last_us = 0;

static void sort_u32(uint32_t* a, size_t n){   // simple insertion sort
  for(size_t i=1;i<n;i++){ uint32_t k=a[i], j=i; 
    while(j>0 && a[j-1]>k){ a[j]=a[j-1]; j--; } a[j]=k; }
}

void setup() {
  Serial.begin(115200);
  while(!Serial && millis()<1500){}           // teensy USB wait
  Serial.println("Clock&Scheduler bring-up @1kHz");
  if(!scheduler.begin()) Serial.println("Timer begin failed!");
}

void loop() {
  TickEvent e;
  while(scheduler.fetch(e)){
    if(last_us){ 
      uint32_t dt = e.tmicros - last_us;
      if(idx<1000) intervals[idx++] = (dt>cfg::TICK_US)? (dt-cfg::TICK_US) : (cfg::TICK_US-dt);
    }
    last_us = e.tmicros;
  }

  if(idx==1000){
    sort_u32(intervals, 1000);
    uint32_t p95 = intervals[950];
    uint32_t p50 = intervals[500];
    uint32_t pmax= intervals[999];
    Serial.printf("Jitter abs-error (us): p50=%lu p95=%lu max=%lu, dropped=%lu, depth~ %lu\n",
                  p50, p95, pmax, (unsigned long)scheduler.dropped(), (unsigned long)0);
    idx=0; last_us=0; delay(1000);            // repeat runs
  }
}