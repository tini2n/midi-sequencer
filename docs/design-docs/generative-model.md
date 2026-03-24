# Generative Model

## Generator Interface (target)

```cpp
enum class ParamId : uint8_t { Density, Length, Velocity, VelRange, PitchRange, BaseNote, Duration, _Count };

struct GeneratorParam {
    const char* name;
    float value, min, max, step;
};

class Generator {
public:
    virtual const char* getName() const = 0;
    virtual const char* getShortName() const = 0;

    // Write result into out — does NOT touch Pattern
    virtual void generate(NotePool<128>& out, const Pattern& ctx) = 0;

    GeneratorParam& param(ParamId id)       { return params_[uint8_t(id)]; }
    const GeneratorParam& param(ParamId id) const { return params_[uint8_t(id)]; }

protected:
    GeneratorParam params_[uint8_t(ParamId::_Count)]{};
};
```

Key change from legacy: `generate()` receives a `NotePool<128>&` output buffer and a
`const Pattern&` for context (tempo, grid, steps). It does NOT call `pattern.track.notes.clear()`.

## GeneratorManager

```cpp
class GeneratorManager {
public:
    void begin();
    bool switchTo(uint8_t index);
    Generator* current();

    // Call this from UI (not from RunLoop)
    void triggerGenerate(Pattern& pattern);

private:
    // Static array — no heap
    EuclideanGenerator euclidean_;
    // MarkovGenerator markov_;  // future
    Generator* generators_[4]{&euclidean_};
    uint8_t count_{1};
    uint8_t current_{0};

    NotePool<128> staging_;
    bool pendingSwap_{false};
};
```

`triggerGenerate` fills `staging_`, sets `pendingSwap_ = true`.
`RunLoop` checks `pendingSwap_` at the start of `service()`, before processing ticks,
and calls `pattern.track.swapGenerative(staging_)` if set.

## Algorithms Planned

| Name | Short | Description |
|------|-------|-------------|
| Euclidean | EUC | Bresenham distribution of hits across steps |
| Markov | MRK | Pitch/rhythm transition probabilities |
| Probability grid | PRB | Per-step trigger probability |
| Cellular automata | CEL | 1D CA rule (Rule 30, 110, etc.) maps to rhythm |
