#pragma once

#include <JuceHeader.h>
#include "../Audio/StretchAudioSource.h"

// Loop In/Out/Toggle plus a crossfade-length control for the loop point.
// Kept as its own small strip (rather than folded into the always-visible
// TransportControls at the bottom) so MainComponent can show it only on
// the Player tab - looping doesn't mean anything while looking at the FX
// or EQ tab. Laid out across two rows (core loop mechanics on top, the
// sampler-style playback refinements below) rather than cramming
// everything into one - a single row of this many controls read as
// illegible even at full window width, since MainComponent gives this
// component a fixed pixel box rather than one that grows with the window.
class LoopControls : public juce::Component
{
public:
    LoopControls();

    void resized() override;

    // Sets which mode button reads as active (fill highlight, matching
    // HeaderTabs' selected-tab convention) without firing onModeChanged -
    // for syncing the display from engine state.
    void setActiveMode(StretchAudioSource::LoopMode mode);

    std::function<void(StretchAudioSource::LoopMode)> onModeChanged;

    juce::TextButton loopInButton { "[" };
    juce::TextButton loopOutButton { "]" };
    juce::ToggleButton loopToggle { "Loop" };

    // Ableton Sampler-style Sustain Loop Mode, as a 3-way segmented control
    // (symbols, not a dropdown, so all three read at a glance) instead of
    // the old ComboBox: Forward (plain repeat), Ping-Pong ("Back and
    // Forth"), Reverse.
    juce::TextButton modeForwardButton { juce::String::fromUTF8("\xe2\x86\x92") };  // rightwards arrow
    juce::TextButton modePingPongButton { juce::String::fromUTF8("\xe2\x86\x94") }; // left-right arrow
    juce::TextButton modeReverseButton { juce::String::fromUTF8("\xe2\x86\x90") };  // leftwards arrow

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
    void updateModeButtonColours(StretchAudioSource::LoopMode active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopControls)
};
