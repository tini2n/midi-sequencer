# Exec Plan 04 — Generator Redesign

> **Note:** Renumbered from 02 → 04. Now Phase 4 in the roadmap (after core pipeline and recording).

**Status:** Not started (depends on 01-data-model, 02-core-pipeline, 03-recording)
**Goal:** Fix parameter system, remove heap, make generation non-destructive.
**Touches:** `src/engine/generator.hpp`, `src/engine/euclidean_generator.*`, `src/engine/generator_manager.*`

---

## Steps

### 1. Replace parameter map with fixed array

In `src/engine/generator.hpp`:
```cpp
enum class ParamId : uint8_t { Density, Length, Velocity, VelRange, PitchRange, BaseNote, Duration, _Count };

struct GeneratorParam {
    const char* name;        // display label
    const char* description;
    float value, min, max, step;

    void set(float v) { value = v < min ? min : (v > max ? max : v); }
    void adjust(float steps) { set(value + steps * step); }
};

class Generator {
public:
    virtual const char* getName() const = 0;
    virtual const char* getShortName() const = 0;
    virtual void generate(NotePool<128>& out, const Pattern& ctx) = 0;
    virtual void resetToDefaults() = 0;

    GeneratorParam& param(ParamId id)             { return params_[uint8_t(id)]; }
    const GeneratorParam& param(ParamId id) const { return params_[uint8_t(id)]; }
    uint8_t paramCount() const                    { return paramCount_; }

protected:
    GeneratorParam params_[uint8_t(ParamId::_Count)]{};
    uint8_t paramCount_{0};
};
```

### 2. Port `EuclideanGenerator`

- Change `generate(Pattern&)` → `generate(NotePool<128>& out, const Pattern& ctx)`.
- Replace `std::vector<bool> rhythm` with `bool rhythm[64]`.
- Remove `parameters_.clear()` / `parameters_["density"] = ...` — init params array instead.
- Remove all `Serial.printf` — add `#ifdef SEQUENCER_DEBUG` guard.

### 3. Update `GeneratorManager`

- Replace `std::vector<std::unique_ptr<Generator>>` with static array.
- Add `NotePool<128> staging_` member.
- `triggerGenerate(Pattern& p)`:
  1. `staging_.clear()`
  2. `current()->generate(staging_, p)`
  3. `pendingSwap_ = true`
- `applyPendingSwap(Pattern& p)`:
  - Called by `RunLoop` before tick drain.
  - If `pendingSwap_`: `p.track.swapGenerative(staging_); pendingSwap_ = false;`

### 4. Wire into `RunLoop::service()`

At the top of `service()`, before tick drain:
```cpp
if (genMgr_) genMgr_->applyPendingSwap(*pat_);
```

### 5. Update `GenerativeView`

- `triggerGeneration(Pattern& p)` calls `generatorManager_.triggerGenerate(p)`.
- Encoder handlers call `generatorManager_.current()->param(id).adjust(delta)`.
- Remove all `std::vector<const char*>` parameter name iteration.

---

## Out of Scope

- New generator algorithms (Phase 4)
