#pragma once

#include <JuceHeader.h>

// Renders the loaded track's waveform via AudioThumbnail and a moving
// playhead. Supports zoom (mouse wheel), pan (shift-drag), and click/drag
// to seek and preview from anywhere in the file.
class WaveformComponent : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit WaveformComponent(juce::AudioThumbnail& thumbnailToDisplay);
    ~WaveformComponent() override;

    // Called each frame to know where to draw the playhead. Position/length in seconds.
    std::function<double()> getPositionSeconds;
    std::function<double()> getLengthSeconds;

    // Optional: return < 0 for "no loop out set" from getLoopOutSeconds.
    std::function<double()> getLoopInSeconds;
    std::function<double()> getLoopOutSeconds;

    // Fired when the user clicks/drags to seek (not panning).
    std::function<void(double seconds)> onSeekSeconds;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void ensureViewInitialised();
    void resetView();
    void clampView();
    double xToTime(int x) const;
    float timeToX(double seconds) const;

    juce::AudioThumbnail& thumbnail;

    double visibleStart = 0.0;
    double visibleEnd = -1.0; // -1 => not yet initialised

    bool isPanning = false;
    int panStartX = 0;
    double panStartVisibleStart = 0.0;
    double panStartVisibleEnd = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
