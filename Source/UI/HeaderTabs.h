#pragma once

#include <JuceHeader.h>

// Player / FX / EQ tab bar that switches the central view. Transport,
// pitch/speed/warp, and export stay persistent regardless of which tab is
// active - only the middle of the window swaps.
class HeaderTabs : public juce::Component
{
public:
    enum class Tab { Player, Fx, Eq };

    HeaderTabs();

    void resized() override;
    void setSelectedTab(Tab tab);
    Tab getSelectedTab() const { return selected; }

    std::function<void(Tab)> onTabSelected;

private:
    void updateButtonStates();
    void lookAndFeelChanged() override;

    juce::TextButton playerButton { "Player" };
    juce::TextButton fxButton { "FX" };
    juce::TextButton eqButton { "EQ" };
    juce::TextButton settingsButton { "..." };
    Tab selected = Tab::Player;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderTabs)
};
