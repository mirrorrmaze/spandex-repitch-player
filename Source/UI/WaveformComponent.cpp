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

std::pair<double, double> WaveformComponent::resolvedTrimRange() const
{
    const double length = getLengthSeconds ? getLengthSeconds() : thumbnail.getTotalLength();
    const double trimStartS = getTrimStartSeconds ? getTrimStartSeconds() : 0.0;
    double trimEndS = length;
    if (getTrimEndSeconds)
    {
        const auto rawEnd = getTrimEndSeconds();
        trimEndS = rawEnd > trimStartS ? rawEnd : length;
    }
    return { trimStartS, trimEndS };
}

WaveformComponent::DragTarget WaveformComponent::hitTestMarker(int x) const
{
    constexpr float tolerance = 6.0f;
    const float fx = (float) x;

    if (getLoopInSeconds && getLoopOutSeconds)
    {
        const auto loopIn = getLoopInSeconds();
        const auto loopOut = getLoopOutSeconds();
        if (loopOut > loopIn)
        {
            if (std::abs(fx - timeToX(loopIn)) <= tolerance)
                return DragTarget::LoopIn;
            if (std::abs(fx - timeToX(loopOut)) <= tolerance)
                return DragTarget::LoopOut;
        }
    }

    const auto [trimStartS, trimEndS] = resolvedTrimRange();
    if (std::abs(fx - timeToX(trimStartS)) <= tolerance)
        return DragTarget::TrimStart;
    if (std::abs(fx - timeToX(trimEndS)) <= tolerance)
        return DragTarget::TrimEnd;

    return DragTarget::None;
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

    const auto [trimStartS, trimEndS] = resolvedTrimRange();
    const bool trimActive = trimStartS > 0.0001 || trimEndS < thumbnail.getTotalLength() - 0.0001;

    if (trimActive)
    {
        // Dim the excluded head/tail so the playable range reads at a
        // glance, rather than the trim markers being the only clue.
        g.setColour(AppLookAndFeel::bg.withAlpha(0.65f));
        if (trimStartS > visibleStart)
            g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, timeToX(trimStartS), (float) bounds.getHeight()));
        if (trimEndS < visibleEnd)
            g.fillRect(juce::Rectangle<float>(timeToX(trimEndS), 0.0f, (float) bounds.getWidth() - timeToX(trimEndS), (float) bounds.getHeight()));
    }

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

    // Trim Start/End: a thin line plus a small inward-pointing flag at the
    // top, so they read as distinct grab handles from the loop's solid bars
    // even in this theme's monochrome palette.
    {
        g.setColour(AppLookAndFeel::dim);
        constexpr float flagH = 9.0f, flagW = 8.0f;

        const auto xs = timeToX(trimStartS);
        g.drawLine(xs, 0.0f, xs, (float) bounds.getHeight(), 1.5f);
        juce::Path startFlag;
        startFlag.addTriangle(xs, 0.0f, xs, flagH, xs + flagW, 0.0f);
        g.fillPath(startFlag);

        const auto xe = timeToX(trimEndS);
        g.drawLine(xe, 0.0f, xe, (float) bounds.getHeight(), 1.5f);
        juce::Path endFlag;
        endFlag.addTriangle(xe, 0.0f, xe, flagH, xe - flagW, 0.0f);
        g.fillPath(endFlag);
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

    draggingMarker = hitTestMarker(e.x);
    if (draggingMarker != DragTarget::None)
        return;

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

    if (draggingMarker != DragTarget::None)
    {
        constexpr double minGapSeconds = 0.02;
        const double t = xToTime(e.x);
        const double length = getLengthSeconds ? getLengthSeconds() : thumbnail.getTotalLength();

        switch (draggingMarker)
        {
            case DragTarget::LoopIn:
                if (onDragLoopInSeconds && getLoopOutSeconds)
                {
                    const double maxT = juce::jmax(0.0, getLoopOutSeconds() - minGapSeconds);
                    onDragLoopInSeconds(juce::jlimit(0.0, maxT, t));
                }
                break;

            case DragTarget::LoopOut:
                if (onDragLoopOutSeconds && getLoopInSeconds)
                {
                    const double minT = getLoopInSeconds() + minGapSeconds;
                    onDragLoopOutSeconds(juce::jlimit(minT, juce::jmax(minT, length), t));
                }
                break;

            case DragTarget::TrimStart:
                if (onDragTrimStartSeconds)
                {
                    const auto [ignoredStart, trimEndS] = resolvedTrimRange();
                    juce::ignoreUnused(ignoredStart);
                    onDragTrimStartSeconds(juce::jlimit(0.0, juce::jmax(0.0, trimEndS - minGapSeconds), t));
                }
                break;

            case DragTarget::TrimEnd:
                if (onDragTrimEndSeconds)
                {
                    const auto [trimStartS, ignoredEnd] = resolvedTrimRange();
                    juce::ignoreUnused(ignoredEnd);
                    const double minT = trimStartS + minGapSeconds;
                    onDragTrimEndSeconds(juce::jlimit(minT, juce::jmax(minT, length), t));
                }
                break;

            default:
                break;
        }
        repaint();
        return;
    }

    if (onSeekSeconds)
        onSeekSeconds(xToTime(e.x));
}

void WaveformComponent::mouseUp(const juce::MouseEvent&)
{
    isPanning = false;
    draggingMarker = DragTarget::None;
}

void WaveformComponent::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(hitTestMarker(e.x) != DragTarget::None
        ? juce::MouseCursor::LeftRightResizeCursor
        : juce::MouseCursor::NormalCursor);
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
