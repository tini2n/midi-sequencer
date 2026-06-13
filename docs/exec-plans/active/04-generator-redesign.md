# Exec Plan 04 — Generator Subsystem

**Status:** Not started — next phase after UI stabilises  
**Goal:** Non-destructive algorithmic note generation into the `generative` layer.  
**Touches:** `src/engine/generator.hpp`, `src/engine/euclidean_generator.*`, `src/engine/generator_manager.*`, `src/core/runloop.hpp`

---

## Steps

### 1. Generator base class

```cpp
// src/engine/generator.hpp
enum class ParamId : uint8_t {
    Density, Length, Velocity, VelRange, PitchRange, BaseNote, Duration, _Count
};

struct GeneratorParam {
    const char* name;
    float value, min, max, step;
    void set(float v) { value = v < min ? min : (v > max ? max : v); }
    void adjust(float steps) { set(value + steps * step); }
};

class Generator {
public:
    virtual const char* getName()      const = 0;
    virtual const char* getShortName() const = 0;
    virtual void generate(NotePool<128>& out, const Pattern& ctx) = 0;
    virtual void resetToDefaults() = 0;

    GeneratorParam& param(ParamId id)             { return params_[uint8_t(id)]; }
    const GeneratorParam& param(ParamId id) const { return params_[uint8_t(id)]; }

protected:
    GeneratorParam params_[uint8_t(ParamId::_Count)]{};
};
```

### 2. Port EuclideanGenerator

- `generate(NotePool<128>& out, const Pattern& ctx)` — writes to `out`, never touches `pat_.track`
- Replace `std::vector<bool> rhythm` with `bool rhythm[64]`
- Replace `std::map` params with `params_[]` array init in constructor
- Gate all `Serial.printf` behind `#ifdef SEQUENCER_DEBUG`

### 3. GeneratorManager

```cpp
// src/engine/generator_manager.hpp
class GeneratorManager {
public:
    void triggerGenerate(Pattern& p);   // call from UI (not ISR, not RunLoop)
    void applyPendingSwap(Pattern& p);  // call from RunLoop before tick drain

    Generator* current() { return generators_[current_]; }
    bool switchTo(uint8_t idx);

private:
    EuclideanGenerator euclidean_;
    Generator* generators_[4]{ &euclidean_ };
    uint8_t    count_{1}, current_{0};

    NotePool<128> staging_;
    bool          pendingSwap_{false};
};
```

`triggerGenerate`: clears `staging_`, calls `current()->generate(staging_, p)`, sets `pendingSwap_=true`.  
`applyPendingSwap`: if `pendingSwap_`, swaps `staging_` into `p.track.generative` and clears flag.

### 4. Wire into RunLoop

```cpp
// top of RunLoop::service(), before tick drain
if (genMgr_) genMgr_->applyPendingSwap(*pat_);
```

`RunLoop::begin()` gains a `GeneratorManager*` parameter (nullable — skipped if null).

### 5. Verify

- Generate fills `track.generative`; recorded notes survive the swap unchanged.
- Both layers play together during playback.
- No heap — confirm with PlatformIO RAM report.

---

## Out of Scope

Generator UI (GenerativeView) — deferred until generator subsystem is stable.  
New algorithms (Markov, probability grid, CA) — Phase 6.
