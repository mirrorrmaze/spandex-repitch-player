#include "FxRoutingBar.h"
#include "AppLookAndFeel.h"

FxRoutingBar::FxRoutingBar()
{
    order = { EffectsChain::FxStage::Eq, EffectsChain::FxStage::Smudge, EffectsChain::FxStage::Lossy,
              EffectsChain::FxStage::Delay, EffectsChain::FxStage::Shifter, EffectsChain::FxStage::Reverb,
              EffectsChain::FxStage::Gain };
}

juce::String FxRoutingBar::nameForStage(EffectsChain::FxStage stage)
{
    switch (stage)
    {
        case EffectsChain::FxStage::Eq:      return "EQ";
        case EffectsChain::FxStage::Reverb:  return "Reverb";
        case EffectsChain::FxStage::Delay:   return "Delay";
        case EffectsChain::FxStage::Shifter: return "Shifter";
        case EffectsChain::FxStage::Smudge:  return "Smudge";
        case EffectsChain::FxStage::Lossy:   return "Lossy";
        case EffectsChain::FxStage::Gain:    return "Gain";
        default:                             return {};
    }
}

void FxRoutingBar::setOrder(const std::vector<EffectsChain::FxStage>& newOrder)
{
    if (newOrder.empty())
        return;
    order = newOrder;
    repaint();
}

juce::Rectangle<float> FxRoutingBar::slotBounds(int index) const
{
    const int count = (int) order.size();
    if (count == 0)
        return {};
    const float slotW = (float) getWidth() / (float) count;
    constexpr float margin = 3.0f;
    return juce::Rectangle<float>(slotW * (float) index + margin, 2.0f, slotW - margin * 2.0f, (float) getHeight() - 4.0f);
}

int FxRoutingBar::slotIndexForX(float x) const
{
    const int count = (int) order.size();
    if (count == 0)
        return 0;
    const float slotW = (float) getWidth() / (float) count;
    return juce::jlimit(0, count - 1, (int) (x / juce::jmax(1.0f, slotW)));
}

void FxRoutingBar::drawChip(juce::Graphics& g, juce::Rectangle<float> bounds, EffectsChain::FxStage stage, bool floating) const
{
    g.setColour(AppLookAndFeel::bg);
    g.fillRect(bounds);
    g.setColour(floating ? AppLookAndFeel::bright : AppLookAndFeel::dim);
    g.drawRect(bounds, floating ? 2.0f : 1.2f);
    g.setColour(floating ? AppLookAndFeel::bright : AppLookAndFeel::bright.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(nameForStage(stage), bounds, juce::Justification::centred);
}

void FxRoutingBar::paint(juce::Graphics& g)
{
    g.fillAll(AppLookAndFeel::bg);
    const int count = (int) order.size();
    if (count == 0)
        return;

    const float slotW = (float) getWidth() / (float) count;

    // Signal-flow arrows between slots, drawn first so chips sit on top.
    g.setColour(AppLookAndFeel::dim);
    for (int i = 1; i < count; ++i)
    {
        const float x = slotW * (float) i;
        const float midY = (float) getHeight() * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(x - 3.0f, midY - 4.0f);
        arrow.lineTo(x + 3.0f, midY);
        arrow.lineTo(x - 3.0f, midY + 4.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.4f));
    }

    for (int i = 0; i < count; ++i)
        if (i != draggingIndex)
            drawChip(g, slotBounds(i), order[(size_t) i], false);

    // The dragged chip is drawn last (on top), following the cursor
    // horizontally rather than snapping to a slot - the slot order updates
    // live underneath it as it crosses a neighbour.
    if (draggingIndex >= 0)
    {
        auto floatingBounds = slotBounds(draggingIndex).withX(dragCurrentX - dragGrabOffsetX);
        drawChip(g, floatingBounds, order[(size_t) draggingIndex], true);
    }
}

void FxRoutingBar::mouseDown(const juce::MouseEvent& e)
{
    const int idx = slotIndexForX((float) e.x);
    if (idx < 0 || idx >= (int) order.size())
        return;
    draggingIndex = idx;
    dragGrabOffsetX = (float) e.x - slotBounds(idx).getX();
    dragCurrentX = (float) e.x;
    repaint();
}

void FxRoutingBar::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingIndex < 0)
        return;

    dragCurrentX = (float) e.x;

    const float slotW = (float) getWidth() / (float) order.size();
    const float chipCentreX = dragCurrentX - dragGrabOffsetX + slotW * 0.5f;
    const int targetIdx = slotIndexForX(chipCentreX);

    if (targetIdx != draggingIndex)
    {
        auto stage = order[(size_t) draggingIndex];
        order.erase(order.begin() + draggingIndex);
        order.insert(order.begin() + targetIdx, stage);
        draggingIndex = targetIdx;
    }

    repaint();
}

void FxRoutingBar::mouseUp(const juce::MouseEvent&)
{
    if (draggingIndex < 0)
        return;
    draggingIndex = -1;
    repaint();
    if (onOrderChanged)
        onOrderChanged(order);
}
