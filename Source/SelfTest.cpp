#include "SelfTest.h"
#include "Audio/AudioFileLoader.h"
#include "Audio/StretchAudioSource.h"
#include "Audio/EffectsChain.h"
#include "Audio/SmudgeProcessor.h"
#include "Audio/GranularDelay.h"
#include "Audio/FreqShifter.h"
#include "Audio/LossyProcessor.h"
#include "Export/ExportEngine.h"

namespace
{
    void writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        file.deleteFile();
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
        if (stream == nullptr)
        {
            juce::Logger::writeToLog("selftest: could not open output stream for " + file.getFullPathName());
            return;
        }

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 16, {}, 0));

        if (writer == nullptr)
        {
            juce::Logger::writeToLog("selftest: could not create WAV writer for " + file.getFullPathName());
            return;
        }

        stream.release(); // writer now owns the stream
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        juce::Logger::writeToLog("selftest: wrote " + file.getFullPathName());
    }

    void renderCase(StretchAudioSource& src, double sourceSampleRate, const juce::File& outputDir,
                     const juce::String& label, bool linked, double semitones, double speedPercent,
                     StretchAudioSource::WarpMode mode, double renderSeconds)
    {
        src.setLinked(linked);
        src.setPitchSemitones(semitones);
        src.setSpeedPercent(speedPercent);
        if (!linked)
            src.setWarpMode(mode);

        src.setNextReadPosition(0);
        src.play();

        const int totalSamples = (int) (sourceSampleRate * renderSeconds);
        juce::AudioBuffer<float> outBuf(2, totalSamples);
        outBuf.clear();

        int rendered = 0;
        const int blockSize = 512;
        while (rendered < totalSamples)
        {
            const int thisBlock = juce::jmin(blockSize, totalSamples - rendered);
            juce::AudioSourceChannelInfo info(&outBuf, rendered, thisBlock);
            src.getNextAudioBlock(info);
            rendered += thisBlock;
        }

        writeWav(outputDir.getChildFile(label + ".wav"), outBuf, sourceSampleRate);
    }
}

void runSelfTest(const juce::File& inputFile, const juce::File& outputDir)
{
    juce::Logger::writeToLog("selftest: starting, input=" + inputFile.getFullPathName());

    outputDir.createDirectory();

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto loaded = AudioFileLoader::load(inputFile, formatManager);
    if (!loaded.success)
    {
        juce::Logger::writeToLog("selftest: FAILED to load input file");
        return;
    }

    StretchAudioSource src;
    src.setBuffer(loaded.buffer, loaded.sourceSampleRate);

    using WM = StretchAudioSource::WarpMode;

    renderCase(src, loaded.sourceSampleRate, outputDir, "repitch_up12", true, 12.0, 100.0, WM::Complex, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "repitch_down12", true, -12.0, 100.0, WM::Complex, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_up7_speed100", false, 7.0, 100.0, WM::Complex, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_pitch0_speed50", false, 0.0, 50.0, WM::Complex, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_pitch0_speed200", false, 0.0, 200.0, WM::Complex, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_beats_up5", false, 5.0, 100.0, WM::Beats, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_complexpro_up3", false, 3.0, 100.0, WM::ComplexPro, 3.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "warp_extreme_slow2pct", false, 0.0, 2.0, WM::Complex, 1.0);
    renderCase(src, loaded.sourceSampleRate, outputDir, "paulstretch_10pct", false, 0.0, 10.0, WM::Paulstretch, 3.0);

    {
        // Linked mode: moving Speed should now drive Pitch too (previously
        // only Pitch drove the audio while linked; Speed had no effect).
        src.setLinked(true);
        src.setPitchSemitones(0.0);
        src.setSpeedPercent(50.0);
        juce::Logger::writeToLog("linktest: after setSpeedPercent(50) pitch="
            + juce::String(src.getPitchSemitones(), 3) + " (expect -12.000)");
        src.setPitchSemitones(12.0);
        juce::Logger::writeToLog("linktest: after setPitchSemitones(12) speed="
            + juce::String(src.getSpeedPercent(), 3) + " (expect 200.000)");
    }

    {
        // Loop region: 0.5s-1.5s of the 4s source, looped, rendered for 3s of
        // output (i.e. past the loop-out point at least once) at unity
        // pitch/speed via Re-Pitch so the loop is the only thing under test.
        src.setLinked(true);
        src.setPitchSemitones(0.0);
        src.setSpeedPercent(100.0);
        src.setLoopRegion((juce::int64) (0.5 * loaded.sourceSampleRate), (juce::int64) (1.5 * loaded.sourceSampleRate));
        src.setLooping(true);
        src.setNextReadPosition((juce::int64) (0.5 * loaded.sourceSampleRate));
        src.play();

        const int totalSamples = (int) (loaded.sourceSampleRate * 3.0);
        juce::AudioBuffer<float> loopBuf(2, totalSamples);
        loopBuf.clear();
        int rendered = 0;
        while (rendered < totalSamples)
        {
            const int thisBlock = juce::jmin(512, totalSamples - rendered);
            juce::AudioSourceChannelInfo info(&loopBuf, rendered, thisBlock);
            src.getNextAudioBlock(info);
            rendered += thisBlock;
        }
        writeWav(outputDir.getChildFile("loop_region.wav"), loopBuf, loaded.sourceSampleRate);
        src.setLooping(false);
        src.setLoopRegion(0, -1);
    }

    {
        StretchAudioSource eqSrc;
        eqSrc.setBuffer(loaded.buffer, loaded.sourceSampleRate);
        eqSrc.setLinked(true);
        eqSrc.setPitchSemitones(0.0);
        eqSrc.play();

        EffectsChain fx(eqSrc);
        fx.prepareToPlay(512, loaded.sourceSampleRate);

        auto renderRms = [&](int numSamples) -> float
        {
            juce::AudioBuffer<float> buf(2, numSamples);
            buf.clear();
            int rendered = 0;
            while (rendered < numSamples)
            {
                const int block = juce::jmin(512, numSamples - rendered);
                juce::AudioSourceChannelInfo info(&buf, rendered, block);
                fx.getNextAudioBlock(info);
                rendered += block;
            }
            return buf.getRMSLevel(0, 0, numSamples);
        };

        const int oneSecond = (int) loaded.sourceSampleRate;

        eqSrc.setNextReadPosition(0);
        const float baseline = renderRms(oneSecond);

        fx.setEqBandEnabled(0, true);
        fx.setEqBandType(0, EffectsChain::FilterType::LowPass);
        fx.setEqBandFrequency(0, 200.0f);
        fx.setEqBandQ(0, 0.707f);
        juce::Logger::writeToLog("eqtest: band0 enabled=" + juce::String((int) fx.getEqBand(0).enabled.load())
            + " magAt440=" + juce::String(fx.getMagnitudeResponseDb(440.0f), 2) + "dB (expect strongly negative)");
        eqSrc.setNextReadPosition(0);
        const float lowpassed = renderRms(oneSecond);
        juce::Logger::writeToLog("eqtest: baseline_rms=" + juce::String(baseline, 4)
            + " lowpass200_rms=" + juce::String(lowpassed, 4) + " (expect lowpass much smaller - 440Hz tone above cutoff)");
        fx.setEqBandEnabled(0, false);

        fx.setEqBandEnabled(1, true);
        fx.setEqBandType(1, EffectsChain::FilterType::HighPass);
        fx.setEqBandFrequency(1, 2000.0f);
        fx.setEqBandQ(1, 0.707f);
        eqSrc.setNextReadPosition(0);
        const float highpassed = renderRms(oneSecond);
        juce::Logger::writeToLog("eqtest: highpass2000_rms=" + juce::String(highpassed, 4)
            + " (expect much smaller than baseline - 440Hz tone below cutoff)");
        fx.setEqBandEnabled(1, false);

        fx.setDelayEnabled(true);
        fx.setDelayTimeMs(100.0f);
        fx.setDelayFeedback(0.5f);
        fx.setDelayMix(0.5f);
        eqSrc.setNextReadPosition(0);
        const float delayed = renderRms(oneSecond);
        juce::Logger::writeToLog("delaytest: rms=" + juce::String(delayed, 4)
            + " baseline=" + juce::String(baseline, 4) + " (expect different from baseline)");
        fx.setDelayEnabled(false);

        fx.setReverbEnabled(true);
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.8f; rp.wetLevel = 0.5f; rp.dryLevel = 0.5f; rp.damping = 0.3f; rp.width = 1.0f;
        fx.setReverbParams(rp);
        eqSrc.setNextReadPosition(0);
        const float reverbed = renderRms(oneSecond);
        juce::Logger::writeToLog("reverbtest: rms=" + juce::String(reverbed, 4)
            + " baseline=" + juce::String(baseline, 4) + " (expect different from baseline, no crash/NaN)");
        fx.setReverbEnabled(false);

        fx.setOutputGainDb(6.0f);
        eqSrc.setNextReadPosition(0);
        const float gained = renderRms(oneSecond);
        juce::Logger::writeToLog("gaintest: +6dB rms=" + juce::String(gained, 4)
            + " baseline=" + juce::String(baseline, 4)
            + " ratio=" + juce::String(gained / baseline, 3) + " (expect ~2.0 - +6dB is ~2x amplitude)");
        fx.setOutputGainDb(0.0f);

        fx.setInputGainDb(-6.0f);
        eqSrc.setNextReadPosition(0);
        const float attenuated = renderRms(oneSecond);
        juce::Logger::writeToLog("gaintest: -6dB rms=" + juce::String(attenuated, 4)
            + " ratio=" + juce::String(attenuated / baseline, 3) + " (expect ~0.5)");
        fx.setInputGainDb(0.0f);

        // Spectrum peak tracking: after rendering a loud steady tone, the
        // tracked peak should sit close to that tone's actual level rather
        // than an arbitrary fixed value.
        eqSrc.setNextReadPosition(0);
        renderRms(oneSecond);
        const auto expectedPeakDb = juce::Decibels::gainToDecibels(0.4f); // test tone amplitude
        juce::Logger::writeToLog("spectrumtest: trackedPeakDb=" + juce::String(fx.getSpectrumPeakDb(), 2)
            + " (expect roughly " + juce::String(expectedPeakDb, 2) + ", within a few dB)");

        // Pause bug: after a loud passage, extended silence used to let the
        // peak tracker decay unbounded, which eventually made the flat
        // silent spectrum climb back up to the top of the display instead
        // of staying collapsed at the bottom. Render well past the point
        // where the old unbounded decay would have inverted the peak-
        // relative normalisation (crossing below -40dB) and confirm the
        // tracked peak holds its floor and the displayed bins stay low.
        eqSrc.pause();
        renderRms((int) (10.0 * loaded.sourceSampleRate)); // ~10s of silence
        const float silentPeakDb = fx.getSpectrumPeakDb();
        const auto silentSpectrum = fx.getLatestSpectrum();
        float maxNormalisedBin = 0.0f;
        constexpr float displayRangeDb = 60.0f;
        const float floorDb = silentPeakDb - displayRangeDb;
        for (float bin : silentSpectrum)
            maxNormalisedBin = juce::jmax(maxNormalisedBin, (bin - floorDb) / displayRangeDb);
        juce::Logger::writeToLog("spectrumtest: afterSilence peakDb=" + juce::String(silentPeakDb, 2)
            + " maxNormalisedBin=" + juce::String(maxNormalisedBin, 3)
            + " (expect peakDb clamped >= -40 and maxNormalisedBin near/below 0 - not pinned near 1.0 at the top)");

        // Declick test: toggle delay and reverb on/off mid-stream (delay
        // with enough feedback to build up real energy first, so the
        // toggle-off has something to abruptly cut if it were still
        // hard-gated) and compare the worst sample-to-sample jump near each
        // toggle instant against the worst jump anywhere else in the same
        // render. Before the enabled-gain crossfade fix, toggling caused a
        // hard step in the output - a jump far larger than the signal's own
        // continuous variation. After the fix they should be comparable.
        fx.setDelayTimeMs(150.0f);
        fx.setDelayFeedback(0.7f);
        fx.setDelayMix(0.6f);
        fx.setDelayDensity(20.0f);
        juce::dsp::Reverb::Parameters declickRp;
        declickRp.roomSize = 0.9f; declickRp.wetLevel = 0.7f; declickRp.dryLevel = 0.3f; declickRp.damping = 0.2f; declickRp.width = 1.0f;
        fx.setReverbParams(declickRp);

        eqSrc.setNextReadPosition(0);
        const int declickTotal = (int) (3.0 * loaded.sourceSampleRate);
        juce::AudioBuffer<float> declickBuf(2, declickTotal);
        declickBuf.clear();

        std::vector<int> toggleSamples;
        int rendered = 0;
        while (rendered < declickTotal)
        {
            const double tSec = (double) rendered / loaded.sourceSampleRate;
            if (tSec >= 0.5 && !fx.isDelayEnabled()) { fx.setDelayEnabled(true); toggleSamples.push_back(rendered); }
            if (tSec >= 1.5 && fx.isDelayEnabled()) { fx.setDelayEnabled(false); toggleSamples.push_back(rendered); }
            if (tSec >= 2.0 && !fx.isReverbEnabled()) { fx.setReverbEnabled(true); toggleSamples.push_back(rendered); }
            if (tSec >= 2.5 && fx.isReverbEnabled()) { fx.setReverbEnabled(false); toggleSamples.push_back(rendered); }

            const int block = juce::jmin(512, declickTotal - rendered);
            juce::AudioSourceChannelInfo info(&declickBuf, rendered, block);
            fx.getNextAudioBlock(info);
            rendered += block;
        }
        fx.setDelayEnabled(false);
        fx.setReverbEnabled(false);

        auto* data = declickBuf.getReadPointer(0);
        const int declickWindow = (int) (0.02 * loaded.sourceSampleRate); // 20ms either side
        float maxJumpNearToggle = 0.0f, maxJumpElsewhere = 0.0f;
        for (int i = 1; i < declickTotal; ++i)
        {
            const float jump = std::abs(data[i] - data[i - 1]);
            bool nearToggle = false;
            for (int t : toggleSamples)
                if (std::abs(i - t) <= declickWindow) { nearToggle = true; break; }
            if (nearToggle)
                maxJumpNearToggle = juce::jmax(maxJumpNearToggle, jump);
            else
                maxJumpElsewhere = juce::jmax(maxJumpElsewhere, jump);
        }
        juce::Logger::writeToLog("declicktest: maxJumpNearToggle=" + juce::String(maxJumpNearToggle, 5)
            + " maxJumpElsewhere=" + juce::String(maxJumpElsewhere, 5)
            + " ratio=" + juce::String(maxJumpNearToggle / juce::jmax(1.0e-6f, maxJumpElsewhere), 2)
            + " (expect ratio close to 1 - a hard-gated toggle would spike far above baseline)");

        // Drive/Compression bus glue: bypass (both at 0) must be transparent
        // - the new stage shouldn't touch anything by default. Drive should
        // keep the signal's peak bounded even pushed to maximum (the tanh
        // waveshaper's whole point). Compression, on a signal that sits
        // above its fixed -18dB threshold, should measurably reduce peak
        // level relative to bypass even after its own +6dB makeup gain -
        // a pure makeup-gain bug (no real gain reduction) would leave peak
        // unchanged or louder instead of quieter.
        auto renderPeak = [&](int numSamples) -> float
        {
            juce::AudioBuffer<float> buf(2, numSamples);
            buf.clear();
            int r = 0;
            while (r < numSamples)
            {
                const int block = juce::jmin(512, numSamples - r);
                juce::AudioSourceChannelInfo info(&buf, r, block);
                fx.getNextAudioBlock(info);
                r += block;
            }
            return buf.getMagnitude(0, 0, numSamples);
        };

        // eqSrc was left paused by the spectrum pause-bug test above and
        // never resumed - without this, every render below reads back
        // silence and the assertions pass vacuously (0.0 vs 0.0).
        eqSrc.play();

        fx.setDriveDb(0.0f);
        fx.setCompAmount(0.0f);
        eqSrc.setNextReadPosition(0);
        const float bypassRms = renderRms(oneSecond);
        juce::Logger::writeToLog("drivecomptest: bypass_rms=" + juce::String(bypassRms, 4)
            + " baseline=" + juce::String(baseline, 4) + " (expect ~identical - bypass must be transparent)");

        eqSrc.setNextReadPosition(0);
        const float baselinePeak = renderPeak(oneSecond);

        fx.setDriveDb(15.0f);
        eqSrc.setNextReadPosition(0);
        const float drivePeak = renderPeak(oneSecond);
        juce::Logger::writeToLog("drivecomptest: baselinePeak=" + juce::String(baselinePeak, 4)
            + " maxDrivePeak=" + juce::String(drivePeak, 4)
            + " (expect bounded near/below ~1.0, not blowing up past it)");
        fx.setDriveDb(0.0f);

        // Compression runs at a fixed aggressive setting (OTT-style) with
        // an RMS-style detector and both downward (loud->down) and upward
        // (quiet->up) gain, and the knob is purely a dry/wet blend. The
        // concrete complaint that motivated this over the first cut (a
        // peak-detector, downward-only version) was "doesn't do much" -
        // the real test of that isn't a level check on a single steady
        // tone, it's whether the gap between a loud passage and a quiet
        // one actually shrinks. Build a synthetic loud-then-quiet tone and
        // check the loud/quiet RMS ratio collapses at full compAmt, and
        // that the quiet half's own RMS rises (confirming the upward side
        // is doing real work, not just the loud half getting squashed).
        {
            const double sr = loaded.sourceSampleRate;
            const int halfSamples = (int) (0.5 * sr);
            juce::AudioBuffer<float> dynBuf(2, halfSamples * 2);
            for (int i = 0; i < halfSamples; ++i)
            {
                const float loud = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);
                dynBuf.setSample(0, i, loud);
                dynBuf.setSample(1, i, loud);
            }
            for (int i = 0; i < halfSamples; ++i)
            {
                const float quiet = 0.03f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);
                dynBuf.setSample(0, halfSamples + i, quiet);
                dynBuf.setSample(1, halfSamples + i, quiet);
            }

            eqSrc.setBuffer(dynBuf, sr);
            eqSrc.setLinked(true);
            eqSrc.setPitchSemitones(0.0);
            eqSrc.play();

            // eqSrc.play() every time: a render that lands exactly on the
            // buffer's end (as the quiet-half render below does, being the
            // last halfSamples of an exactly-2*halfSamples buffer) flips
            // playing() to false internally (see StretchAudioSource::
            // renderRePitch's non-looping end-of-buffer handling) -
            // setNextReadPosition() alone doesn't revive it, which
            // silently zeroed out every render after the first two here.
            auto rmsOfHalf = [&](bool loudHalf) -> float
            {
                eqSrc.setNextReadPosition(loudHalf ? 0 : halfSamples);
                eqSrc.play();
                return renderRms(halfSamples);
            };

            fx.setCompAmount(0.0f);
            const float bypassLoudRms = rmsOfHalf(true);
            const float bypassQuietRms = rmsOfHalf(false);

            fx.setCompAmount(1.0f);
            const float compLoudRms = rmsOfHalf(true);
            const float compQuietRms = rmsOfHalf(false);

            const float bypassRatio = bypassLoudRms / juce::jmax(1.0e-6f, bypassQuietRms);
            const float compRatio = compLoudRms / juce::jmax(1.0e-6f, compQuietRms);

            juce::Logger::writeToLog("drivecomptest: dynamicRange bypassRatio=" + juce::String(bypassRatio, 2)
                + " compRatio=" + juce::String(compRatio, 2)
                + " quietRms bypass=" + juce::String(bypassQuietRms, 4) + " comp=" + juce::String(compQuietRms, 4)
                + " (expect compRatio well below bypassRatio - the loud/quiet gap shrinking - and"
                + " compQuietRms clearly above bypassQuietRms - upward compression actually lifting"
                + " the quiet half, not just the loud half getting squashed)");

            fx.setCompAmount(0.0f);
        }
    }

    {
        // Export pipeline: render pitch+7st/100% through each output format,
        // then decode each back with the app's own AudioFormatManager (which
        // includes MP3 read support) and re-save as plain WAV, so the pitch
        // can be verified post-encode with the same zero-crossing check used
        // for the live-playback cases above.
        ExportEngine::Settings settings;
        settings.linked = false;
        settings.pitchSemitones = 7.0;
        settings.speedPercent = 100.0;
        settings.warpMode = WM::Complex;
        settings.mp3BitrateKbps = 256;

        struct FormatCase { ExportEngine::Format format; const char* ext; };
        const FormatCase cases[] = {
            { ExportEngine::Format::WAV, "wav" },
            { ExportEngine::Format::AIFF, "aiff" },
            { ExportEngine::Format::FLAC, "flac" },
            { ExportEngine::Format::MP3, "mp3" },
        };

        for (auto& c : cases)
        {
            settings.format = c.format;
            const auto outFile = outputDir.getChildFile(juce::String("export_test.") + c.ext);
            juce::String exportError;

            const bool ok = ExportEngine::exportToFile(loaded.buffer, loaded.sourceSampleRate, settings,
                                                        outFile, nullptr, exportError);
            if (!ok)
            {
                juce::Logger::writeToLog(juce::String("exporttest: FAILED (") + c.ext + "): " + exportError);
                continue;
            }
            juce::Logger::writeToLog(juce::String("exporttest: wrote ") + outFile.getFullPathName()
                                      + " (" + juce::String(outFile.getSize()) + " bytes)");

            auto decoded = AudioFileLoader::load(outFile, formatManager);
            if (!decoded.success)
            {
                juce::Logger::writeToLog(juce::String("exporttest: FAILED to decode back (") + c.ext + ")");
                continue;
            }

            writeWav(outputDir.getChildFile(juce::String("roundtrip_") + c.ext + ".wav"),
                     decoded.buffer, decoded.sourceSampleRate);
        }
    }

    {
        const double sr = 44100.0;
        auto rmsOfRange = [](const std::vector<float>& v, int start, int len) -> float
        {
            double sum = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double s = (double) v[(size_t) (start + i)];
                sum += s * s;
            }
            return (float) std::sqrt(sum / (double) juce::jmax(1, len));
        };

        const int toneSamples = (int) (1.0 * sr);
        const int silenceSamples = (int) (1.0 * sr);
        const int total = toneSamples + silenceSamples;
        std::vector<float> in((size_t) total, 0.0f);
        for (int i = 0; i < toneSamples; ++i)
            in[(size_t) i] = 0.4f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);
        std::vector<float> out((size_t) total, 0.0f);

        SmudgeProcessor smudge;
        smudge.prepare();

        smudge.setAmount(0.0f);
        smudge.process(in.data(), out.data(), total);
        const float dryRms = rmsOfRange(in, 0, toneSamples);
        const float passthroughRms = rmsOfRange(out, 4096, toneSamples - 8192);
        juce::Logger::writeToLog("smudgetest: amount0 passthroughRms=" + juce::String(passthroughRms, 4)
            + " dryRms=" + juce::String(dryRms, 4) + " (expect close - transparent at amount=0)");

        smudge.setAmount(0.9f);
        smudge.reset();
        std::fill(out.begin(), out.end(), 0.0f);
        smudge.process(in.data(), out.data(), total);
        const float silenceRegionRms = rmsOfRange(out, toneSamples + 2048, silenceSamples - 4096);
        juce::Logger::writeToLog("smudgetest: amount0.9 silenceRegionRms=" + juce::String(silenceRegionRms, 5)
            + " (expect clearly nonzero - spectral content smeared past the tone's end)");
    }

    {
        // Simulates a knob being dragged continuously: change the window
        // size every ~5ms (far more often than a real drag would fire, to
        // stress-test worst case) while a steady tone plays through, and
        // check for hard discontinuities (a click is a huge sample-to-sample
        // jump) rather than the previous behaviour where every change would
        // instantly wipe all STFT state mid-stream.
        const double sr = 44100.0;
        const int totalSamples = (int) (2.0 * sr);
        std::vector<float> in((size_t) totalSamples), out((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
            in[(size_t) i] = 0.4f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);

        SmudgeProcessor smudge;
        smudge.prepare();
        smudge.setAmount(0.6f);

        const int chunk = 220; // ~5ms at 44.1kHz
        juce::Random rng(99);
        for (int pos = 0; pos < totalSamples; pos += chunk)
        {
            const int n = juce::jmin(chunk, totalSamples - pos);
            const int newSize = 512 << rng.nextInt(5); // 512..8192
            smudge.setWindowSizeSamples(newSize);
            smudge.process(in.data() + pos, out.data() + pos, n);
        }

        float maxJump = 0.0f;
        int maxJumpIndex = -1;
        for (int i = 1; i < totalSamples; ++i)
        {
            const float jump = std::abs(out[(size_t) i] - out[(size_t) (i - 1)]);
            if (jump > maxJump) { maxJump = jump; maxJumpIndex = i; }
        }
        juce::Logger::writeToLog("smudgetest: declick maxSampleJump=" + juce::String(maxJump, 5)
            + " atSample=" + juce::String(maxJumpIndex)
            + " (expect small - a hard reset would show a jump near the full amplitude, ~0.4-0.8)");
    }

    {
        // Feedback pushes the smear into more resonant/extreme territory on
        // top of Amount's spectral hold. A steady continuous tone gives it
        // nothing new to reinforce (the held spectrum is already just that
        // tone, feedback or not) - same trick as the Amount test above: a
        // burst then silence, so any *persisted/reinforced* energy in the
        // silence region is attributable to feedback specifically. Also
        // checks it stays bounded (no NaN/inf/runaway) with both maxed.
        const double sr = 44100.0;
        const int toneSamples = (int) (0.5 * sr);
        const int totalSamples = (int) (2.0 * sr);
        std::vector<float> in((size_t) totalSamples, 0.0f);
        for (int i = 0; i < toneSamples; ++i)
            in[(size_t) i] = 0.4f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);

        std::vector<float> outNoFeedback((size_t) totalSamples), outWithFeedback((size_t) totalSamples);

        SmudgeProcessor smudgeA;
        smudgeA.prepare();
        smudgeA.setAmount(0.9f);
        smudgeA.setFeedback(0.0f);
        smudgeA.process(in.data(), outNoFeedback.data(), totalSamples);

        SmudgeProcessor smudgeB;
        smudgeB.prepare();
        smudgeB.setAmount(0.9f);
        smudgeB.setFeedback(0.9f);
        smudgeB.process(in.data(), outWithFeedback.data(), totalSamples);

        double sumNo = 0.0, sumWith = 0.0;
        bool anyNonFinite = false;
        const int silenceStart = toneSamples + 2048;
        for (int i = silenceStart; i < totalSamples; ++i)
        {
            sumNo += (double) outNoFeedback[(size_t) i] * outNoFeedback[(size_t) i];
            sumWith += (double) outWithFeedback[(size_t) i] * outWithFeedback[(size_t) i];
            if (!std::isfinite(outWithFeedback[(size_t) i]))
                anyNonFinite = true;
        }
        const int silenceLen = totalSamples - silenceStart;
        const float rmsNo = (float) std::sqrt(sumNo / (double) silenceLen);
        const float rmsWith = (float) std::sqrt(sumWith / (double) silenceLen);
        juce::Logger::writeToLog("smudgetest: silenceRegion feedback0.0Rms=" + juce::String(rmsNo, 5)
            + " feedback0.9Rms=" + juce::String(rmsWith, 5)
            + " anyNonFinite=" + juce::String((int) anyNonFinite)
            + " (expect feedback0.9 clearly different/louder, and anyNonFinite=0 - bounded even maxed)");
    }

    {
        // Lossy (bitcrusher/downsampler): mix=0 must be an exact bypass
        // (not just "close", since the dry/wet blend at mix=0 is a literal
        // 100% dry weight with no crushing contribution at all). At the
        // transparent end of its range (16 bits, a target rate at/above
        // Nyquist so every sample is "fresh") it should track the input
        // closely. At the extreme end (1 bit, 200Hz hold rate) it should
        // read as clearly, measurably different - the entire point of the
        // effect - while staying bounded.
        const double sr = 44100.0;
        const int totalSamples = (int) (0.5 * sr);
        std::vector<float> in((size_t) totalSamples), out((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
            in[(size_t) i] = 0.4f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);

        LossyProcessor lossy;
        lossy.prepare(sr);

        lossy.setMix(0.0f);
        lossy.process(in.data(), out.data(), totalSamples);
        float maxDiffAtMix0 = 0.0f;
        for (int i = 0; i < totalSamples; ++i)
            maxDiffAtMix0 = juce::jmax(maxDiffAtMix0, std::abs(out[(size_t) i] - in[(size_t) i]));
        juce::Logger::writeToLog("lossytest: mix0 maxDiffFromDry=" + juce::String(maxDiffAtMix0, 6)
            + " (expect ~0 - mix=0 must be an exact bypass)");

        lossy.reset();
        lossy.setMix(1.0f);
        lossy.setBits(16.0f);
        lossy.setRateHz(48000.0f);
        std::fill(out.begin(), out.end(), 0.0f);
        lossy.process(in.data(), out.data(), totalSamples);
        double sumSqDiffTransparent = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            const double d = (double) out[(size_t) i] - (double) in[(size_t) i];
            sumSqDiffTransparent += d * d;
        }
        const float transparentDiffRms = (float) std::sqrt(sumSqDiffTransparent / (double) totalSamples);
        juce::Logger::writeToLog("lossytest: transparent(16bit/48kHz) diffRms=" + juce::String(transparentDiffRms, 5)
            + " (expect small relative to the 0.4-amplitude tone - close to the dry signal)");

        lossy.reset();
        lossy.setBits(1.0f);
        lossy.setRateHz(200.0f);
        std::fill(out.begin(), out.end(), 0.0f);
        lossy.process(in.data(), out.data(), totalSamples);
        double sumSqDiffExtreme = 0.0;
        bool anyNonFinite = false;
        float maxAbsExtreme = 0.0f;
        for (int i = 0; i < totalSamples; ++i)
        {
            const double d = (double) out[(size_t) i] - (double) in[(size_t) i];
            sumSqDiffExtreme += d * d;
            if (!std::isfinite(out[(size_t) i]))
                anyNonFinite = true;
            maxAbsExtreme = juce::jmax(maxAbsExtreme, std::abs(out[(size_t) i]));
        }
        const float extremeDiffRms = (float) std::sqrt(sumSqDiffExtreme / (double) totalSamples);
        juce::Logger::writeToLog("lossytest: extreme(1bit/200Hz) diffRms=" + juce::String(extremeDiffRms, 5)
            + " maxAbsSample=" + juce::String(maxAbsExtreme, 4) + " anyNonFinite=" + juce::String((int) anyNonFinite)
            + " (expect diffRms clearly larger than the transparent case above, bounded, anyNonFinite=0)");
    }

    {
        // A long burst (200ms) is wider than the grain spawn spacing (50ms at
        // density=20/s), so several grains are statistically guaranteed to land
        // inside it once the delay-tap read position sweeps across it - this
        // avoids relying on any single grain's random spray landing exactly
        // right. The measurement window targets that first, un-decayed delay
        // tap (writePos - delaySamples in burst range => t in [delayMs, delayMs
        // + burstMs)), not the feedback tail, which attenuates by roughly
        // feedback*grainNormFactor per 50ms round-trip and is inaudible within
        // a couple of hops.
        const double sr = 44100.0;
        GranularDelay gran;
        gran.prepare(sr);
        gran.setDelayMs(300.0f);
        gran.setGrainSizeMs(80.0f);
        gran.setDensity(20.0f);
        gran.setSpreadMs(10.0f);
        gran.setPitchScatterSemitones(2.0f);
        gran.setFeedback(0.5f);
        gran.setMix(0.9f);

        const int burstSamples = (int) (0.2 * sr);
        const int totalSamples = (int) (1.5 * sr);
        std::vector<float> L((size_t) totalSamples, 0.0f), R((size_t) totalSamples, 0.0f);
        for (int i = 0; i < burstSamples; ++i)
        {
            const float s = 0.7f * std::sin(2.0f * juce::MathConstants<float>::pi * 880.0f * (float) i / (float) sr);
            L[(size_t) i] = s;
            R[(size_t) i] = s;
        }

        float* chans[2] = { L.data(), R.data() };
        gran.process(chans, 2, totalSamples);

        auto rmsOfRange = [](const std::vector<float>& v, int start, int len) -> float
        {
            double sum = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double s = (double) v[(size_t) (start + i)];
                sum += s * s;
            }
            return (float) std::sqrt(sum / (double) juce::jmax(1, len));
        };
        const int tapStart = (int) (0.31 * sr);
        const int tapLen = (int) (0.18 * sr);
        const float tapRms = rmsOfRange(L, tapStart, tapLen);
        juce::Logger::writeToLog("granulartest: primaryTapRms=" + juce::String(tapRms, 5)
            + " (expect clearly nonzero - delayed grains reading the burst back at ~delayMs)");
    }

    {
        // Reproduce the user-reported "no audible effect" case with the exact
        // knob values from their screenshot, on a sustained tone (not a
        // burst) since that's closer to real program material.
        const double sr = 44100.0;
        GranularDelay gran;
        gran.prepare(sr);
        gran.setDelayMs(1617.0f);
        gran.setGrainSizeMs(6.0f);
        gran.setDensity(0.7f);
        gran.setSpreadMs(250.0f);
        gran.setPitchScatterSemitones(13.6f);
        gran.setFeedback(0.95f);
        gran.setMix(1.0f);

        const int totalSamples = (int) (5.0 * sr);
        std::vector<float> L((size_t) totalSamples), R((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            const float s = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr);
            L[(size_t) i] = s;
            R[(size_t) i] = s;
        }
        std::vector<float> dry = L;

        float* chans[2] = { L.data(), R.data() };
        gran.process(chans, 2, totalSamples);

        double diffSum = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            const double d = (double) L[(size_t) i] - (double) dry[(size_t) i];
            diffSum += d * d;
        }
        const float diffRms = (float) std::sqrt(diffSum / (double) totalSamples);
        juce::Logger::writeToLog("granulartest: screenshotParams diffFromDryRms=" + juce::String(diffRms, 6)
            + " (grain=6ms density=0.7/s over 5s => ~3-4 grains total; expect small but nonzero)");
    }

    {
        const double sr = 44100.0;
        GranularDelay gran;
        gran.prepare(sr); // out-of-the-box defaults: delay=300ms grain=80ms density=8 spread=30ms scatter=3 fb=0.3 mix=0.35

        const int totalSamples = (int) (3.0 * sr);
        std::vector<float> L((size_t) totalSamples), R((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            const float s = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr);
            L[(size_t) i] = s;
            R[(size_t) i] = s;
        }
        std::vector<float> dry = L;

        float* chans[2] = { L.data(), R.data() };
        gran.process(chans, 2, totalSamples);

        double diffSum = 0.0, dryRmsSum = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            const double d = (double) L[(size_t) i] - (double) dry[(size_t) i];
            diffSum += d * d;
            dryRmsSum += (double) dry[(size_t) i] * dry[(size_t) i];
        }
        const float diffRms = (float) std::sqrt(diffSum / (double) totalSamples);
        const float dryRms = (float) std::sqrt(dryRmsSum / (double) totalSamples);
        juce::Logger::writeToLog("granulartest: defaultParams diffFromDryRms=" + juce::String(diffRms, 5)
            + " dryRms=" + juce::String(dryRms, 5) + " ratio=" + juce::String(diffRms / dryRms, 4)
            + " (expect a clearly audible fraction, not <1%)");
    }

    {
        // "Drastic" settings - density and grain size both cranked toward
        // their max, the scenario the reported "barely audible on drastic
        // changes" complaint was about. Expected overlap here is 60*0.4=24,
        // well into the range where the old flat 1/sqrt attenuation was
        // squashing the effect quiet instead of making it bigger.
        const double sr = 44100.0;
        GranularDelay gran;
        gran.prepare(sr);
        gran.setDensity(60.0f);
        gran.setGrainSizeMs(400.0f);
        gran.setDelayMs(250.0f);
        gran.setFeedback(0.4f);
        gran.setMix(0.8f);

        const int totalSamples = (int) (2.0 * sr);
        std::vector<float> L((size_t) totalSamples), R((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            const float s = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr);
            L[(size_t) i] = s;
            R[(size_t) i] = s;
        }
        std::vector<float> dry = L;

        float* chans[2] = { L.data(), R.data() };
        gran.process(chans, 2, totalSamples);

        double diffSum = 0.0, dryRmsSum = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            const double d = (double) L[(size_t) i] - (double) dry[(size_t) i];
            diffSum += d * d;
            dryRmsSum += (double) dry[(size_t) i] * dry[(size_t) i];
        }
        const float diffRms = (float) std::sqrt(diffSum / (double) totalSamples);
        const float dryRms = (float) std::sqrt(dryRmsSum / (double) totalSamples);
        juce::Logger::writeToLog("granulartest: drasticParams diffFromDryRms=" + juce::String(diffRms, 5)
            + " dryRms=" + juce::String(dryRms, 5) + " ratio=" + juce::String(diffRms / dryRms, 4)
            + " (expect clearly louder than defaultParams' ratio, not quieter)");
    }

    {
        // Stability stress test: max density/grain-size/feedback held for
        // 8 seconds of sustained input - a live user report was that
        // throwing these knobs around made the gain "shoot up and brick
        // the VST", requiring the host to be force-closed. Random grain
        // spawn clustering can momentarily push the real overlap well past
        // the density-average grainNormFactor accounts for, and with high
        // feedback that spike re-entering the buffer can compound into an
        // exponential runaway. Verifies output stays finite throughout and
        // that late-window energy isn't growing relative to an earlier
        // window (the signature of a feedback loop building rather than
        // settling).
        const double sr = 44100.0;
        GranularDelay gran;
        gran.prepare(sr);
        gran.setDensity(60.0f);
        gran.setGrainSizeMs(500.0f);
        gran.setSpreadMs(0.0f); // no spread - worst case for clustering, grains all target the same read region
        gran.setDelayMs(50.0f);
        gran.setFeedback(0.95f);
        gran.setMix(1.0f);

        const int totalSamples = (int) (8.0 * sr);
        std::vector<float> L((size_t) totalSamples), R((size_t) totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            const float s = 0.6f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr);
            L[(size_t) i] = s;
            R[(size_t) i] = s;
        }

        float* chans[2] = { L.data(), R.data() };
        gran.process(chans, 2, totalSamples);

        bool anyNonFinite = false;
        float maxAbsSample = 0.0f;
        for (int i = 0; i < totalSamples; ++i)
        {
            if (!std::isfinite(L[(size_t) i]) || !std::isfinite(R[(size_t) i]))
                anyNonFinite = true;
            maxAbsSample = juce::jmax(maxAbsSample, std::abs(L[(size_t) i]), std::abs(R[(size_t) i]));
        }
        juce::Logger::writeToLog("granulartest: stabilityStress maxAbsSample=" + juce::String(maxAbsSample, 4)
            + " (expect well-bounded, not blown up to a large value - the output-stage limiter should catch"
            + " it even where the feedback-tap saturation alone doesn't)");

        auto rmsOfRange = [&](int start, int len) -> float
        {
            double sum = 0.0;
            for (int i = 0; i < len; ++i)
                sum += (double) L[(size_t) (start + i)] * L[(size_t) (start + i)];
            return (float) std::sqrt(sum / (double) len);
        };
        const int oneSecond = (int) sr;
        const float earlyRms = rmsOfRange(oneSecond, oneSecond);       // second 1-2
        const float lateRms = rmsOfRange(totalSamples - oneSecond, oneSecond); // last second

        juce::Logger::writeToLog("granulartest: stabilityStress anyNonFinite=" + juce::String((int) anyNonFinite)
            + " earlyRms=" + juce::String(earlyRms, 4) + " lateRms=" + juce::String(lateRms, 4)
            + " lateOverEarly=" + juce::String(lateRms / juce::jmax(1.0e-6f, earlyRms), 3)
            + " (expect anyNonFinite=0 and lateOverEarly not >> 1 - growing ratio means the feedback loop is building, not settling)");
    }

    {
        // Finds the dominant frequency in a signal via a large windowed FFT
        // - used to verify Shift mode moves a tone by a fixed Hz amount
        // (not a ratio, which is what a pitch shift would do), and that
        // Ring Mod produces symmetric sum/difference sidebands instead.
        auto findPeakFrequencyHz = [](const std::vector<float>& signal, double sr) -> double
        {
            constexpr int order = 15;
            constexpr int size = 1 << order;
            juce::dsp::FFT localFft(order);
            std::vector<float> data((size_t) size * 2, 0.0f);
            const int n = juce::jmin((int) signal.size(), size);
            for (int i = 0; i < n; ++i)
            {
                const float w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1)));
                data[(size_t) i] = signal[(size_t) i] * w;
            }
            localFft.performFrequencyOnlyForwardTransform(data.data(), true);
            int peakBin = 1;
            float peakMag = 0.0f;
            for (int b = 1; b < size / 2; ++b)
                if (data[(size_t) b] > peakMag) { peakMag = data[(size_t) b]; peakBin = b; }
            return (double) peakBin * sr / (double) size;
        };

        const double sr = 44100.0;

        {
            FreqShifter shifter;
            shifter.prepare(sr);
            shifter.setMode(FreqShifter::Mode::Shift);
            shifter.setShiftHz(200.0f);

            const int totalSamples = (int) (2.0 * sr);
            std::vector<float> in((size_t) totalSamples), out((size_t) totalSamples);
            for (int i = 0; i < totalSamples; ++i)
                in[(size_t) i] = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / (float) sr);
            shifter.process(in.data(), out.data(), totalSamples);

            std::vector<float> tail(out.begin() + totalSamples / 2, out.end());
            const double peakHz = findPeakFrequencyHz(tail, sr);
            juce::Logger::writeToLog("freqshiftertest: shiftMode inputHz=1000 shiftHz=200 outputPeakHz=" + juce::String(peakHz, 1)
                + " (expect ~1200 - a fixed Hz shift, not ~1000*ratio like a pitch shift)");
        }

        {
            FreqShifter shifter;
            shifter.prepare(sr);
            shifter.setMode(FreqShifter::Mode::RingMod);
            shifter.setShiftHz(300.0f);

            const int totalSamples = (int) (1.0 * sr);
            std::vector<float> in((size_t) totalSamples), out((size_t) totalSamples);
            for (int i = 0; i < totalSamples; ++i)
                in[(size_t) i] = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / (float) sr);
            shifter.process(in.data(), out.data(), totalSamples);

            const double peakHz = findPeakFrequencyHz(out, sr);
            juce::Logger::writeToLog("freqshiftertest: ringModMode inputHz=1000 modHz=300 outputPeakHz=" + juce::String(peakHz, 1)
                + " (expect ~700 or ~1300 - symmetric AM sidebands, and specifically NOT ~1000)");
        }
    }

    juce::Logger::writeToLog("selftest: done");
}
