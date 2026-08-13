#include "SmudgeProcessor.h"

void SmudgeProcessor::prepare(int windowSizeSamples)
{
    windowSize = juce::nextPowerOfTwo(windowSizeSamples);
    pendingWindowSize.store(windowSize);
    hopSize = windowSize / 4;

    const int order = (int) std::round(std::log2((double) windowSize));
    fft = std::make_unique<juce::dsp::FFT>(order);

    hannWindow.resize((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        hannWindow[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) (windowSize - 1)));

    inputRing.assign((size_t) windowSize, 0.0f);
    fftScratch.assign((size_t) windowSize * 2, 0.0f);
    heldRe.assign((size_t) (windowSize / 2 + 1), 0.0f);
    heldIm.assign((size_t) (windowSize / 2 + 1), 0.0f);
    accum.assign((size_t) windowSize, 0.0f);

    {
        std::vector<double> sumSq((size_t) windowSize * 2, 0.0);
        for (int hopIndex = 0; hopIndex < 8; ++hopIndex)
        {
            const int offset = hopIndex * hopSize;
            for (int i = 0; i < windowSize; ++i)
            {
                const size_t idx = (size_t) (offset + i);
                if (idx < sumSq.size())
                    sumSq[idx] += (double) hannWindow[(size_t) i] * hannWindow[(size_t) i];
            }
        }
        const double steadyState = sumSq[(size_t) (windowSize + windowSize / 2)];
        normalisation = steadyState > 0.0 ? (float) (1.0 / steadyState) : 1.0f;
    }

    reset();
}

void SmudgeProcessor::reset()
{
    std::fill(inputRing.begin(), inputRing.end(), 0.0f);
    std::fill(heldRe.begin(), heldRe.end(), 0.0f);
    std::fill(heldIm.begin(), heldIm.end(), 0.0f);
    std::fill(accum.begin(), accum.end(), 0.0f);
    outputFifo.clear();
    inputWritePos = 0;
    samplesSinceHop = 0;
    feedbackState = 0.0f;
}

void SmudgeProcessor::processHop()
{
    for (int i = 0; i < windowSize; ++i)
    {
        const int idx = (inputWritePos + i) % windowSize;
        fftScratch[(size_t) i] = inputRing[(size_t) idx] * hannWindow[(size_t) i];
    }
    std::fill(fftScratch.begin() + windowSize, fftScratch.end(), 0.0f);

    fft->performRealOnlyForwardTransform(fftScratch.data());

    const float amt = smoothedAmount;
    const int nyquistBin = windowSize / 2;
    for (int k = 0; k <= nyquistBin; ++k)
    {
        const float newRe = fftScratch[(size_t) (2 * k)];
        const float newIm = fftScratch[(size_t) (2 * k + 1)];
        const float blendedRe = heldRe[(size_t) k] * amt + newRe * (1.0f - amt);
        const float blendedIm = heldIm[(size_t) k] * amt + newIm * (1.0f - amt);
        heldRe[(size_t) k] = blendedRe;
        heldIm[(size_t) k] = blendedIm;
        fftScratch[(size_t) (2 * k)] = blendedRe;
        fftScratch[(size_t) (2 * k + 1)] = blendedIm;
    }

    fft->performRealOnlyInverseTransform(fftScratch.data());

    for (int i = 0; i < windowSize; ++i)
        accum[(size_t) i] += fftScratch[(size_t) i] * hannWindow[(size_t) i] * normalisation;

    for (int i = 0; i < hopSize; ++i)
        outputFifo.push_back(accum[(size_t) i]);

    std::move(accum.begin() + hopSize, accum.end(), accum.begin());
    std::fill(accum.end() - hopSize, accum.end(), 0.0f);
}

float SmudgeProcessor::processOneSample(float inSample)
{
    smoothedAmount += (amount.load() - smoothedAmount) * paramSmoothingCoeff;
    smoothedFeedback += (feedback.load() - smoothedFeedback) * paramSmoothingCoeff;

    const float fb = smoothedFeedback;
    inputRing[(size_t) inputWritePos] = inSample + feedbackState * fb;
    inputWritePos = (inputWritePos + 1) % windowSize;
    ++samplesSinceHop;

    if (samplesSinceHop >= hopSize)
    {
        processHop();
        samplesSinceHop = 0;
    }

    float out = 0.0f;
    if (!outputFifo.empty())
    {
        out = outputFifo.front();
        outputFifo.pop_front();
    }

    // Saturate only the fed-back tap, not the returned sample - keeps the
    // loop bounded even with Amount and Feedback both maxed, without ever
    // clipping the clean (feedback=0) case.
    feedbackState = std::tanh(out);
    return out;
}

void SmudgeProcessor::process(const float* in, float* out, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (resizeState == ResizeState::Steady)
        {
            const int desired = pendingWindowSize.load();
            if (desired != windowSize)
            {
                resizeState = ResizeState::FadingOut;
                fadeSamplesRemaining = fadeLengthSamples;
                pendingResizeTarget = desired;
            }
        }

        float sample = processOneSample(in[i]);

        if (resizeState == ResizeState::FadingOut)
        {
            // Equal-power (not linear) so the fade reads as a smooth
            // dissolve rather than a perceptible dip in loudness partway
            // through - sin/cos sum to constant power at every point along
            // the curve, unlike a linear ramp.
            const float t = (float) fadeSamplesRemaining / (float) fadeLengthSamples;
            sample *= std::sin(t * juce::MathConstants<float>::halfPi);
            if (--fadeSamplesRemaining <= 0)
            {
                prepare(pendingResizeTarget);
                resizeState = ResizeState::FadingIn;
                fadeSamplesRemaining = fadeLengthSamples;
            }
        }
        else if (resizeState == ResizeState::FadingIn)
        {
            const float t = (float) fadeSamplesRemaining / (float) fadeLengthSamples;
            sample *= std::cos(t * juce::MathConstants<float>::halfPi);
            if (--fadeSamplesRemaining <= 0)
                resizeState = ResizeState::Steady;
        }

        out[i] = sample;
    }
}
