#pragma once

#include <JuceHeader.h>
#include <deque>

// A frequency shifter / ring modulator in the spirit of Ableton's
// "Frequency Shifter" (Shifter) device.
//
// Shift mode does a true single-sideband frequency shift - every partial
// moves by the same number of Hz (not the same ratio, like a pitch shift),
// which is what turns a harmonic sound inharmonic/metallic/bell-like as the
// shift amount increases. Implemented via an STFT analytic-signal approach:
// each frame's spectrum is folded into its positive-frequency-only analytic
// form (Hilbert transform via FFT), the resulting complex time-domain
// signal is multiplied by a continuously-phased complex exponential at the
// shift frequency, and the real part is taken as output, overlap-added like
// a phase vocoder.
//
// Ring Mod mode is the classic simple case: multiply the input directly by
// a sine oscillator at the same frequency control, producing sum/difference
// sidebands - cheaper and glitchier/more percussive than Shift mode, no
// latency.
//
// Mono - EffectsChain owns one instance per channel (so Spread can detune
// L/R independently for a wide, phasey stereo image).
class FreqShifter
{
public:
    enum class Mode { Shift, RingMod };

    FreqShifter() = default;

    void prepare(double sampleRateIn, int windowSizeSamples = 2048);
    void reset();

    void setMode(Mode m) { mode.store(m); }
    void setShiftHz(float hz) { shiftHz.store(juce::jlimit(-4000.0f, 4000.0f, hz)); }

    // In-place is not possible in Shift mode (output lags input by the
    // window's latency); in and out may not alias. Ring Mod mode has no
    // latency but uses the same signature for a uniform call site.
    void process(const float* in, float* out, int numSamples);

private:
    void processHop();

    double sampleRate = 44100.0;
    std::atomic<Mode> mode { Mode::Shift };
    std::atomic<float> shiftHz { 0.0f };

    // Ring mod: simple continuous phase accumulator, no latency.
    double ringPhase = 0.0;

    // Shift mode: STFT with a full complex FFT (needed to build the
    // analytic/positive-frequencies-only spectrum), 4x overlap.
    int windowSize = 2048;
    int hopSize = 512;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hannWindow;
    float normalisation = 1.0f;

    std::vector<float> inputRing;
    int inputWritePos = 0;
    int samplesSinceHop = 0;

    std::vector<juce::dsp::Complex<float>> timeDomain, freqDomain;
    std::vector<float> accum;
    std::deque<float> outputFifo;

    // Continuous modulation phase, advanced by hopSize*delta once per hop
    // (delta = 2*pi*shiftHz/sampleRate at that moment) so overlapping
    // frames apply consistent modulation to the same underlying sample
    // regardless of which frame is contributing to it - see FreqShifter.cpp
    // for the derivation. This is what keeps the shift click-free even
    // while the Hz knob is being dragged.
    double shiftPhaseAccum = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreqShifter)
};
