#include "EffectsChain.h"

EffectsChain::EffectsChain(juce::AudioSource& inputSource)
    : input(inputSource)
{
    analyzerHannWindow.resize((size_t) analyzerFftSize);
    for (int i = 0; i < analyzerFftSize; ++i)
        analyzerHannWindow[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                                  * (float) i / (float) (analyzerFftSize - 1)));

    analyzerCaptureBuffer.assign((size_t) analyzerFftSize, 0.0f);
    analyzerFftScratch.assign((size_t) analyzerFftSize * 2, 0.0f);
    latestSpectrumDb.assign((size_t) spectrumSize, -100.0f);

    // JUCE's FFT doesn't normalise by size or window gain, so raw magnitude
    // bins come out ~40-50dB "hot" relative to true amplitude (a bin-aligned
    // full-scale sine would read as magnitude ~N/2, not 1.0). Normalise by
    // the window's actual coherent gain (its mean value) so dB readings are
    // meaningful on their own, not just relative to the tracked peak.
    double windowSum = 0.0;
    for (float w : analyzerHannWindow)
        windowSum += w;
    analyzerMagnitudeNormalisation = (float) (windowSum / 2.0);

    // Bands 1-4 on by default, spread ~evenly across the log-frequency
    // graph (not clustered at the default 1kHz) so a first-time user sees
    // four distinct, draggable nodes rather than an empty-looking curve.
    // Left at 0dB/Bell so enabling them doesn't change the sound by
    // default - they're just visible and ready to be pulled into shape.
    static constexpr float defaultBandFrequencies[numEqBands] = { 100.0f, 500.0f, 2000.0f, 8000.0f };
    for (int i = 0; i < 4; ++i)
    {
        bands[(size_t) i].state.enabled.store(true);
        bands[(size_t) i].state.frequency.store(defaultBandFrequencies[i]);
    }

    for (int i = 0; i < numEqBands; ++i)
        updateEqBandCoefficients(i);
}

EffectsChain::~EffectsChain() = default;

void EffectsChain::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    input.prepareToPlay(samplesPerBlockExpected, newSampleRate);

    const juce::ScopedLock sl(lock);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlockExpected);
    spec.numChannels = 2;

    constexpr double enabledRampSeconds = 0.015;

    for (auto& band : bands)
    {
        band.processor.prepare(spec);
        band.enabledGain.reset(sampleRate, enabledRampSeconds);
        band.enabledGain.setCurrentAndTargetValue(band.state.enabled.load() ? 1.0f : 0.0f);
    }

    for (int i = 0; i < numEqBands; ++i)
        updateEqBandCoefficients(i);

    granularDelay.prepare(sampleRate);
    delayEnabledGain.reset(sampleRate, enabledRampSeconds);
    delayEnabledGain.setCurrentAndTargetValue(delayEnabled.load() ? 1.0f : 0.0f);

    shifterL.prepare(sampleRate);
    shifterR.prepare(sampleRate);
    shifterFeedbackStateL = shifterFeedbackStateR = 0.0f;
    shifterEnabledGain.reset(sampleRate, enabledRampSeconds);
    shifterEnabledGain.setCurrentAndTargetValue(shifterEnabled.load() ? 1.0f : 0.0f);

    reverb.prepare(spec);
    reverb.reset();
    reverbEnabledGain.reset(sampleRate, enabledRampSeconds);
    reverbEnabledGain.setCurrentAndTargetValue(reverbEnabled.load() ? 1.0f : 0.0f);

    smudgeL.prepare();
    smudgeR.prepare();
    smudgeL.reset();
    smudgeR.reset();
    setSmudgeRateMs(smudgeRateMs.load());
    smudgeEnabledGain.reset(sampleRate, enabledRampSeconds);
    smudgeEnabledGain.setCurrentAndTargetValue(smudgeEnabled.load() ? 1.0f : 0.0f);

    lossyL.prepare(sampleRate);
    lossyR.prepare(sampleRate);
    lossyL.reset();
    lossyR.reset();
    lossyEnabledGain.reset(sampleRate, enabledRampSeconds);
    lossyEnabledGain.setCurrentAndTargetValue(lossyEnabled.load() ? 1.0f : 0.0f);

    clipperL.prepare();
    clipperR.prepare();
    clipperL.reset();
    clipperR.reset();
}

void EffectsChain::releaseResources()
{
    input.releaseResources();
}

void EffectsChain::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    input.getNextAudioBlock(bufferToFill);

    const juce::ScopedLock sl(lock);

    if (bufferToFill.buffer == nullptr || bufferToFill.buffer->getNumChannels() < 1 || bufferToFill.numSamples <= 0)
        return;

    juce::dsp::AudioBlock<float> block(*bufferToFill.buffer);
    auto sub = block.getSubBlock((size_t) bufferToFill.startSample, (size_t) bufferToFill.numSamples);
    juce::dsp::ProcessContextReplacing<float> context(sub);

    const int numCh = bufferToFill.buffer->getNumChannels();
    const int n = bufferToFill.numSamples;

    const float inGain = juce::Decibels::decibelsToGain(inputGainDb.load());
    if (inGain != 1.0f)
        sub.multiplyBy(inGain);

    auto ensureDryScratch = [&]
    {
        if (dryScratch.getNumChannels() < numCh || dryScratch.getNumSamples() < n)
            dryScratch.setSize(juce::jmax(2, numCh), juce::jmax(n, dryScratch.getNumSamples()), false, false, true);
    };
    auto captureDry = [&]
    {
        ensureDryScratch();
        for (int ch = 0; ch < numCh; ++ch)
            dryScratch.copyFrom(ch, 0, *bufferToFill.buffer, ch, bufferToFill.startSample, n);
    };
    // Blends the block just processed in place back against the dry
    // snapshot using a per-sample ramping gain, so enabling/disabling a
    // stage fades its wet contribution in/out over ~15ms instead of
    // instantly switching it - the switch itself is what caused the
    // reported clicks (a sudden step in the output, worse when the stage
    // had built-up energy like delay feedback or a reverb tail).
    auto blendWithGain = [&](juce::SmoothedValue<float>& gain)
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
                const float dry = dryScratch.getSample(ch, i);
                data[i] = dry + (data[i] - dry) * g;
            }
        }
    };

    for (auto& band : bands)
    {
        auto& gain = band.enabledGain;
        gain.setTargetValue(band.state.enabled.load() ? 1.0f : 0.0f);

        if (!gain.isSmoothing())
        {
            if (gain.getCurrentValue() <= 0.0001f)
                continue; // steady off - matches the old skip-entirely behaviour
            band.processor.process(context); // steady on - no blend overhead needed
            continue;
        }

        captureDry();
        band.processor.process(context);
        blendWithGain(gain);
    }

    // Smudge, Delay, and Reverb all hold onto meaningful internal state
    // (frozen spectrum, delay/grain buffers, reverb tail) that should keep
    // evolving even while bypassed, the same way a real pedal's spring tank
    // keeps ringing whether or not you're stepping on it. Earlier this block
    // only ran these while transitioning or steady-on, which meant a stage
    // left "off" for a while went stale (e.g. the reverb tank frozen mid-
    // decay) - re-enabling it then flooded the ramp with a burst of stale,
    // uncorrelated energy, which is exactly the residual click a self-test
    // caught even after adding the gain ramp. So these always run,
    // unconditionally, and only the output blend is gated by the ramp.
    // Lossy is a streaming STFT with its own held spectral frame, so it
    // always runs here too for the same reasoning and to keep the pattern
    // consistent - stopping it mid-hold would go just as stale as the
    // reverb tank does.
    {
        smudgeEnabledGain.setTargetValue(smudgeEnabled.load() ? 1.0f : 0.0f);
        const int smudgeCh = juce::jmin(2, numCh);
        if ((int) smudgeScratch.size() < n)
            smudgeScratch.resize((size_t) n);
        captureDry();

        for (int ch = 0; ch < smudgeCh; ++ch)
        {
            auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
            auto& proc = (ch == 0) ? smudgeL : smudgeR;
            proc.process(data, smudgeScratch.data(), n);
            std::copy(smudgeScratch.data(), smudgeScratch.data() + n, data);
        }

        blendWithGain(smudgeEnabledGain);
    }

    {
        lossyEnabledGain.setTargetValue(lossyEnabled.load() ? 1.0f : 0.0f);
        const int lossyCh = juce::jmin(2, numCh);
        if ((int) lossyScratch.size() < n)
            lossyScratch.resize((size_t) n);
        captureDry();

        for (int ch = 0; ch < lossyCh; ++ch)
        {
            auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
            auto& proc = (ch == 0) ? lossyL : lossyR;
            proc.process(data, lossyScratch.data(), n);
            std::copy(lossyScratch.data(), lossyScratch.data() + n, data);
        }

        blendWithGain(lossyEnabledGain);
    }

    {
        delayEnabledGain.setTargetValue(delayEnabled.load() ? 1.0f : 0.0f);
        captureDry();

        const int delayCh = juce::jmin(2, numCh);
        float* channelPtrs[2] = { nullptr, nullptr };
        for (int ch = 0; ch < delayCh; ++ch)
            channelPtrs[ch] = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
        granularDelay.process(channelPtrs, delayCh, n);

        blendWithGain(delayEnabledGain);
    }

    {
        shifterEnabledGain.setTargetValue(shifterEnabled.load() ? 1.0f : 0.0f);
        captureDry();

        const int shifterCh = juce::jmin(2, numCh);
        const float coarse = shifterCoarseHz.load();
        const float fine = shifterFineHz.load();
        const float spread = shifterSpreadHz.load();
        const float fb = shifterFeedback.load();
        const float mixAmt = shifterMix.load();
        const auto shifterModeNow = shifterMode.load();
        shifterL.setMode(shifterModeNow);
        shifterR.setMode(shifterModeNow);
        shifterL.setShiftHz(coarse + fine - spread * 0.5f);
        shifterR.setShiftHz(coarse + fine + spread * 0.5f);

        for (int ch = 0; ch < shifterCh; ++ch)
        {
            auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
            auto& proc = (ch == 0) ? shifterL : shifterR;
            auto& fbState = (ch == 0) ? shifterFeedbackStateL : shifterFeedbackStateR;
            for (int i = 0; i < n; ++i)
            {
                const float dryIn = data[i];
                float fedIn = dryIn + fbState * fb;
                float shifted = 0.0f;
                proc.process(&fedIn, &shifted, 1);
                fbState = shifted;
                data[i] = dryIn * (1.0f - mixAmt) + shifted * mixAmt;
            }
        }

        blendWithGain(shifterEnabledGain);
    }

    if (numCh >= 2)
    {
        reverbEnabledGain.setTargetValue(reverbEnabled.load() ? 1.0f : 0.0f);
        captureDry();
        reverb.process(context);
        blendWithGain(reverbEnabledGain);
    }

    {
        // Bus glue stage: a frequency-domain hard clipper (see
        // SpectralClipperProcessor) blended against the dry spectrum by the
        // Clip knob, followed by a tanh drive/saturation waveshaper.
        // Applied to the fully mixed signal right before the final output
        // trim. Replaces the old single-knob compressor, which read as
        // "doesn't do much" per direct feedback - clipping whichever
        // frequencies are hot, per channel, reads as clearly more
        // distorted/present than ducking the whole signal with one shared
        // gain-reduction envelope did. The clipper's own STFT always runs
        // (like Smudge/Lossy above) so raising Clip back up doesn't resume
        // from stale/reset internal state; only Drive's stateless tanh
        // stage is worth skipping outright when it's at 0dB.
        const float driveDbNow = driveDb.load();
        const int clipCh = juce::jmin(2, numCh);
        if ((int) clipperScratchL.size() < n)
        {
            clipperScratchL.resize((size_t) n);
            clipperScratchR.resize((size_t) n);
        }

        for (int ch = 0; ch < clipCh; ++ch)
        {
            auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
            auto& proc = (ch == 0) ? clipperL : clipperR;
            auto& scratch = (ch == 0) ? clipperScratchL : clipperScratchR;
            proc.process(data, scratch.data(), n);
            std::copy(scratch.data(), scratch.data() + n, data);
        }

        if (driveDbNow > 0.001f)
        {
            const float driveK = juce::Decibels::decibelsToGain(driveDbNow);
            const float driveNorm = std::tanh(driveK) > 1.0e-6f ? 1.0f / std::tanh(driveK) : 1.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
                for (int i = 0; i < n; ++i)
                    data[i] = std::tanh(data[i] * driveK) * driveNorm;
            }
        }
    }

    const float outGain = juce::Decibels::decibelsToGain(outputGainDb.load());
    if (outGain != 1.0f)
        sub.multiplyBy(outGain);

    pushToAnalyzer(bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample), bufferToFill.numSamples);
}

void EffectsChain::updateEqBandCoefficients(int index)
{
    // Caller holds `lock` (or, from the constructor, no other thread can see `this` yet).
    auto& band = bands[(size_t) index];
    if (sampleRate <= 0.0)
        return;

    const auto freq = juce::jlimit(20.0f, (float) juce::jmin(20000.0, sampleRate * 0.49), band.state.frequency.load());
    const auto q = juce::jlimit(0.1f, 18.0f, band.state.q.load());
    const auto gainLinear = juce::Decibels::decibelsToGain(band.state.gainDb.load());

    Coeffs::Ptr newCoeffs;
    switch (band.state.type.load())
    {
        case FilterType::HighPass:  newCoeffs = Coeffs::makeHighPass(sampleRate, freq, q); break;
        case FilterType::LowPass:   newCoeffs = Coeffs::makeLowPass(sampleRate, freq, q); break;
        case FilterType::LowShelf:  newCoeffs = Coeffs::makeLowShelf(sampleRate, freq, q, gainLinear); break;
        case FilterType::HighShelf: newCoeffs = Coeffs::makeHighShelf(sampleRate, freq, q, gainLinear); break;
        case FilterType::Notch:     newCoeffs = Coeffs::makeNotch(sampleRate, freq, q); break;
        case FilterType::BandPass:  newCoeffs = Coeffs::makeBandPass(sampleRate, freq, q); break;
        case FilterType::Bell:
        default:                    newCoeffs = Coeffs::makePeakFilter(sampleRate, freq, q, gainLinear); break;
    }

    // ProcessorDuplicator's per-channel filters each captured their own copy
    // of the `state` pointer when prepare() ran, so replacing the pointer
    // here wouldn't reach them - mutate the coefficients object they already
    // share instead.
    if (band.processor.state == nullptr)
        band.processor.state = newCoeffs;
    else
        *band.processor.state = *newCoeffs;
}

void EffectsChain::setEqBandEnabled(int index, bool enabled)
{
    bands[(size_t) index].state.enabled.store(enabled);
}

void EffectsChain::setEqBandType(int index, FilterType type)
{
    const juce::ScopedLock sl(lock);
    bands[(size_t) index].state.type.store(type);
    updateEqBandCoefficients(index);
}

void EffectsChain::setEqBandFrequency(int index, float hz)
{
    const juce::ScopedLock sl(lock);
    bands[(size_t) index].state.frequency.store(hz);
    updateEqBandCoefficients(index);
}

void EffectsChain::setEqBandGainDb(int index, float db)
{
    const juce::ScopedLock sl(lock);
    bands[(size_t) index].state.gainDb.store(db);
    updateEqBandCoefficients(index);
}

void EffectsChain::setEqBandQ(int index, float q)
{
    const juce::ScopedLock sl(lock);
    bands[(size_t) index].state.q.store(q);
    updateEqBandCoefficients(index);
}

float EffectsChain::getMagnitudeResponseDb(float frequencyHz) const
{
    const juce::ScopedLock sl(lock);
    double totalDb = 0.0;
    for (auto& band : bands)
    {
        if (!band.state.enabled.load() || band.processor.state == nullptr)
            continue;
        const auto mag = band.processor.state->getMagnitudeForFrequency((double) frequencyHz, sampleRate);
        totalDb += juce::Decibels::gainToDecibels(mag, -100.0);
    }
    return (float) totalDb;
}

void EffectsChain::setSmudgeAmount(float amount01)
{
    smudgeAmount.store(juce::jlimit(0.0f, 1.0f, amount01));
    smudgeL.setAmount(amount01);
    smudgeR.setAmount(amount01);
}

void EffectsChain::setSmudgeFeedback(float feedback01)
{
    smudgeFeedback.store(juce::jlimit(0.0f, 0.95f, feedback01));
    smudgeL.setFeedback(feedback01);
    smudgeR.setFeedback(feedback01);
}

void EffectsChain::setSmudgeRateMs(float ms)
{
    ms = juce::jlimit(5.0f, 200.0f, ms);
    smudgeRateMs.store(ms);
    const int samples = (int) std::round(ms * 0.001 * sampleRate);
    smudgeL.setWindowSizeSamples(samples);
    smudgeR.setWindowSizeSamples(samples);
}

void EffectsChain::setReverbParams(const juce::dsp::Reverb::Parameters& p)
{
    const juce::ScopedLock sl(lock);
    reverb.setParameters(p);
}

juce::dsp::Reverb::Parameters EffectsChain::getReverbParams() const
{
    const juce::ScopedLock sl(lock);
    return reverb.getParameters();
}

void EffectsChain::pushToAnalyzer(const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        analyzerCaptureBuffer[(size_t) analyzerCaptureIndex] = data[i];
        ++analyzerCaptureIndex;
        if (analyzerCaptureIndex >= analyzerFftSize)
        {
            computeAnalyzerFrame();
            analyzerCaptureIndex = 0;
        }
    }
}

void EffectsChain::computeAnalyzerFrame()
{
    for (int i = 0; i < analyzerFftSize; ++i)
        analyzerFftScratch[(size_t) i] = analyzerCaptureBuffer[(size_t) i] * analyzerHannWindow[(size_t) i];
    std::fill(analyzerFftScratch.begin() + analyzerFftSize, analyzerFftScratch.end(), 0.0f);

    analyzerFft.performFrequencyOnlyForwardTransform(analyzerFftScratch.data(), true);

    const float norm = analyzerMagnitudeNormalisation > 1.0e-9f ? 1.0f / analyzerMagnitudeNormalisation : 1.0f;

    float frameMaxDb = -100.0f;
    for (int i = 0; i < spectrumSize; ++i)
        frameMaxDb = juce::jmax(frameMaxDb, analyzerFftScratch[(size_t) i] * norm);
    frameMaxDb = juce::Decibels::gainToDecibels(frameMaxDb, -100.0f);

    const juce::ScopedLock sl(analyzerLock);
    for (int i = 0; i < spectrumSize; ++i)
        latestSpectrumDb[(size_t) i] = juce::Decibels::gainToDecibels(analyzerFftScratch[(size_t) i] * norm, -100.0f);

    // Fast attack (jump up immediately so the display never clips a sudden
    // loud passage), slow release (~2s time constant at one analyzer frame
    // per ~46ms) so it doesn't visibly bounce on every beat.
    if (frameMaxDb > spectrumPeakDb)
        spectrumPeakDb = frameMaxDb;
    else
        spectrumPeakDb = spectrumPeakDb * 0.98f + frameMaxDb * 0.02f;

    // The display window floor is peak-relative (peak - 60dB, see
    // EqCurveEditor). With no lower bound here, extended silence (paused,
    // or between tracks) lets the peak keep decaying indefinitely below the
    // analyzer's silence floor of -100dB. Once it decays past -40dB, that
    // relationship inverts: silence (-100dB bins) computes as *above* the
    // display floor instead of below it, and the flat silent spectrum
    // visibly climbs, eventually pinning to the top of the graph instead of
    // dropping out. Clamping the peak here keeps silence reading as fully
    // collapsed to the bottom regardless of how long it persists.
    spectrumPeakDb = juce::jmax(-40.0f, spectrumPeakDb);
}

std::vector<float> EffectsChain::getLatestSpectrum() const
{
    const juce::ScopedLock sl(analyzerLock);
    return latestSpectrumDb;
}

float EffectsChain::getSpectrumPeakDb() const
{
    const juce::ScopedLock sl(analyzerLock);
    return spectrumPeakDb;
}
