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
    const double rate = std::pow(2.0, pitchSemitones.load() / 12.0);
    const int totalSamples = sourceBuffer.getNumSamples();
    const int numOutCh = bufferToFill.buffer->getNumChannels();
    const int numSrcCh = sourceBuffer.getNumChannels();

    const bool isLoopingNow = looping.load();
    const double endBoundary = isLoopingNow
        ? (double) juce::jmin((juce::int64) (totalSamples - 1), effectiveLoopEnd())
        : (double) (totalSamples - 1);
    const double startBoundary = isLoopingNow ? (double) effectiveLoopStart() : 0.0;

    int i = 0;
    for (; i < bufferToFill.numSamples; ++i)
    {
        if (linkedCursor < 0.0 || linkedCursor >= endBoundary)
        {
            if (isLoopingNow)
            {
                linkedCursor = startBoundary;
            }
            else
            {
                playing.store(false);
                break;
            }
        }

        const int idx0 = (int) linkedCursor;
        const float frac = (float) (linkedCursor - (double) idx0);
        const float fadeGain = isLoopingNow ? loopFadeGain(linkedCursor, startBoundary, endBoundary) : 1.0f;

        for (int ch = 0; ch < numOutCh; ++ch)
        {
            const int srcCh = juce::jmin(ch, numSrcCh - 1);
            const float s0 = sourceBuffer.getSample(srcCh, idx0);
            const float s1 = sourceBuffer.getSample(srcCh, juce::jmin(idx0 + 1, totalSamples - 1));
            bufferToFill.buffer->setSample(ch, bufferToFill.startSample + i, (s0 + frac * (s1 - s0)) * fadeGain);
        }

        linkedCursor += rate;
    }

    readPosition.store((juce::int64) linkedCursor);
}

void StretchAudioSource::renderWarped(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (stretcher == nullptr)
        return;

    const auto trackEnd = (juce::int64) sourceBuffer.getNumSamples();
    const int numOutCh = bufferToFill.buffer->getNumChannels();
    const int numSrcCh = sourceBuffer.getNumChannels();

    const bool isLoopingNow = looping.load();
    const auto regionEnd = isLoopingNow ? juce::jmin(trackEnd, effectiveLoopEnd()) : trackEnd;
    const auto regionStart = isLoopingNow ? effectiveLoopStart() : (juce::int64) 0;

    int needed = bufferToFill.numSamples;
    int outOffset = bufferToFill.startSample;

    while (needed > 0)
    {
        const int avail = stretcher->available();

        if (avail < 0)
        {
            if (isLoopingNow)
            {
                stretcher->reset();
                readPosition.store(regionStart);
                linkedCursor = (double) regionStart;
                finalSent = false;
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
                // Defensive: shouldn't normally be reached (available() should
                // report -1 once final input is fully drained), but avoids a
                // stall if it does.
                for (int ch = 0; ch < numOutCh; ++ch)
                    bufferToFill.buffer->clear(ch, outOffset, needed);
                playing.store(false);
                return;
            }

            size_t desired = stretcher->getSamplesRequired();
            if (desired == 0)
                desired = 256;
            desired = juce::jmin(desired, (size_t) scratchCapacity);

            const auto pos = readPosition.load();
            const juce::int64 remaining = regionEnd - pos;
            const size_t provide = (size_t) juce::jlimit((juce::int64) 0, (juce::int64) desired, remaining);

            for (int ch = 0; ch < 2; ++ch)
            {
                const int srcCh = juce::jmin(ch, numSrcCh - 1);
                scratchInput.copyFrom(ch, 0, sourceBuffer, srcCh, (int) pos, (int) provide);
            }

            const bool isFinal = (pos + (juce::int64) provide) >= regionEnd;
            const float* inPtrs[2] = { scratchInput.getReadPointer(0), scratchInput.getReadPointer(1) };
            stretcher->process(inPtrs, provide, isFinal);

            readPosition.store(pos + (juce::int64) provide);
            if (isFinal)
                finalSent = true;
        }
        else
        {
            const int toCopy = juce::jmin(avail, needed);
            float* outPtrs[2] = { scratchOutput.getWritePointer(0), scratchOutput.getWritePointer(1) };
            const auto got = (int) stretcher->retrieve(outPtrs, (size_t) toCopy);

            // Approximate: readPosition tracks the input side, which lags/
            // leads this retrieved output by the stretcher's own internal
            // latency, and only updates once per provide() call rather than
            // per retrieved sample - a single gain applied to the whole
            // chunk rather than a true sample-accurate ramp, but enough to
            // turn the loop-wrap discontinuity into a dip instead of a click.
            if (isLoopingNow)
            {
                const float fadeGain = loopFadeGain((double) readPosition.load(), (double) regionStart, (double) regionEnd);
                if (fadeGain < 0.999f)
                    for (int ch = 0; ch < 2; ++ch)
                        juce::FloatVectorOperations::multiply(scratchOutput.getWritePointer(ch), fadeGain, got);
            }

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
    const auto regionEnd = isLoopingNow ? juce::jmin((juce::int64) totalSamples, effectiveLoopEnd())
                                         : (juce::int64) totalSamples;
    const auto regionStart = isLoopingNow ? effectiveLoopStart() : (juce::int64) 0;

    if (paulstretchScratch.getNumSamples() < numSamples)
        paulstretchScratch.setSize(2, numSamples, false, false, true);

    const float* srcL = sourceBuffer.getReadPointer(0);
    const float* srcR = sourceBuffer.getReadPointer(juce::jmin(1, numSrcCh - 1));
    const double speedRatio = speedPercent.load() / 100.0;

    const float fadeGain = isLoopingNow ? loopFadeGain(paulstretchCursor, (double) regionStart, (double) regionEnd) : 1.0f;

    paulstretch.process(srcL, srcR, totalSamples, paulstretchCursor, speedRatio,
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

    if (paulstretchCursor >= (double) regionEnd)
    {
        if (isLoopingNow)
        {
            paulstretch.reset();
            paulstretchCursor = (double) regionStart;
            readPosition.store(regionStart);
        }
        else
        {
            playing.store(false);
        }
    }
}

void StretchAudioSource::setLoopRegion(juce::int64 startSample, juce::int64 endSample)
{
    loopStart.store(juce::jmax((juce::int64) 0, startSample));
    loopEnd.store(endSample);
}

void StretchAudioSource::setNextReadPosition(juce::int64 newPosition)
{
    const juce::ScopedLock sl(lock);
    readPosition.store(juce::jlimit((juce::int64) 0, (juce::int64) sourceBuffer.getNumSamples(), newPosition));
    seekPending.store(true);
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
