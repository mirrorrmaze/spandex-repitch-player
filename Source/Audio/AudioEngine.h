#pragma once

#include <JuceHeader.h>
#include "StretchAudioSource.h"
#include "EffectsChain.h"

// Owns the processing graph (stretch source -> effects -> resampling) and
// the waveform thumbnail data source. UI components talk to this instead of
// touching JUCE audio plumbing directly.
//
// Runs as a plugin (VST3) or Standalone via juce_add_plugin, so audio I/O
// comes from the host/wrapper calling prepareToPlay()/processBlock(), not
// from a self-owned AudioDeviceManager - this class exposes the graph as a
// plain prepareToPlay()/renderNextBlock() pair for SpandexAudioProcessor
// to drive.
class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void prepareToPlay(double sampleRate, int samplesPerBlockExpected);
    void releaseResources();
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int numSamples);

    // Returns true on success. Errors (unreadable format, missing file) are
    // reported via errorMessage.
    bool loadFile(const juce::File& file, juce::String& errorMessage);

    void play();
    void pause();
    void stop();
    bool isPlaying() const { return stretchSource.isPlaying(); }

    void setPositionSeconds(double seconds);
    double getPositionSeconds() const;
    double getLengthSeconds() const { return stretchSource.getLengthSeconds(); }
    bool hasTrackLoaded() const { return stretchSource.hasAudio(); }

    juce::AudioThumbnail& getThumbnail() { return thumbnail; }

    void setPitchSemitones(double semitones) { stretchSource.setPitchSemitones(semitones); }
    double getPitchSemitones() const { return stretchSource.getPitchSemitones(); }

    void setSpeedPercent(double percent) { stretchSource.setSpeedPercent(percent); }
    double getSpeedPercent() const { return stretchSource.getSpeedPercent(); }

    void setLinked(bool shouldLink) { stretchSource.setLinked(shouldLink); }
    bool isLinked() const { return stretchSource.isLinked(); }

    void setWarpMode(StretchAudioSource::WarpMode mode) { stretchSource.setWarpMode(mode); }
    StretchAudioSource::WarpMode getWarpMode() const { return stretchSource.getWarpMode(); }

    juce::AudioBuffer<float> copySourceBuffer() const { return stretchSource.copySourceBuffer(); }
    double getSourceSampleRate() const { return stretchSource.getSourceSampleRate(); }

    void setLooping(bool shouldLoop) { stretchSource.setLooping(shouldLoop); }
    bool isLooping() const { return stretchSource.isLooping(); }

    void setLoopMode(StretchAudioSource::LoopMode mode) { stretchSource.setLoopMode(mode); }
    StretchAudioSource::LoopMode getLoopMode() const { return stretchSource.getLoopMode(); }

    void setLoopInSeconds(double seconds)
    {
        stretchSource.setLoopRegion((juce::int64) (seconds * sourceSampleRateAtLoad), stretchSource.getLoopEnd());
    }
    void setLoopOutSeconds(double seconds)
    {
        stretchSource.setLoopRegion(stretchSource.getLoopStart(), (juce::int64) (seconds * sourceSampleRateAtLoad));
    }
    double getLoopInSeconds() const
    {
        return sourceSampleRateAtLoad > 0.0 ? (double) stretchSource.getLoopStart() / sourceSampleRateAtLoad : 0.0;
    }
    double getLoopOutSeconds() const
    {
        return sourceSampleRateAtLoad > 0.0 ? (double) stretchSource.getLoopEnd() / sourceSampleRateAtLoad : -1.0;
    }
    bool hasLoopRegion() const { return stretchSource.hasLoopRegion(); }

    void setLoopCrossfadeMs(float ms) { stretchSource.setLoopCrossfadeSeconds(ms * 0.001); }
    float getLoopCrossfadeMs() const { return (float) (stretchSource.getLoopCrossfadeSeconds() * 1000.0); }

    EffectsChain& getEffectsChain() { return effectsChain; }

private:
    void updateResamplingRatio();

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 4 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    StretchAudioSource stretchSource;
    EffectsChain effectsChain { stretchSource };
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

    double sourceSampleRateAtLoad = 44100.0;
    double hostSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
