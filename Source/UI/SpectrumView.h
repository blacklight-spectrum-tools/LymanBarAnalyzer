#pragma once
#include <JuceHeader.h>
#include <array>

class BarSpectrumAnalyzerAudioProcessor;

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
    static void styleSegment (juce::TextButton&, int index, int count);

    void drawReadouts (juce::Graphics&);
    void drawLevelMeter (juce::Graphics&);

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

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
