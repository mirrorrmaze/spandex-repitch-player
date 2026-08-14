#include "LoopControls.h"
#include "AppLookAndFeel.h"

LoopControls::LoopControls()
{
    addAndMakeVisible(loopInButton);
    addAndMakeVisible(loopOutButton);
    addAndMakeVisible(loopToggle);
    addAndMakeVisible(playFromStartToggle);
    addAndMakeVisible(linkLoopToStartToggle);

    loopModeBox.addItem("Forward", 1);
    loopModeBox.addItem("Ping-Pong", 2);
    loopModeBox.addItem("Reverse", 3);
    loopModeBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(loopModeBox);

    crossfadeLabel.setJustificationType(juce::Justification::centredRight);
    crossfadeLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(crossfadeLabel);

    crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfadeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    crossfadeSlider.setRange(0.0, 200.0, 1.0);
    crossfadeSlider.setValue(20.0, juce::dontSendNotification);
    crossfadeSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(crossfadeSlider);
}

void LoopControls::resized()
{
    auto bounds = getLocalBounds();

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    fb.items.add(juce::FlexItem(loopInButton).withWidth(74).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(loopOutButton).withWidth(74).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(loopToggle).withWidth(56).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(loopModeBox).withWidth(98).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(playFromStartToggle).withWidth(82).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(linkLoopToStartToggle).withWidth(52).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(crossfadeLabel).withWidth(46).withHeight(28).withMargin(2));
    fb.items.add(juce::FlexItem(crossfadeSlider).withWidth(110).withHeight(28).withMargin(2));
    fb.performLayout(bounds);
}
