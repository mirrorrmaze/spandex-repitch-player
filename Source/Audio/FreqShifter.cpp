#include "FreqShifter.h"

void FreqShifter::prepare(double sampleRateIn, int windowSizeSamples)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    windowSize = juce::nextPowerOfTwo(windowSizeSamples);
    hopSize = windowSize / 4;

    const int order = (int) std::round(std::log2((double) windowSize));
    fft = std::make_unique<juce::dsp::FFT>(order);

    hannWindow.resize((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        hannWindow[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) (windowSize - 1)));

    inputRing.assign((size_t) windowSize, 0.0f);
    timeDomain.assign((size_t) windowSize, {});
    freqDomain.assign((size_t) windowSize, {});
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

void FreqShifter::reset()
{
    std::fill(inputRing.begin(), inputRing.end(), 0.0f);
    std::fill(accum.begin(), accum.end(), 0.0f);
    outputFifo.clear();
    inputWritePos = 0;
    samplesSinceHop = 0;
    shiftPhaseAccum = 0.0;
    ringPhase = 0.0;
}

void FreqShifter::processHop()
{
    for (int i = 0; i < windowSize; ++i)
    {
        const int idx = (inputWritePos + i) % windowSize;
        timeDomain[(size_t) i] = { inputRing[(size_t) idx] * hannWindow[(size_t) i], 0.0f };
    }

    fft->perform(timeDomain.data(), freqDomain.data(), false);

    // Fold into the analytic (positive-frequencies-only) spectrum: DC and
    // Nyquist stay as-is, everything between is doubled, the negative-
    // frequency half is zeroed. Inverse-transforming this gives a complex
    // signal whose real part is the original and imaginary part is its
    // Hilbert transform - exactly the input needed for single-sideband
    // modulation below.
    const int nyquistBin = windowSize / 2;
    for (int k = 1; k < nyquistBin; ++k)
        freqDomain[(size_t) k] *= 2.0f;
    for (int k = nyquistBin + 1; k < windowSize; ++k)
        freqDomain[(size_t) k] = {};

    fft->perform(freqDomain.data(), timeDomain.data(), true);

    // Modulate by a continuous complex exponential at the shift frequency
    // and keep only the real part - see the header comment on
    // shiftPhaseAccum for why the phase must be snapshotted once per hop
    // (not recomputed from scratch) for overlapping frames to agree.
    const float currentShiftHz = shiftHz.load();
    const double deltaPerSample = juce::MathConstants<double>::twoPi * (double) currentShiftHz / sampleRate;
    const double frameStartPhase = shiftPhaseAccum;

    for (int n = 0; n < windowSize; ++n)
    {
        const double modPhase = frameStartPhase + (double) n * deltaPerSample;
        const float cosP = (float) std::cos(modPhase);
        const float sinP = (float) std::sin(modPhase);
        const auto z = timeDomain[(size_t) n];
        const float shiftedRe = z.real() * cosP - z.imag() * sinP;
        accum[(size_t) n] += shiftedRe * hannWindow[(size_t) n] * normalisation;
    }

    shiftPhaseAccum = std::fmod(shiftPhaseAccum + (double) hopSize * deltaPerSample, juce::MathConstants<double>::twoPi);

    for (int i = 0; i < hopSize; ++i)
        outputFifo.push_back(accum[(size_t) i]);

    std::move(accum.begin() + hopSize, accum.end(), accum.begin());
    std::fill(accum.end() - hopSize, accum.end(), 0.0f);
}

void FreqShifter::process(const float* in, float* out, int numSamples)
{
    if (mode.load() == Mode::RingMod)
    {
        const float hz = shiftHz.load();
        const double delta = juce::MathConstants<double>::twoPi * (double) hz / sampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            out[i] = in[i] * (float) std::cos(ringPhase);
            ringPhase = std::fmod(ringPhase + delta, juce::MathConstants<double>::twoPi);
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        inputRing[(size_t) inputWritePos] = in[i];
        inputWritePos = (inputWritePos + 1) % windowSize;
        ++samplesSinceHop;

        if (samplesSinceHop >= hopSize)
        {
            processHop();
            samplesSinceHop = 0;
        }

        if (!outputFifo.empty())
        {
            out[i] = outputFifo.front();
            outputFifo.pop_front();
        }
        else
        {
            out[i] = 0.0f;
        }
    }
}
