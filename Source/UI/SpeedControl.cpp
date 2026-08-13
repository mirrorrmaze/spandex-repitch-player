#include "SpeedControl.h"
#include "AppLookAndFeel.h"

SpeedControl::SpeedControl()
{
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    slider.setRange(0.5, 400.0, 0.01);
    slider.setSkewFactorFromMidPoint(100.0);
    slider.setValue(100.0, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, 100.0);
    slider.setTextValueSuffix(" %");
    slider.onValueChange = [this]
    {
        if (onValueChanged)
            onValueChanged(slider.getValue());
    };
    addAndMakeVisible(slider);

    resetButton.onClick = [this] { slider.setValue(100.0, juce::sendNotificationSync); };
    addAndMakeVisible(resetButton);

    linkButton.setToggleState(true, juce::dontSendNotification);
    linkButton.onClick = [this]
    {
        if (onLinkChanged)
            onLinkChanged(linkButton.getToggleState());
    };
    addAndMakeVisible(linkButton);
}

void SpeedControl::resized()
{
    auto bounds = getLocalBounds().reduced(2, 2);
    auto top = bounds.removeFromTop(20);
    titleLabel.setBounds(top.removeFromLeft(top.getWidth() - 160));
    linkButton.setBounds(top);
    bounds.removeFromTop(6);

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    fb.items.add(juce::FlexItem(slider).withFlex(1.0f).withHeight(36));
    fb.items.add(juce::FlexItem(resetButton).withWidth(70).withHeight(32).withMargin({ 0, 0, 0, 8 }));
    fb.performLayout(bounds);
}

void SpeedControl::setValue(double percent, juce::NotificationType notification)
{
    slider.setValue(percent, notification);
}

void SpeedControl::setLinked(bool shouldLink, juce::NotificationType notification)
{
    linkButton.setToggleState(shouldLink, notification);
}
