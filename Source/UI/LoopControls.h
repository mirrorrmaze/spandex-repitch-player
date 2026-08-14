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
    // Ableton Sampler-style Sustain Loop Mode: Forward (plain repeat),
    // Ping-Pong ("Back and Forth"), Reverse.
    juce::ComboBox loopModeBox;
    juce::Label crossfadeLabel { {}, "X-fade" };
    juce::Slider crossfadeSlider;

    // When on, pressing Play always seeks to the Sample Start trim marker
    // first, rather than resuming wherever playback last was - matches how
    // a sampler retriggers from Start on every note.
    juce::ToggleButton playFromStartToggle { "From Start" };
    // When on, dragging the Sample Start trim marker on the waveform also
    // drags the Loop Start marker by the same delta, preserving their
    // relative offset - Ableton Sampler's actual Start/Loop-Start Link
    // behaviour. Off (default) keeps them fully independent, as before.
    juce::ToggleButton linkLoopToStartToggle { "Link" };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopControls)
};
