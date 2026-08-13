#include "ExportPanel.h"
#include "AppLookAndFeel.h"

namespace
{
    constexpr int idOffset = 1;
}

ExportPanel::ExportPanel()
{
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    formatBox.addItem("WAV (24-bit)", (int) ExportEngine::Format::WAV + idOffset);
    formatBox.addItem("AIFF (24-bit)", (int) ExportEngine::Format::AIFF + idOffset);
    formatBox.addItem("FLAC (24-bit)", (int) ExportEngine::Format::FLAC + idOffset);
    formatBox.addItem("MP3", (int) ExportEngine::Format::MP3 + idOffset);
    formatBox.setSelectedId((int) ExportEngine::Format::WAV + idOffset, juce::dontSendNotification);
    addAndMakeVisible(formatBox);

    bitrateSlider.setRange(96.0, 320.0, 32.0);
    bitrateSlider.setValue(320.0, juce::dontSendNotification);
    bitrateSlider.setTextValueSuffix(" kbps");
    addAndMakeVisible(bitrateSlider);

    addAndMakeVisible(exportButton);
}

void ExportPanel::resized()
{
    auto bounds = getLocalBounds().reduced(4, 2);
    titleLabel.setBounds(bounds.removeFromTop(18));

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    fb.items.add(juce::FlexItem(formatBox).withWidth(160).withHeight(32).withMargin(3));
    fb.items.add(juce::FlexItem(bitrateSlider).withWidth(140).withHeight(32).withMargin(3));
    fb.items.add(juce::FlexItem(exportButton).withWidth(110).withHeight(32).withMargin(3));
    fb.performLayout(bounds);
}

ExportEngine::Format ExportPanel::getFormat() const
{
    return (ExportEngine::Format) (formatBox.getSelectedId() - idOffset);
}
