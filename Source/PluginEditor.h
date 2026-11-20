#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/BlacklightLookAndFeel.h"
#include "UI/SpectrumView.h"
#include "UI/PhaseView.h"
#include "UI/MasterView.h"

class BarSpectrumAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit BarSpectrumAnalyzerAudioProcessorEditor (BarSpectrumAnalyzerAudioProcessor&);
    ~BarSpectrumAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int index);

    BarSpectrumAnalyzerAudioProcessor& audioProcessor;

    // Va primero: los componentes se destruyen antes que su LookAndFeel
    BlacklightLookAndFeel lookAndFeel;

    juce::TextButton spectrumTab { "Spectrum" };
    juce::TextButton phaseTab    { "Phase" };
    juce::TextButton masterTab   { "Master" };

    SpectrumView spectrumView;
    PhaseView    phaseView;
    MasterView   masterView;

    int currentTab = 0;
    bool dcShown = false;          // aviso de DC offset en la cabecera
    juce::Rectangle<int> dcArea;

    static constexpr int headerHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessorEditor)
};
