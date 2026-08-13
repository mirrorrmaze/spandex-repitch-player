#include "GranularDelay.h"

void GranularDelay::prepare(double newSampleRate, int maxDelaySeconds)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    bufferSize = juce::jmax(1, (int) (maxDelaySeconds * sampleRate));
    bufferL.assign((size_t) bufferSize, 0.0f);
    bufferR.assign((size_t) bufferSize, 0.0f);
    reset();
}

void GranularDelay::reset()
{
    std::fill(bufferL.begin(), bufferL.end(), 0.0f);
    std::fill(bufferR.begin(), bufferR.end(), 0.0f);
    writePos = 0;
    samplesUntilNextGrain = 0;
    limiterEnvelope = 0.0f;
    for (auto& g : grains)
        g.active = false;
}

float GranularDelay::readInterpolated(const std::vector<float>& buf, double position)
{
    const int size = (int) buf.size();
    if (size <= 0)
        return 0.0f;

    double pos = std::fmod(position, (double) size);
    if (pos < 0.0)
        pos += size;

    const int idx0 = (int) pos;
    const int idx1 = (idx0 + 1) % size;
    const float frac = (float) (pos - idx0);
    return buf[(size_t) idx0] + frac * (buf[(size_t) idx1] - buf[(size_t) idx0]);
}

void GranularDelay::spawnGrain()
{
    int slot = -1;
    for (int i = 0; i < maxGrains; ++i)
    {
        if (!grains[(size_t) i].active)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return;

    auto& g = grains[(size_t) slot];

    const float spray = (random.nextFloat() * 2.0f - 1.0f) * spreadMs.load() * 0.001f * (float) sampleRate;
    double basePos = (double) writePos - (double) delayMs.load() * 0.001 * sampleRate + (double) spray;
    basePos = std::fmod(basePos, (double) bufferSize);
    if (basePos < 0.0)
        basePos += bufferSize;
    g.readPos = basePos;

    const float semis = (random.nextFloat() * 2.0f - 1.0f) * pitchScatter.load();
    g.pitchRatio = std::pow(2.0, (double) semis / 12.0);
    g.length = juce::jmax(64, (int) (grainSizeMs.load() * 0.001f * (float) sampleRate));
    g.age = 0;
    g.active = true;

    const float panRand = random.nextFloat();
    g.panL = 1.0f - panRand * 0.6f;
    g.panR = 0.4f + panRand * 0.6f;
}

void GranularDelay::process(float* const* channelData, int numChannels, int numSamples)
{
    if (bufferSize <= 0 || numChannels <= 0)
        return;

    const float fb = feedback.load();
    const float mix = wetMix.load();

    // Compensate grain amplitude by expected concurrent overlap (density *
    // grain duration) rather than a flat attenuation: at sparse settings
    // (the common case - a handful of grains per second) overlap is well
    // under 1, so grains need close to full amplitude to be audible at all;
    // at dense settings many grains stack up, so they're scaled down to
    // avoid the sum building into a wash/clipping. A straight 1/sqrt curve
    // (equal-power assumption) turned out too conservative at the dense end
    // - cranking density/grain size for a dramatic wash made the effect
    // quieter instead of bigger. A gentler 1/x^0.32 curve plus a modest
    // fixed makeup gain keeps the anti-buildup protection but leaves
    // "drastic" settings sounding powerful rather than timid.
    const float expectedOverlap = juce::jmax(1.0f, density.load() * (grainSizeMs.load() * 0.001f));
    constexpr float makeupGain = 1.25f;
    const float grainNormFactor = makeupGain / std::pow(expectedOverlap, 0.32f);

    // Limiter ceiling and time constants for the output-stage safety net
    // below - fast enough attack to catch a single hot grain cluster
    // within a couple of samples, slow enough release that it doesn't
    // pump audibly on every grain.
    constexpr float ceiling = 0.98f;
    const float attackCoeff = (float) std::exp(-1.0 / (0.001 * sampleRate));  // ~1ms
    const float releaseCoeff = (float) std::exp(-1.0 / (0.100 * sampleRate)); // ~100ms

    for (int i = 0; i < numSamples; ++i)
    {
        float wetL = 0.0f, wetR = 0.0f;

        for (auto& g : grains)
        {
            if (!g.active)
                continue;

            const float t = (float) g.age / (float) g.length;
            const float env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * t));
            const float sL = readInterpolated(bufferL, g.readPos);
            const float sR = readInterpolated(bufferR, g.readPos);
            wetL += sL * env * g.panL;
            wetR += sR * env * g.panR;

            g.readPos += g.pitchRatio;
            if (g.readPos >= bufferSize)
                g.readPos -= bufferSize;

            ++g.age;
            if (g.age >= g.length)
                g.active = false;
        }

        wetL *= grainNormFactor;
        wetR *= grainNormFactor;

        const float inL = channelData[0][i];
        const float inR = numChannels > 1 ? channelData[1][i] : inL;

        // grainNormFactor compensates for the *expected* (density-averaged)
        // overlap, but grain spawn timing is randomised - several grains
        // can statistically cluster and momentarily read the same hot
        // buffer region, spiking the real overlap well above the average.
        // With feedback high, that spike re-enters the buffer amplified,
        // gets read by the next cluster, and compounds - an exponential
        // runaway that (per a live user report) can blow up fast enough to
        // hang the host. Saturating just the fed-back portion caps each
        // round-trip's injected energy regardless of how hot a spike gets,
        // which breaks the runaway mechanism outright rather than relying
        // on the average-case gain math to always be conservative enough.
        bufferL[(size_t) writePos] = inL + std::tanh(wetL * fb);
        bufferR[(size_t) writePos] = inR + std::tanh(wetR * fb);
        writePos = (writePos + 1) % bufferSize;

        float outL = inL + wetL * mix;
        float outR = inR + wetR * mix;

        // Output-stage limiter: catches the case the feedback-tap
        // saturation above doesn't - a hot grain cluster spiking the
        // *direct* wet contribution (not just what re-enters the delay
        // buffer) on a single pass, before feedback even has a chance to
        // build it up. Envelope-follows the combined output's peak and
        // pulls gain down only once it crosses the ceiling, so normal
        // levels are completely untouched.
        const float peak = juce::jmax(std::abs(outL), std::abs(outR));
        const float coeff = peak > limiterEnvelope ? attackCoeff : releaseCoeff;
        limiterEnvelope = peak + coeff * (limiterEnvelope - peak);
        if (limiterEnvelope > ceiling)
        {
            const float reduction = ceiling / limiterEnvelope;
            outL *= reduction;
            outR *= reduction;
        }

        channelData[0][i] = outL;
        if (numChannels > 1)
            channelData[1][i] = outR;

        if (--samplesUntilNextGrain <= 0)
        {
            spawnGrain();
            const float d = juce::jmax(0.5f, density.load());
            samplesUntilNextGrain = (int) (sampleRate / d);
        }
    }
}
