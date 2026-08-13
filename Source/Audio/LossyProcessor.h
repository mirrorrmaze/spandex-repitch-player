#pragma once

#include <JuceHeader.h>
#include <deque>

// A spectral, "codec-style" lo-fi degradation effect, modeled after
// Goodhertz's Lossy - not a time-domain bitcrusher, but a streaming STFT
// that quantizes the magnitude spectrum and randomizes per-bin phase,
// refreshing that quantized/jittered frame at a controllable rate: held
// (low rate) for a smeared, underwater texture, or refreshed almost every
// hop (high rate) for a garbled, glitchy one. Phase jitter specifically is
// what gives this a "bad cellphone codec" character a purely time-domain
// bitcrusher can't produce - real low-bitrate speech codecs lose phase
// coherence between frames in exactly this way.
//
// Mono - EffectsChain owns one instance per channel.
class LossyProcessor
{
public:
    LossyProcessor() = default;

    void prepare(double sampleRate);
    void reset();

    // 1 (extreme, only a handful of magnitude steps) .. 16 (transparent).
    void setBits(float bitsIn) { bits.store(juce::jlimit(1.0f, 16.0f, bitsIn)); }
    // How many times per second the quantized/jittered spectral frame
    // refreshes - low values hold old spectral content longer (spectral
    // smear), high values refresh almost every hop (garbled/glitchy).
    void setRefreshHz(float hz) { refreshHz.store(juce::jlimit(1.0f, 200.0f, hz)); }
    // Per-bin phase randomization amount, applied only at a refresh - the
    // primary ingredient behind the garbled/robotic "lost sync" character.
    void setJitter(float jitter01) { jitter.store(juce::jlimit(0.0f, 1.0f, jitter01)); }
    // Blends the quantized+jittered spectrum against the original one,
    // entirely within the spectral domain (not a separate sample-level
    // dry path - the whole pipeline is windowSize-delayed either way, so
    // mixing an undelayed dry signal back in would just comb-filter
    // against it). 0 leaves the frame untouched (still delayed by the
    // STFT, but otherwise transparent); 1 is fully quantized+jittered
    // whenever a refresh happens.
    void setMix(float mix01) { mix.store(juce::jlimit(0.0f, 1.0f, mix01)); }

    // In-place is not possible (output lags input by the window's
    // latency); in and out may not alias.
    void process(const float* in, float* out, int numSamples);

private:
    void processHop();
    float processOneSample(float inSample);

    static constexpr int windowSize = 1024;
    static constexpr int hopSize = windowSize / 4;
    static constexpr int fftOrder = 10; // 2^10 = 1024

    double sampleRate = 44100.0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> hannWindow;
    // Overlap-add reconstruction gain (Smudge's steady-state trick) and a
    // separate reference scale for what "full-scale" magnitude means for
    // this window, so the Bits control quantizes something perceptually
    // meaningful rather than FFT's unnormalized raw magnitude units.
    float olaNormalisation = 1.0f;
    float magnitudeScale = 1.0f;

    std::vector<float> inputRing;
    int inputWritePos = 0;
    int samplesSinceHop = 0;

    std::vector<float> fftScratch;
    // Only the quantized+jittered "wet" spectrum is held/refreshed at the
    // configured rate - the dry portion of the Mix blend always uses the
    // current hop's actual spectrum, so mix=0 stays a true bypass no
    // matter how slow refreshHz is set.
    std::vector<float> heldWetRe, heldWetIm;
    std::vector<float> accum;
    std::deque<float> outputFifo;

    int hopsSinceRefresh = 0;
    juce::Random random;

    std::atomic<float> bits { 8.0f };
    std::atomic<float> refreshHz { 40.0f };
    std::atomic<float> jitter { 0.3f };
    std::atomic<float> mix { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LossyProcessor)
};
