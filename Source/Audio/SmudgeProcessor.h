#pragma once

#include <JuceHeader.h>
#include <deque>

// A spectral freeze/smear effect (FabFilter Saturn 2's "Smudge"): a
// streaming STFT where each frequency bin's complex value is blended with
// what was there before (held = held*amount + new*(1-amount)) rather than
// replaced outright. At amount=0 it's transparent; as amount approaches 1,
// spectral content is retained and blurred across time, smearing transients
// into a sustained, hazy texture instead of Paulstretch's fully randomised-
// phase "cloud" - here phase evolves naturally, only magnitude/phase
// *change* is damped.
//
// Mono - EffectsChain owns one instance per channel.
class SmudgeProcessor
{
public:
    SmudgeProcessor() = default;

    void prepare(int windowSizeSamples = 2048);
    void reset();

    void setAmount(float amount01) { amount = juce::jlimit(0.0f, 0.98f, amount01); }

    // Feeds the processed output back into the input, on top of the
    // spectral-hold blend Amount already does - a second, independent axis
    // for pushing the smear into more resonant/extreme, self-reinforcing
    // territory rather than a flat freeze. The feedback tap is internally
    // saturated so it stays bounded even at Amount+Feedback both maxed.
    void setFeedback(float feedback01) { feedback = juce::jlimit(0.0f, 0.95f, feedback01); }

    // Changes the STFT window size (and so the smear's effective "rate" -
    // shorter windows update faster/more responsively, longer windows are
    // more sluggish and hold content longer). Safe to call from any thread;
    // the change is picked up and applied (reallocating internal buffers)
    // at the start of the next process() call rather than immediately, so
    // it never tears audio-thread state mid-block.
    void setWindowSizeSamples(int samples) { pendingWindowSize.store(juce::nextPowerOfTwo(juce::jlimit(256, 8192, samples))); }
    int getWindowSizeSamples() const { return windowSize; }

    // In-place is not possible (output lags input by the window's latency);
    // in and out may not alias.
    void process(const float* in, float* out, int numSamples);

private:
    void processHop();
    float processOneSample(float inSample);

    std::atomic<int> pendingWindowSize { 2048 };
    int windowSize = 2048;
    int hopSize = 512;

    // A resize (window size change) tears down and rebuilds all STFT state,
    // which is a hard discontinuity if applied instantly - and sliders fire
    // onValueChange continuously during a drag, so an instant reset per
    // change would click repeatedly throughout. Instead the resize is
    // sandwiched between a short fade-out (old engine) and fade-in (new
    // engine, from silence) so the transition is inaudible as a click even
    // if the user drags continuously.
    enum class ResizeState { Steady, FadingOut, FadingIn };
    ResizeState resizeState = ResizeState::Steady;
    // Was 256 (~5.8ms at 44.1kHz) with a linear fade - short and linear
    // enough that a rate-knob drag crossing several window sizes produced a
    // rapid back-to-back sequence of audible dips (reads as "stepping"
    // rather than one smooth change). Longer + equal-power (see the sin/cos
    // curve in process()) makes each individual transition read as a soft
    // cross-dissolve instead of a cut; FxPanel also now debounces the rate
    // knob so a drag only ever triggers one of these, not several.
    static constexpr int fadeLengthSamples = 1024;
    int fadeSamplesRemaining = 0;
    int pendingResizeTarget = 2048;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hannWindow;
    float normalisation = 1.0f;

    std::vector<float> inputRing;
    int inputWritePos = 0;
    int samplesSinceHop = 0;

    std::vector<float> fftScratch;
    std::vector<float> heldRe, heldIm;
    std::vector<float> accum;
    std::deque<float> outputFifo;

    std::atomic<float> amount { 0.0f };
    std::atomic<float> feedback { 0.0f };
    // amount/feedback are read fresh at every hop with no smoothing of their
    // own - a fast slider drag (several onValueChange calls between hops)
    // could jump the blend ratio abruptly enough to click, especially at
    // small hop sizes. These track the atomics above with a simple one-pole
    // ramp (per-sample, not tied to sample rate - this class doesn't know
    // it - just tuned by ear to be fast enough not to feel laggy) so every
    // hop always blends with a value that's continuous sample-to-sample.
    float smoothedAmount = 0.0f;
    float smoothedFeedback = 0.0f;
    static constexpr float paramSmoothingCoeff = 0.0015f;
    float feedbackState = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmudgeProcessor)
};
