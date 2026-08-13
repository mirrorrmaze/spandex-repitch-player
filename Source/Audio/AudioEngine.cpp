#include "AudioEngine.h"
#include "AudioFileLoader.h"

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();

    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(&effectsChain, false, 2);
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlockExpected)
{
    hostSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    resamplingSource->prepareToPlay(samplesPerBlockExpected, hostSampleRate);
    updateResamplingRatio();
}

void AudioEngine::releaseResources()
{
    resamplingSource->releaseResources();
}

void AudioEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
    resamplingSource->getNextAudioBlock(info);
}

bool AudioEngine::loadFile(const juce::File& file, juce::String& errorMessage)
{
    stop();

    auto result = AudioFileLoader::load(file, formatManager);
    if (!result.success)
    {
        errorMessage = "Could not read audio file: " + file.getFullPathName();
        return false;
    }

    sourceSampleRateAtLoad = result.sourceSampleRate;
    stretchSource.setBuffer(std::move(result.buffer), result.sourceSampleRate);

    thumbnail.setSource(new juce::FileInputSource(file));

    updateResamplingRatio();
    return true;
}

void AudioEngine::updateResamplingRatio()
{
    const double ratio = hostSampleRate > 0.0 ? sourceSampleRateAtLoad / hostSampleRate : 1.0;
    resamplingSource->setResamplingRatio(ratio);
}

void AudioEngine::play()
{
    stretchSource.play();
}

void AudioEngine::pause()
{
    stretchSource.pause();
}

void AudioEngine::stop()
{
    stretchSource.stopAndReset();
}

void AudioEngine::setPositionSeconds(double seconds)
{
    stretchSource.setNextReadPosition((juce::int64) (seconds * sourceSampleRateAtLoad));
}

double AudioEngine::getPositionSeconds() const
{
    if (sourceSampleRateAtLoad <= 0.0)
        return 0.0;
    return (double) stretchSource.getNextReadPosition() / sourceSampleRateAtLoad;
}
