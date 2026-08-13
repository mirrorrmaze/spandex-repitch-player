#include "TransportControls.h"
#include "AppLookAndFeel.h"

TransportControls::TransportControls()
{
    addAndMakeVisible(openButton);
    addAndMakeVisible(playPauseButton);
    addAndMakeVisible(stopButton);

    timeLabel.setJustificationType(juce::Justification::centredRight);
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(6);

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::center;

    fb.items.add(juce::FlexItem(openButton).withWidth(100).withHeight(36).withMargin(4));
    fb.items.add(juce::FlexItem(playPauseButton).withWidth(56).withHeight(36).withMargin(4));
    fb.items.add(juce::FlexItem(stopButton).withWidth(56).withHeight(36).withMargin(4));
    fb.items.add(juce::FlexItem().withFlex(1.0f));
    fb.items.add(juce::FlexItem(timeLabel).withWidth(180).withHeight(36).withMargin(4));

    fb.performLayout(bounds);
}

void TransportControls::setPlayingState(bool isPlaying)
{
    playPauseButton.setIcon(isPlaying ? IconButton::Icon::Pause : IconButton::Icon::Play);
}

void TransportControls::setTimeText(const juce::String& text)
{
    timeLabel.setText(text, juce::dontSendNotification);
}
