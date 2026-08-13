#include "WaveformComponent.h"
#include "MatrixEffects.h"

WaveformComponent::WaveformComponent(juce::AudioThumbnail& thumbnailToDisplay)
    : thumbnail(thumbnailToDisplay)
{
    thumbnail.addChangeListener(this);
    startTimerHz(30);
}

WaveformComponent::~WaveformComponent()
{
    thumbnail.removeChangeListener(this);
}

void WaveformComponent::ensureViewInitialised()
{
    if (visibleEnd <= visibleStart)
        resetView();
}

void WaveformComponent::resetView()
{
    visibleStart = 0.0;
    visibleEnd = thumbnail.getTotalLength() > 0.0 ? thumbnail.getTotalLength() : 1.0;
}

void WaveformComponent::clampView()
{
    const auto total = thumbnail.getTotalLength();
    if (total <= 0.0)
        return;

    if (visibleStart < 0.0)
    {
        visibleEnd -= visibleStart;
        visibleStart = 0.0;
    }
    if (visibleEnd > total)
    {
        const auto excess = visibleEnd - total;
        visibleStart = juce::jmax(0.0, visibleStart - excess);
        visibleEnd = total;
    }
}

double WaveformComponent::xToTime(int x) const
{
    if (getWidth() <= 0)
        return visibleStart;
    return visibleStart + ((double) x / (double) getWidth()) * (visibleEnd - visibleStart);
}

float WaveformComponent::timeToX(double seconds) const
{
    const auto span = visibleEnd - visibleStart;
    if (span <= 0.0)
        return 0.0f;
    return (float) (((seconds - visibleStart) / span) * (double) getWidth());
}

void WaveformComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(AppLookAndFeel::bg);

    if (thumbnail.getTotalLength() <= 0.0)
    {
        g.setColour(AppLookAndFeel::dim);
        g.setFont(16.0f);
        g.drawText("DROP OR OPEN AN AUDIO FILE TO BEGIN", bounds, juce::Justification::centred);
        MatrixEffects::drawScanlines(g, bounds);
        return;
    }

    ensureViewInitialised();

    g.setColour(AppLookAndFeel::accent);
    thumbnail.drawChannels(g, bounds.reduced(0, 4), visibleStart, visibleEnd, 1.0f);

    if (getLoopInSeconds && getLoopOutSeconds)
    {
        const auto loopOut = getLoopOutSeconds();
        if (loopOut > getLoopInSeconds())
        {
            // Monochrome theme: no hue left to distinguish this from the
            // waveform with, so it reads as an overlay/boundary lines
            // instead - a light fill wash plus solid edge lines rather than
            // a competing colour.
            const auto loopColour = AppLookAndFeel::bright;
            const auto x1 = timeToX(getLoopInSeconds());
            const auto x2 = timeToX(loopOut);
            g.setColour(loopColour.withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(x1, 0.0f, x2 - x1, (float) bounds.getHeight()));
            g.setColour(loopColour);
            g.drawLine(x1, 0.0f, x1, (float) bounds.getHeight(), 2.0f);
            g.drawLine(x2, 0.0f, x2, (float) bounds.getHeight(), 2.0f);
        }
    }

    if (getLengthSeconds && getPositionSeconds)
    {
        const auto length = getLengthSeconds();
        if (length > 0.0)
        {
            const auto x = timeToX(getPositionSeconds());
            g.setColour(juce::Colours::white);
            g.drawLine(x, 0.0f, x, (float) bounds.getHeight(), 2.0f);
        }
    }

    MatrixEffects::drawScanlines(g, bounds);
}

void WaveformComponent::resized() {}

void WaveformComponent::mouseDown(const juce::MouseEvent& e)
{
    ensureViewInitialised();

    if (e.mods.isShiftDown())
    {
        isPanning = true;
        panStartX = e.x;
        panStartVisibleStart = visibleStart;
        panStartVisibleEnd = visibleEnd;
        return;
    }

    if (onSeekSeconds)
        onSeekSeconds(xToTime(e.x));
}

void WaveformComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isPanning)
    {
        const auto span = panStartVisibleEnd - panStartVisibleStart;
        const auto deltaSeconds = -((double) (e.x - panStartX) / (double) juce::jmax(1, getWidth())) * span;
        visibleStart = panStartVisibleStart + deltaSeconds;
        visibleEnd = panStartVisibleEnd + deltaSeconds;
        clampView();
        repaint();
        return;
    }

    if (onSeekSeconds)
        onSeekSeconds(xToTime(e.x));
}

void WaveformComponent::mouseUp(const juce::MouseEvent&)
{
    isPanning = false;
}

void WaveformComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    resetView();
    repaint();
}

void WaveformComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    ensureViewInitialised();

    const auto cursorTime = xToTime(e.x);
    const auto span = visibleEnd - visibleStart;
    const auto zoomFactor = wheel.deltaY > 0.0f ? (1.0 / 1.25) : 1.25;
    const auto maxSpan = juce::jmax(thumbnail.getTotalLength(), 1.0);
    const auto newSpan = juce::jlimit(0.05, maxSpan, span * zoomFactor);

    const auto proportion = span > 0.0 ? (cursorTime - visibleStart) / span : 0.5;
    visibleStart = cursorTime - proportion * newSpan;
    visibleEnd = visibleStart + newSpan;
    clampView();
    repaint();
}

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}

void WaveformComponent::timerCallback()
{
    repaint();
}
