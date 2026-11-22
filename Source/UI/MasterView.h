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

    Sabe en que pista esta: en el master muestra la tabla de streaming; en un
    canal la cambia por una guia para comparar canales (la pestana se llama
    "Loudness"). El DAW manda el nombre de la pista; si no, se elige a mano.
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
    void drawChannel (juce::Graphics&);
    void updateRoleButtons();
    void drawLed (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String&);

    static juce::String formatLufs (float);
    static juce::String formatTime (double seconds);

public:
    // true si Lyman esta en el master (lo decide el procesador)
    bool showsMaster() const { return master; }
    void refreshRole();                       // desde el timer del editor

private:
    BarSpectrumAnalyzerAudioProcessor& processor;
    LevelMeter&    level;
    TruePeakMeter& truePeak;

    juce::TextButton resetButton { "Reset" };
    juce::TextButton roleAuto { "Auto" }, roleMaster { "Master" }, roleChannel { "Channel" };
    juce::Label roleLabel;

    bool master = true;
    juce::String trackName;

    juce::Rectangle<float> loudArea, tpArea, plrArea, streamArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterView)
};
