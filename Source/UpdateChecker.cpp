#include "UpdateChecker.h"

namespace
{
    constexpr const char* releasesApiUrl = "https://api.github.com/repos/mirrorrmaze/spandex-repitch-player/releases/latest";
    constexpr const char* releasesPageUrl = "https://github.com/mirrorrmaze/spandex-repitch-player/releases";
}

UpdateChecker::UpdateChecker() : Thread("SPANDEX Update Check") {}

UpdateChecker::~UpdateChecker()
{
    // Blocks until run() has actually returned (it polls threadShouldExit()
    // at each step below), so by the time this destructor's body finishes,
    // nothing can still be touching pendingVersionTag/pendingReleaseUrl on
    // another thread. cancelPendingUpdate() then drops any triggerAsyncUpdate()
    // that was queued but hasn't run yet, so handleAsyncUpdate() can never
    // fire against a half-destroyed object.
    stopThread(4000);
    cancelPendingUpdate();
}

void UpdateChecker::checkAsync()
{
    startThread(juce::Thread::Priority::background);
}

bool UpdateChecker::isNewerVersion(const juce::String& remote, const juce::String& current)
{
    auto parseTriple = [](juce::String s)
    {
        if (s.startsWithIgnoreCase("v"))
            s = s.substring(1);

        juce::StringArray parts;
        parts.addTokens(s, ".", "");

        std::array<int, 3> nums { 0, 0, 0 };
        for (int i = 0; i < 3 && i < parts.size(); ++i)
            nums[(size_t) i] = parts[i].getIntValue(); // stops at the first non-digit, e.g. "0-beta" -> 0
        return nums;
    };

    const auto r = parseTriple(remote);
    const auto c = parseTriple(current);

    for (int i = 0; i < 3; ++i)
        if (r[(size_t) i] != c[(size_t) i])
            return r[(size_t) i] > c[(size_t) i];
    return false; // equal - not "newer"
}

void UpdateChecker::run()
{
    juce::URL url(releasesApiUrl);

    // GitHub's REST API returns 403 without a User-Agent header.
    const juce::String headers = "User-Agent: SPANDEX-UpdateChecker\r\n";

    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                             .withExtraHeaders(headers)
                                             .withConnectionTimeoutMs(4000)
                                             .withStatusCode(&statusCode));

    if (threadShouldExit() || stream == nullptr || statusCode != 200)
        return;

    const auto body = stream->readEntireStreamAsString();
    if (threadShouldExit())
        return;

    const auto parsed = juce::JSON::parse(body);
    if (!parsed.isObject())
        return;

    const auto tag = parsed.getProperty("tag_name", {}).toString();
    if (tag.isEmpty() || !isNewerVersion(tag, SPANDEX_VERSION))
        return;

    const auto htmlUrl = parsed.getProperty("html_url", {}).toString();
    pendingVersionTag = tag;
    pendingReleaseUrl = htmlUrl.isNotEmpty() ? htmlUrl : releasesPageUrl;

    if (!threadShouldExit())
        triggerAsyncUpdate();
}

void UpdateChecker::handleAsyncUpdate()
{
    if (onUpdateAvailable)
        onUpdateAvailable(pendingVersionTag, pendingReleaseUrl);
}
