#include "PluginProcessor.h"
#include "PluginEditor.h"

SynthVisualizerProcessor::SynthVisualizerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& a : ringL) a.store (0.0f);
    for (auto& a : ringR) a.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout SynthVisualizerProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Synthwave", "Spectrum", "Oscilloscope", "Radial", "Particles" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 }, "Sensitivity",
        juce::NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    return layout;
}

void SynthVisualizerProcessor::prepareToPlay (double, int) {}

bool SynthVisualizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void SynthVisualizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    // Audio wird unverändert durchgereicht, wir lesen nur mit.
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (numChannels > 1 ? 1 : 0);

    int pos = writePos.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        ringL[(size_t) pos].store (l[i], std::memory_order_relaxed);
        ringR[(size_t) pos].store (r[i], std::memory_order_relaxed);
        pos = (pos + 1) & (ringSize - 1);
    }
    writePos.store (pos, std::memory_order_release);
}

void SynthVisualizerProcessor::getLatest (float* left, float* right, int numSamples) const
{
    numSamples = juce::jmin (numSamples, ringSize);
    const int pos = writePos.load (std::memory_order_acquire);

    for (int i = 0; i < numSamples; ++i)
    {
        const int idx = (pos - numSamples + i + ringSize) & (ringSize - 1);
        left[i]  = ringL[(size_t) idx].load (std::memory_order_relaxed);
        right[i] = ringR[(size_t) idx].load (std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* SynthVisualizerProcessor::createEditor()
{
    return new SynthVisualizerEditor (*this);
}

void SynthVisualizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthVisualizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthVisualizerProcessor();
}
