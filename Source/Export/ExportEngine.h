#pragma once

#include <JuceHeader.h>
#include "../Audio/StretchAudioSource.h"

// Offline render + file write for the Export button. Uses the same pitch/time
// math as playback (Re-Pitch resample when linked) but, for warp modes, a
// two-pass offline Rubber Band render (study then process) rather than the
// real-time streaming mode used for live playback - higher quality since it
// can look at the whole track instead of processing block-by-block.
class ExportEngine
{
public:
    enum class Format { WAV, AIFF, FLAC, MP3 };

    struct Settings
    {
        double pitchSemitones = 0.0;
        double speedPercent = 100.0;
        bool linked = true;
        StretchAudioSource::WarpMode warpMode = StretchAudioSource::WarpMode::Complex;
        Format format = Format::WAV;
        int mp3BitrateKbps = 320;
    };

    // progressCallback is invoked periodically with 0..1 from the calling thread.
    static bool exportToFile(const juce::AudioBuffer<float>& sourceBuffer,
                              double sourceSampleRate,
                              const Settings& settings,
                              const juce::File& outputFile,
                              const std::function<void(float)>& progressCallback,
                              juce::String& errorMessage);

private:
    static void renderOffline(const juce::AudioBuffer<float>& source, double sourceSampleRate,
                               const Settings& settings, juce::AudioBuffer<float>& outBuffer,
                               const std::function<void(float)>& progressCallback);

    static bool writeToFile(const juce::AudioBuffer<float>& buffer, double sampleRate,
                             const juce::File& outputFile, Format format, int mp3BitrateKbps,
                             juce::String& errorMessage);

    // Extreme Paulstretch ratios can imply hours of output from a short
    // source; cap render length so export can't exhaust memory/disk.
    static constexpr double maxExportSeconds = 1800.0;
};
