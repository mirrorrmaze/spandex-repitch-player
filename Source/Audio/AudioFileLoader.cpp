#include "AudioFileLoader.h"

namespace AudioFileLoader
{
    Result load(const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        Result result;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            return result;

        const auto numChannels = (int) reader->numChannels;
        const auto numSamples = (int) reader->lengthInSamples;

        if (numChannels <= 0 || numSamples <= 0)
            return result;

        result.buffer.setSize(numChannels, numSamples);
        reader->read(&result.buffer, 0, numSamples, 0, true, true);
        result.sourceSampleRate = reader->sampleRate;
        result.success = true;

        return result;
    }
}
