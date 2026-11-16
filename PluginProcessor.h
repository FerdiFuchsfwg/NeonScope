#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class SynthVisualizerProcessor : public juce::AudioProcessor
{
public:
    static constexpr int ringSize = 16384; // muss Zweierpotenz sein

    SynthVisualizerProcessor();
    ~SynthVisualizerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Synth Visualizer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Kopiert die letzten numSamples Samples (links/rechts) in die Zielpuffer.
    void getLatest (float* left, float* right, int numSamples) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::atomic<float>, ringSize> ringL, ringR;
    std::atomic<int> writePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVisualizerProcessor)
};
