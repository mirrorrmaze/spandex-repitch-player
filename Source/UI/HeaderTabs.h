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

    // Called (at most once, from UpdateChecker's callback) when a newer
    // release is found: tints the settings button to the accent colour as
    // a passive, always-visible cue, and adds a menu item that opens the
    // release page in the default browser when clicked.
    void setUpdateAvailable(const juce::String& versionTag, const juce::String& releaseUrl);

private:
    void updateButtonStates();
    void lookAndFeelChanged() override;

    juce::TextButton playerButton { "Player" };
    juce::TextButton fxButton { "FX" };
    juce::TextButton eqButton { "EQ" };
    juce::TextButton settingsButton { "..." };
    Tab selected = Tab::Player;

    bool updateAvailable = false;
    juce::String updateVersionTag, updateReleaseUrl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderTabs)
};
