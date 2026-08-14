#pragma once

#include <JuceHeader.h>
#include <utility>

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

    // Ableton Sampler-style Start/End trim markers - the outer playable
    // range. Return < 0 for "no trim end set" from getTrimEndSeconds (Start
    // defaults to and is always drawn/draggable from 0).
    std::function<double()> getTrimStartSeconds;
    std::function<double()> getTrimEndSeconds;

    // Fired when the user clicks/drags to seek (not panning, not dragging a marker).
    std::function<void(double seconds)> onSeekSeconds;

    // Fired continuously while the user drags a loop or trim marker's handle
    // directly on the waveform.
    std::function<void(double seconds)> onDragLoopInSeconds;
    std::function<void(double seconds)> onDragLoopOutSeconds;
    std::function<void(double seconds)> onDragTrimStartSeconds;
    std::function<void(double seconds)> onDragTrimEndSeconds;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
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

    // Which draggable marker (if any) is under a given x - used by both
    // mouseDown (to grab a marker instead of seeking) and mouseMove (to show
    // a resize cursor over one). Trim Start/End are always present (default
    // to the file's edges); Loop In/Out only count as hit-testable once an
    // actual loop region exists (matches the Loop In/Out buttons already
    // being the way a region first gets created).
    enum class DragTarget { None, LoopIn, LoopOut, TrimStart, TrimEnd };
    DragTarget hitTestMarker(int x) const;

    // Resolves the trim range actually in effect right now: Start defaults
    // to 0 and End defaults to the file length when either getter is
    // missing or End hasn't been set past Start yet - the single place this
    // fallback logic lives, used identically by painting, hit-testing, and
    // drag-clamping so they can never disagree with each other.
    std::pair<double, double> resolvedTrimRange() const;

    juce::AudioThumbnail& thumbnail;

    double visibleStart = 0.0;
    double visibleEnd = -1.0; // -1 => not yet initialised

    bool isPanning = false;
    int panStartX = 0;
    double panStartVisibleStart = 0.0;
    double panStartVisibleEnd = 0.0;

    DragTarget draggingMarker = DragTarget::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
