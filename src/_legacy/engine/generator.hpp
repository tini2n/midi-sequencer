#pragma once
#include <Arduino.h>
#include <map>
#include <string>

#include "model/pattern.hpp"

/**
 * Parameter value that can be controlled via encoders or serial commands.
 * Each parameter has a name, current value, min/max range, and description.
 */
struct GeneratorParameter
{
    const char* name;        // Short name for display (e.g., "DENS", "VAR")
    const char* description; // Full description for help
    float value;        // Current value
    float minValue;     // Minimum allowed value
    float maxValue;     // Maximum allowed value
    float step;         // Increment/decrement step size
    
    GeneratorParameter() = default;
    GeneratorParameter(const char* n, const char* desc, float val, float minVal, float maxVal, float stepSize = 1.0f)
        : name(n), description(desc), value(val), minValue(minVal), maxValue(maxVal), step(stepSize) {}
    
    // Clamp value to valid range
    void clamp() {
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
    }
    
    // Set value with automatic clamping
    void setValue(float newValue) {
        value = newValue;
        clamp();
    }
    
    // Adjust by step amount
    void adjust(float steps) {
        setValue(value + (steps * step));
    }
};

/**
 * Abstract base class for all pattern generators.
 * Each generator implements a specific algorithm (Euclidean, Cellular, etc.)
 * and exposes configurable parameters.
 */
class Generator
{
public:
    virtual ~Generator() = default;
    
    /**
     * Get the name of this generator algorithm.
     */
    virtual const char* getName() const = 0;
    
    /**
     * Get short display name (3-4 chars for OLED).
     */
    virtual const char* getShortName() const = 0;
    
    /**
     * Generate notes and populate the given pattern.
     * @param pattern Pattern to populate with generated notes
     */
    virtual void generate(Pattern& pattern) = 0;
    
    /**
     * Get all configurable parameters for this generator.
     * @return Map of parameter ID to parameter object
     */
    virtual const std::map<const char*, GeneratorParameter>& getParameters() const = 0;
    
    /**
     * Set a parameter value by name.
     * @param paramName Name of parameter to set
     * @param value New value (will be clamped to valid range)
     * @return true if parameter exists and was set, false otherwise
     */
    virtual bool setParameter(const char* paramName, float value) = 0;
    
    /**
     * Get a parameter value by name.
     * @param paramName Name of parameter to get
     * @param outValue Reference to store the value
     * @return true if parameter exists, false otherwise
     */
    virtual bool getParameter(const char* paramName, float& outValue) const = 0;
    
    /**
     * Get list of all parameter names for iteration.
     */
    virtual std::vector<const char*> getParameterNames() const = 0;
    
    /**
     * Reset all parameters to their default values.
     */
    virtual void resetToDefaults() = 0;
    
    /**
     * Print current parameter values to Serial for debugging.
     */
    virtual void printParameters() const;

protected:
    // Common parameters that most generators will use
    static constexpr float DEFAULT_DENSITY = 64.0f;      // 0-127
    static constexpr float DEFAULT_VARIATION = 32.0f;    // 0-127  
    static constexpr float DEFAULT_LENGTH = 16.0f;       // 1-64 steps
    static constexpr float DEFAULT_VELOCITY = 100.0f;    // 1-127
    static constexpr float DEFAULT_VEL_RANGE = 20.0f;    // 0-64
    static constexpr float DEFAULT_PITCH_RANGE = 12.0f;  // 0-24 semitones
    static constexpr float DEFAULT_BASE_NOTE = 60.0f;    // MIDI note number
};