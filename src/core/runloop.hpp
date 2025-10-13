#pragma once
#include <vector>

#include "model/pattern.hpp"
#include "engine/playback_engine.hpp"

#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"

struct AppEvent
{
    enum class Type : uint8_t { Play, Stop, Pause, Resume } type;
};

class RunLoop
{
public:
    void begin(TickScheduler *s, Transport *tr, PlaybackEngine *pe, MidiIO *mi, Pattern *pa)
    {
        sched_ = s;
        tx_ = tr;
        eng_ = pe;
        midi_ = mi;
        pat_ = pa;
        evs_.reserve(32);
    }
    void post(const AppEvent &e) { if (evtN_ < MaxEvt) evtQ_[evtN_++] = e; }
    void service()
    {
        // Handle posted app events first
        for (size_t i = 0; i < evtN_; ++i)
        {
            const auto &e = evtQ_[i];
            switch (e.type)
            {
            case AppEvent::Type::Play: tx_->resume(); midi_->sendContinue(); break;
            case AppEvent::Type::Stop: tx_->stop(); midi_->sendStop(); break;
            case AppEvent::Type::Pause: tx_->pause(); break;
            case AppEvent::Type::Resume: tx_->resume(); midi_->sendContinue(); break;
            }
        }
        evtN_ = 0;

        TickEvent e;
        TickWindow w;

        while (sched_->fetch(e))
            tx_->on1ms();

        while (tx_->next(w))
        {
            eng_->processTick(w.prev, w.curr, *pat_, evs_);
            if (++clkDiv_ == 4)
            {
                midi_->sendClock();
                clkDiv_ = 0;
            }
        }

        for (const auto &m : evs_)
            midi_->send(m);

        evs_.clear();
        midi_->update();
    }
    uint32_t playTick() const { return tx_->playTick(); }

private:
    TickScheduler *sched_{};
    Transport *tx_{};
    PlaybackEngine *eng_{};
    MidiIO *midi_{};
    Pattern *pat_{};

    std::vector<MidiEvent> evs_;
    
    uint8_t clkDiv_{0}; // 96/24 = 4 ticks per MIDI clock

    // Simple event queue
    static constexpr size_t MaxEvt = 8;
    AppEvent evtQ_[MaxEvt]{};
    size_t evtN_{0};
};
