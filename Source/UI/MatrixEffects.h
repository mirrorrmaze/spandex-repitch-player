#pragma once

#include <JuceHeader.h>
#include "AppLookAndFeel.h"

// Small shared decoration helpers for the Matrix/old-PC terminal theme, used
// by every tab's paint() so the CRT cues (scanlines, hairline card frames)
// stay consistent without repeating the same drawing code three times.
namespace MatrixEffects
{
    // Faint horizontal lines across the given area - the classic CRT
    // scanline cue. Cheap (one draw call per pair of pixel rows), static
    // (no animation), deliberately subtle so it reads as texture, not noise.
    inline void drawScanlines(juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        for (int y = area.getY(); y < area.getBottom(); y += 3)
            g.drawHorizontalLine(y, (float) area.getX(), (float) area.getRight());
    }

    // A flat card frame: black fill, thin green hairline border - no fill
    // shading, no shadow, matching the flat/schematic look of the theme.
    inline void drawCardFrame(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        g.setColour(AppLookAndFeel::bg);
        g.fillRect(bounds);
        g.setColour(AppLookAndFeel::dim);
        g.drawRect(bounds.toFloat(), 1.0f);
    }
}
