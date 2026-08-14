#include "SpectralClipperProcessor.h"

void SpectralClipperProcessor::prepare()
{
    hannWindow.resize((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        hannWindow[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) (windowSize - 1)));

    inputRing.assign((size_t) windowSize, 0.0f);
    fftScratch.assign((size_t) windowSize * 2, 0.0f);
    accum.assign((size_t) windowSize, 0.0f);

    // Overlap-add reconstruction gain, computed the same steady-state way
    // as SmudgeProcessor/LossyProcessor: sum the squared window across 8
    // overlapping hops and read the settled middle value.
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
        olaNormalisation = steadyState > 0.0 ? (float) (1.0 / steadyState) : 1.0f;
    }

    {
        double windowSum = 0.0;
        for (float w : hannWindow)
            windowSum += w;
        magnitudeScale = (float) windowSum;
    }

    reset();
}

void SpectralClipperProcessor::reset()
{
    std::fill(inputRing.begin(), inputRing.end(), 0.0f);
    std::fill(accum.begin(), accum.end(), 0.0f);
    outputFifo.clear();
    inputWritePos = 0;
    samplesSinceHop = 0;
}

void SpectralClipperProcessor::processHop()
{
    for (int i = 0; i < windowSize; ++i)
    {
        const int idx = (inputWritePos + i) % windowSize;
        fftScratch[(size_t) i] = inputRing[(size_t) idx] * hannWindow[(size_t) i];
    }
    std::fill(fftScratch.begin() + windowSize, fftScratch.end(), 0.0f);

    fft.performRealOnlyForwardTransform(fftScratch.data());

    const float amountNow = amount.load();
    // Ceiling shrinks steeply as Amount increases - most of the knob's
    // range spends time in genuinely aggressive territory instead of a
    // barely-audible top few percent, matching "more distortion" rather
    // than "gentle brightness taming."
    const float ceiling = magnitudeScale * juce::jmap(amountNow, 0.0f, 1.0f, 4.0f, 0.05f);
    // Compensates for the average level clipping itself removes, so higher
    // Amount reads as more present/in-your-face rather than just quieter -
    // the "bite" the old Compression knob was going for, through a
    // harsher, frequency-domain mechanism instead of broadband dynamics.
    const float makeupGain = 1.0f + amountNow * 3.0f;
    const int nyquistBin = windowSize / 2;

    for (int k = 0; k <= nyquistBin; ++k)
    {
        const float re = fftScratch[(size_t) (2 * k)];
        const float im = fftScratch[(size_t) (2 * k + 1)];
        const float mag = std::sqrt(re * re + im * im);

        float wetRe, wetIm;
        if (mag > ceiling && mag > 1.0e-9f)
        {
            const float g = (ceiling / mag) * makeupGain;
            wetRe = re * g;
            wetIm = im * g;
        }
        else
        {
            wetRe = re * makeupGain;
            wetIm = im * makeupGain;
        }

        // Blended in the complex domain (not magnitude/phase separately,
        // which would misbehave across the phase wrap) - Amount at 0
        // leaves this bin untouched, at 1 it's fully clipped+made-up.
        fftScratch[(size_t) (2 * k)] = re * (1.0f - amountNow) + wetRe * amountNow;
        fftScratch[(size_t) (2 * k + 1)] = im * (1.0f - amountNow) + wetIm * amountNow;
    }

    fft.performRealOnlyInverseTransform(fftScratch.data());

    for (int i = 0; i < windowSize; ++i)
        accum[(size_t) i] += fftScratch[(size_t) i] * hannWindow[(size_t) i] * olaNormalisation;

    for (int i = 0; i < hopSize; ++i)
        outputFifo.push_back(accum[(size_t) i]);

    std::move(accum.begin() + hopSize, accum.end(), accum.begin());
    std::fill(accum.end() - hopSize, accum.end(), 0.0f);
}

float SpectralClipperProcessor::processOneSample(float inSample)
{
    inputRing[(size_t) inputWritePos] = inSample;
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
    return out;
}

void SpectralClipperProcessor::process(const float* in, float* out, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        out[i] = processOneSample(in[i]);
}
