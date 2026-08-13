#pragma once

#include <JuceHeader.h>
#include "AppLookAndFeel.h"

// A small rotary knob with a label above it and a value readout below -
// the standard plugin-GUI building block (Ableton/FabFilter style), reused
// across the FX and EQ panels to avoid repeating the same layout code.
class LabeledKnob : public juce::Component
{
public:
    explicit LabeledKnob(const juce::String& labelText)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, AppLookAndFeel::dim);
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        // Fixed-width text box - if the knob's own bounds end up narrower
        // than this (see FxPanel::layoutSection's matching withMinWidth),
        // JUCE can't shrink it to fit and the readout gets clipped mid-
        // digit ("0..."). 52 is deliberately on the small side to keep the
        // window's forced minimum size reasonable while still fitting
        // typical readouts like "0.500" or "-24.0".
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 18);
        addAndMakeVisible(slider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromTop(16));
        slider.setBounds(bounds);
    }

    juce::Slider slider;

private:
    // The label is deliberately dim (not the LookAndFeel's default bright
    // text colour) to read as a secondary caption under the knob - a
    // per-instance override that, unlike everything left at the LookAndFeel
    // default, won't pick up a theme change on its own.
    void lookAndFeelChanged() override
    {
        label.setColour(juce::Label::textColourId, AppLookAndFeel::dim);
    }

    juce::Label label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledKnob)
};
