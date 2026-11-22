#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/BlacklightLookAndFeel.h"
#include "UI/SpectrumView.h"
#include "UI/PhaseView.h"
#include "UI/MasterView.h"
#include "UI/ListenButton.h"

class BarSpectrumAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit BarSpectrumAnalyzerAudioProcessorEditor (BarSpectrumAnalyzerAudioProcessor&);
    ~BarSpectrumAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
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

    // Engrane junto al logo: velocidad de la curva, rango vertical, peak hold
    GearButton settingsButton;

    // Escucha Mono / Side (mantener presionado). Unico control que toca el audio.
    ListenButton listenButton;
    bool listenShown = false;

    SpectrumView spectrumView;
    PhaseView    phaseView;
    MasterView   masterView;

    juce::TooltipWindow tooltipWindow { this, 700 };   // ayudas al pasar el mouse

    int currentTab = 0;
    bool dcShown = false;          // aviso de DC offset en la cabecera
    juce::Rectangle<int> dcArea;
    bool compactLogo = false;      // ventanas chicas: solo la marca, sin texto
    int themeVersion = -1;         // para notar un cambio de tema de color
    int roleTicks = 0;             // cada tanto se revisa en que pista esta

    static constexpr int headerHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessorEditor)
};
