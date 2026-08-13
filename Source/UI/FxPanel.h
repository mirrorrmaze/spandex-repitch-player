#pragma once

#include <JuceHeader.h>
#include "LabeledKnob.h"
#include "../Audio/EffectsChain.h"

// Reverb, granular Delay, Frequency Shifter, Smudge, and Gain controls,
// bound directly to an EffectsChain instance - there are enough individual
// parameters here that routing them all through decoupled callbacks (as the
// rest of the UI does) would be pure boilerplate, so this panel talks to
// the DSP object directly, like a plugin GUI bound to its processor.
class FxPanel : public juce::Component,
                private juce::Timer
{
public:
    explicit FxPanel(EffectsChain& chain);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshFromEngine();
    void timerCallback() override;
    void layoutSection(juce::Rectangle<int> area, juce::Label& title, juce::ToggleButton* enable,
                        std::initializer_list<LabeledKnob*> knobs, juce::ComboBox* modeBox = nullptr);

    EffectsChain& fx;

    juce::Rectangle<int> reverbCard, delayCard, shifterCard, smudgeCard, gainCard;

    juce::Label reverbTitle { {}, "Reverb" };
    juce::ToggleButton reverbEnable { "On" };
    LabeledKnob reverbSize { "Size" };
    LabeledKnob reverbDamping { "Damping" };
    LabeledKnob reverbWet { "Wet" };
    LabeledKnob reverbDry { "Dry" };
    LabeledKnob reverbWidth { "Width" };

    juce::Label delayTitle { {}, "Granular Delay" };
    juce::ToggleButton delayEnable { "On" };
    LabeledKnob delayTime { "Time (ms)" };
    LabeledKnob delayGrainSize { "Grain (ms)" };
    LabeledKnob delayDensity { "Density" };
    LabeledKnob delaySpread { "Spread (ms)" };
    LabeledKnob delayPitchScatter { "Pitch Scatter" };
    LabeledKnob delayFeedback { "Feedback" };
    LabeledKnob delayMix { "Mix" };

    juce::Label shifterTitle { {}, "Freq Shifter" };
    juce::ToggleButton shifterEnable { "On" };
    juce::ComboBox shifterModeBox;
    LabeledKnob shifterCoarse { "Coarse (Hz)" };
    LabeledKnob shifterFine { "Fine (Hz)" };
    LabeledKnob shifterSpread { "Spread (Hz)" };
    LabeledKnob shifterFeedback { "Feedback" };
    LabeledKnob shifterMix { "Mix" };

    juce::Label smudgeTitle { {}, "Smudge" };
    juce::ToggleButton smudgeEnable { "On" };
    LabeledKnob smudgeAmount { "Amount" };
    LabeledKnob smudgeRate { "Rate (ms)" };
    LabeledKnob smudgeFeedback { "Feedback" };

    // Debounces the Smudge rate knob - each window-size change costs a
    // ~46ms crossfade (see SmudgeProcessor), so committing on every
    // onValueChange during a drag through several sizes strung together
    // sounded like distinct steps rather than one clean change. Only the
    // value from the last drag tick within the debounce window is applied.
    float pendingSmudgeRateMs = 0.0f;

    juce::Label gainTitle { {}, "Gain" };
    LabeledKnob inputGain { "Input (dB)" };
    LabeledKnob outputGain { "Output (dB)" };
    LabeledKnob driveKnob { "Drive (dB)" };
    LabeledKnob compKnob { "Compression" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanel)
};
