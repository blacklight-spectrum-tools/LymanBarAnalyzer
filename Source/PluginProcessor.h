#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "Analysis/PhaseAnalyzer.h"
#include "Analysis/LevelMeter.h"
#include "Analysis/StereoAnalyzer.h"
#include "Analysis/LowBandBank.h"
#include "Analysis/TruePeakMeter.h"
#include <memory>
#include <vector>

/**
    Analizador de espectro de referencia en BARRAS (estilo SPAN pero
    con niveles por banda en vez de curva continua), con pestana de fase
    estereo (goniometro + correlacion).

    No modifica el audio: solo lo mide. Pensado para insertarse justo
    ANTES de un compresor, para decidir con mas precision en que
    bandas de frecuencia y a que nivel (dB) fijar el umbral.
*/
class BarSpectrumAnalyzerAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int   numBars    = 28;          // cantidad de barras mostradas
    static constexpr float lowSplitHz = 240.0f;      // debajo: banco de filtros; arriba: FFT
    static constexpr int   numCurvePoints = 240;     // puntos de la vista en curva (20 Hz a 20 kHz, ~1/24 oct)

    BarSpectrumAnalyzerAudioProcessor();
    ~BarSpectrumAnalyzerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- Acceso para la interfaz ---
    float getBarLevelDb (int barIndex) const  { return barLevels[(size_t) barIndex].load(); }
    float getSideLevelDb (int barIndex) const { return sideLevels[(size_t) barIndex].load(); }
    float getBandFrequency (int edgeIndex) const { return bandEdgesHz[(size_t) edgeIndex]; }

    // Aviso de fase por banda: true si esa banda se cancelaria en mono
    // (Side mas fuerte que Mid de forma sostenida, con energia real)
    bool  isBandPhaseRisk (int barIndex) const   { return phaseRisk[(size_t) barIndex].load(); }

    // Vista en curva: mas puntos que las barras (1/12 de octava aprox.)
    float getCurveDb (int point, bool side) const
    {
        return (side ? curveSide : curveMid)[(size_t) point].load();
    }
    static float getCurveFrequency (int point)
    {
        return 20.0f * std::pow (1000.0f, (float) point / (float) (numCurvePoints - 1));
    }
    PhaseAnalyzer& getPhaseAnalyzer() noexcept { return phaseAnalyzer; }
    LevelMeter&    getLevelMeter() noexcept    { return levelMeter; }
    StereoAnalyzer& getStereoAnalyzer() noexcept { return stereoAnalyzer; }
    TruePeakMeter&  getTruePeakMeter() noexcept  { return truePeak; }

    juce::AudioProcessorValueTreeState apvts;

    // Pestana abierta la ultima vez (para que el editor la recuerde al reabrirse)
    std::atomic<int> lastTab { 0 };

    // Ventanas abiertas (la curva solo se calcula si alguien la esta viendo)
    std::atomic<int> editorsOpen { 0 };

    // --- En que pista esta Lyman (pestana Master) ------------------------
    // El DAW manda el nombre de la pista (VST3; en FL: "Master", "Insert 3"...).
    // Si no lo manda, o se equivoca, el usuario elige a mano.
    enum TrackRole { roleAuto = 0, roleMaster, roleChannel };

    void updateTrackProperties (const TrackProperties&) override;
    juce::String getTrackName() const;
    int  getTrackRole() const;                 // lo que eligio el usuario
    void setTrackRole (int role);
    bool isOnMaster() const;                   // decision final (hilo de la interfaz)
    static bool looksLikeMaster (const juce::String& trackName);

private:
    struct SpectrumChannel
    {
        std::vector<float> fifo;
        std::vector<float> data;
    };

    juce::CriticalSection trackLock;
    juce::String trackName;

    void pushSamplesForFFT (float mid, float side);
    void analyse (const float* left, const float* right, int numChannels, int numSamples);
    void computeBars (SpectrumChannel&, int lowChannel, std::array<std::atomic<float>, numBars>& levels);
    void updatePhaseRisk();
    void computeCurveSpectrum (bool restart);
    void computeCurve (const std::vector<float>& avgPower, std::array<std::atomic<float>, numCurvePoints>& curve);
    float averagingCoefficient (int hopSamples) const;
    void setupAnalysis (double sampleRate);

    // El tamano de la FFT crece con la frecuencia de muestreo (2048 a 44.1/48 kHz,
    // 4096 a 88.2/96 kHz, 8192 a 176.4/192 kHz) para que cada cajon siga
    // midiendo ~23 Hz y la ventana dure ~43 ms.
    int fftSize = 2048;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    LowBandBank lowBank;
    int numLowBands = 0;

    SpectrumChannel midChannel, sideChannel;   // Mid = (L+R)/2, Side = (L-R)/2
    int  fifoIndex  = 0;
    bool blockReady = false;

    // Vista en curva: su propia FFT, 8 veces mas grande que la de las barras
    // (16384 a 44.1/48 kHz -> divisiones de ~2.9 Hz, ventana de ~340 ms), con
    // el espectro PROMEDIADO en energia (como SPAN) para que la forma se vea
    // estable. Una sola FFT para toda la curva: sin costuras entre graves y
    // agudos. Solo se calcula mientras alguien ve la curva (ahorra CPU).
    int curveFftSize = 16384;
    int curveEvery = 1;                                  // cada cuantos bloques de barras
    int curveFrameCounter = 0;
    bool curveActive = false;
    std::unique_ptr<juce::dsp::FFT> curveFft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> curveWindow;
    std::array<std::vector<float>, 2> curveRing;         // historia (Mid, Side)
    std::vector<float> curveData;                        // buffer de trabajo
    std::vector<float> avgPowerMid, avgPowerSide;
    int curveWrite = 0;

    float listenGain = 0.0f, listenStep = 0.001f;
    bool  listenSide = false;           // modo que esta sonando (cambia sin clic)
    std::array<int, numCurvePoints>    curveLoBin {}, curveHiBin {};   // ventana de cada punto
    std::array<double, numCurvePoints> curvePos {};                    // posicion exacta (en divisiones)

    std::array<float, numBars + 1>          bandEdgesHz {};   // limites de cada banda en Hz
    std::array<std::atomic<float>, numBars> barLevels;        // nivel suavizado en dB por barra (Mid)
    std::array<std::atomic<float>, numBars> sideLevels;       // lo mismo para el canal Side

    // Fase por banda: potencias Mid/Side promediadas (~1 s) y estado con
    // histeresis (tarda 0.5 s en prender y 1 s en apagar, sin parpadeo)
    std::array<float, numBars> phaseMidPow {}, phaseSidePow {}, phaseOnTime {}, phaseOffTime {};
    std::array<std::atomic<bool>, numBars> phaseRisk {};
    std::array<std::atomic<float>, numCurvePoints> curveMid, curveSide;

    PhaseAnalyzer phaseAnalyzer;
    LevelMeter    levelMeter;
    StereoAnalyzer stereoAnalyzer;
    TruePeakMeter  truePeak;

    double currentSampleRate = 44100.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessor)
};
