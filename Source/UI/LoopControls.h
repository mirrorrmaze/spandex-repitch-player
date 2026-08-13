#pragma once

#include <JuceHeader.h>

// Loop In/Out/Toggle plus a crossfade-length control for the loop point.
// Kept as its own small strip (rather than folded into the always-visible
// TransportControls at the bottom) so MainComponent can show it only on
// the Player tab - looping doesn't mean anything while looking at the FX
// or EQ tab.
class LoopControls : public juce::Component
{
public:
    LoopControls();

    void resized() override;

    juce::TextButton loopInButton { "Loop In" };
    juce::TextButton loopOutButton { "Loop Out" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::Label crossfadeLabel { {}, "X-fade" };
    juce::Slider crossfadeSlider;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopControls)
};
