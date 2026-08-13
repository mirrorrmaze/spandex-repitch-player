#pragma once

#include <JuceHeader.h>
#include <deque>

// A real Paulstretch-style extreme time-stretcher: large-window FFT,
// magnitude kept but phase randomised per bin, Hann-windowed overlap-add
// resynthesis. Standard phase-vocoder stretchers (Rubber Band included)
// smear/robotise badly beyond ~10-20x slowdown because phase relationships
// between overlapping windows stay coherent, producing comb-filtering.
// Randomising phase decorrelates overlapping windows on purpose, trading
// transient precision for a smooth, artifact-free "ambient cloud" sound -
// exactly the effect Paulstretch is known for. Time-only: no pitch control.
class Paulstretch
{
public:
    Paulstretch() = default;

    void prepare(int windowSizeSamples = 8192);
    void reset();

    // Emits exactly numSamples of stereo output into outL/outR, reading from
    // a stereo source and advancing cursor (fractional source sample
    // position) by roughly hopSize * speedRatio per internally-synthesised
    // hop. speedRatio: 1.0 = normal speed, 0.02 = 50x slower. Reads beyond
    // [0, sourceLength) are treated as silence.
    void process(const float* srcL, const float* srcR, int sourceLength,
                 double& cursor, double speedRatio,
                 float* outL, float* outR, int numSamples);

private:
    void synthesiseHop(const float* srcL, const float* srcR, int sourceLength, double cursor);
    static float readInterpolated(const float* data, int length, double position);
    void randomisePhase(float* fftData, juce::Random& rng) const;

    int windowSize = 8192;
    int hopSize = 2048;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hannWindow;
    float normalisation = 1.0f;

    std::vector<float> fftBufL, fftBufR;
    std::vector<float> accumL, accumR;
    juce::Random randomL { 1 }, randomR { 2 };

    std::deque<float> fifoL, fifoR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Paulstretch)
};
