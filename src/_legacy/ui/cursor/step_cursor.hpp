#pragma once
#include <Arduino.h>
#include <string.h>
#include "io/pcf8575.hpp"
#include "step/step_pattern.hpp"
#include "step/step_transport.hpp"

namespace step {

class Cursor {
public:
    struct Pins { uint8_t rows[3]{0,1,2}; uint8_t cols[8]{8,9,10,11,12,13,14,15}; };

    void begin(PCF8575* io, const Pins& map, Pattern* pat, Transport* tx) {
        pcf_ = io; pins_ = map; pat_ = pat; tx_ = tx;
        memset(lastDown_, 0, sizeof(lastDown_));
        memset(debUntil_, 0, sizeof(debUntil_));
        pcf_->write(0xFFFF); // all inputs high
    }

    void poll() {
        uint32_t now = micros();
        for (uint8_t r = 0; r < 3; r++) {
            driveRow(r);
            uint16_t v = 0xFFFF; pcf_->read(v);
            for (uint8_t c = 0; c < 8; c++) {
                bool down = ((v >> pins_.cols[c]) & 1) == 0;
                uint8_t id = btnId(r, c);
                if (now < debUntil_[id]) continue;
                if (down == lastDown_[r][c]) continue;
                lastDown_[r][c] = down; debUntil_[id] = now + pat_->track().debounce_us;
                if (!down) continue; // rising edge only
                if (r == 0) onTop(c);
                else if (r == 1) onBottom(c);
                else onCtrl(c);
            }
        }
    }

    uint16_t cursor() const { return cur_; }

private:
    static inline uint8_t btnId(uint8_t r, uint8_t c) { return r * 8 + c; }
    void driveRow(uint8_t r) {
        uint16_t out = 0xFFFF; // all input-high
        out &= ~(1u << pins_.rows[r]);
        for (uint8_t i = 0; i < 3; i++) if (i != r) out |= (1u << pins_.rows[i]);
        pcf_->write(out);
    }
    void onTop(uint8_t col) { cur_ = (pat_->track().page * 16u + col) & 127u; }
    void onBottom(uint8_t col) {
        Track& tr = pat_->track();
        uint8_t idx = (tr.page * 16u + col) & 127u;
        tr.steps.flip(idx);
    }
    void onCtrl(uint8_t col) {
        if (col == 0) { tx_->toggle(); return; }
        if (col == 1) { pat_->track().page = (pat_->track().page + 1) & 7; }
    }

    PCF8575* pcf_{}; Pins pins_{};
    Pattern* pat_{}; Transport* tx_{};
    bool lastDown_[3][8]{}; uint32_t debUntil_[24]{};
    uint16_t cur_{0};
};

} // namespace step
