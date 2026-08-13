#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MainComponent.h"
#include "UI/AppLookAndFeel.h"

// The plugin's UI half - just a thin AudioProcessorEditor shell around the
// same MainComponent the Standalone build uses, bound to the processor's
// (long-lived) AudioEngine rather than owning one itself. Can be closed and
// reopened by the host freely without affecting playback.
class SpandexAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpandexAudioProcessorEditor(SpandexAudioProcessor& p);
    ~SpandexAudioProcessorEditor() override;

    void resized() override;

private:
    SpandexAudioProcessor& processor;
    AppLookAndFeel lookAndFeel;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpandexAudioProcessorEditor)
};
