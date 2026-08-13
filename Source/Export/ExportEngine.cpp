#include "ExportEngine.h"
#include "../Audio/Paulstretch.h"
#include <lame/lame.h>

namespace
{
    constexpr int renderChunk = 4096;

    void renderRePitchOffline(const juce::AudioBuffer<float>& source, double pitchSemitones,
                               juce::AudioBuffer<float>& outBuffer,
                               const std::function<void(float)>& progressCallback)
    {
        const int numSrcCh = source.getNumChannels();
        const int totalIn = source.getNumSamples();
        const double rate = std::pow(2.0, pitchSemitones / 12.0);
        const int totalOut = totalIn > 1 ? (int) std::floor((double) (totalIn - 1) / rate) : 0;

        outBuffer.setSize(2, juce::jmax(0, totalOut), false, false, false);

        double cursor = 0.0;
        for (int i = 0; i < totalOut; ++i)
        {
            const int idx0 = (int) cursor;
            const float frac = (float) (cursor - (double) idx0);

            for (int ch = 0; ch < 2; ++ch)
            {
                const int srcCh = juce::jmin(ch, numSrcCh - 1);
                const float s0 = source.getSample(srcCh, idx0);
                const float s1 = source.getSample(srcCh, juce::jmin(idx0 + 1, totalIn - 1));
                outBuffer.setSample(ch, i, s0 + frac * (s1 - s0));
            }

            cursor += rate;

            if (progressCallback && (i % 65536) == 0)
                progressCallback((float) i / (float) juce::jmax(1, totalOut));
        }

        if (progressCallback)
            progressCallback(1.0f);
    }

    void renderPaulstretchOffline(const juce::AudioBuffer<float>& source, double sourceSampleRate,
                                   const ExportEngine::Settings& settings,
                                   juce::int64 maxOutSamples, juce::AudioBuffer<float>& outBuffer,
                                   const std::function<void(float)>& progressCallback)
    {
        const int numSrcCh = source.getNumChannels();
        const int totalIn = source.getNumSamples();
        const double speedRatio = juce::jmax(0.0001, settings.speedPercent / 100.0);
        const auto uncappedOut = (juce::int64) std::ceil((double) totalIn / speedRatio);
        const auto totalOut = juce::jlimit((juce::int64) 0, maxOutSamples, uncappedOut);

        if (totalOut < uncappedOut && sourceSampleRate > 0.0)
            juce::Logger::writeToLog("Paulstretch export: capped at " + juce::String((double) maxOutSamples / sourceSampleRate / 60.0, 1)
                                      + " min (uncapped would have been " + juce::String((double) uncappedOut / sourceSampleRate / 60.0, 1) + " min)");

        outBuffer.setSize(2, (int) totalOut, false, false, false);

        Paulstretch stretcher;
        stretcher.prepare();

        const float* srcL = source.getReadPointer(0);
        const float* srcR = source.getReadPointer(juce::jmin(1, numSrcCh - 1));

        double cursor = 0.0;
        int written = 0;
        while (written < outBuffer.getNumSamples())
        {
            const int n = juce::jmin(renderChunk, outBuffer.getNumSamples() - written);
            stretcher.process(srcL, srcR, totalIn, cursor, speedRatio,
                               outBuffer.getWritePointer(0) + written,
                               outBuffer.getWritePointer(1) + written, n);
            written += n;

            if (progressCallback)
                progressCallback((float) written / (float) juce::jmax(1, outBuffer.getNumSamples()));
        }

        if (progressCallback)
            progressCallback(1.0f);
    }

    void renderWarpedOffline(const juce::AudioBuffer<float>& source, double sourceSampleRate,
                              const ExportEngine::Settings& settings, juce::AudioBuffer<float>& outBuffer,
                              const std::function<void(float)>& progressCallback)
    {
        using RB = RubberBand::RubberBandStretcher;

        const int numSrcCh = source.getNumChannels();
        const int totalIn = source.getNumSamples();

        juce::AudioBuffer<float> stereoSource(2, totalIn);
        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = juce::jmin(ch, numSrcCh - 1);
            stereoSource.copyFrom(ch, 0, source, srcCh, 0, totalIn);
        }

        const auto options = StretchAudioSource::optionsForWarpMode(settings.warpMode) | RB::OptionProcessOffline;
        RB stretcher((size_t) sourceSampleRate, 2, options,
                      100.0 / settings.speedPercent, std::pow(2.0, settings.pitchSemitones / 12.0));

        // Study pass: lets the offline engine look ahead across the whole
        // track before committing to a stretch profile (higher quality than
        // the block-by-block real-time path used during playback).
        {
            int pos = 0;
            while (pos < totalIn)
            {
                const int n = juce::jmin(renderChunk, totalIn - pos);
                const float* ptrs[2] = { stereoSource.getReadPointer(0, pos), stereoSource.getReadPointer(1, pos) };
                stretcher.study(ptrs, (size_t) n, (pos + n) >= totalIn);
                pos += n;
            }
        }

        const auto estimatedOut = (juce::int64) std::ceil((double) totalIn * (100.0 / settings.speedPercent))
                                 + (juce::int64) stretcher.getLatency() + renderChunk;
        outBuffer.setSize(2, (int) juce::jmax((juce::int64) 0, estimatedOut), false, false, false);

        juce::int64 written = 0;
        auto ensureCapacity = [&outBuffer](juce::int64 needed)
        {
            if (needed > outBuffer.getNumSamples())
                outBuffer.setSize(2, (int) needed, true, false, false);
        };

        auto drain = [&]
        {
            for (;;)
            {
                const auto avail = stretcher.available();
                if (avail <= 0)
                    return avail;

                ensureCapacity(written + avail);
                float* outPtrs[2] = { outBuffer.getWritePointer(0, (int) written),
                                       outBuffer.getWritePointer(1, (int) written) };
                const auto got = (juce::int64) stretcher.retrieve(outPtrs, (size_t) avail);
                written += got;
                if (got == 0)
                    return avail;
            }
        };

        int pos = 0;
        while (pos < totalIn)
        {
            const int n = juce::jmin(renderChunk, totalIn - pos);
            const bool isFinal = (pos + n) >= totalIn;
            const float* ptrs[2] = { stereoSource.getReadPointer(0, pos), stereoSource.getReadPointer(1, pos) };
            stretcher.process(ptrs, (size_t) n, isFinal);
            pos += n;

            drain();

            if (progressCallback)
                progressCallback(0.9f * (float) pos / (float) totalIn);
        }

        // Keep draining until Rubber Band reports -1 (fully finished).
        while (drain() >= 0) {}

        outBuffer.setSize(2, (int) written, true, false, false);

        if (progressCallback)
            progressCallback(1.0f);
    }

    bool writeWithJuceFormat(juce::AudioFormat& format, const juce::AudioBuffer<float>& buffer,
                              double sampleRate, const juce::File& outFile, int bitsPerSample,
                              juce::String& errorMessage)
    {
        outFile.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
        if (stream == nullptr)
        {
            errorMessage = "Could not create output file: " + outFile.getFullPathName();
            return false;
        }

        const auto options = juce::AudioFormatWriterOptions{}
                                  .withSampleRate(sampleRate)
                                  .withNumChannels(buffer.getNumChannels())
                                  .withBitsPerSample(bitsPerSample);

        auto writer = format.createWriterFor(stream, options);
        if (writer == nullptr)
        {
            errorMessage = "Could not create a writer for this format/bit depth.";
            return false;
        }

        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        return true;
    }

    bool writeMp3(const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& outFile,
                  int bitrateKbps, juce::String& errorMessage)
    {
        outFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
        if (stream == nullptr)
        {
            errorMessage = "Could not create output file: " + outFile.getFullPathName();
            return false;
        }

        lame_t gfp = lame_init();
        if (gfp == nullptr)
        {
            errorMessage = "Could not initialise the MP3 encoder.";
            return false;
        }

        lame_set_in_samplerate(gfp, (int) sampleRate);
        lame_set_num_channels(gfp, juce::jmin(2, buffer.getNumChannels()));
        lame_set_brate(gfp, bitrateKbps);
        lame_set_quality(gfp, 2);

        if (lame_init_params(gfp) < 0)
        {
            errorMessage = "Could not configure the MP3 encoder.";
            lame_close(gfp);
            return false;
        }

        const int numSamples = buffer.getNumSamples();
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;

        const int mp3BufSize = (int) (1.25 * (double) renderChunk) + 7200;
        juce::HeapBlock<unsigned char> mp3Buf((size_t) mp3BufSize);

        bool ok = true;
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = juce::jmin(renderChunk, numSamples - pos);
            const int bytes = lame_encode_buffer_ieee_float(gfp, left + pos, right + pos, n, mp3Buf, mp3BufSize);
            if (bytes < 0)
            {
                ok = false;
                break;
            }
            if (bytes > 0)
                stream->write(mp3Buf, (size_t) bytes);
            pos += n;
        }

        if (ok)
        {
            const int flushBytes = lame_encode_flush(gfp, mp3Buf, mp3BufSize);
            if (flushBytes > 0)
                stream->write(mp3Buf, (size_t) flushBytes);
        }

        lame_close(gfp);

        if (!ok)
        {
            errorMessage = "MP3 encoding failed.";
            return false;
        }

        return true;
    }
}

void ExportEngine::renderOffline(const juce::AudioBuffer<float>& source, double sourceSampleRate,
                                  const Settings& settings, juce::AudioBuffer<float>& outBuffer,
                                  const std::function<void(float)>& progressCallback)
{
    if (settings.linked)
        renderRePitchOffline(source, settings.pitchSemitones, outBuffer, progressCallback);
    else if (settings.warpMode == StretchAudioSource::WarpMode::Paulstretch)
        renderPaulstretchOffline(source, sourceSampleRate, settings, (juce::int64) (sourceSampleRate * maxExportSeconds),
                                  outBuffer, progressCallback);
    else
        renderWarpedOffline(source, sourceSampleRate, settings, outBuffer, progressCallback);
}

bool ExportEngine::writeToFile(const juce::AudioBuffer<float>& buffer, double sampleRate,
                                const juce::File& outputFile, Format format, int mp3BitrateKbps,
                                juce::String& errorMessage)
{
    switch (format)
    {
        case Format::WAV:
        {
            juce::WavAudioFormat wav;
            return writeWithJuceFormat(wav, buffer, sampleRate, outputFile, 24, errorMessage);
        }
        case Format::AIFF:
        {
            juce::AiffAudioFormat aiff;
            return writeWithJuceFormat(aiff, buffer, sampleRate, outputFile, 24, errorMessage);
        }
        case Format::FLAC:
        {
            juce::FlacAudioFormat flac;
            return writeWithJuceFormat(flac, buffer, sampleRate, outputFile, 24, errorMessage);
        }
        case Format::MP3:
            return writeMp3(buffer, sampleRate, outputFile, mp3BitrateKbps, errorMessage);
    }

    errorMessage = "Unknown export format.";
    return false;
}

bool ExportEngine::exportToFile(const juce::AudioBuffer<float>& sourceBuffer, double sourceSampleRate,
                                 const Settings& settings, const juce::File& outputFile,
                                 const std::function<void(float)>& progressCallback, juce::String& errorMessage)
{
    if (sourceBuffer.getNumSamples() == 0)
    {
        errorMessage = "No track loaded.";
        return false;
    }

    juce::AudioBuffer<float> rendered;
    renderOffline(sourceBuffer, sourceSampleRate, settings, rendered, progressCallback);

    if (rendered.getNumSamples() == 0)
    {
        errorMessage = "Rendered audio was empty.";
        return false;
    }

    return writeToFile(rendered, sourceSampleRate, outputFile, settings.format, settings.mp3BitrateKbps, errorMessage);
}
