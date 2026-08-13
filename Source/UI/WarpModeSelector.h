#pragma once

#include <JuceHeader.h>
#include "../Audio/StretchAudioSource.h"

// Dropdown choosing which Rubber Band option preset to use when the Link
// toggle (see SpeedControl) is off, i.e. independent pitch/time warping.
class WarpModeSelector : public juce::Component
{
public:
    WarpModeSelector();

    void resized() override;

    void setMode(StretchAudioSource::WarpMode mode, juce::NotificationType notification = juce::dontSendNotification);
    StretchAudioSource::WarpMode getMode() const;

    void setEnabled(bool shouldBeEnabled) { comboBox.setEnabled(shouldBeEnabled); }

    std::function<void(StretchAudioSource::WarpMode)> onModeChanged;

private:
    juce::Label titleLabel { {}, "Warp Mode" };
    juce::ComboBox comboBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WarpModeSelector)
};
