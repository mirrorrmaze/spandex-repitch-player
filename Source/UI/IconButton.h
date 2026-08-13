#pragma once

#include <JuceHeader.h>
#include "AppLookAndFeel.h"

// A flat icon button matching the app's hairline-bordered look, for
// transport controls (play/pause/stop) where a small glyph reads faster
// at a glance than text. Reuses the active LookAndFeel's button background
// (so it stays theme-consistent) and just draws a glyph on top.
class IconButton : public juce::Button
{
public:
    enum class Icon { Play, Pause, Stop };

    explicit IconButton(Icon iconToShow) : juce::Button({}), icon(iconToShow) {}

    void setIcon(Icon newIcon)
    {
        if (icon != newIcon)
        {
            icon = newIcon;
            repaint();
        }
    }

private:
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        getLookAndFeel().drawButtonBackground(g, *this, findColour(juce::TextButton::buttonColourId),
                                               shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        auto bounds = getLocalBounds().toFloat().reduced(getWidth() * 0.32f, getHeight() * 0.28f);
        g.setColour(isEnabled() ? AppLookAndFeel::bright : AppLookAndFeel::dim);

        switch (icon)
        {
            case Icon::Play:
            {
                juce::Path p;
                p.addTriangle(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getBottom(),
                              bounds.getRight(), bounds.getCentreY());
                g.fillPath(p);
                break;
            }
            case Icon::Pause:
            {
                const float barWidth = bounds.getWidth() * 0.32f;
                g.fillRect(bounds.getX(), bounds.getY(), barWidth, bounds.getHeight());
                g.fillRect(bounds.getRight() - barWidth, bounds.getY(), barWidth, bounds.getHeight());
                break;
            }
            case Icon::Stop:
                g.fillRect(bounds);
                break;
        }
    }

    Icon icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};
