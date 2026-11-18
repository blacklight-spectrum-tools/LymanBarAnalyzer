#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class BarSpectrumAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit BarSpectrumAnalyzerAudioProcessorEditor (BarSpectrumAnalyzerAudioProcessor&);
    ~BarSpectrumAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    juce::String frequencyLabel (float hz) const;

    BarSpectrumAnalyzerAudioProcessor& audioProcessor;

    juce::Slider referenceSlider;
    juce::Label  referenceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> referenceAttachment;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessorEditor)
};
