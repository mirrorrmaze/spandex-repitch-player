#include "StretchAudioSource.h"

RubberBand::RubberBandStretcher::Options StretchAudioSource::optionsForWarpMode(WarpMode mode)
{
    using RB = RubberBand::RubberBandStretcher;

    switch (mode)
    {
        case WarpMode::Beats:
            return RB::OptionEngineFiner | RB::OptionDetectorPercussive
                 | RB::OptionTransientsCrisp | RB::OptionWindowShort;

        case WarpMode::Tones:
            return RB::OptionEngineFiner | RB::OptionDetectorCompound
                 | RB::OptionFormantPreserved | RB::OptionWindowStandard;

        case WarpMode::Texture:
            return RB::OptionEngineFiner | RB::OptionDetectorSoft
                 | RB::OptionTransientsSmooth | RB::OptionWindowLong;

        case WarpMode::Complex:
            return RB::OptionEngineFiner | RB::OptionDetectorCompound
                 | RB::OptionTransientsMixed | RB::OptionWindowStandard;

        // Highest-quality preset, tuned like Ableton's "Complex Pro": adds
        // pitch consistency across changing ratios and coherent stereo
        // handling on top of Complex, at greater CPU cost.
        case WarpMode::ComplexPro:
            return RB::OptionEngineFiner | RB::OptionDetectorCompound
                 | RB::OptionTransientsMixed | RB::OptionWindowStandard
                 | RB::OptionPitchHighConsistency | RB::OptionChannelsTogether;

        // Paulstretch doesn't use Rubber Band at all (see renderPaulstretch);
        // this case only exists so the switch is complete.
        case WarpMode::Paulstretch:
        default:
            return RB::OptionEngineFiner | RB::OptionDetectorCompound
                 | RB::OptionTransientsMixed | RB::OptionWindowStandard;
    }
}

StretchAudioSource::StretchAudioSource() = default;
StretchAudioSource::~StretchAudioSource() = default;

void StretchAudioSource::setBuffer(juce::AudioBuffer<float> newBuffer, double newSourceSampleRate)
{
    const juce::ScopedLock sl(lock);
    playing.store(false);
    readPosition.store(0);
    linkedCursor = 0.0;
    loopGoingForward.store(true);
    sourceBuffer = std::move(newBuffer);
    sourceSampleRate = newSourceSampleRate > 0.0 ? newSourceSampleRate : 44100.0;
    rebuildStretcher();
}

void StretchAudioSource::clear()
{
    const juce::ScopedLock sl(lock);
    playing.store(false);
    readPosition.store(0);
    linkedCursor = 0.0;
    sourceBuffer.setSize(0, 0);
    stretcher.reset();
}

double StretchAudioSource::getLengthSeconds() const noexcept
{
    if (sourceSampleRate <= 0.0)
        return 0.0;
    return (double) sourceBuffer.getNumSamples() / sourceSampleRate;
}

void StretchAudioSource::setPitchSemitones(double semitones)
{
    pitchSemitones.store(semitones);
    if (linked.load())
        speedPercent.store(std::pow(2.0, semitones / 12.0) * 100.0);

    const juce::ScopedLock sl(lock);
    if (stretcher != nullptr)
        stretcher->setPitchScale(std::pow(2.0, semitones / 12.0));
}

void StretchAudioSource::setSpeedPercent(double percent)
{
    percent = juce::jmax(minSpeedPercent, percent);
    speedPercent.store(percent);
    if (linked.load())
        pitchSemitones.store(12.0 * std::log2(percent / 100.0));

    const juce::ScopedLock sl(lock);
    if (stretcher != nullptr)
        stretcher->setTimeRatio(100.0 / percent);
}

void StretchAudioSource::setLinked(bool shouldLink)
{
    const juce::ScopedLock sl(lock);
    if (shouldLink)
        speedPercent.store(std::pow(2.0, pitchSemitones.load() / 12.0) * 100.0);
    if (linked.exchange(shouldLink) == shouldLink)
        return;
    rebuildStretcher();
}

void StretchAudioSource::setWarpMode(WarpMode mode)
{
    const juce::ScopedLock sl(lock);
    if (currentWarpMode.exchange(mode) == mode)
        return;
    if (!linked.load())
        rebuildStretcher();
}

void StretchAudioSource::rebuildStretcher()
{
    // Caller holds `lock`.
    finalSent = false;
    stretcher.reset();

    if (linked.load() || sourceSampleRate <= 0.0 || sourceBuffer.getNumSamples() == 0)
        return;

    if (currentWarpMode.load() == WarpMode::Paulstretch)
    {
        paulstretch.prepare();
        paulstretch.reset();
        paulstretchCursor = (double) readPosition.load();
        paulstretchScratch.setSize(2, scratchCapacity, false, false, true);
        return;
    }

    const auto options = optionsForWarpMode(currentWarpMode.load()) | RubberBand::RubberBandStretcher::OptionProcessRealTime;

    stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        (size_t) sourceSampleRate,
        2,
        options,
        100.0 / speedPercent.load(),
        std::pow(2.0, pitchSemitones.load() / 12.0));

    scratchInput.setSize(2, scratchCapacity, false, false, true);
    scratchOutput.setSize(2, scratchCapacity, false, false, true);
}

void StretchAudioSource::prepareToPlay(int, double) {}

void StretchAudioSource::releaseResources() {}

void StretchAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    const juce::ScopedLock sl(lock);

    bufferToFill.clearActiveBufferRegion();

    if (!playing.load() || sourceBuffer.getNumSamples() == 0)
        return;

    if (seekPending.exchange(false))
    {
        linkedCursor = (double) readPosition.load();
        if (stretcher != nullptr)
            stretcher->reset();
        paulstretch.reset();
        paulstretchCursor = (double) readPosition.load();
        finalSent = false;
    }

    if (linked.load())
        renderRePitch(bufferToFill);
    else if (currentWarpMode.load() == WarpMode::Paulstretch)
        renderPaulstretch(bufferToFill);
    else
        renderWarped(bufferToFill);
}

void StretchAudioSource::renderRePitch(const juce::AudioSourceChannelInfo& bufferToFill)
{
    const double rateMag = std::pow(2.0, pitchSemitones.load() / 12.0);
    const int totalSamples = sourceBuffer.getNumSamples();
    const int numOutCh = bufferToFill.buffer->getNumChannels();
    const int numSrcCh = sourceBuffer.getNumChannels();

    const bool isLoopingNow = looping.load();
    const auto mode = loopMode.load();
    const double endBoundary = isLoopingNow
        ? (double) juce::jmin((juce::int64) (totalSamples - 1), effectiveLoopEnd())
        : (double) (totalSamples - 1);
    const double startBoundary = isLoopingNow ? (double) effectiveLoopStart() : 0.0;
    const double loopLen = endBoundary - startBoundary;

    // Loop mode only governs direction while actually looping - if Loop is
    // off, single-shot playback is always plain forward regardless of what
    // mode was last selected.
    bool goingForward = true;
    if (isLoopingNow)
    {
        if (mode == LoopMode::Reverse)
            goingForward = false;
        else if (mode == LoopMode::PingPong)
            goingForward = loopGoingForward.load();
    }

    int i = 0;
    for (; i < bufferToFill.numSamples; ++i)
    {
        if (isLoopingNow && loopLen > 0.0)
        {
            // A guarded loop rather than a single if: at extreme pitch
            // shifts the per-sample step can exceed the loop length, so a
            // single reflection/wrap might still leave the cursor out of
            // bounds. Capped well short of infinite in case loopLen is
            // pathologically tiny.
            for (int guard = 0; guard < 8; ++guard)
            {
                if (goingForward && linkedCursor >= endBoundary)
                {
                    if (mode == LoopMode::PingPong)
                    {
                        linkedCursor = 2.0 * endBoundary - linkedCursor;
                        goingForward = false;
                    }
                    else
                    {
                        linkedCursor -= loopLen;
                    }
                }
                else if (!goingForward && linkedCursor < startBoundary)
                {
                    if (mode == LoopMode::PingPong)
                    {
                        linkedCursor = 2.0 * startBoundary - linkedCursor;
                        goingForward = true;
                    }
                    else
                    {
                        linkedCursor += loopLen;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else if (linkedCursor < 0.0 || linkedCursor >= endBoundary)
        {
            playing.store(false);
            break;
        }

        for (int ch = 0; ch < numOutCh; ++ch)
        {
            const int srcCh = juce::jmin(ch, numSrcCh - 1);
            const float sample = (isLoopingNow && mode != LoopMode::PingPong)
                ? loopCrossfadeSample(srcCh, linkedCursor, startBoundary, endBoundary, mode == LoopMode::Forward)
                : readSourceSample(srcCh, linkedCursor);
            bufferToFill.buffer->setSample(ch, bufferToFill.startSample + i, sample);
        }

        linkedCursor += goingForward ? rateMag : -rateMag;
    }

    if (isLoopingNow && mode == LoopMode::PingPong)
        loopGoingForward.store(goingForward);

    readPosition.store((juce::int64) linkedCursor);
}

void StretchAudioSource::fillWarpChunkWrapping(juce::AudioBuffer<float>& dest, int count, juce::int64& pos,
                                                bool forward, juce::int64 regionStart, juce::int64 regionEnd)
{
    const juce::int64 loopLen = regionEnd - regionStart;
    const int numSrcCh = sourceBuffer.getNumChannels();

    for (int i = 0; i < count; ++i)
    {
        if (forward && pos >= regionEnd)
            pos -= loopLen;
        else if (!forward && pos < regionStart)
            pos += loopLen;

        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = juce::jmin(ch, numSrcCh - 1);
            dest.setSample(ch, i, loopCrossfadeSample(srcCh, (double) pos, (double) regionStart, (double) regionEnd, forward));
        }

        pos += forward ? 1 : -1;
    }
}

void StretchAudioSource::fillWarpChunkClamped(juce::AudioBuffer<float>& dest, int count, juce::int64 pos, bool forward)
{
    const int numSrcCh = sourceBuffer.getNumChannels();

    for (int i = 0; i < count; ++i)
    {
        const juce::int64 srcPos = forward ? (pos + i) : (pos - i);
        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = juce::jmin(ch, numSrcCh - 1);
            dest.setSample(ch, i, readSourceSample(srcCh, (double) srcPos));
        }
    }
}

void StretchAudioSource::renderWarped(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (stretcher == nullptr)
        return;

    const auto trackEnd = (juce::int64) sourceBuffer.getNumSamples();
    const int numOutCh = bufferToFill.buffer->getNumChannels();

    const bool isLoopingNow = looping.load();
    const auto mode = loopMode.load();
    const auto regionEnd = isLoopingNow ? juce::jmin(trackEnd, effectiveLoopEnd()) : trackEnd;
    const auto regionStart = isLoopingNow ? effectiveLoopStart() : (juce::int64) 0;

    // Forward/Reverse loop by continuously feeding the stretcher a wrapped,
    // crossfade-blended stream (fillWarpChunkWrapping) instead of ever
    // sending a "final" flag and resetting at the repeat - so in steady
    // state they never actually reach the boundary-handling branches below
    // (only reachable if the region touches the file's edge and there
    // isn't room for the crossfade's pre/post-roll material). PingPong
    // still runs one direction at a time up to the boundary and resets
    // there, since Rubber Band can't reverse direction mid-stream the way
    // it can absorb a same-direction position wrap.
    const bool continuousLoop = isLoopingNow && mode != LoopMode::PingPong && regionEnd > regionStart;

    bool goingForward = true;
    if (isLoopingNow)
    {
        if (mode == LoopMode::Reverse)
            goingForward = false;
        else if (mode == LoopMode::PingPong)
            goingForward = loopGoingForward.load();
    }

    const auto resumeLooping = [&]()
    {
        stretcher->reset();
        if (mode == LoopMode::PingPong)
        {
            goingForward = !goingForward;
            loopGoingForward.store(goingForward);
        }
        readPosition.store(goingForward ? regionStart : regionEnd - 1);
        finalSent = false;
    };

    int needed = bufferToFill.numSamples;
    int outOffset = bufferToFill.startSample;

    while (needed > 0)
    {
        const int avail = stretcher->available();

        if (avail < 0)
        {
            if (isLoopingNow)
            {
                resumeLooping();
                continue;
            }

            playing.store(false);
            for (int ch = 0; ch < numOutCh; ++ch)
                bufferToFill.buffer->clear(ch, outOffset, needed);
            return;
        }

        if (avail == 0)
        {
            if (finalSent)
            {
                if (isLoopingNow)
                {
                    resumeLooping();
                    continue;
                }

                for (int ch = 0; ch < numOutCh; ++ch)
                    bufferToFill.buffer->clear(ch, outOffset, needed);
                playing.store(false);
                return;
            }

            size_t desired = stretcher->getSamplesRequired();
            if (desired == 0)
                desired = 256;
            desired = juce::jmin(desired, (size_t) scratchCapacity);

            bool isFinal = false;
            size_t provide = desired;

            if (continuousLoop)
            {
                auto pos = readPosition.load();
                fillWarpChunkWrapping(scratchInput, (int) provide, pos, goingForward, regionStart, regionEnd);
                readPosition.store(pos);
            }
            else
            {
                const auto pos = readPosition.load();
                const juce::int64 remaining = goingForward ? (regionEnd - pos) : (pos - regionStart + 1);
                provide = (size_t) juce::jlimit((juce::int64) 0, (juce::int64) desired, remaining);
                fillWarpChunkClamped(scratchInput, (int) provide, pos, goingForward);
                readPosition.store(goingForward ? pos + (juce::int64) provide : pos - (juce::int64) provide);
                isFinal = (juce::int64) provide >= remaining;
            }

            const float* inPtrs[2] = { scratchInput.getReadPointer(0), scratchInput.getReadPointer(1) };
            stretcher->process(inPtrs, provide, isFinal);

            if (isFinal)
                finalSent = true;
        }
        else
        {
            const int toCopy = juce::jmin(avail, needed);
            float* outPtrs[2] = { scratchOutput.getWritePointer(0), scratchOutput.getWritePointer(1) };
            const auto got = (int) stretcher->retrieve(outPtrs, (size_t) toCopy);

            for (int ch = 0; ch < numOutCh; ++ch)
            {
                const int srcCh = juce::jmin(ch, 1);
                bufferToFill.buffer->copyFrom(ch, outOffset, scratchOutput, srcCh, 0, got);
            }

            outOffset += got;
            needed -= got;
        }
    }
}

void StretchAudioSource::renderPaulstretch(const juce::AudioSourceChannelInfo& bufferToFill)
{
    const int totalSamples = sourceBuffer.getNumSamples();
    const int numSrcCh = sourceBuffer.getNumChannels();
    const int numOutCh = bufferToFill.buffer->getNumChannels();
    const int numSamples = bufferToFill.numSamples;

    const bool isLoopingNow = looping.load();
    const auto mode = loopMode.load();
    const auto regionEnd = isLoopingNow ? juce::jmin((juce::int64) totalSamples, effectiveLoopEnd())
                                         : (juce::int64) totalSamples;
    const auto regionStart = isLoopingNow ? effectiveLoopStart() : (juce::int64) 0;

    if (paulstretchScratch.getNumSamples() < numSamples)
        paulstretchScratch.setSize(2, numSamples, false, false, true);

    const float* srcL = sourceBuffer.getReadPointer(0);
    const float* srcR = sourceBuffer.getReadPointer(juce::jmin(1, numSrcCh - 1));
    const double speedMag = speedPercent.load() / 100.0;

    bool goingForward = true;
    if (isLoopingNow)
    {
        if (mode == LoopMode::Reverse)
            goingForward = false;
        else if (mode == LoopMode::PingPong)
            goingForward = loopGoingForward.load();
    }

    // An equal-power duck near the loop boundary, not the true source-domain
    // crossfade the Re-Pitch/Warp paths use: Paulstretch's own analysis
    // window (thousands of samples, far wider than a typical crossfade
    // length) already smears content across a loop point on its own, so
    // pre-blending a narrow region of source audio before it's read
    // wouldn't register as a distinct improvement - a duck is still enough
    // to avoid a hard click.
    float fadeGain = 1.0f;
    if (isLoopingNow)
    {
        const double xfade = loopCrossfadeSeconds.load() * sourceSampleRate;
        if (xfade > 0.0)
        {
            double t = -1.0;
            if (paulstretchCursor < (double) regionStart + xfade)
                t = (paulstretchCursor - (double) regionStart) / xfade;
            else if (paulstretchCursor > (double) regionEnd - xfade)
                t = ((double) regionEnd - paulstretchCursor) / xfade;
            if (t >= 0.0)
                fadeGain = (float) std::sin(juce::jlimit(0.0, 1.0, t) * juce::MathConstants<double>::halfPi);
        }
    }

    paulstretch.process(srcL, srcR, totalSamples, paulstretchCursor, goingForward ? speedMag : -speedMag,
                         paulstretchScratch.getWritePointer(0), paulstretchScratch.getWritePointer(1),
                         numSamples);

    for (int ch = 0; ch < numOutCh; ++ch)
    {
        const int srcCh = juce::jmin(ch, 1);
        bufferToFill.buffer->copyFrom(ch, bufferToFill.startSample, paulstretchScratch, srcCh, 0, numSamples);
    }

    if (fadeGain < 0.999f)
        for (int ch = 0; ch < numOutCh; ++ch)
            juce::FloatVectorOperations::multiply(bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample), fadeGain, numSamples);

    readPosition.store((juce::int64) paulstretchCursor);

    if (isLoopingNow)
    {
        const double loopLen = (double) (regionEnd - regionStart);

        if (goingForward && paulstretchCursor >= (double) regionEnd)
        {
            if (mode == LoopMode::PingPong)
            {
                goingForward = false;
                loopGoingForward.store(false);
                paulstretchCursor = 2.0 * (double) regionEnd - paulstretchCursor;
            }
            else
            {
                paulstretchCursor -= loopLen;
            }
            paulstretch.reset();
            readPosition.store((juce::int64) paulstretchCursor);
        }
        else if (!goingForward && paulstretchCursor < (double) regionStart)
        {
            if (mode == LoopMode::PingPong)
            {
                goingForward = true;
                loopGoingForward.store(true);
                paulstretchCursor = 2.0 * (double) regionStart - paulstretchCursor;
            }
            else
            {
                paulstretchCursor += loopLen;
            }
            paulstretch.reset();
            readPosition.store((juce::int64) paulstretchCursor);
        }
    }
    else if (paulstretchCursor >= (double) regionEnd)
    {
        playing.store(false);
    }
}

void StretchAudioSource::setLoopRegion(juce::int64 startSample, juce::int64 endSample)
{
    loopStart.store(juce::jmax((juce::int64) 0, startSample));
    loopEnd.store(endSample);
    loopGoingForward.store(true);
}

void StretchAudioSource::setNextReadPosition(juce::int64 newPosition)
{
    const juce::ScopedLock sl(lock);
    readPosition.store(juce::jlimit((juce::int64) 0, (juce::int64) sourceBuffer.getNumSamples(), newPosition));
    seekPending.store(true);
    loopGoingForward.store(true);
}

juce::int64 StretchAudioSource::getNextReadPosition() const
{
    return readPosition.load();
}

juce::int64 StretchAudioSource::getTotalLength() const
{
    const juce::ScopedLock sl(lock);
    return sourceBuffer.getNumSamples();
}
