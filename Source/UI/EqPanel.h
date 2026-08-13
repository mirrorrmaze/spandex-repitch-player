#pragma once

#include <JuceHeader.h>
#include "EqCurveEditor.h"
#include "LabeledKnob.h"
#include "../Audio/EffectsChain.h"

// One band's controls: enable, filter type, and freq/gain/Q knobs. Eight of
// these sit in a strip below the EqCurveEditor graph.
class EqBandColumn : public juce::Component
{
public:
    EqBandColumn(EffectsChain& chain, int bandIndex);

    void resized() override;
    void refresh();

    std::function<void()> onChanged;

private:
    void lookAndFeelChanged() override;

    EffectsChain& fx;
    int index;

    juce::Label header;
    juce::ToggleButton enableToggle { "On" };
    juce::ComboBox typeBox;
    LabeledKnob freqKnob { "Freq" };
    LabeledKnob gainKnob { "Gain" };
    LabeledKnob qKnob { "Q" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBandColumn)
};

// Ableton EQ Eight / FabFilter Pro-Q style EQ tab: the curve+spectrum graph
// on top, an 8-band control strip underneath.
class EqPanel : public juce::Component
{
public:
    explicit EqPanel(EffectsChain& chain);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EffectsChain& fx;
    EqCurveEditor curveEditor;
    juce::OwnedArray<EqBandColumn> columns;
    juce::Rectangle<int> stripCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqPanel)
};
