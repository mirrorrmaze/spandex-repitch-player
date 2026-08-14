#pragma once

#include <JuceHeader.h>
#include "Audio/AudioEngine.h"
#include "UI/WaveformComponent.h"
#include "UI/TransportControls.h"
#include "UI/LoopControls.h"
#include "UI/PitchFader.h"
#include "UI/SpeedControl.h"
#include "UI/WarpModeSelector.h"
#include "UI/ExportPanel.h"
#include "UI/HeaderTabs.h"
#include "UI/FxPanel.h"
#include "UI/EqPanel.h"
#include "UpdateChecker.h"

class MainComponent : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    explicit MainComponent(AudioEngine& engineToUse);
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Used by the Open button, drag-and-drop, and command-line file args.
    void openFile(const juce::File& file, bool autoPlay = false);

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    static juce::String formatTime(double seconds);

    AudioEngine& audioEngine;
    WaveformComponent waveform { audioEngine.getThumbnail() };
    TransportControls transport;
    LoopControls loopControls;
    PitchFader pitchFader;
    SpeedControl speedControl;
    WarpModeSelector warpModeSelector;
#if JucePlugin_Build_Standalone
    // Exporting to a file only makes sense standalone - as a VST3 you'd
    // normally just record/resample the plugin's output on another track in
    // the host instead.
    ExportPanel exportPanel;
#endif
    HeaderTabs headerTabs;
    UpdateChecker updateChecker;
    FxPanel fxPanel { audioEngine.getEffectsChain() };
    EqPanel eqPanel { audioEngine.getEffectsChain() };

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File currentFile;
    int diagnosticFrameCount = 0;

#if JucePlugin_Build_Standalone
    void startExport();
#endif
    void updateTabVisibility();

    juce::Rectangle<int> controlsCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
