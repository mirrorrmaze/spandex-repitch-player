#include "PluginEditor.h"

SpandexAudioProcessorEditor::SpandexAudioProcessorEditor(SpandexAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), mainComponent(p.getAudioEngine())
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    addAndMakeVisible(mainComponent);
    setResizable(true, true);
    // Without a floor, the window could be dragged smaller than the layout
    // can handle - components don't reflow into overlapping nonsense, they
    // just render at whatever (possibly negative/degenerate) bounds the
    // subtractive layout in MainComponent::resized() produces, which reads
    // as "the GUI doesn't follow the resize". The FX tab's top row (Reverb/
    // Delay/Shifter cards, split 5:7:5 by knob count in FxPanel::resized())
    // needs each card at least knobCount*60+32px wide so no card's knobs
    // compress below their own text boxes - 1150 turned out to be a few tens
    // of px too tight for that once Shifter's share of the row was properly
    // weighted (a host docking the plugin right at the floor, as opposed to
    // the Standalone's free-resize, was clipping Shifter's rightmost knob).
    setResizeLimits(1280, 720, 2600, 1700);
    setSize(mainComponent.getWidth(), mainComponent.getHeight());
}

SpandexAudioProcessorEditor::~SpandexAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void SpandexAudioProcessorEditor::resized()
{
    mainComponent.setBounds(getLocalBounds());
}
