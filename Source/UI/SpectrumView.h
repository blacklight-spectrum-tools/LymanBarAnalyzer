#pragma once
#include <JuceHeader.h>
#include <array>

class BarSpectrumAnalyzerAudioProcessor;

/** Boton de engrane (configuracion): dibujado a mano, sin depender de fuentes. */
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("Settings") {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
};

/**
    Pestana "Spectrum": barras por banda, linea de referencia de compresion,
    inclinacion (tilt), dB por encima de la referencia, peak hold por barra
    y medidor de nivel L/R con pico, RMS, LUFS de corto plazo y clip.
*/
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (BarSpectrumAnalyzerAudioProcessor&);
    ~SpectrumView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int numBars = 28;

private:
    static juce::String frequencyLabel (float hz);
    static juce::String formatDb (float db);
    void setTiltButtons (int index);
    void setViewButtons (int index);
    void setStyleButtons (int index);
    static void styleSegment (juce::TextButton&, int index, int count);

    void drawReadouts (juce::Graphics&);
    void drawLevelMeter (juce::Graphics&);
    void drawCurve (juce::Graphics&, bool side, float tilt, float refDb);
    void showSettingsMenu();

    BarSpectrumAnalyzerAudioProcessor& audioProcessor;

    juce::Slider referenceSlider;
    juce::Label  referenceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> referenceAttachment;

    juce::Label tiltLabel;
    std::array<juce::TextButton, 3> tiltButtons;
    std::unique_ptr<juce::ParameterAttachment> tiltAttachment;

    juce::Label viewLabel;
    std::array<juce::TextButton, 2> viewButtons;
    std::unique_ptr<juce::ParameterAttachment> viewAttachment;

    juce::Label styleLabel;
    std::array<juce::TextButton, 2> styleButtons;
    std::unique_ptr<juce::ParameterAttachment> styleAttachment;

    // Engrane: velocidad de la curva y rango vertical (cosas que no son
    // del analisis del dia a dia, para no saturar la ventana)
    GearButton settingsButton;

    // Zonas calculadas en resized()
    juce::Rectangle<float> plotArea, readoutArea, meterArea, clipArea;

    // Peak hold por barra (en la escala que se dibuja)
    std::array<float, numBars>  holdDb {};
    std::array<double, numBars> holdTime {};

    // Balistica del medidor
    std::array<float, 2> meterPeakDb { -100.0f, -100.0f };
    float  numericPeakDb = -100.0f;
    double numericPeakTime = 0.0;
    double lastPaintMs = 0.0;
    float  frameDt = 0.0f;

    float minDb = -60.0f;                    // fondo de la grafica (parametro "range")
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
