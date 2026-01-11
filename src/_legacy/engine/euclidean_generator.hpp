#pragma once
#include "generator.hpp"
#include <vector>

/**
 * Euclidean rhythm generator.
 * Distributes a given number of hits evenly across a specified number of steps
 * using the Euclidean algorithm (similar to Bresenham's line algorithm).
 */
class EuclideanGenerator : public Generator
{
public:
    EuclideanGenerator();
    
    // Generator interface
    const char* getName() const override { return "Euclidean Rhythm"; }
    const char* getShortName() const override { return "EUC"; }
    void generate(Pattern& pattern) override;
    const std::map<const char*, GeneratorParameter>& getParameters() const override { return parameters_; }
    bool setParameter(const char* paramName, float value) override;
    bool getParameter(const char* paramName, float& outValue) const override;
    std::vector<const char*> getParameterNames() const override;
    void resetToDefaults() override;

private:
    std::map<const char*, GeneratorParameter> parameters_;
    
    void initializeParameters();
    std::vector<bool> generateEuclideanRhythm(uint8_t hits, uint8_t steps) const;
};