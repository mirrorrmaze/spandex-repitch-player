#pragma once

#include <JuceHeader.h>
#include "../Export/ExportEngine.h"

// Format selector + Export button. Export always renders the track using
// whatever pitch/speed/warp settings are currently active in the engine -
// "what you hear is what you get."
class ExportPanel : public juce::Component
{
public:
    ExportPanel();

    void resized() override;

    ExportEngine::Format getFormat() const;
    int getMp3Bitrate() const { return (int) bitrateSlider.getValue(); }

    juce::TextButton exportButton { "Export..." };

private:
    juce::Label titleLabel { {}, "Export" };
    juce::ComboBox formatBox;
    juce::Slider bitrateSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportPanel)
};
