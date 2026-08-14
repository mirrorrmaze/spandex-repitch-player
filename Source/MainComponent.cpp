#include "MainComponent.h"
#include "UI/MatrixEffects.h"
#include <thread>

#if JucePlugin_Build_Standalone
namespace
{
    // Runs the export on a background thread with a modal progress dialog.
    // Uses the async launchThread()/threadComplete() pattern (rather than the
    // blocking runThread()) since modern JUCE disables modal loops by default.
    // Self-deletes in threadComplete(), as the class's own docs recommend.
    class ExportJob : public juce::ThreadWithProgressWindow
    {
    public:
        ExportJob(juce::AudioBuffer<float> sourceBufferIn, double sourceSampleRateIn,
                   ExportEngine::Settings settingsIn, juce::File outputFileIn)
            : ThreadWithProgressWindow("Exporting...", true, false),
              sourceBuffer(std::move(sourceBufferIn)),
              sourceSampleRate(sourceSampleRateIn),
              settings(settingsIn),
              outputFile(std::move(outputFileIn))
        {
        }

        void run() override
        {
            success = ExportEngine::exportToFile(
                sourceBuffer, sourceSampleRate, settings, outputFile,
                [this](float p) { setProgress((double) p); },
                errorMessage);
        }

        void threadComplete(bool userPressedCancel) override
        {
            if (!userPressedCancel)
            {
                if (success)
                    juce::Logger::writeToLog("Export succeeded: " + outputFile.getFullPathName());
                else
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Export failed", errorMessage);
            }

            delete this;
        }

        bool success = false;
        juce::String errorMessage;

    private:
        juce::AudioBuffer<float> sourceBuffer;
        double sourceSampleRate;
        ExportEngine::Settings settings;
        juce::File outputFile;
    };

    juce::String extensionForFormat(ExportEngine::Format format)
    {
        switch (format)
        {
            case ExportEngine::Format::WAV: return ".wav";
            case ExportEngine::Format::AIFF: return ".aiff";
            case ExportEngine::Format::FLAC: return ".flac";
            case ExportEngine::Format::MP3: return ".mp3";
        }
        return ".wav";
    }
}
#endif

MainComponent::MainComponent(AudioEngine& engineToUse)
    : audioEngine(engineToUse)
{
    addAndMakeVisible(waveform);
    addAndMakeVisible(transport);

    waveform.getPositionSeconds = [this] { return audioEngine.getPositionSeconds(); };
    waveform.getLengthSeconds = [this] { return audioEngine.getLengthSeconds(); };
    waveform.onSeekSeconds = [this](double seconds)
    {
        audioEngine.setPositionSeconds(seconds);
    };
    waveform.getLoopInSeconds = [this] { return audioEngine.getLoopInSeconds(); };
    waveform.getLoopOutSeconds = [this] { return audioEngine.getLoopOutSeconds(); };
    waveform.getTrimStartSeconds = [this] { return audioEngine.getTrimStartSeconds(); };
    waveform.getTrimEndSeconds = [this] { return audioEngine.getTrimEndSeconds(); };

    // Dragging a marker's handle directly on the waveform reuses the same
    // AudioEngine calls the Loop In/Out buttons already use - the buttons
    // set a point to the current playhead, dragging repositions it freely.
    waveform.onDragLoopInSeconds = [this](double seconds) { audioEngine.setLoopInSeconds(seconds); };
    waveform.onDragLoopOutSeconds = [this](double seconds) { audioEngine.setLoopOutSeconds(seconds); };
    waveform.onDragTrimStartSeconds = [this](double seconds) { audioEngine.setTrimStartSeconds(seconds); };
    waveform.onDragTrimEndSeconds = [this](double seconds) { audioEngine.setTrimEndSeconds(seconds); };

    transport.openButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open an audio file",
            juce::File(),
            "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

        const auto flags = juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
                openFile(file);
        });
    };

    transport.playPauseButton.onClick = [this]
    {
        if (!audioEngine.hasTrackLoaded())
            return;

        if (audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.play();

        transport.setPlayingState(audioEngine.isPlaying());
    };

    transport.stopButton.onClick = [this]
    {
        audioEngine.stop();
        transport.setPlayingState(false);
    };

    loopControls.loopInButton.onClick = [this]
    {
        audioEngine.setLoopInSeconds(audioEngine.getPositionSeconds());
    };
    loopControls.loopOutButton.onClick = [this]
    {
        audioEngine.setLoopOutSeconds(audioEngine.getPositionSeconds());
    };
    loopControls.loopToggle.onClick = [this]
    {
        audioEngine.setLooping(loopControls.loopToggle.getToggleState());
    };
    loopControls.loopModeBox.onChange = [this]
    {
        using LoopMode = StretchAudioSource::LoopMode;
        switch (loopControls.loopModeBox.getSelectedId())
        {
            case 2:  audioEngine.setLoopMode(LoopMode::PingPong); break;
            case 3:  audioEngine.setLoopMode(LoopMode::Reverse);  break;
            default: audioEngine.setLoopMode(LoopMode::Forward);  break;
        }
    };
    loopControls.crossfadeSlider.onValueChange = [this]
    {
        audioEngine.setLoopCrossfadeMs((float) loopControls.crossfadeSlider.getValue());
    };
    audioEngine.setLoopCrossfadeMs((float) loopControls.crossfadeSlider.getValue());
    addAndMakeVisible(loopControls);

    pitchFader.onValueChanged = [this](double semitones)
    {
        audioEngine.setPitchSemitones(semitones);
        if (audioEngine.isLinked())
            speedControl.setValue(audioEngine.getSpeedPercent(), juce::dontSendNotification);
    };
    addAndMakeVisible(pitchFader);

    speedControl.onValueChanged = [this](double percent)
    {
        audioEngine.setSpeedPercent(percent);
        if (audioEngine.isLinked())
            pitchFader.setValue(audioEngine.getPitchSemitones(), juce::dontSendNotification);
    };
    speedControl.onLinkChanged = [this](bool isLinked)
    {
        audioEngine.setLinked(isLinked);
        warpModeSelector.setEnabled(!isLinked);
        if (isLinked)
        {
            speedControl.setValue(audioEngine.getSpeedPercent(), juce::dontSendNotification);
            pitchFader.setEnabled(true);
        }
        else
        {
            pitchFader.setEnabled(audioEngine.getWarpMode() != StretchAudioSource::WarpMode::Paulstretch);
        }
    };
    addAndMakeVisible(speedControl);

    warpModeSelector.onModeChanged = [this](StretchAudioSource::WarpMode mode)
    {
        audioEngine.setWarpMode(mode);
        // Paulstretch is time-only; the pitch fader has no effect while it's active.
        pitchFader.setEnabled(mode != StretchAudioSource::WarpMode::Paulstretch);
    };
    warpModeSelector.setEnabled(!audioEngine.isLinked());
    addAndMakeVisible(warpModeSelector);

#if JucePlugin_Build_Standalone
    exportPanel.exportButton.onClick = [this] { startExport(); };
    addAndMakeVisible(exportPanel);
#endif

    headerTabs.onTabSelected = [this](HeaderTabs::Tab)
    {
        updateTabVisibility();
        resized();
    };
    addAndMakeVisible(headerTabs);
    addAndMakeVisible(fxPanel);
    addAndMakeVisible(eqPanel);
    updateTabVisibility();

    // Quiet, best-effort background check - see UpdateChecker's own header
    // comment for why this only ever offers a link, never auto-installs.
    updateChecker.onUpdateAvailable = [this](juce::String versionTag, juce::String releaseUrl)
    {
        headerTabs.setUpdateAvailable(versionTag, releaseUrl);
    };
    updateChecker.checkAsync();

    setSize(1300, 850);
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    startTimerHz(15);
}

MainComponent::~MainComponent() = default;

void MainComponent::openFile(const juce::File& file, bool autoPlay)
{
    juce::String error;
    if (audioEngine.loadFile(file, error))
    {
        currentFile = file;
        juce::Logger::writeToLog("Loaded: " + file.getFullPathName()
                                  + "  length=" + juce::String(audioEngine.getLengthSeconds(), 2) + "s");

        if (autoPlay)
            audioEngine.play();

        transport.setPlayingState(audioEngine.isPlaying());
    }
    else
    {
        juce::Logger::writeToLog("Load failed: " + error);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Could not open file", error);
    }
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    if (!files.isEmpty())
        openFile(juce::File(files[0]));
}

#if JucePlugin_Build_Standalone
void MainComponent::startExport()
{
    if (!audioEngine.hasTrackLoaded())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Nothing to export", "Open a track first.");
        return;
    }

    const auto format = exportPanel.getFormat();
    const auto ext = extensionForFormat(format);

    auto defaultFile = currentFile.existsAsFile()
        ? currentFile.getSiblingFile(currentFile.getFileNameWithoutExtension() + "_repitch" + ext)
        : juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("export" + ext);

    fileChooser = std::make_unique<juce::FileChooser>("Export as...", defaultFile, "*" + ext);

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(chooserFlags, [this, format](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        ExportEngine::Settings settings;
        settings.pitchSemitones = audioEngine.getPitchSemitones();
        settings.speedPercent = audioEngine.getSpeedPercent();
        settings.linked = audioEngine.isLinked();
        settings.warpMode = audioEngine.getWarpMode();
        settings.format = format;
        settings.mp3BitrateKbps = exportPanel.getMp3Bitrate();

        auto* job = new ExportJob(audioEngine.copySourceBuffer(), audioEngine.getSourceSampleRate(), settings, file);
        job->launchThread();
    });
}
#endif

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(AppLookAndFeel::bg);

    if (!controlsCard.isEmpty())
        MatrixEffects::drawCardFrame(g, controlsCard);

    MatrixEffects::drawScanlines(g, getLocalBounds());
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto transportArea = bounds.removeFromBottom(64);
    transport.setBounds(transportArea);

#if JucePlugin_Build_Standalone
    auto exportArea = bounds.removeFromBottom(60).reduced(8, 4);
    loopControls.setBounds(exportArea.removeFromRight(480));
    exportPanel.setBounds(exportArea);
#else
    auto loopArea = bounds.removeFromBottom(60).reduced(8, 4);
    loopControls.setBounds(loopArea.removeFromRight(480));
#endif

    controlsCard = bounds.removeFromBottom(120).reduced(8, 4);
    auto controlArea = controlsCard.reduced(12, 10);
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.items.add(juce::FlexItem(pitchFader).withFlex(1.0f).withMargin(4));
    fb.items.add(juce::FlexItem(speedControl).withFlex(1.0f).withMargin(4));
    fb.items.add(juce::FlexItem(warpModeSelector).withWidth(240).withMargin(4));
    fb.performLayout(controlArea);

    auto tabBarArea = bounds.removeFromTop(40);
    headerTabs.setBounds(tabBarArea);
    bounds.removeFromTop(6);

    switch (headerTabs.getSelectedTab())
    {
        case HeaderTabs::Tab::Player:
            waveform.setBounds(bounds.reduced(4));
            break;
        case HeaderTabs::Tab::Fx:
            fxPanel.setBounds(bounds);
            break;
        case HeaderTabs::Tab::Eq:
            eqPanel.setBounds(bounds);
            break;
    }

    repaint();
}

void MainComponent::updateTabVisibility()
{
    const auto tab = headerTabs.getSelectedTab();
    waveform.setVisible(tab == HeaderTabs::Tab::Player);
    fxPanel.setVisible(tab == HeaderTabs::Tab::Fx);
    eqPanel.setVisible(tab == HeaderTabs::Tab::Eq);
    loopControls.setVisible(tab == HeaderTabs::Tab::Player);
}

void MainComponent::timerCallback()
{
    transport.setTimeText(formatTime(audioEngine.getPositionSeconds()) + " / "
                           + formatTime(audioEngine.getLengthSeconds()));

    if (!audioEngine.isPlaying())
        transport.setPlayingState(false);

    if (audioEngine.isPlaying() && (++diagnosticFrameCount % 15) == 0)
    {
        juce::Logger::writeToLog("pos=" + juce::String(audioEngine.getPositionSeconds(), 2)
                                  + "s / " + juce::String(audioEngine.getLengthSeconds(), 2) + "s");
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (!audioEngine.hasTrackLoaded())
            return true;

        if (audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.play();

        transport.setPlayingState(audioEngine.isPlaying());
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        audioEngine.setPositionSeconds(0.0);
        return true;
    }

    return false;
}

juce::String MainComponent::formatTime(double seconds)
{
    if (seconds < 0.0 || std::isnan(seconds))
        seconds = 0.0;

    const auto totalSeconds = (int) seconds;
    return juce::String::formatted("%02d:%02d", totalSeconds / 60, totalSeconds % 60);
}
