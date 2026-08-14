#pragma once

#include <JuceHeader.h>
#include <deque>

// A frequency-domain hard clipper: each analysis hop's magnitude spectrum
// is clipped to an Amount-controlled ceiling (phase left untouched), then
// blended against the dry spectrum by that same Amount - a harsher, more
// "digital"/present distortion character than a broadband compressor or a
// time-domain saturator (like the Gain card's separate Drive knob) can
// produce, since whichever frequencies happen to be hot get flattened on
// their own rather than the whole signal being ducked by one shared
// gain-reduction envelope. Replaces the old Compression knob, which this
// project's own feedback called out as "doesn't do much... should be
// adding a lot of bite."
//
// Mono - EffectsChain owns one instance per channel.
class SpectralClipperProcessor
{
public:
    SpectralClipperProcessor() = default;

    void prepare();
    void reset();

    // 0 = transparent bypass (still delayed by the STFT). 1 = maximally
    // aggressive: a very low clip ceiling flattens most bins, with makeup
    // gain added back in proportionally so it reads as more present/
    // in-your-face rather than just quieter.
    void setAmount(float amount01) { amount.store(juce::jlimit(0.0f, 1.0f, amount01)); }
    float getAmount() const noexcept { return amount.load(); }

    // In-place is not possible (output lags input by the window's
    // latency); in and out may not alias.
    void process(const float* in, float* out, int numSamples);

private:
    void processHop();
    float processOneSample(float inSample);

    static constexpr int windowSize = 1024;
    static constexpr int hopSize = windowSize / 4;
    static constexpr int fftOrder = 10; // 2^10 = 1024

    juce::dsp::FFT fft { fftOrder };
    std::vector<float> hannWindow;
    // Overlap-add reconstruction gain, and a separate reference scale for
    // what "full-scale" magnitude means for this window, so Amount clips
    // against something perceptually meaningful rather than FFT's
    // unnormalized raw magnitude units.
    float olaNormalisation = 1.0f;
    float magnitudeScale = 1.0f;

    std::vector<float> inputRing;
    int inputWritePos = 0;
    int samplesSinceHop = 0;

    std::vector<float> fftScratch;
    std::vector<float> accum;
    std::deque<float> outputFifo;

    std::atomic<float> amount { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralClipperProcessor)
};
