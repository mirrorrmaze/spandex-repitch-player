#pragma once

#include <JuceHeader.h>
#include "../Audio/EffectsChain.h"

// A horizontal row of draggable "chip" tokens, one per reorderable FX
// chain stage (EQ/Reverb/Delay/Shifter/Smudge/Lossy/Gain), showing the
// chain's current processing order - the routing counterpart to
// EffectsChain::setChainOrder()/getChainOrder(). Drag a chip past a
// neighbour to swap their order; drop to commit.
class FxRoutingBar : public juce::Component
{
public:
    FxRoutingBar();

    // Sets the displayed order without firing onOrderChanged - for
    // initializing the bar from EffectsChain::getChainOrder().
    void setOrder(const std::vector<EffectsChain::FxStage>& newOrder);
    std::vector<EffectsChain::FxStage> getOrder() const { return order; }

    // Fired once, on mouseUp, with the final order after a drag.
    std::function<void(const std::vector<EffectsChain::FxStage>&)> onOrderChanged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    static juce::String nameForStage(EffectsChain::FxStage stage);
    juce::Rectangle<float> slotBounds(int index) const;
    int slotIndexForX(float x) const;
    void drawChip(juce::Graphics& g, juce::Rectangle<float> bounds, EffectsChain::FxStage stage, bool floating) const;

    std::vector<EffectsChain::FxStage> order;

    int draggingIndex = -1;
    float dragGrabOffsetX = 0.0f;
    float dragCurrentX = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxRoutingBar)
};
