#pragma once

#include <JuceHeader.h>

// Hidden diagnostic path (invoked as `SPANDEX.exe --selftest <in.wav> <outDir>`)
// that renders StretchAudioSource offline under a handful of known pitch/speed/warp
// settings and writes each result as a WAV, so the DSP chain's correctness can be
// checked numerically (e.g. measured frequency) without a human listening.
void runSelfTest(const juce::File& inputFile, const juce::File& outputDir);
