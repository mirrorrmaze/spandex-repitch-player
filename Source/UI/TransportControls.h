#pragma once

#include <JuceHeader.h>
#include "IconButton.h"

// Open/Play-Pause/Stop buttons plus a position readout. Pure UI - MainComponent
// wires the button callbacks to the AudioEngine.
class TransportControls : public juce::Component
{
public:
    TransportControls();

    void resized() override;

    void setPlayingState(bool isPlaying);
    void setTimeText(const juce::String& text);

    juce::TextButton openButton { "Open..." };
    IconButton playPauseButton { IconButton::Icon::Play };
    IconButton stopButton { IconButton::Icon::Stop };
    juce::Label timeLabel;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
