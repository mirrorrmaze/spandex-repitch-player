#include "Paulstretch.h"

void Paulstretch::prepare(int windowSizeSamples)
{
    windowSize = juce::nextPowerOfTwo(windowSizeSamples);
    hopSize = windowSize / 4;

    const int order = (int) std::round(std::log2((double) windowSize));
    fft = std::make_unique<juce::dsp::FFT>(order);

    hannWindow.resize((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        hannWindow[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) (windowSize - 1)));

    fftBufL.assign((size_t) windowSize * 2, 0.0f);
    fftBufR.assign((size_t) windowSize * 2, 0.0f);
    accumL.assign((size_t) windowSize, 0.0f);
    accumR.assign((size_t) windowSize, 0.0f);

    // Empirically compute the overlap-add normalisation constant for a Hann
    // window applied twice (analysis + synthesis = hann^2) at 4x overlap,
    // by summing shifted copies and reading the steady-state value.
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

void Paulstretch::reset()
{
    std::fill(accumL.begin(), accumL.end(), 0.0f);
    std::fill(accumR.begin(), accumR.end(), 0.0f);
    fifoL.clear();
    fifoR.clear();
}

float Paulstretch::readInterpolated(const float* data, int length, double position)
{
    if (position < 0.0 || position >= (double) length)
        return 0.0f;

    const int idx0 = (int) position;
    if (idx0 >= length - 1)
        return data[idx0];

    const float frac = (float) (position - (double) idx0);
    return data[idx0] + frac * (data[idx0 + 1] - data[idx0]);
}

void Paulstretch::randomisePhase(float* fftData, juce::Random& rng) const
{
    const int nyquistBin = windowSize / 2;
    for (int k = 0; k <= nyquistBin; ++k)
    {
        float& re = fftData[2 * k];
        float& im = fftData[2 * k + 1];

        if (k == 0 || k == nyquistBin)
        {
            im = 0.0f;
            continue;
        }

        const float mag = std::sqrt(re * re + im * im);
        const float phase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        re = mag * std::cos(phase);
        im = mag * std::sin(phase);
    }
}

void Paulstretch::synthesiseHop(const float* srcL, const float* srcR, int sourceLength, double cursor)
{
    for (int i = 0; i < windowSize; ++i)
    {
        const double pos = cursor + (double) i;
        fftBufL[(size_t) i] = readInterpolated(srcL, sourceLength, pos) * hannWindow[(size_t) i];
        fftBufR[(size_t) i] = readInterpolated(srcR, sourceLength, pos) * hannWindow[(size_t) i];
    }
    std::fill(fftBufL.begin() + windowSize, fftBufL.end(), 0.0f);
    std::fill(fftBufR.begin() + windowSize, fftBufR.end(), 0.0f);

    fft->performRealOnlyForwardTransform(fftBufL.data());
    fft->performRealOnlyForwardTransform(fftBufR.data());

    randomisePhase(fftBufL.data(), randomL);
    randomisePhase(fftBufR.data(), randomR);

    fft->performRealOnlyInverseTransform(fftBufL.data());
    fft->performRealOnlyInverseTransform(fftBufR.data());

    for (int i = 0; i < windowSize; ++i)
    {
        accumL[(size_t) i] += fftBufL[(size_t) i] * hannWindow[(size_t) i] * normalisation;
        accumR[(size_t) i] += fftBufR[(size_t) i] * hannWindow[(size_t) i] * normalisation;
    }

    for (int i = 0; i < hopSize; ++i)
    {
        fifoL.push_back(accumL[(size_t) i]);
        fifoR.push_back(accumR[(size_t) i]);
    }

    std::move(accumL.begin() + hopSize, accumL.end(), accumL.begin());
    std::move(accumR.begin() + hopSize, accumR.end(), accumR.begin());
    std::fill(accumL.end() - hopSize, accumL.end(), 0.0f);
    std::fill(accumR.end() - hopSize, accumR.end(), 0.0f);
}

void Paulstretch::process(const float* srcL, const float* srcR, int sourceLength,
                           double& cursor, double speedRatio,
                           float* outL, float* outR, int numSamples)
{
    int written = 0;
    while (written < numSamples)
    {
        if (fifoL.empty())
        {
            synthesiseHop(srcL, srcR, sourceLength, cursor);
            cursor += (double) hopSize * speedRatio;
        }

        const int available = (int) fifoL.size();
        const int toCopy = juce::jmin(available, numSamples - written);

        for (int i = 0; i < toCopy; ++i)
        {
            outL[written + i] = fifoL.front();
            fifoL.pop_front();
            outR[written + i] = fifoR.front();
            fifoR.pop_front();
        }

        written += toCopy;
    }
}
