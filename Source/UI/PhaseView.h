#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../Analysis/PhaseAnalyzer.h"
#include "../Analysis/StereoAnalyzer.h"

class BarSpectrumAnalyzerAudioProcessor;

/**
    Pestana "Phase": goniometro (escala automatica o por nivel), medidor de
    correlacion de -1 a +1 con luz de advertencia y minimo/maximo, luz de
    graves en mono (< 120 Hz) y balance L/R.
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
    void drawLed (juce::Graphics&, juce::Rectangle<float> row, juce::Colour, const juce::String& text,
                  bool lit, float textSize);
    void drawHold (juce::Graphics&);
    void drawLowEnd (juce::Graphics&);
    void drawBalance (juce::Graphics&);

    void setScaleMode (int mode);
    int  getScaleMode() const;

    static juce::Colour colourFor (PhaseAnalyzer::Status);
    static juce::Colour colourFor (StereoAnalyzer::Status);
    static juce::String textFor (PhaseAnalyzer::Status);
    static juce::String formatCorrelation (float);

    float meterX (float correlation) const;

    juce::AudioProcessorValueTreeState& apvts;
    PhaseAnalyzer&  phase;
    StereoAnalyzer& stereo;

    juce::TextButton resetButton { "Reset min / max" };
    std::array<juce::TextButton, 2> scaleButtons;

    static constexpr int scopePoints = 1024;
    std::vector<float> bufL, bufR;
    float displayGain = 1.0f;

    juce::Image trail;          // persistencia del goniometro
    double lastTrailMs = 0.0;
    int    trailMode = -1;

    juce::Rectangle<float> scopeArea, titleArea, valueArea, meterArea, ticksArea,
                           statusArea, holdArea, lowTitleArea, lowStatusArea,
                           balTitleArea, balMeterArea, balTicksArea, hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseView)
};
