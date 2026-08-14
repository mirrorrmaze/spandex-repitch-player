#pragma once

#include <JuceHeader.h>

// One-shot, best-effort background check against GitHub's Releases API for
// a newer published version than this build. Never blocks startup, never
// surfaces an error - a failed/blocked/rate-limited/no-releases-yet check
// just means onUpdateAvailable never fires, same outcome as "no update
// available." Not an auto-updater: a running VST3's DLL is locked by the
// host while loaded (a plugin can't replace its own file - that needs a
// separate updater process running before the plugin loads), and these
// installers aren't code-signed/notarized, so a background silent
// download-and-run is exactly the pattern AV/SmartScreen/Gatekeeper flag
// as malware. This only ever offers a link to the release page for the
// user to install manually.
class UpdateChecker : private juce::Thread,
                       private juce::AsyncUpdater
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    // Kicks off the background check. Safe to call at most once per
    // instance (matches the "on launch" use case - not designed to be
    // re-triggered).
    void checkAsync();

    // Fired on the message thread, only if a newer version was found.
    // Set this before calling checkAsync().
    std::function<void(juce::String newVersionTag, juce::String releaseUrl)> onUpdateAvailable;

private:
    void run() override;
    void handleAsyncUpdate() override;

    // Returns true if `remote` (e.g. "v0.2.0", or "0.2.0-macos-test") is a
    // higher X.Y.Z than `current` (e.g. "0.1.0") - a deliberately simple
    // integer compare, not a full semver implementation. Any component
    // that fails to parse as a leading integer reads as 0, and a missing
    // component reads as 0 too, so "0.2" > "0.1.5" behaves as expected.
    static bool isNewerVersion(const juce::String& remote, const juce::String& current);

    juce::String pendingVersionTag, pendingReleaseUrl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
