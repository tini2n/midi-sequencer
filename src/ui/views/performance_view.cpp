#include "performance_view.hpp"

void PerformanceView::octaveUp()
{
    if (st_.octave < 9)
        st_.octave++;
}
void PerformanceView::octaveDown()
{
    if (st_.octave > 0)
        st_.octave--;
}
void PerformanceView::cycleMode() { st_.mode = (PerformanceMode)(((uint8_t)st_.mode + 1) % 3); } // KB→SC→CHR
void PerformanceView::toggleHold() { /* TODO: latch */ }
void PerformanceView::panic(MidiIO &m)
{
    for (auto &kv : active_)
        m.send({st_.channel, kv.second, 0, false, 0});
    active_.clear();
}

void PerformanceView::handleFunction(uint8_t btn)
{
    if (btn == 7)
    {
        if (st_.octave < 9)
            st_.octave++;
    }
    else if (btn == 8)
    {
        if (st_.octave > 0)
            st_.octave--;
    }
    else if (btn == 14)
    {
        st_.mode = (PerformanceMode)(((uint8_t)st_.mode + 1) % 3); // Keyboard→Scale→Chromatic
    }
    else if (btn == 15)
    {
        // TODO: latch/hold
    }
}

void PerformanceView::handleInput(MidiIO &midi)
{
    std::vector<CursorEvent> evs;
    evs.reserve(8);
    input_.poll(evs);

    for (auto &e : evs)
    {
        auto mr = km_.map(e.btnId, st_);
        
        if (mr.pitch < 0)
            continue; // ignore non-note pads here

        const uint8_t pitch = (uint8_t)mr.pitch;
        if (e.down)
        {
            active_[e.btnId] = pitch;
            sendOn(midi, pitch);
            lastPitch_ = pitch;
        }
        else
        {
            auto it = active_.find(e.btnId);
            if (it != active_.end())
            {
                sendOff(midi, it->second);
                active_.erase(it);
            }
        }
    }
}

void PerformanceView::tick(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick)
{
    (void)midi;
    (void)pat;
    (void)vp;
    (void)playTick;
    char hud[48];

    const char *modeStr = (st_.mode == PerformanceMode::Keyboard) ? "KB" : (st_.mode == PerformanceMode::Scale) ? "SC"
                                                                                                                : "CHR";
    snprintf(hud, sizeof(hud), "%s CH:%u OC:%d BP:%d", modeStr, st_.channel, st_.octave, (int)pat.tempo);

    PianoRoll::Options o = {};
    o.highlightPitch = lastPitch_;               // 1-line stripe near label
    oled.rollSetOptions(o);                      // uses stripe drawing in roll render :contentReference[oaicite:0]{index=0}
    oled.drawFrame(pat, vp, now, playTick, hud); // HUD bar at top (5x7) :contentReference[oaicite:1]{index=1}
}
