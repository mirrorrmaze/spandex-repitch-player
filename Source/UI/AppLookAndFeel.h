#pragma once

#include <JuceHeader.h>
#include "Theme.h"

// Old-PC terminal theme: flat rectangular widgets with hairline borders
// instead of gradients or glows - closer to a CRT monitor or a DOS-era
// hardware panel than a modern plugin GUI. Simpler by design: no shadows,
// no soft edges, high-contrast two-tone everything. The actual colour
// values are switchable at runtime (see applyTheme()) - Default keeps the
// original charcoal/white look, other themes swap the whole palette.
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static juce::Colour bg;         // background
    static juce::Colour bright;     // primary text/foreground/active elements
    static juce::Colour dim;        // borders, inactive elements
    static juce::Colour dimmer;     // grid lines, faint decoration
    static juce::Colour accent;     // waveform trace + EQ spectrum fill

    AppLookAndFeel()
    {
        // Resolved once, up front, via Typeface::createSystemTypefaceFor -
        // NOT by constructing a Font and calling getTypefacePtr() on it,
        // which resolves lazily back through the active LookAndFeel's
        // getTypefaceForFont() and recurses infinitely (stack overflow -
        // crashed on launch the first time this was tried).
        monoRegular = juce::Typeface::createSystemTypefaceFor(juce::Font(juce::FontOptions("Consolas", 16.0f, juce::Font::plain)));
        monoBold = juce::Typeface::createSystemTypefaceFor(juce::Font(juce::FontOptions("Consolas", 16.0f, juce::Font::bold)));

        applyColours();
    }

    // Swaps the live palette (the bg/bright/dim/dimmer/accent statics above)
    // and re-runs every setColour() call that depends on them, so every
    // component that reads its colour from this LookAndFeel's defaults
    // (rather than a per-instance override - see the various
    // lookAndFeelChanged() overrides scattered through the UI for the
    // exceptions) picks the new palette up on its next repaint.
    void applyTheme(const Theme& theme)
    {
        currentThemeName = theme.name;
        bg = theme.bg;
        bright = theme.bright;
        dim = theme.dim;
        dimmer = theme.dimmer;
        accent = theme.accent;
        applyColours();
    }

    static const juce::String& getCurrentThemeName() { return currentThemeName; }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override
    {
        return f.isBold() ? monoBold : monoRegular;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
        const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = diameter * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const bool enabled = slider.isEnabled();

        g.setColour(bg);
        g.fillEllipse(juce::Rectangle<float>(radius * 1.5f, radius * 1.5f).withCentre(centre));
        g.setColour(enabled ? dim : dimmer);
        g.drawEllipse(juce::Rectangle<float>(radius * 1.5f, radius * 1.5f).withCentre(centre), 1.2f);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius * 0.95f, radius * 0.95f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(dimmer);
        g.strokePath(track, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

        if (sliderPos > 0.0001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, radius * 0.95f, radius * 0.95f, 0.0f,
                                    rotaryStartAngle, angle, true);
            g.setColour(enabled ? bright : dim);
            g.strokePath(valueArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        }

        // Sharp radar-sweep style pointer - a straight line, no rounded cap.
        juce::Path pointer;
        pointer.startNewSubPath(0.0f, -radius * 0.15f);
        pointer.lineTo(0.0f, -radius * 0.82f);
        g.setColour(enabled ? bright : dim);
        g.strokePath(pointer, juce::PathStrokeType(2.0f), juce::AffineTransform::rotation(angle).translated(centre));

        // Tick marks around the sweep, instrument-panel style.
        g.setColour(dim);
        for (int t = 0; t <= 8; ++t)
        {
            const float tickAngle = rotaryStartAngle + (float) t / 8.0f * (rotaryEndAngle - rotaryStartAngle);
            const auto p1 = centre.getPointOnCircumference(radius * 0.98f, tickAngle);
            const auto p2 = centre.getPointOnCircumference(radius * 1.08f, tickAngle);
            g.drawLine({ p1, p2 }, 1.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

        // A button whose fill differs from the plain bg default (e.g.
        // HeaderTabs setting buttonColourId to bright for the selected tab)
        // is signalling "treat me as filled/active" - honour whatever colour
        // was actually resolved rather than a hardcoded heuristic on its RGB
        // values, which used to assume the old Matrix theme's green "on"
        // colour specifically and silently stopped matching once the theme
        // became switchable (the selected tab's black text ended up on the
        // same dark bg as every other button, unreadable).
        const bool isFilled = backgroundColour != bg;

        if (isFilled)
        {
            g.setColour(shouldDrawButtonAsDown ? backgroundColour.darker(0.4f) : backgroundColour);
            g.fillRect(bounds);
        }
        else
        {
            g.setColour(bg);
            g.fillRect(bounds);
            g.setColour(shouldDrawButtonAsHighlighted ? bright : dim);
            g.drawRect(bounds, 1.5f);
        }

        if (shouldDrawButtonAsHighlighted && !isFilled)
        {
            g.setColour(dimmer);
            g.fillRect(bounds.reduced(2.0f));
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        const float boxSize = juce::jmin(16.0f, (float) button.getHeight() - 6.0f);
        const auto box = juce::Rectangle<float>(2.0f, (button.getHeight() - boxSize) * 0.5f, boxSize, boxSize);
        const bool on = button.getToggleState();

        g.setColour(bg);
        g.fillRect(box);
        if (on)
        {
            g.setColour(bright);
            g.fillRect(box.reduced(2.0f));
        }
        g.setColour(shouldDrawButtonAsHighlighted ? bright : dim);
        g.drawRect(box, 1.2f);

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font(juce::FontOptions(juce::jmin(15.0f, button.getHeight() * 0.55f))));
        g.drawFittedText(button.getButtonText(),
                          button.getLocalBounds().withTrimmedLeft((int) (boxSize + 8.0f)),
                          juce::Justification::centredLeft, 1);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(1.0f);
        g.setColour(bg);
        g.fillRect(bounds);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRect(bounds, 1.2f);

        const auto arrowZone = juce::Rectangle<float>((float) width - 22.0f, 0.0f, 16.0f, (float) height);
        juce::Path arrow;
        arrow.startNewSubPath(arrowZone.getX(), arrowZone.getCentreY() - 3.0f);
        arrow.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
        arrow.lineTo(arrowZone.getRight(), arrowZone.getCentreY() - 3.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(arrow, juce::PathStrokeType(1.6f));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto trackBounds = juce::Rectangle<float>((float) x, (float) y + (float) height * 0.5f - 2.0f, (float) width, 4.0f);
        g.setColour(bg);
        g.fillRect(trackBounds);
        g.setColour(dim);
        g.drawRect(trackBounds, 1.0f);

        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float originX = bipolar ? (float) slider.valueToProportionOfLength(0.0) * (float) width + (float) x : trackBounds.getX();
        const float fillLeft = juce::jmin(originX, sliderPos);
        const float fillRight = juce::jmax(originX, sliderPos);
        g.setColour(slider.isEnabled() ? bright : dim);
        g.fillRect(juce::Rectangle<float>(fillLeft, trackBounds.getY(), juce::jmax(2.0f, fillRight - fillLeft), trackBounds.getHeight()));

        const float thumbHeight = (float) height * 0.7f;
        const auto thumb = juce::Rectangle<float>(4.0f, thumbHeight).withCentre({ sliderPos, (float) y + (float) height * 0.5f });
        g.setColour(bright);
        g.fillRect(thumb);
    }

private:
    void applyColours()
    {
        setColour(juce::ResizableWindow::backgroundColourId, bg);

        setColour(juce::Slider::backgroundColourId, bg);
        setColour(juce::Slider::trackColourId, bright);
        setColour(juce::Slider::thumbColourId, bright);
        setColour(juce::Slider::textBoxTextColourId, bright);
        setColour(juce::Slider::textBoxBackgroundColourId, bg);
        setColour(juce::Slider::textBoxOutlineColourId, dim);

        setColour(juce::TextButton::buttonColourId, bg);
        setColour(juce::TextButton::buttonOnColourId, bright);
        setColour(juce::TextButton::textColourOffId, bright);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);

        setColour(juce::ToggleButton::textColourId, bright);
        setColour(juce::ToggleButton::tickColourId, bright);
        setColour(juce::ToggleButton::tickDisabledColourId, dim);

        setColour(juce::ComboBox::backgroundColourId, bg);
        setColour(juce::ComboBox::textColourId, bright);
        setColour(juce::ComboBox::outlineColourId, dim);
        setColour(juce::ComboBox::arrowColourId, bright);

        setColour(juce::Label::textColourId, bright);

        setColour(juce::PopupMenu::backgroundColourId, bg);
        setColour(juce::PopupMenu::textColourId, bright);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, bright);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::black);

        setColour(juce::TextEditor::backgroundColourId, bg);
        setColour(juce::TextEditor::textColourId, bright);
        setColour(juce::TextEditor::outlineColourId, dim);
    }

    static juce::String currentThemeName;

    juce::Typeface::Ptr monoRegular, monoBold;
};

inline juce::Colour AppLookAndFeel::bg { Themes::defaultTheme.bg };
inline juce::Colour AppLookAndFeel::bright { Themes::defaultTheme.bright };
inline juce::Colour AppLookAndFeel::dim { Themes::defaultTheme.dim };
inline juce::Colour AppLookAndFeel::dimmer { Themes::defaultTheme.dimmer };
inline juce::Colour AppLookAndFeel::accent { Themes::defaultTheme.accent };
inline juce::String AppLookAndFeel::currentThemeName { Themes::defaultTheme.name };
