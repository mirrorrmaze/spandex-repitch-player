#include "HeaderTabs.h"
#include "AppLookAndFeel.h"
#include "Theme.h"

HeaderTabs::HeaderTabs()
{
    playerButton.onClick = [this] { setSelectedTab(Tab::Player); if (onTabSelected) onTabSelected(selected); };
    fxButton.onClick = [this] { setSelectedTab(Tab::Fx); if (onTabSelected) onTabSelected(selected); };
    eqButton.onClick = [this] { setSelectedTab(Tab::Eq); if (onTabSelected) onTabSelected(selected); };

    settingsButton.onClick = [this]
    {
        juce::PopupMenu themeMenu;
        for (auto* theme : Themes::all)
        {
            themeMenu.addItem(theme->name, true, theme->name == AppLookAndFeel::getCurrentThemeName(), [theme]
            {
                if (auto* laf = dynamic_cast<AppLookAndFeel*>(&juce::LookAndFeel::getDefaultLookAndFeel()))
                    laf->applyTheme(*theme);

                // sendLookAndFeelChange() recurses through a component's
                // whole child tree, but this menu isn't part of the main
                // window's tree - go through every desktop top-level
                // component instead so the whole app repaints with the new
                // palette, not just wherever the menu happened to attach.
                auto& desktop = juce::Desktop::getInstance();
                for (int i = 0; i < desktop.getNumComponents(); ++i)
                    if (auto* c = desktop.getComponent(i))
                        c->sendLookAndFeelChange();
            });
        }

        juce::PopupMenu mainMenu;
        mainMenu.addSubMenu("Theme", themeMenu);
        mainMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(settingsButton));
    };

    addAndMakeVisible(playerButton);
    addAndMakeVisible(fxButton);
    addAndMakeVisible(eqButton);
    addAndMakeVisible(settingsButton);

    updateButtonStates();
}

void HeaderTabs::lookAndFeelChanged()
{
    updateButtonStates();
}

void HeaderTabs::setSelectedTab(Tab tab)
{
    selected = tab;
    updateButtonStates();
}

void HeaderTabs::updateButtonStates()
{
    const auto& accent = AppLookAndFeel::bright;
    const auto& inactive = AppLookAndFeel::bg;

    playerButton.setColour(juce::TextButton::buttonColourId, selected == Tab::Player ? accent : inactive);
    fxButton.setColour(juce::TextButton::buttonColourId, selected == Tab::Fx ? accent : inactive);
    eqButton.setColour(juce::TextButton::buttonColourId, selected == Tab::Eq ? accent : inactive);

    playerButton.setColour(juce::TextButton::textColourOffId, selected == Tab::Player ? juce::Colours::black : AppLookAndFeel::bright);
    fxButton.setColour(juce::TextButton::textColourOffId, selected == Tab::Fx ? juce::Colours::black : AppLookAndFeel::bright);
    eqButton.setColour(juce::TextButton::textColourOffId, selected == Tab::Eq ? juce::Colours::black : AppLookAndFeel::bright);
}

void HeaderTabs::resized()
{
    auto bounds = getLocalBounds().reduced(4);
    settingsButton.setBounds(bounds.removeFromRight(44).reduced(2));

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.items.add(juce::FlexItem(playerButton).withWidth(100).withMargin(2));
    fb.items.add(juce::FlexItem(fxButton).withWidth(100).withMargin(2));
    fb.items.add(juce::FlexItem(eqButton).withWidth(100).withMargin(2));
    fb.performLayout(bounds);
}
