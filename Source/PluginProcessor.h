#pragma once

#include <JuceHeader.h>
#include "Audio/AudioEngine.h"

// The AudioProcessor is the plugin's persistent half - owned by the host
// (or, in the Standalone wrapper, by JUCE's StandaloneFilterWindow) for as
// long as the plugin instance exists, independent of whether its editor
// window is currently open. It owns the AudioEngine (the actual DSP graph);
// SpandexAudioProcessorEditor is just a view onto it, so closing and
// reopening the plugin window never interrupts playback.
class SpandexAudioProcessor : public juce::AudioProcessor
{
public:
    SpandexAudioProcessor();
    ~SpandexAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    // Declared (not actually used - SPANDEX plays a loaded file, not
    // incoming notes) because Ableton's VST3 host refuses to load an
    // Instrument-category plugin that has no event/MIDI input bus at all.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // No AudioProcessorValueTreeState-backed parameters yet - state
    // save/restore (DAW session recall) isn't wired up in this build.
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    AudioEngine& getAudioEngine() { return audioEngine; }

private:
    AudioEngine audioEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpandexAudioProcessor)
};
