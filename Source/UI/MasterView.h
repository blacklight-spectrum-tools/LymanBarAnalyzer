#pragma once
#include <JuceHeader.h>
#include "../Analysis/LevelMeter.h"
#include "../Analysis/TruePeakMeter.h"

class BarSpectrumAnalyzerAudioProcessor;

/**
    Pestana "Master": sonoridad completa (Integrated, Momentary, Short-term,
    LRA y maximos), True Peak con contador de clips, PLR (cuanto espacio
    dinamico queda) y lo que cada plataforma de streaming haria con el nivel.
    Solo mide y explica: no toca el audio.
*/
class MasterView : public juce::Component
{
public:
    explicit MasterView (BarSpectrumAnalyzerAudioProcessor&);
    ~MasterView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawLoudness (juce::Graphics&);
    void drawTruePeak (juce::Graphics&);
    void drawPlr (juce::Graphics&);
    void drawStreaming (juce::Graphics&);
    void drawLed (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String&);

    static juce::String formatLufs (float);
    static juce::String formatTime (double seconds);

    LevelMeter&    level;
    TruePeakMeter& truePeak;

    juce::TextButton resetButton { "Reset" };

    juce::Rectangle<float> loudArea, tpArea, plrArea, streamArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterView)
};
