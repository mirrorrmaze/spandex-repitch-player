#pragma once

#include <JuceHeader.h>
#include "../Audio/EffectsChain.h"

// The graph at the top of the EQ tab: log-frequency x-axis, dB y-axis, a
// live spectrum analyzer behind everything, the combined response curve on
// top, and one draggable node per enabled band - drag to set freq/gain,
// scroll to set Q. Modelled on Ableton EQ Eight / FabFilter Pro-Q.
class EqCurveEditor : public juce::Component, private juce::Timer
{
public:
    explicit EqCurveEditor(EffectsChain& chain);
    ~EqCurveEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Fired when a band's freq/gain/Q changes via graph interaction, so the
    // band strip's numeric knobs can be kept in sync.
    std::function<void(int bandIndex)> onBandChanged;
    // Fired when a band is clicked, so the band strip can highlight it.
    std::function<void(int bandIndex)> onBandSelected;

    void setSelectedBand(int index) { selectedBand = index; repaint(); }
    static juce::Colour colourForBand(int index);

private:
    void timerCallback() override { repaint(); }

    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float gainToY(float gainDb) const;
    float yToGain(float y) const;
    int findNodeNear(juce::Point<float> position) const;

    EffectsChain& fx;
    int draggingBand = -1;
    int hoveredBand = -1;
    int selectedBand = -1;

    static constexpr float minFreqHz = 20.0f;
    static constexpr float maxFreqHz = 20000.0f;
    static constexpr float minDb = -24.0f;
    static constexpr float maxDb = 24.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqCurveEditor)
};
