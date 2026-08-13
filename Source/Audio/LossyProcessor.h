#pragma once

#include <JuceHeader.h>

// A lo-fi bitcrusher/downsampler ("Lossy" in the FX chain): quantizes to a
// reduced bit depth and holds samples at a reduced rate (the classic
// aliased, stair-stepped digital-degradation character - think an old
// sampler starved of bits and Hz), with an optional pre-gain drive stage
// for a harder crunch before the crush. Mono - EffectsChain owns one
// instance per channel, each with its own independent hold state.
class LossyProcessor
{
public:
    LossyProcessor() = default;

    void prepare(double sampleRate);
    void reset();

    // 1 (extreme, near square-wave) .. 16 (transparent).
    void setBits(float bitsIn) { bits.store(juce::jlimit(1.0f, 16.0f, bitsIn)); }
    // Target sample-and-hold rate in Hz - low values hold each sample
    // longer, producing the classic decimated/aliased lo-fi stair-step.
    void setRateHz(float hz) { targetRateHz.store(juce::jlimit(200.0f, 48000.0f, hz)); }
    // Pre-gain into the crush stage, for a harder/dirtier character.
    void setDriveDb(float db) { driveDb.store(juce::jlimit(0.0f, 24.0f, db)); }
    void setMix(float mix01) { mix.store(juce::jlimit(0.0f, 1.0f, mix01)); }

    // In-place is not possible (the held sample lags input by up to one
    // hold period); in and out may not alias.
    void process(const float* in, float* out, int numSamples);

private:
    double sampleRate = 44100.0;

    std::atomic<float> bits { 8.0f };
    std::atomic<float> targetRateHz { 4000.0f };
    std::atomic<float> driveDb { 0.0f };
    std::atomic<float> mix { 1.0f };

    double holdPhase = 0.0;
    float heldSample = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LossyProcessor)
};
