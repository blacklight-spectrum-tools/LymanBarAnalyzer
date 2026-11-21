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
    static constexpr int   numCurvePoints = 120;     // puntos de la vista en curva (20 Hz a 20 kHz)

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

private:
    struct SpectrumChannel
    {
        std::vector<float> fifo;
        std::vector<float> data;
    };

    void pushSamplesForFFT (float mid, float side);
    void analyse (const float* left, const float* right, int numChannels, int numSamples);
    void computeBars (SpectrumChannel&, int lowChannel, std::array<std::atomic<float>, numBars>& levels,
                      std::array<std::atomic<float>, numCurvePoints>& curve);
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

    std::array<float, numBars + 1>          bandEdgesHz {};   // limites de cada banda en Hz
    std::array<std::atomic<float>, numBars> barLevels;        // nivel suavizado en dB por barra (Mid)
    std::array<std::atomic<float>, numBars> sideLevels;       // lo mismo para el canal Side
    std::array<std::atomic<float>, numCurvePoints> curveMid, curveSide;

    PhaseAnalyzer phaseAnalyzer;
    LevelMeter    levelMeter;
    StereoAnalyzer stereoAnalyzer;
    TruePeakMeter  truePeak;

    double currentSampleRate = 44100.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessor)
};
