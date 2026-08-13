#include "EqCurveEditor.h"
#include "MatrixEffects.h"

namespace
{
    bool bandTypeHasGain(EffectsChain::FilterType t)
    {
        using FT = EffectsChain::FilterType;
        return t == FT::LowShelf || t == FT::HighShelf || t == FT::Bell;
    }
}

EqCurveEditor::EqCurveEditor(EffectsChain& chain)
    : fx(chain)
{
    setWantsKeyboardFocus(false);
    startTimerHz(24);
}

EqCurveEditor::~EqCurveEditor() = default;

juce::Colour EqCurveEditor::colourForBand(int index)
{
    // Bands are told apart by brightness variation on the theme's "bright"
    // colour rather than assigning each an unrelated hue - deliberately
    // NOT a function-local static, since AppLookAndFeel::bright can change
    // at runtime when the theme switches and this needs to reflect that on
    // its very next call, not whatever was cached at first use.
    const juce::Colour palette[] = {
        AppLookAndFeel::bright.withBrightness(1.00f),
        AppLookAndFeel::bright.withBrightness(0.82f),
        AppLookAndFeel::bright.withBrightness(0.64f),
        AppLookAndFeel::bright.withBrightness(0.46f),
        AppLookAndFeel::bright.withBrightness(0.90f),
        AppLookAndFeel::bright.withBrightness(0.73f),
        AppLookAndFeel::bright.withBrightness(0.55f),
        AppLookAndFeel::bright.withBrightness(0.37f)
    };
    return palette[(size_t) (index % 8)];
}

float EqCurveEditor::freqToX(float freqHz) const
{
    const float logMin = std::log10(minFreqHz), logMax = std::log10(maxFreqHz);
    const float t = (std::log10(juce::jlimit(minFreqHz, maxFreqHz, freqHz)) - logMin) / (logMax - logMin);
    return t * (float) getWidth();
}

float EqCurveEditor::xToFreq(float x) const
{
    const float logMin = std::log10(minFreqHz), logMax = std::log10(maxFreqHz);
    const float t = juce::jlimit(0.0f, 1.0f, x / (float) juce::jmax(1, getWidth()));
    return std::pow(10.0f, logMin + t * (logMax - logMin));
}

float EqCurveEditor::gainToY(float gainDb) const
{
    const float t = (juce::jlimit(minDb, maxDb, gainDb) - minDb) / (maxDb - minDb);
    return (float) getHeight() * (1.0f - t);
}

float EqCurveEditor::yToGain(float y) const
{
    const float t = 1.0f - juce::jlimit(0.0f, 1.0f, y / (float) juce::jmax(1, getHeight()));
    return minDb + t * (maxDb - minDb);
}

int EqCurveEditor::findNodeNear(juce::Point<float> position) const
{
    for (int i = 0; i < EffectsChain::numEqBands; ++i)
    {
        auto& band = fx.getEqBand(i);
        if (!band.enabled.load())
            continue;

        const float freq = band.frequency.load();
        const float gain = bandTypeHasGain(band.type.load()) ? band.gainDb.load() : 0.0f;
        const juce::Point<float> nodePos(freqToX(freq), gainToY(gain));

        if (nodePos.getDistanceFrom(position) < 12.0f)
            return i;
    }
    return -1;
}

void EqCurveEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(AppLookAndFeel::bg);

    g.setFont(11.0f);
    for (float f : { 30.0f, 100.0f, 300.0f, 1000.0f, 3000.0f, 10000.0f })
    {
        const float x = freqToX(f);
        g.setColour(AppLookAndFeel::dimmer);
        g.drawVerticalLine((int) x, 0.0f, bounds.getHeight());
        g.setColour(AppLookAndFeel::dim);
        const auto label = f >= 1000.0f ? juce::String((int) (f / 1000.0f)) + "k" : juce::String((int) f);
        g.drawText(label, (int) x + 3, (int) bounds.getHeight() - 16, 40, 14, juce::Justification::left);
    }

    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
    {
        const float y = gainToY(db);
        g.setColour(db == 0.0f ? AppLookAndFeel::dim : AppLookAndFeel::dimmer);
        g.drawHorizontalLine((int) y, 0.0f, bounds.getWidth());
    }

    const auto spectrum = fx.getLatestSpectrum();
    if (!spectrum.empty())
    {
        // Auto-normalised to the signal's own tracked peak (fast attack,
        // slow release) rather than a fixed dB range, so quiet material
        // still shows detail and loud material doesn't just pin flat
        // against the top of the graph.
        constexpr float displayRangeDb = 60.0f;
        const float peakDb = fx.getSpectrumPeakDb();
        const float floorDb = peakDb - displayRangeDb;

        const double nyquist = fx.getAnalyzerSampleRate() * 0.5;
        juce::Path spectrumPath;
        bool started = false;

        for (size_t i = 1; i < spectrum.size(); ++i)
        {
            const auto freq = (float) (nyquist * (double) i / (double) spectrum.size());
            if (freq < minFreqHz)
                continue;

            const float x = freqToX(freq);
            const float normalised = juce::jlimit(0.0f, 1.0f, (spectrum[i] - floorDb) / displayRangeDb);
            const float y = bounds.getHeight() * (1.0f - normalised * 0.9f);

            if (!started)
            {
                spectrumPath.startNewSubPath(x, bounds.getHeight());
                spectrumPath.lineTo(x, y);
                started = true;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }
        }

        if (started)
        {
            spectrumPath.lineTo(bounds.getWidth(), bounds.getHeight());
            spectrumPath.closeSubPath();
            g.setColour(AppLookAndFeel::accent.withAlpha(0.35f));
            g.fillPath(spectrumPath);
        }
    }

    juce::Path curve;
    for (int x = 0; x <= (int) bounds.getWidth(); x += 2)
    {
        const float freq = xToFreq((float) x);
        const float y = gainToY(fx.getMagnitudeResponseDb(freq));
        if (x == 0)
            curve.startNewSubPath((float) x, y);
        else
            curve.lineTo((float) x, y);
    }
    g.setColour(AppLookAndFeel::bright);
    g.strokePath(curve, juce::PathStrokeType(2.0f));

    for (int i = 0; i < EffectsChain::numEqBands; ++i)
    {
        auto& band = fx.getEqBand(i);
        if (!band.enabled.load())
            continue;

        const float freq = band.frequency.load();
        const float gain = bandTypeHasGain(band.type.load()) ? band.gainDb.load() : 0.0f;
        const float x = freqToX(freq);
        const float y = gainToY(gain);
        const auto colour = colourForBand(i);

        if (i == selectedBand)
        {
            g.setColour(colour.withAlpha(0.25f));
            g.fillEllipse(x - 11, y - 11, 22, 22);
        }

        g.setColour(colour.withAlpha(i == hoveredBand || i == draggingBand ? 1.0f : 0.85f));
        g.drawEllipse(x - 7, y - 7, 14, 14, 2.0f);
        g.fillEllipse(x - 3, y - 3, 6, 6);
    }

    MatrixEffects::drawScanlines(g, getLocalBounds());
}

void EqCurveEditor::resized() {}

void EqCurveEditor::mouseDown(const juce::MouseEvent& e)
{
    const int idx = findNodeNear(e.position);
    draggingBand = idx;
    if (idx >= 0)
    {
        selectedBand = idx;
        if (onBandSelected)
            onBandSelected(idx);
    }
    repaint();
}

void EqCurveEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    fx.setEqBandFrequency(draggingBand, xToFreq(e.position.x));

    auto& band = fx.getEqBand(draggingBand);
    if (bandTypeHasGain(band.type.load()))
        fx.setEqBandGainDb(draggingBand, yToGain(e.position.y));

    if (onBandChanged)
        onBandChanged(draggingBand);
    repaint();
}

void EqCurveEditor::mouseUp(const juce::MouseEvent&)
{
    draggingBand = -1;
}

void EqCurveEditor::mouseMove(const juce::MouseEvent& e)
{
    const int idx = findNodeNear(e.position);
    if (idx != hoveredBand)
    {
        hoveredBand = idx;
        repaint();
    }
}

void EqCurveEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int idx = findNodeNear(e.position);
    if (idx < 0)
        return;

    auto& band = fx.getEqBand(idx);
    const float newQ = juce::jlimit(0.1f, 18.0f, band.q.load() * (wheel.deltaY > 0.0f ? 1.12f : 1.0f / 1.12f));
    fx.setEqBandQ(idx, newQ);

    if (onBandChanged)
        onBandChanged(idx);
    repaint();
}
