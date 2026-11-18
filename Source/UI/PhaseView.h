#pragma once
#include <JuceHeader.h>
#include <vector>
#include "../Analysis/PhaseAnalyzer.h"

class BarSpectrumAnalyzerAudioProcessor;

/**
    Pestana "Phase": goniometro (circulo), medidor de correlacion de -1 a +1,
    luz de advertencia y minimo/maximo retenidos.
*/
class PhaseView : public juce::Component
{
public:
    explicit PhaseView (BarSpectrumAnalyzerAudioProcessor&);
    ~PhaseView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawGoniometer (juce::Graphics&);
    void drawCorrelationMeter (juce::Graphics&, float correlation, bool signal);
    void drawStatus (juce::Graphics&, PhaseAnalyzer::Status);
    void drawHold (juce::Graphics&);

    static juce::Colour colourFor (PhaseAnalyzer::Status);
    static juce::String textFor (PhaseAnalyzer::Status);
    static juce::String formatCorrelation (float);

    float meterX (float correlation) const;

    PhaseAnalyzer& phase;

    juce::TextButton resetButton { "Reset min / max" };

    static constexpr int scopePoints = 1024;
    std::vector<float> bufL, bufR;
    float displayGain = 1.0f;

    juce::Rectangle<float> scopeArea, titleArea, valueArea, meterArea, ticksArea,
                           statusArea, holdArea, hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseView)
};
