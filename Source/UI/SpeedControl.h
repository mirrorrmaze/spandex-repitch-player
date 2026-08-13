#pragma once

#include <JuceHeader.h>

// Horizontal speed (%) slider plus the Link toggle that ties speed to pitch
// (classic Re-Pitch/vinyl behaviour) or frees them to move independently.
class SpeedControl : public juce::Component
{
public:
    SpeedControl();

    void resized() override;

    void setValue(double percent, juce::NotificationType notification = juce::dontSendNotification);
    double getValue() const { return slider.getValue(); }

    void setLinked(bool shouldLink, juce::NotificationType notification = juce::dontSendNotification);
    bool isLinked() const { return linkButton.getToggleState(); }

    std::function<void(double percent)> onValueChanged;
    std::function<void(bool linked)> onLinkChanged;

private:
    juce::Label titleLabel { {}, "Speed (%)" };
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton resetButton { "Reset" };
    juce::ToggleButton linkButton { "Link (Re-Pitch)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpeedControl)
};
