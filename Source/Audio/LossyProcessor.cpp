#include "LossyProcessor.h"

void LossyProcessor::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void LossyProcessor::reset()
{
    holdPhase = 0.0;
    heldSample = 0.0f;
}

void LossyProcessor::process(const float* in, float* out, int numSamples)
{
    const float driveDbNow = driveDb.load();
    const bool hasDrive = driveDbNow > 0.001f;
    const float driveGain = juce::Decibels::decibelsToGain(driveDbNow);
    // 2^(bits-1) rather than 2^bits: keeps the quantization step symmetric
    // around 0 for a bipolar signal (matches how a real fixed-point sample
    // format spends one bit on sign).
    const float levels = std::pow(2.0f, bits.load() - 1.0f);
    const double rateInc = (double) targetRateHz.load();

    for (int i = 0; i < numSamples; ++i)
    {
        const float dry = in[i];

        // Sample-and-hold decimation: only grab a fresh sample once every
        // sampleRate/targetRateHz input samples, holding the last one in
        // between - the phase accumulator tracks how much "target-rate
        // time" has elapsed without needing sampleRate/targetRateHz to be
        // an integer ratio.
        holdPhase += rateInc;
        if (holdPhase >= sampleRate)
        {
            holdPhase -= sampleRate;
            // Skipping tanh entirely at 0dB drive (rather than always
            // applying it with driveGain=1) matters here: tanh(x) isn't a
            // no-op except very close to 0, so leaving it unconditional
            // would mean even "no drive" quietly shaved level off anything
            // above a whisper - the max-bits/max-rate settings wouldn't
            // actually be transparent the way a bitcrusher's clean end of
            // the range should read.
            const float driven = hasDrive ? std::tanh(dry * driveGain) : dry;
            heldSample = std::round(driven * levels) / levels;
        }

        out[i] = dry * (1.0f - mix.load()) + heldSample * mix.load();
    }
}
