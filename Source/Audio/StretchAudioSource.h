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
    // Engaging the loop always (re)starts its forward pass, even in
    // PingPong/Reverse mode - matches how samplers start a fresh loop
    // cycle from the top rather than resuming mid-bounce from last time.
    void setLooping(bool shouldLoop) override
    {
        looping.store(shouldLoop);
        if (shouldLoop)
            loopGoingForward.store(true);
    }

    // Constrains looping (when isLooping() is true) to [startSample, endSample)
    // instead of the whole track. Pass endSample <= startSample to disable and
    // fall back to whole-track looping.
    void setLoopRegion(juce::int64 startSample, juce::int64 endSample);
    juce::int64 getLoopStart() const noexcept { return loopStart.load(); }
    juce::int64 getLoopEnd() const noexcept { return loopEnd.load(); }
    bool hasLoopRegion() const noexcept { return loopEnd.load() > loopStart.load(); }

    // Ableton Sampler-style loop types (its Sustain Loop Mode: a plain
    // repeat, "Back and Forth", or a reverse loop). Forward is the classic
    // wrap-to-start repeat; PingPong alternates direction at each boundary
    // instead of wrapping, so it never needs a crossfade (the waveform is
    // already continuous at a direction flip - reading forward through a
    // point then backward from it has no discontinuity); Reverse plays the
    // region backward, wrapping end-to-start each cycle the same way
    // Forward wraps start-to-end.
    enum class LoopMode { Forward, PingPong, Reverse };
    void setLoopMode(LoopMode mode) { loopMode.store(mode); loopGoingForward.store(mode != LoopMode::Reverse); }
    LoopMode getLoopMode() const noexcept { return loopMode.load(); }

    // Length of the crossfade blended in at a Forward/Reverse loop wrap, to
    // turn the hard sample discontinuity into a seamless splice instead of
    // a click. 0 disables it (instant cut). Unused in PingPong mode, which
    // has no wrap to smooth over.
    void setLoopCrossfadeSeconds(double seconds) { loopCrossfadeSeconds.store(juce::jlimit(0.0, 1.0, seconds)); }
    double getLoopCrossfadeSeconds() const noexcept { return loopCrossfadeSeconds.load(); }

private:
    void rebuildStretcher();
    void renderRePitch(const juce::AudioSourceChannelInfo& bufferToFill);
    void renderWarped(const juce::AudioSourceChannelInfo& bufferToFill);
    void renderPaulstretch(const juce::AudioSourceChannelInfo& bufferToFill);

    // Fills `count` samples of `dest` from the source buffer for the Warp
    // path's continuous Forward/Reverse looping: wraps `pos` across the
    // loop boundary (rather than clamping/stopping there) and blends the
    // standard loop crossfade in as it crosses, so the stream fed to Rubber
    // Band never has a discontinuity and the stretcher never needs to reset
    // at a loop repeat.
    void fillWarpChunkWrapping(juce::AudioBuffer<float>& dest, int count, juce::int64& pos,
                                bool forward, juce::int64 regionStart, juce::int64 regionEnd);
    // Fills `count` samples of `dest` from a fixed starting position with no
    // wrapping or blending - used for one-shot (non-looping) playback, and
    // for PingPong mode's per-direction run up to the boundary where it
    // flips (a direction flip has no discontinuity to crossfade).
    void fillWarpChunkClamped(juce::AudioBuffer<float>& dest, int count, juce::int64 pos, bool forward);

    juce::CriticalSection lock;
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSampleRate = 44100.0;

    std::atomic<juce::int64> readPosition { 0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> looping { false };
    std::atomic<bool> seekPending { false };
    std::atomic<juce::int64> loopStart { 0 };
    std::atomic<juce::int64> loopEnd { -1 };

    std::atomic<LoopMode> loopMode { LoopMode::Forward };
    // Current playback direction - only meaningful (and mutated mid-stream)
    // in PingPong mode; Forward/Reverse hold it fixed. Shared across render
    // paths so switching Link/Warp Mode mid-loop keeps a consistent sense
    // of direction rather than resetting to Forward each time.
    std::atomic<bool> loopGoingForward { true };

    juce::int64 effectiveLoopStart() const noexcept { return hasLoopRegion() ? loopStart.load() : 0; }
    juce::int64 effectiveLoopEnd() const noexcept
    {
        return hasLoopRegion() ? loopEnd.load() : (juce::int64) sourceBuffer.getNumSamples();
    }

    std::atomic<double> loopCrossfadeSeconds { 0.02 };

    // Direction-agnostic linear-interpolated read from the raw source
    // buffer; silence outside [0, sourceBuffer.getNumSamples()).
    float readSourceSample(int channel, double pos) const
    {
        const int total = sourceBuffer.getNumSamples();
        if (pos < 0.0 || pos >= (double) total)
            return 0.0f;
        const int idx0 = (int) pos;
        const float frac = (float) (pos - (double) idx0);
        const float s0 = sourceBuffer.getSample(channel, idx0);
        const float s1 = sourceBuffer.getSample(channel, juce::jmin(idx0 + 1, total - 1));
        return s0 + frac * (s1 - s0);
    }

    // True crossfade at a Forward/Reverse loop wrap: "crossfades the loop
    // with itself" the way sampler loop crossfades typically work - blends
    // the loop's own boundary content with material from just outside the
    // loop region (the "postroll" just past regionEnd for a Forward wrap,
    // the "preroll" just before regionStart for a Reverse wrap), so the
    // splice is seamless instead of the old approach of ducking the volume
    // to silence and back at every repeat. Falls back to a plain read
    // outside the crossfade zone, or if there isn't enough pre/post-roll
    // material past the region's edge to draw from (e.g. the loop end sits
    // at the very end of the file). PingPong never calls this - a direction
    // flip has no discontinuity to smooth over in the first place.
    float loopCrossfadeSample(int channel, double pos, double regionStart, double regionEnd, bool forwardWrap) const
    {
        const int total = sourceBuffer.getNumSamples();
        double xfade = juce::jmin(loopCrossfadeSeconds.load() * sourceSampleRate, (regionEnd - regionStart) * 0.5);

        if (forwardWrap)
        {
            xfade = juce::jmin(xfade, (double) total - regionEnd);
            if (xfade <= 0.0 || pos < regionStart || pos >= regionStart + xfade)
                return readSourceSample(channel, pos);
            const double t = (pos - regionStart) / xfade;
            const float head = readSourceSample(channel, pos);
            const float postroll = readSourceSample(channel, regionEnd + (pos - regionStart));
            return head * (float) std::sin(t * juce::MathConstants<double>::halfPi)
                 + postroll * (float) std::cos(t * juce::MathConstants<double>::halfPi);
        }
        else
        {
            xfade = juce::jmin(xfade, regionStart);
            if (xfade <= 0.0 || pos > regionEnd || pos <= regionEnd - xfade)
                return readSourceSample(channel, pos);
            const double t = (regionEnd - pos) / xfade;
            const float head = readSourceSample(channel, pos);
            const float preroll = readSourceSample(channel, regionStart - (regionEnd - pos));
            return head * (float) std::sin(t * juce::MathConstants<double>::halfPi)
                 + preroll * (float) std::cos(t * juce::MathConstants<double>::halfPi);
        }
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
