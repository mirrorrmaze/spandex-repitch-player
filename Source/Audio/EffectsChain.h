#pragma once

#include <JuceHeader.h>
#include <array>
#include "SmudgeProcessor.h"
#include "GranularDelay.h"
#include "FreqShifter.h"
#include "LossyProcessor.h"

// Sits between StretchAudioSource and the device output: an 8-band
// parametric EQ (Ableton EQ Eight / FabFilter Pro-Q style band types), a
// Smudge spectral freeze/smear, a granular delay, and a reverb - plus a
// live spectrum analyzer tap the EQ view polls to draw the signal behind
// the response curve.
class EffectsChain : public juce::AudioSource
{
public:
    enum class FilterType
    {
        HighPass,
        LowShelf,
        Bell,
        Notch,
        BandPass,
        HighShelf,
        LowPass
    };

    static constexpr int numEqBands = 8;
    static constexpr int spectrumSize = 1024; // fftSize / 2

    struct EqBandState
    {
        std::atomic<bool> enabled { false };
        std::atomic<FilterType> type { FilterType::Bell };
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> q { 0.707f };
    };

    explicit EffectsChain(juce::AudioSource& inputSource);
    ~EffectsChain() override;

    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // EQ
    const EqBandState& getEqBand(int index) const { return bands[(size_t) index].state; }
    void setEqBandEnabled(int index, bool enabled);
    void setEqBandType(int index, FilterType type);
    void setEqBandFrequency(int index, float hz);
    void setEqBandGainDb(int index, float db);
    void setEqBandQ(int index, float q);

    // Combined magnitude response of all enabled bands, in dB, at frequencyHz.
    // For drawing the EQ curve.
    float getMagnitudeResponseDb(float frequencyHz) const;

    // Smudge (spectral freeze/smear, FabFilter Saturn 2 style)
    void setSmudgeEnabled(bool e) { smudgeEnabled.store(e); }
    bool isSmudgeEnabled() const { return smudgeEnabled.load(); }
    void setSmudgeAmount(float amount01);
    float getSmudgeAmount() const { return smudgeAmount.load(); }
    // STFT window size expressed as milliseconds - shorter is a faster/more
    // responsive smear, longer is slower/hazier and holds content longer.
    void setSmudgeRateMs(float ms);
    float getSmudgeRateMs() const { return smudgeRateMs.load(); }
    void setSmudgeFeedback(float feedback01);
    float getSmudgeFeedback() const { return smudgeFeedback.load(); }

    // Lossy (bitcrusher/downsampler lo-fi degradation)
    void setLossyEnabled(bool e) { lossyEnabled.store(e); }
    bool isLossyEnabled() const { return lossyEnabled.load(); }
    void setLossyBits(float bitsIn) { lossyBits.store(juce::jlimit(1.0f, 16.0f, bitsIn)); lossyL.setBits(bitsIn); lossyR.setBits(bitsIn); }
    float getLossyBits() const { return lossyBits.load(); }
    void setLossyRateHz(float hz) { lossyRateHz.store(juce::jlimit(200.0f, 48000.0f, hz)); lossyL.setRateHz(hz); lossyR.setRateHz(hz); }
    float getLossyRateHz() const { return lossyRateHz.load(); }
    void setLossyDriveDb(float db) { lossyDriveDb.store(juce::jlimit(0.0f, 24.0f, db)); lossyL.setDriveDb(db); lossyR.setDriveDb(db); }
    float getLossyDriveDb() const { return lossyDriveDb.load(); }
    void setLossyMix(float mix01) { lossyMix.store(juce::jlimit(0.0f, 1.0f, mix01)); lossyL.setMix(mix01); lossyR.setMix(mix01); }
    float getLossyMix() const { return lossyMix.load(); }

    // Granular delay (Ableton "Granular Mirror Maze" style)
    void setDelayEnabled(bool e) { delayEnabled.store(e); }
    bool isDelayEnabled() const { return delayEnabled.load(); }
    void setDelayTimeMs(float ms) { granularDelay.setDelayMs(ms); }
    float getDelayTimeMs() const { return granularDelay.getDelayMs(); }
    void setDelayGrainSizeMs(float ms) { granularDelay.setGrainSizeMs(ms); }
    float getDelayGrainSizeMs() const { return granularDelay.getGrainSizeMs(); }
    void setDelayDensity(float grainsPerSecond) { granularDelay.setDensity(grainsPerSecond); }
    float getDelayDensity() const { return granularDelay.getDensity(); }
    void setDelaySpreadMs(float ms) { granularDelay.setSpreadMs(ms); }
    float getDelaySpreadMs() const { return granularDelay.getSpreadMs(); }
    void setDelayPitchScatterSemitones(float semis) { granularDelay.setPitchScatterSemitones(semis); }
    float getDelayPitchScatterSemitones() const { return granularDelay.getPitchScatterSemitones(); }
    void setDelayFeedback(float fb) { granularDelay.setFeedback(fb); }
    float getDelayFeedback() const { return granularDelay.getFeedback(); }
    void setDelayMix(float mix) { granularDelay.setMix(mix); }
    float getDelayMix() const { return granularDelay.getMix(); }

    // Frequency Shifter / Ring Modulator (Ableton "Shifter" style). Shift
    // mode does a true SSB frequency shift (every partial moves by the same
    // Hz, turning harmonic material inharmonic/metallic as the amount
    // increases); Ring Mod multiplies by an oscillator at the same
    // frequency for a more percussive/robotic sideband effect.
    void setShifterEnabled(bool e) { shifterEnabled.store(e); }
    bool isShifterEnabled() const { return shifterEnabled.load(); }
    void setShifterMode(FreqShifter::Mode m) { shifterMode.store(m); }
    FreqShifter::Mode getShifterMode() const { return shifterMode.load(); }
    void setShifterCoarseHz(float hz) { shifterCoarseHz.store(juce::jlimit(-2000.0f, 2000.0f, hz)); }
    float getShifterCoarseHz() const { return shifterCoarseHz.load(); }
    void setShifterFineHz(float hz) { shifterFineHz.store(juce::jlimit(-50.0f, 50.0f, hz)); }
    float getShifterFineHz() const { return shifterFineHz.load(); }
    // Detunes L/R shift amounts apart for a wide, phasey stereo image.
    void setShifterSpreadHz(float hz) { shifterSpreadHz.store(juce::jlimit(0.0f, 200.0f, hz)); }
    float getShifterSpreadHz() const { return shifterSpreadHz.load(); }
    // Feeds the shifted output back into the input - the classic analog
    // frequency-shifter trick for cascading "barberpole" infinite-riser/
    // falling effects. Clamped short of 1 to stay stable.
    void setShifterFeedback(float fb) { shifterFeedback.store(juce::jlimit(0.0f, 0.95f, fb)); }
    float getShifterFeedback() const { return shifterFeedback.load(); }
    void setShifterMix(float mix) { shifterMix.store(juce::jlimit(0.0f, 1.0f, mix)); }
    float getShifterMix() const { return shifterMix.load(); }

    // Reverb
    void setReverbEnabled(bool e) { reverbEnabled.store(e); }
    bool isReverbEnabled() const { return reverbEnabled.load(); }
    void setReverbParams(const juce::dsp::Reverb::Parameters& p);
    juce::dsp::Reverb::Parameters getReverbParams() const;

    // Live spectrum, in absolute dB (roughly -100..0), one value per bin,
    // log-spaced interpretation left to the caller (bin index -> frequency
    // is linear; UI maps to a log-frequency x-axis itself).
    std::vector<float> getLatestSpectrum() const;
    double getAnalyzerSampleRate() const { return sampleRate; }

    // Fast-attack/slow-release peak level (dB) tracked across the spectrum,
    // so a display can auto-scale to the signal's actual level instead of
    // pinning near the top for loud material or looking flat for quiet
    // material against a fixed dB range.
    float getSpectrumPeakDb() const;

    // Input trim (applied before the EQ/delay/reverb chain) and output trim
    // (applied last, after everything) - for final mix gain-staging.
    void setInputGainDb(float db) { inputGainDb.store(db); }
    float getInputGainDb() const { return inputGainDb.load(); }
    void setOutputGainDb(float db) { outputGainDb.store(db); }
    float getOutputGainDb() const { return outputGainDb.load(); }

    // Drive/Compression bus glue, applied after everything else (Reverb)
    // and before the final output trim. Drive is a tanh waveshaper (0dB =
    // bypassed/clean); Compression is a single-knob feedforward compressor
    // (fixed threshold, ratio and auto-makeup scale with the knob) so it's
    // one control instead of threshold/ratio/attack/release/makeup.
    void setDriveDb(float db) { driveDb.store(juce::jlimit(0.0f, 15.0f, db)); }
    float getDriveDb() const { return driveDb.load(); }
    void setCompAmount(float amount01) { compAmount.store(juce::jlimit(0.0f, 1.0f, amount01)); }
    float getCompAmount() const { return compAmount.load(); }

private:
    void updateEqBandCoefficients(int index);
    void pushToAnalyzer(const float* data, int numSamples);
    void computeAnalyzerFrame();

    juce::AudioSource& input;
    double sampleRate = 44100.0;
    juce::CriticalSection lock;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    struct Band
    {
        EqBandState state;
        juce::dsp::ProcessorDuplicator<Filter, Coeffs> processor;
        // Toggling a band on/off used to hard-gate whether process() ran at
        // all, which is an instant step in the output (a click) - this ramps
        // the wet/dry blend over ~15ms instead so switching is inaudible as
        // a discontinuity.
        juce::SmoothedValue<float> enabledGain { 0.0f };
    };
    std::array<Band, numEqBands> bands;

    std::atomic<bool> smudgeEnabled { false };
    std::atomic<float> smudgeAmount { 0.5f };
    std::atomic<float> smudgeRateMs { 46.4f };
    std::atomic<float> smudgeFeedback { 0.0f };
    SmudgeProcessor smudgeL, smudgeR;
    std::vector<float> smudgeScratch;
    juce::SmoothedValue<float> smudgeEnabledGain { 0.0f };

    std::atomic<bool> lossyEnabled { false };
    std::atomic<float> lossyBits { 8.0f };
    std::atomic<float> lossyRateHz { 4000.0f };
    std::atomic<float> lossyDriveDb { 0.0f };
    std::atomic<float> lossyMix { 1.0f };
    LossyProcessor lossyL, lossyR;
    std::vector<float> lossyScratch;
    juce::SmoothedValue<float> lossyEnabledGain { 0.0f };

    std::atomic<bool> delayEnabled { false };
    GranularDelay granularDelay;
    juce::SmoothedValue<float> delayEnabledGain { 0.0f };

    std::atomic<bool> shifterEnabled { false };
    std::atomic<FreqShifter::Mode> shifterMode { FreqShifter::Mode::Shift };
    std::atomic<float> shifterCoarseHz { 0.0f };
    std::atomic<float> shifterFineHz { 0.0f };
    std::atomic<float> shifterSpreadHz { 0.0f };
    std::atomic<float> shifterFeedback { 0.0f };
    std::atomic<float> shifterMix { 1.0f };
    FreqShifter shifterL, shifterR;
    float shifterFeedbackStateL = 0.0f, shifterFeedbackStateR = 0.0f;
    juce::SmoothedValue<float> shifterEnabledGain { 0.0f };

    std::atomic<bool> reverbEnabled { false };
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> reverbEnabledGain { 0.0f };

    // Reusable scratch for capturing a "dry" snapshot of the block before an
    // always-on stage (Smudge/Delay/Reverb) processes it in place, so the
    // enabled-gain ramp can blend between the two rather than hard-switching.
    juce::AudioBuffer<float> dryScratch;

    static constexpr int analyzerFftOrder = 11; // 2048-point FFT -> 1024 bins
    static constexpr int analyzerFftSize = 1 << analyzerFftOrder;
    juce::dsp::FFT analyzerFft { analyzerFftOrder };
    std::vector<float> analyzerHannWindow;
    std::vector<float> analyzerCaptureBuffer;
    int analyzerCaptureIndex = 0;
    std::vector<float> analyzerFftScratch;
    std::vector<float> latestSpectrumDb;
    float spectrumPeakDb = -100.0f;
    float analyzerMagnitudeNormalisation = 1.0f;
    mutable juce::CriticalSection analyzerLock;

    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };

    std::atomic<float> driveDb { 0.0f };
    std::atomic<float> compAmount { 0.0f };
    // Smoothed gain-reduction envelope, in dB (positive = reduction,
    // negative = upward boost) - only touched from the audio thread under
    // `lock`, so a plain float is fine.
    float compEnvelopeDb = 0.0f;
    // Short RMS-style loudness detector (mean-square, not yet sqrt'd) the
    // compressor reads instead of the instantaneous sample - see the bus
    // glue stage in getNextAudioBlock() for why.
    float compRmsEnvelope = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsChain)
};
