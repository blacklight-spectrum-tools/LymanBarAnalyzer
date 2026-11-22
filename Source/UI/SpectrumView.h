#pragma once
#include <JuceHeader.h>
#include <array>
#include <optional>

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
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr int numBars = 28;

    void setFrozen (bool shouldFreeze);
    void resetPeaks();

    // Menu del engrane (el boton vive en la cabecera, junto al logo).
    // parent solo se usa en pruebas, para dibujar el menu dentro de la ventana.
    void showSettingsMenu (juce::Component& target, juce::Component* parent = nullptr);

    // Para pruebas / capturas: posicion del mouse y zona de la grafica
    void setHoverPosition (std::optional<juce::Point<float>> p) { hoverPos = p; }
    juce::Rectangle<float> getPlotArea() const { return plotArea; }
    juce::Rectangle<float> getGuideArea() const { return guideArea; }
    void setGuideHover (std::optional<juce::Point<float>> p) { guideHoverPos = p; }

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
    void drawHover (juce::Graphics&, float tilt, float refDb, bool showBars, bool showSide,
                    float barWidth, float barGap);

    // Guide: zonas por instrumento en una tira arriba de la grafica
    void setGuide (bool on);
    int  currentInstrument();                  // indice en GuideZones::instruments
    void showInstrumentMenu();
    float zoneOverRef (float lo, float hi, float tilt, float refDb, bool showBars) const;
    void drawGuideStrip (juce::Graphics&, float tilt, float refDb, bool showBars);
    void drawGuideInfo (juce::Graphics&, float tilt, float refDb, bool showBars);
    int  zoneAt (float x);

    // Datos que se dibujan: en vivo, o la foto tomada al congelar
    float barDb (int i) const;
    float sideBarDb (int i) const;
    float curveDb (int i, bool side) const;
    bool  phaseRisk (int i) const;
    int   bandAt (float hz) const;
    static juce::String noteName (float hz);

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

    // Freeze: congela la grafica para estudiarla (los medidores siguen vivos)
    juce::TextButton freezeButton { "Freeze" };
    bool frozen = false;
    std::array<float, numBars> frozenBars {}, frozenSideBars {};
    std::array<bool, numBars>  frozenPhaseRisk {};
    std::vector<float> frozenCurveMid, frozenCurveSide;

    // Lectura al pasar el mouse por la grafica
    std::optional<juce::Point<float>> hoverPos;

    // Guide (zonas por instrumento). Se guarda con el proyecto.
    juce::TextButton guideButton { "Guide" };
    juce::TextButton instrumentButton;
    bool guideOn = false;
    int  guideChoice = 0;                      // 0 = auto (nombre de la pista), 1.. = a mano
    juce::String lastTrackName;
    bool lastOnMaster = true;
    int  autoInstrument = 0;
    std::optional<juce::Point<float>> guideHoverPos;

    // Zonas calculadas en resized()
    juce::Rectangle<float> plotArea, readoutArea, meterArea, clipArea, guideArea;

    // Peak hold por barra y de la curva (nivel medido, sin tilt)
    std::array<float, numBars>  holdDb {};
    std::array<double, numBars> holdTime {};
    std::vector<float> curveHoldMid, curveHoldSide;     // maximo de la curva (hold infinito)

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
