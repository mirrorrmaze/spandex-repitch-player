#pragma once

#include <JuceHeader.h>

// Horizontal pitch-shift fader, in semitones, with a numeric readout and reset.
class PitchFader : public juce::Component
{
public:
    PitchFader();

    void resized() override;

    void setValue(double semitones, juce::NotificationType notification = juce::dontSendNotification);
    double getValue() const { return slider.getValue(); }

    std::function<void(double semitones)> onValueChanged;

private:
    juce::Label titleLabel { {}, "Pitch (semitones)" };
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton resetButton { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchFader)
};
