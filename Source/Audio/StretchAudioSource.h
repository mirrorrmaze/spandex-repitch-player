#pragma once

#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>
#include "Paulstretch.h"

// Wraps an in-memory decoded track and streams it as a PositionableAudioSource,
// with live pitch (semitones) and speed (%) control.
//
// Two rendering paths:
//  - Linked ("Re-Pitch"): bypasses Rubber Band entirely and reads the source
//    at a resampled rate, so pitch and speed move together - classic
//    turntable/vinyl behaviour.
//  - Unlinked (Warp modes): feeds a Rubber Band RubberBandStretcher in
//    real-time streaming mode, with pitch scale and time ratio set
//    independently. The Options preset (transient detector / window /
//    formant handling) is chosen per warp mode to approximate Ableton-style
//    Beats / Tones / Texture / Complex behaviour.
class StretchAudioSource : public juce::PositionableAudioSource
{
public:
    enum class WarpMode
    {
        Beats,
        Tones,
        Texture,
        Complex,
        ComplexPro,

        // Real Paulstretch-style extreme time-stretch (FFT phase
        // randomisation). Time only - the pitch fader has no effect while
        // this mode is active. Meant for very slow Speed% values where
        // Rubber Band's phase vocoder starts to smear/robotise.
        Paulstretch
    };

    StretchAudioSource();
    ~StretchAudioSource() override;

    // Replaces the loaded track. Safe to call while the audio device is running.
    void setBuffer(juce::AudioBuffer<float> newBuffer, double newSourceSampleRate);
    void clear();
    bool hasAudio() const noexcept { return sourceBuffer.getNumSamples() > 0; }
    double getSourceSampleRate() const noexcept { return sourceSampleRate; }
    double getLengthSeconds() const noexcept;

    // Snapshot of the original (unprocessed) decoded track, for ExportEngine.
    juce::AudioBuffer<float> copySourceBuffer() const
    {
        const juce::ScopedLock sl(lock);
        return sourceBuffer;
    }

    void play() { playing.store(true); }
    void pause() { playing.store(false); }
    void stopAndReset()
    {
        playing.store(false);
        setNextReadPosition(0);
    }
    bool isPlaying() const noexcept { return playing.load(); }

    // Pitch shift in semitones, e.g. -12..+12 (UI may allow a wider range).
    void setPitchSemitones(double semitones);
    double getPitchSemitones() const noexcept { return pitchSemitones.load(); }

    // Playback speed as a percentage, 100 = normal.
    void setSpeedPercent(double percent);
    double getSpeedPercent() const noexcept { return speedPercent.load(); }

    // true = Re-Pitch (speed and pitch move together, Rubber Band bypassed).
    // false = independent pitch/time via the selected warp mode.
    void setLinked(bool shouldLink);
    bool isLinked() const noexcept { return linked.load(); }

    void setWarpMode(WarpMode mode);
    WarpMode getWarpMode() const noexcept { return currentWarpMode.load(); }

    // Shared with ExportEngine, which builds its own (offline-mode) stretcher
    // using the same option presets so exported audio matches what was heard.
    static RubberBand::RubberBandStretcher::Options optionsForWarpMode(WarpMode mode);

    // juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double deviceSampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // juce::PositionableAudioSource
    void setNextReadPosition(juce::int64 newPosition) override;
    juce::int64 getNextReadPosition() const override;
    juce::int64 getTotalLength() const override;
    bool isLooping() const override { return looping.load(); }
    void setLooping(bool shouldLoop) override { looping.store(shouldLoop); }

    // Constrains looping (when isLooping() is true) to [startSample, endSample)
    // instead of the whole track. Pass endSample <= startSample to disable and
    // fall back to whole-track looping.
    void setLoopRegion(juce::int64 startSample, juce::int64 endSample);
    juce::int64 getLoopStart() const noexcept { return loopStart.load(); }
    juce::int64 getLoopEnd() const noexcept { return loopEnd.load(); }
    bool hasLoopRegion() const noexcept { return loopEnd.load() > loopStart.load(); }

    // Length of the fade applied approaching/leaving a loop boundary, to
    // turn the hard sample discontinuity at the wrap into an inaudible
    // dip instead of a click. 0 disables it (instant cut, old behaviour).
    void setLoopCrossfadeSeconds(double seconds) { loopCrossfadeSeconds.store(juce::jlimit(0.0, 1.0, seconds)); }
    double getLoopCrossfadeSeconds() const noexcept { return loopCrossfadeSeconds.load(); }

private:
    void rebuildStretcher();
    void renderRePitch(const juce::AudioSourceChannelInfo& bufferToFill);
    void renderWarped(const juce::AudioSourceChannelInfo& bufferToFill);
    void renderPaulstretch(const juce::AudioSourceChannelInfo& bufferToFill);

    juce::CriticalSection lock;
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSampleRate = 44100.0;

    std::atomic<juce::int64> readPosition { 0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> looping { false };
    std::atomic<bool> seekPending { false };
    std::atomic<juce::int64> loopStart { 0 };
    std::atomic<juce::int64> loopEnd { -1 };

    juce::int64 effectiveLoopStart() const noexcept { return hasLoopRegion() ? loopStart.load() : 0; }
    juce::int64 effectiveLoopEnd() const noexcept
    {
        return hasLoopRegion() ? loopEnd.load() : (juce::int64) sourceBuffer.getNumSamples();
    }

    std::atomic<double> loopCrossfadeSeconds { 0.02 };

    // Gain envelope for the loop crossfade: ramps 0->1 over the first
    // loopCrossfadeSeconds after regionStart, 1->0 over the last
    // loopCrossfadeSeconds before regionEnd, 1 elsewhere. pos/regionStart/
    // regionEnd are all in source-domain samples. Applied in all three
    // render paths, though only renderRePitch can apply it exactly
    // sample-by-sample - the warped/Paulstretch paths approximate it at
    // chunk/block granularity since their output doesn't map 1:1 to a
    // source sample position the way direct playback does; still enough
    // to turn the loop-point discontinuity into a brief dip instead of a
    // hard click.
    float loopFadeGain(double pos, double regionStart, double regionEnd) const
    {
        const double xfade = loopCrossfadeSeconds.load() * sourceSampleRate;
        if (xfade <= 0.0)
            return 1.0f;
        if (pos < regionStart + xfade)
            return (float) juce::jlimit(0.0, 1.0, (pos - regionStart) / xfade);
        if (pos > regionEnd - xfade)
            return (float) juce::jlimit(0.0, 1.0, (regionEnd - pos) / xfade);
        return 1.0f;
    }

    std::atomic<double> pitchSemitones { 0.0 };
    std::atomic<double> speedPercent { 100.0 };
    std::atomic<bool> linked { true };
    std::atomic<WarpMode> currentWarpMode { WarpMode::Complex };

    // In linked (Re-Pitch) mode there's only one real control - setting
    // either pitch or speed keeps the other's stored value consistent, so
    // ExportEngine (which reads both independently) always agrees with what
    // was heard, and MainComponent can mirror the change onto the other slider.
    static constexpr double minSpeedPercent = 0.5;

    // Re-Pitch path state (fractional source-domain read cursor).
    double linkedCursor = 0.0;

    // Warp-mode path state.
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    juce::AudioBuffer<float> scratchInput, scratchOutput;
    bool finalSent = false;
    static constexpr int scratchCapacity = 8192;

    // Paulstretch path state.
    Paulstretch paulstretch;
    double paulstretchCursor = 0.0;
    juce::AudioBuffer<float> paulstretchScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StretchAudioSource)
};
