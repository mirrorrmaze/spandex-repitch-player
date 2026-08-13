#pragma once

#include <JuceHeader.h>

// A named colour palette for AppLookAndFeel::applyTheme() to switch to at
// runtime. Kept as a plain data struct (not baked into AppLookAndFeel
// itself) so adding a theme is just adding an entry to allThemes() below.
struct Theme
{
    juce::String name;
    juce::Colour bg;      // background
    juce::Colour bright;  // primary text/foreground/active elements
    juce::Colour dim;      // borders, inactive elements
    juce::Colour dimmer;   // grid lines, faint decoration
    juce::Colour accent;   // waveform trace + EQ spectrum fill - the one
                            // element that's allowed a distinct colour from
                            // the rest of an otherwise near-monochrome UI
};

namespace Themes
{
    // Charcoal/white, matching the look the app already shipped with -
    // just with the waveform and EQ spectrum pulled out to a burnt-orange
    // accent instead of plain white.
    inline const Theme defaultTheme {
        "Default",
        juce::Colour { 32, 32, 34 },
        juce::Colour { 255, 255, 255 },
        juce::Colour { 165, 165, 168 },
        juce::Colour { 75, 75, 78 },
        juce::Colour { 191, 87, 39 }
    };

    // The original black/phosphor-green retro-terminal look this app
    // shipped with for a while, brought back as a selectable theme instead
    // of the only option.
    inline const Theme matrix {
        "Matrix",
        juce::Colour { 8, 10, 8 },
        juce::Colour { 77, 255, 130 },
        juce::Colour { 40, 140, 65 },
        juce::Colour { 18, 60, 30 },
        juce::Colour { 170, 255, 60 }
    };

    inline const Theme amberTerminal {
        "Amber Terminal",
        juce::Colour { 12, 9, 6 },
        juce::Colour { 255, 176, 0 },
        juce::Colour { 150, 105, 20 },
        juce::Colour { 60, 42, 10 },
        juce::Colour { 255, 219, 140 }
    };

    inline const std::array<const Theme*, 3> all { &defaultTheme, &matrix, &amberTerminal };

    inline const Theme& findByName(const juce::String& name)
    {
        for (auto* t : all)
            if (t->name == name)
                return *t;
        return defaultTheme;
    }
}
