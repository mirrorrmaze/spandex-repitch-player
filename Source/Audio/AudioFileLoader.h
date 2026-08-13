#pragma once

#include <JuceHeader.h>

// Decodes an entire audio file into memory in one shot. The app works on
// whole DJ-length tracks, not multi-hour files, so preloading keeps the
// real-time stretch engine (added later) free of disk-streaming concerns.
namespace AudioFileLoader
{
    struct Result
    {
        bool success = false;
        juce::AudioBuffer<float> buffer;
        double sourceSampleRate = 44100.0;
    };

    Result load(const juce::File& file, juce::AudioFormatManager& formatManager);
}
