#include "LoopControls.h"
#include "AppLookAndFeel.h"

LoopControls::LoopControls()
{
    loopInButton.setTooltip("Loop In");
    loopOutButton.setTooltip("Loop Out");
    addAndMakeVisible(loopInButton);
    addAndMakeVisible(loopOutButton);
    addAndMakeVisible(loopToggle);

    modeForwardButton.setTooltip("Forward");
    modePingPongButton.setTooltip("Ping-Pong");
    modeReverseButton.setTooltip("Reverse");
    modeForwardButton.onClick = [this]
    {
        updateModeButtonColours(StretchAudioSource::LoopMode::Forward);
        if (onModeChanged)
            onModeChanged(StretchAudioSource::LoopMode::Forward);
    };
    modePingPongButton.onClick = [this]
    {
        updateModeButtonColours(StretchAudioSource::LoopMode::PingPong);
        if (onModeChanged)
            onModeChanged(StretchAudioSource::LoopMode::PingPong);
    };
    modeReverseButton.onClick = [this]
    {
        updateModeButtonColours(StretchAudioSource::LoopMode::Reverse);
        if (onModeChanged)
            onModeChanged(StretchAudioSource::LoopMode::Reverse);
    };
    addAndMakeVisible(modeForwardButton);
    addAndMakeVisible(modePingPongButton);
    addAndMakeVisible(modeReverseButton);
    updateModeButtonColours(StretchAudioSource::LoopMode::Forward);

    addAndMakeVisible(playFromStartToggle);
    addAndMakeVisible(linkLoopToStartToggle);

    crossfadeLabel.setJustificationType(juce::Justification::centredRight);
    crossfadeLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(crossfadeLabel);

    crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfadeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 24);
    crossfadeSlider.setRange(0.0, 200.0, 1.0);
    crossfadeSlider.setValue(20.0, juce::dontSendNotification);
    crossfadeSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(crossfadeSlider);
}

void LoopControls::updateModeButtonColours(StretchAudioSource::LoopMode active)
{
    using LM = StretchAudioSource::LoopMode;
    const auto& accent = AppLookAndFeel::bright;
    const auto& inactive = AppLookAndFeel::bg;

    modeForwardButton.setColour(juce::TextButton::buttonColourId, active == LM::Forward ? accent : inactive);
    modePingPongButton.setColour(juce::TextButton::buttonColourId, active == LM::PingPong ? accent : inactive);
    modeReverseButton.setColour(juce::TextButton::buttonColourId, active == LM::Reverse ? accent : inactive);

    modeForwardButton.setColour(juce::TextButton::textColourOffId, active == LM::Forward ? juce::Colours::black : AppLookAndFeel::bright);
    modePingPongButton.setColour(juce::TextButton::textColourOffId, active == LM::PingPong ? juce::Colours::black : AppLookAndFeel::bright);
    modeReverseButton.setColour(juce::TextButton::textColourOffId, active == LM::Reverse ? juce::Colours::black : AppLookAndFeel::bright);
}

void LoopControls::setActiveMode(StretchAudioSource::LoopMode mode)
{
    updateModeButtonColours(mode);
}

void LoopControls::resized()
{
    auto bounds = getLocalBounds();
    constexpr int rowHeight = 32;
    constexpr int rowGap = 6;

    auto topRow = bounds.removeFromTop(rowHeight);
    bounds.removeFromTop(rowGap);
    auto bottomRow = bounds.removeFromTop(rowHeight);

    // Row 1: core loop mechanics - large enough that the bracket/arrow
    // glyphs read clearly at a glance instead of needing a label.
    juce::FlexBox top;
    top.flexDirection = juce::FlexBox::Direction::row;
    top.alignItems = juce::FlexBox::AlignItems::center;
    top.items.add(juce::FlexItem(loopInButton).withWidth(44).withHeight(rowHeight).withMargin(3));
    top.items.add(juce::FlexItem(loopOutButton).withWidth(44).withHeight(rowHeight).withMargin(3));
    top.items.add(juce::FlexItem(loopToggle).withWidth(76).withHeight(rowHeight).withMargin(3));
    // The 3 mode buttons sit close together as a segmented control, with a
    // wider gap on either side separating them from their neighbours.
    top.items.add(juce::FlexItem(modeForwardButton).withWidth(44).withHeight(rowHeight).withMargin({ 3, 3, 3, 12 }));
    top.items.add(juce::FlexItem(modePingPongButton).withWidth(44).withHeight(rowHeight).withMargin({ 3, 3, 3, 0 }));
    top.items.add(juce::FlexItem(modeReverseButton).withWidth(44).withHeight(rowHeight).withMargin({ 3, 12, 3, 0 }));
    top.performLayout(topRow);

    // Row 2: sampler-style playback refinements plus the loop crossfade.
    juce::FlexBox bottom;
    bottom.flexDirection = juce::FlexBox::Direction::row;
    bottom.alignItems = juce::FlexBox::AlignItems::center;
    bottom.items.add(juce::FlexItem(playFromStartToggle).withWidth(110).withHeight(rowHeight).withMargin(3));
    bottom.items.add(juce::FlexItem(linkLoopToStartToggle).withWidth(70).withHeight(rowHeight).withMargin(3));
    bottom.items.add(juce::FlexItem(crossfadeLabel).withWidth(56).withHeight(rowHeight).withMargin({ 3, 3, 3, 16 }));
    bottom.items.add(juce::FlexItem(crossfadeSlider).withWidth(140).withHeight(rowHeight).withMargin(3));
    bottom.performLayout(bottomRow);
}
