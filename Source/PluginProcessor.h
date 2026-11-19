#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "Analysis/PhaseAnalyzer.h"
#include "Analysis/LevelMeter.h"
#include "Analysis/StereoAnalyzer.h"

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
    static constexpr int fftOrder = 11;              // tamano de FFT = 2048 muestras
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int numBars  = 28;              // cantidad de barras mostradas

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
    PhaseAnalyzer& getPhaseAnalyzer() noexcept { return phaseAnalyzer; }
    LevelMeter&    getLevelMeter() noexcept    { return levelMeter; }
    StereoAnalyzer& getStereoAnalyzer() noexcept { return stereoAnalyzer; }

    juce::AudioProcessorValueTreeState apvts;

    // Pestana abierta la ultima vez (para que el editor la recuerde al reabrirse)
    std::atomic<int> lastTab { 0 };

private:
    struct SpectrumChannel
    {
        std::array<float, fftSize>     fifo {};
        std::array<float, fftSize * 2> data {};
    };

    void pushSamplesForFFT (float mid, float side);
    void computeBars (std::array<float, fftSize * 2>& data, std::array<std::atomic<float>, numBars>& levels);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    SpectrumChannel midChannel, sideChannel;   // Mid = (L+R)/2, Side = (L-R)/2
    int  fifoIndex  = 0;
    bool blockReady = false;

    std::array<float, numBars + 1>          bandEdgesHz {};   // limites de cada banda en Hz
    std::array<std::atomic<float>, numBars> barLevels;        // nivel suavizado en dB por barra (Mid)
    std::array<std::atomic<float>, numBars> sideLevels;       // lo mismo para el canal Side

    PhaseAnalyzer phaseAnalyzer;
    LevelMeter    levelMeter;
    StereoAnalyzer stereoAnalyzer;

    double currentSampleRate = 44100.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSpectrumAnalyzerAudioProcessor)
};
