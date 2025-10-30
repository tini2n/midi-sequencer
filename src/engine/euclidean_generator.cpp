#include "euclidean_generator.hpp"
#include "model/note.hpp"
#include <cstdlib>

EuclideanGenerator::EuclideanGenerator()
{
    initializeParameters();
}

void EuclideanGenerator::initializeParameters()
{
    parameters_.clear();

    parameters_["density"] = GeneratorParameter(
        "DENS", "Number of hits (steps with notes)",
        DEFAULT_DENSITY, 1.0f, 64.0f, 1.0f);

    parameters_["length"] = GeneratorParameter(
        "LEN", "Pattern length in steps",
        DEFAULT_LENGTH, 1.0f, 64.0f, 1.0f);

    parameters_["velocity"] = GeneratorParameter(
        "VEL", "Base velocity for generated notes",
        DEFAULT_VELOCITY, 1.0f, 127.0f, 1.0f);

    parameters_["vel_range"] = GeneratorParameter(
        "VRAN", "Velocity randomization range (+/-)",
        DEFAULT_VEL_RANGE, 0.0f, 64.0f, 1.0f);

    parameters_["pitch_range"] = GeneratorParameter(
        "PRAN", "Pitch randomization range (semitones)",
        DEFAULT_PITCH_RANGE, 0.0f, 24.0f, 1.0f);

    parameters_["base_note"] = GeneratorParameter(
        "NOTE", "Base MIDI note number",
        DEFAULT_BASE_NOTE, 0.0f, 127.0f, 1.0f);

    parameters_["duration"] = GeneratorParameter(
        "DUR", "Note duration as fraction of step (0.1-1.0)",
        0.5f, 0.1f, 1.0f, 0.1f);
}

bool EuclideanGenerator::setParameter(const char* paramName, float value)
{
    auto it = parameters_.find(paramName);
    if (it != parameters_.end())
    {
        it->second.setValue(value);
        return true;
    }
    return false;
}

bool EuclideanGenerator::getParameter(const char* paramName, float& outValue) const
{
    auto it = parameters_.find(paramName);
    if (it != parameters_.end())
    {
        outValue = it->second.value;
        return true;
    }
    return false;
}

std::vector<const char*> EuclideanGenerator::getParameterNames() const
{
    std::vector<const char*> names;
    for (const auto& pair : parameters_)
    {
        names.push_back(pair.first);
    }
    return names;
}

void EuclideanGenerator::resetToDefaults()
{
    initializeParameters();
}

void EuclideanGenerator::generate(Pattern &pattern)
{
    // Clear existing notes
    pattern.track.notes.clear();

    // Get parameters
    float density = parameters_["density"].value;
    float length = parameters_["length"].value;
    float velocity = parameters_["velocity"].value;
    float velRange = parameters_["vel_range"].value;
    float pitchRange = parameters_["pitch_range"].value;
    float baseNote = parameters_["base_note"].value;
    float duration = parameters_["duration"].value;

    if (length == 0 || density == 0)
    {
        Serial.println("EuclideanGenerator: Invalid length or density");
        return;
    }

    uint8_t steps = (uint8_t)length;
    uint8_t hits = (uint8_t)density; // Density is now absolute number of hits
    if (hits < 1)
        hits = 1;
    if (hits > steps)
        hits = steps;

    // Generate Euclidean rhythm
    std::vector<bool> rhythm = generateEuclideanRhythm(hits, steps);

    // Convert rhythm to MIDI notes
    uint32_t ticksPerStep = (96 * 4) / pattern.grid; // Assuming 16th note grid

    for (uint8_t i = 0; i < steps && i < pattern.steps; i++)
    {
        if (rhythm[i])
        {
            Note note;
            note.on = i * ticksPerStep;
            note.duration = (uint32_t)(ticksPerStep * duration);

            // Add pitch variation
            int8_t pitchOffset = 0;
            if (pitchRange > 0)
            {
                pitchOffset = (rand() % (int)(pitchRange * 2 + 1)) - (int)pitchRange;
            }
            int pitchValue = (int)baseNote + pitchOffset;
            if (pitchValue < 0) pitchValue = 0;
            if (pitchValue > 127) pitchValue = 127;
            note.pitch = (uint8_t)pitchValue;

            // Add velocity variation
            int8_t velOffset = 0;
            if (velRange > 0)
            {
                velOffset = (rand() % (int)(velRange * 2 + 1)) - (int)velRange;
            }
            int velValue = (int)velocity + velOffset;
            if (velValue < 1) velValue = 1;
            if (velValue > 127) velValue = 127;
            note.vel = (uint8_t)velValue;

            note.micro = 0;
            note.micro_q8 = 0;
            note.flags = 0;

            pattern.track.notes.push_back(note);
        }
    }

    Serial.printf("EuclideanGenerator: Generated %d notes (%d hits in %d steps)\n",
                  pattern.track.notes.size(), hits, steps);
}

std::vector<bool> EuclideanGenerator::generateEuclideanRhythm(uint8_t hits, uint8_t steps) const
{
    std::vector<bool> rhythm(steps, false);

    if (hits == 0 || steps == 0)
        return rhythm;

    // Euclidean algorithm (Bresenham-like)
    int bucket = 0;
    for (uint8_t i = 0; i < steps; i++)
    {
        bucket += hits;
        if (bucket >= steps)
        {
            bucket -= steps;
            rhythm[i] = true;
        }
    }

    return rhythm;
}