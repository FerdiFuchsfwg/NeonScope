#pragma once

#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

class SynthVisualizerEditor : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    struct ThemeColours
{
    juce::Colour background { 0xff07050d };
    juce::Colour accent     { 0xff00e5ff };      
    juce::Colour waveform   { 0xff00e5ff };
    juce::Colour waveform2  { 0xffff3da8 };
    juce::Colour grid       { 0xff1c1438 };
    juce::Colour sweep      { 0xffff3da8 };
    juce::Colour text       { 0xffffffff };
};
    explicit SynthVisualizerEditor (SynthVisualizerProcessor&);
    ~SynthVisualizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    static constexpr int numBands = 64;

    struct Particle { float x, y, vx, vy, life, hue; };

    void timerCallback() override;
    void analyse();
    void updateParticles (float dt);

    void drawSynthwave (juce::Graphics&, juce::Rectangle<float>);
    void drawSpectrum  (juce::Graphics&, juce::Rectangle<float>);
    void drawScope     (juce::Graphics&, juce::Rectangle<float>);
    void drawRadial    (juce::Graphics&, juce::Rectangle<float>);
    void drawParticles (juce::Graphics&, juce::Rectangle<float>);
    void openThemeMenu();
    void applyTheme (int themeId);
    void saveTheme();
    void loadTheme();
    
    void openColourPicker (int colourId);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    
    ThemeColours theme;
    
    juce::TextButton themeButton { "⚙" };
    
    int activeColourTarget = -1;

    SynthVisualizerProcessor& proc;

    juce::ComboBox modeBox;
    juce::Slider   gainSlider;
    juce::Label    gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   gainAtt;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann,
                                                 false };

    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> scopeL {}, scopeR {}, mono {};
    std::array<float, numBands> bands {}, peaks {};

    float level = 0.0f, bass = 0.0f, beat = 0.0f, bassAvg = 0.0f;
    float scrollPhase = 0.0f, rotation = 0.0f, gainValue = 1.0f;
    double animTime = 0.0, lastTime = 0.0;

    std::vector<Particle> particles;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVisualizerEditor)
};
//idk
