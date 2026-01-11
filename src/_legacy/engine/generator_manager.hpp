#pragma once
#include <Arduino.h>
#include <vector>
#include <memory>

#include "generator.hpp"
#include "model/pattern.hpp"

/**
 * Manages multiple generators and provides a unified interface for pattern generation.
 * Handles generator selection, parameter management, and coordinates pattern generation.
 */
class GeneratorManager
{
public:
    GeneratorManager();
    ~GeneratorManager() = default;
    
    /**
     * Initialize with available generators.
     */
    void begin();
    
    /**
     * Register a generator with the manager.
     * @param generator Unique pointer to generator (manager takes ownership)
     */
    void registerGenerator(std::unique_ptr<Generator> generator);
    
    /**
     * Get number of registered generators.
     */
    size_t getGeneratorCount() const { return generators_.size(); }
    
    /**
     * Switch to a specific generator by index.
     * @param index Generator index (0-based)
     * @return true if switch successful, false if invalid index
     */
    bool switchToGenerator(size_t index);
    
    /**
     * Switch to next generator (cycles through available generators).
     */
    void switchToNextGenerator();
    
    /**
     * Switch to previous generator.
     */
    void switchToPreviousGenerator();
    
    /**
     * Get current generator index.
     */
    size_t getCurrentGeneratorIndex() const { return currentIndex_; }
    
    /**
     * Get current generator instance.
     */
    Generator* getCurrentGenerator() const;
    
    /**
     * Generate pattern using current generator.
     * @param pattern Pattern to populate with generated notes
     */
    void generatePattern(Pattern& pattern);
    
    /**
     * Set a parameter for the current generator.
     * @param paramName Name of parameter to set
     * @param value New value
     * @return true if parameter was set successfully
     */
    bool setParameter(const char* paramName, float value);
    
    /**
     * Get a parameter from the current generator.
     * @param paramName Name of parameter to get
     * @param outValue Reference to store the value
     * @return true if parameter exists
     */
    bool getParameter(const char* paramName, float& outValue) const;
    
    /**
     * Get list of parameter names for the current generator.
     */
    std::vector<const char*> getCurrentParameterNames() const;
    
    /**
     * Print current generator info and parameters.
     */
    void printCurrentGenerator() const;
    
    /**
     * Print list of all available generators.
     */
    void printAvailableGenerators() const;
    
    /**
     * Reset current generator parameters to defaults.
     */
    void resetCurrentGeneratorToDefaults();

private:
    std::vector<std::unique_ptr<Generator>> generators_;
    size_t currentIndex_{0};
    
    bool isValidIndex(size_t index) const;
};