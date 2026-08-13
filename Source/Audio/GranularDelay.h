#pragma once

#include <JuceHeader.h>
#include <array>

// A granular delay in the spirit of Ableton's "Granular Mirror Maze" M4L
// device: instead of one clean tap, a stream of short overlapping grains is
// read from a delay history buffer around a target position (delay time +
// random spray), each with its own randomised pitch (scatter) and pan,
// windowed with a Hann envelope so grains fade in/out rather than click.
// Feeds granulated output back into the buffer so echoes re-granulate on
// later passes, building up a diffuse, textured "maze" rather than discrete
// repeats.
class GranularDelay
{
public:
    GranularDelay() = default;

    void prepare(double sampleRate, int maxDelaySeconds = 4);
    void reset();

    void setDelayMs(float ms) { delayMs.store(juce::jlimit(1.0f, 4000.0f, ms)); }
    float getDelayMs() const { return delayMs.load(); }

    void setGrainSizeMs(float ms) { grainSizeMs.store(juce::jlimit(5.0f, 500.0f, ms)); }
    float getGrainSizeMs() const { return grainSizeMs.load(); }

    // Grains spawned per second.
    void setDensity(float grainsPerSecond) { density.store(juce::jlimit(0.5f, 60.0f, grainsPerSecond)); }
    float getDensity() const { return density.load(); }

    // Random jitter applied to each grain's read position, +/- this many ms.
    void setSpreadMs(float ms) { spreadMs.store(juce::jlimit(0.0f, 500.0f, ms)); }
    float getSpreadMs() const { return spreadMs.load(); }

    // Random pitch offset applied per grain, +/- this many semitones.
    void setPitchScatterSemitones(float semitones) { pitchScatter.store(juce::jlimit(0.0f, 24.0f, semitones)); }
    float getPitchScatterSemitones() const { return pitchScatter.load(); }

    void setFeedback(float fb) { feedback.store(juce::jlimit(0.0f, 0.95f, fb)); }
    float getFeedback() const { return feedback.load(); }

    void setMix(float mix) { wetMix.store(juce::jlimit(0.0f, 1.0f, mix)); }
    float getMix() const { return wetMix.load(); }

    // Processes in place, stereo (numChannels 1 or 2).
    void process(float* const* channelData, int numChannels, int numSamples);

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;
        double pitchRatio = 1.0;
        int age = 0;
        int length = 1;
        float panL = 1.0f, panR = 1.0f;
    };

    void spawnGrain();
    static float readInterpolated(const std::vector<float>& buf, double position);

    double sampleRate = 44100.0;
    std::vector<float> bufferL, bufferR;
    int bufferSize = 0;
    int writePos = 0;

    // Output-stage safety limiter: the feedback-tap saturation (see
    // process()) keeps the internal delay buffer from compounding, but a
    // live user report showed the *direct* wet output could still spike
    // hard enough to clip/"blow up" when density and feedback were both
    // thrown to extremes at once - grain clustering can momentarily stack
    // far more overlap than grainNormFactor's average-case scaling expects.
    // A simple feedforward peak limiter (fast attack, slower release) on
    // the final combined output catches that regardless of which knob
    // combination caused it, rather than relying on every internal gain
    // stage to individually stay conservative enough.
    float limiterEnvelope = 0.0f;

    static constexpr int maxGrains = 32;
    std::array<Grain, maxGrains> grains;
    int samplesUntilNextGrain = 0;
    juce::Random random;

    std::atomic<float> delayMs { 300.0f };
    std::atomic<float> grainSizeMs { 80.0f };
    std::atomic<float> density { 8.0f };
    std::atomic<float> spreadMs { 30.0f };
    std::atomic<float> pitchScatter { 3.0f };
    std::atomic<float> feedback { 0.3f };
    std::atomic<float> wetMix { 0.35f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularDelay)
};
