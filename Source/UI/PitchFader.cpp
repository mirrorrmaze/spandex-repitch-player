#include "PitchFader.h"
#include "AppLookAndFeel.h"

PitchFader::PitchFader()
{
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    slider.setRange(-24.0, 24.0, 0.01);
    slider.setValue(0.0, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, 0.0);
    slider.setTextValueSuffix(" st");
    slider.onValueChange = [this]
    {
        if (onValueChanged)
            onValueChanged(slider.getValue());
    };
    addAndMakeVisible(slider);

    resetButton.onClick = [this] { slider.setValue(0.0, juce::sendNotificationSync); };
    addAndMakeVisible(resetButton);
}

void PitchFader::resized()
{
    auto bounds = getLocalBounds().reduced(2, 2);
    titleLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(6);

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    fb.items.add(juce::FlexItem(slider).withFlex(1.0f).withHeight(36));
    fb.items.add(juce::FlexItem(resetButton).withWidth(70).withHeight(32).withMargin({ 0, 0, 0, 8 }));
    fb.performLayout(bounds);
}

void PitchFader::setValue(double semitones, juce::NotificationType notification)
{
    slider.setValue(semitones, notification);
}
