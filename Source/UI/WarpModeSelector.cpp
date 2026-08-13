#include "WarpModeSelector.h"
#include "AppLookAndFeel.h"

namespace
{
    // ComboBox item IDs are 1-based; index 0 = Beats.
    constexpr int idOffset = 1;
}

WarpModeSelector::WarpModeSelector()
{
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    comboBox.addItem("Beats (percussive)", (int) StretchAudioSource::WarpMode::Beats + idOffset);
    comboBox.addItem("Tones (monophonic)", (int) StretchAudioSource::WarpMode::Tones + idOffset);
    comboBox.addItem("Texture (pads/ambient)", (int) StretchAudioSource::WarpMode::Texture + idOffset);
    comboBox.addItem("Complex (full mix)", (int) StretchAudioSource::WarpMode::Complex + idOffset);
    comboBox.addItem("Complex Pro (highest quality)", (int) StretchAudioSource::WarpMode::ComplexPro + idOffset);
    comboBox.addItem("Paulstretch (extreme ambient)", (int) StretchAudioSource::WarpMode::Paulstretch + idOffset);
    comboBox.setSelectedId((int) StretchAudioSource::WarpMode::Complex + idOffset, juce::dontSendNotification);
    comboBox.onChange = [this]
    {
        if (onModeChanged)
            onModeChanged(getMode());
    };
    addAndMakeVisible(comboBox);
}

void WarpModeSelector::resized()
{
    auto bounds = getLocalBounds();
    titleLabel.setBounds(bounds.removeFromTop(18));
    comboBox.setBounds(bounds);
}

void WarpModeSelector::setMode(StretchAudioSource::WarpMode mode, juce::NotificationType notification)
{
    comboBox.setSelectedId((int) mode + idOffset, notification);
}

StretchAudioSource::WarpMode WarpModeSelector::getMode() const
{
    return (StretchAudioSource::WarpMode) (comboBox.getSelectedId() - idOffset);
}
