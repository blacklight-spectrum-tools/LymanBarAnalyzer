#include "PluginProcessor.h"
#include "PluginEditor.h"

BarSpectrumAnalyzerAudioProcessor::BarSpectrumAnalyzerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      fft (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    for (auto& b : barLevels)
        b.store (-100.0f);
    for (auto& b : sideLevels)
        b.store (-100.0f);

    // Bordes de banda espaciados logaritmicamente entre 20 Hz y 20 kHz,
    // igual que un analizador de espectro musical (no lineal).
    const double minF = 20.0, maxF = 20000.0;
    for (int i = 0; i <= numBars; ++i)
    {
        double t = (double) i / (double) numBars;
        bandEdgesHz[(size_t) i] = (float) (minF * std::pow (maxF / minF, t));
    }
}

BarSpectrumAnalyzerAudioProcessor::~BarSpectrumAnalyzerAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout BarSpectrumAnalyzerAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "referenceDb", 1 }, "Reference (dB)",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f));

    // Inclinacion de la vista (dB por octava, pivote en 1 kHz). Solo cambia
    // como se dibujan las barras, nunca el audio.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "tilt", 1 }, "Tilt",
        juce::StringArray { "0 dB/oct", "3 dB/oct", "4.5 dB/oct" }, 0));

    // Vista: suma (Mid) o Mid/Side superpuestos
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "view", 1 }, "View",
        juce::StringArray { "Sum", "M/S" }, 0));
    return { params.begin(), params.end() };
}

void BarSpectrumAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    fifoIndex  = 0;
    blockReady = false;
    midChannel.fifo.fill (0.0f);
    sideChannel.fifo.fill (0.0f);
    phaseAnalyzer.prepare (sampleRate);
    stereoAnalyzer.prepare (sampleRate);
    levelMeter.prepare (sampleRate);
}

void BarSpectrumAnalyzerAudioProcessor::pushSamplesForFFT (float mid, float side)
{
    if (fifoIndex == fftSize)
    {
        if (! blockReady)
        {
            for (auto* ch : { &midChannel, &sideChannel })
            {
                std::fill (ch->data.begin(), ch->data.end(), 0.0f);
                std::copy (ch->fifo.begin(), ch->fifo.end(), ch->data.begin());
            }
            blockReady = true;
        }
        fifoIndex = 0;
    }
    midChannel.fifo[(size_t) fifoIndex]  = mid;
    sideChannel.fifo[(size_t) fifoIndex] = side;
    ++fifoIndex;
}

void BarSpectrumAnalyzerAudioProcessor::computeBars (std::array<float, fftSize * 2>& data,
                                                     std::array<std::atomic<float>, numBars>& levels)
{
    window.multiplyWithWindowingTable (data.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (data.data());

    const double binWidth = currentSampleRate / (double) fftSize;

    // Ataque rapido, liberacion mas lenta -> se comporta como un medidor real.
    const float attack  = 0.6f;
    const float release = 0.15f;

    for (int barIdx = 0; barIdx < numBars; ++barIdx)
    {
        float loF = bandEdgesHz[(size_t) barIdx];
        float hiF = bandEdgesHz[(size_t) barIdx + 1];

        int loBin = juce::jlimit (0, fftSize / 2, (int) std::floor (loF / binWidth));
        int hiBin = juce::jlimit (0, fftSize / 2, (int) std::ceil  (hiF / binWidth));
        if (hiBin <= loBin) hiBin = loBin + 1;

        float peak = 0.0f;
        for (int bin = loBin; bin < hiBin && bin < fftSize / 2; ++bin)
            peak = juce::jmax (peak, data[(size_t) bin]);

        float db = juce::Decibels::gainToDecibels (peak / (float) fftSize, -100.0f);

        float prev   = levels[(size_t) barIdx].load();
        float coeff  = (db > prev) ? attack : release;
        float smooth = prev + coeff * (db - prev);
        levels[(size_t) barIdx].store (smooth);
    }
}

void BarSpectrumAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numCh == 0)
        return;

    // Con una sola entrada (mono) L y R son el mismo canal.
    const float* left  = buffer.getReadPointer (0);
    const float* right = buffer.getReadPointer (numCh > 1 ? 1 : 0);

    for (int s = 0; s < numSamples; ++s)
        pushSamplesForFFT (0.5f * (left[s] + right[s]), 0.5f * (left[s] - right[s]));

    if (blockReady)
    {
        computeBars (midChannel.data, barLevels);
        computeBars (sideChannel.data, sideLevels);
        blockReady = false;
    }

    phaseAnalyzer.process (left, right, numSamples);
    stereoAnalyzer.process (left, right, numSamples);

    levelMeter.process (buffer.getArrayOfReadPointers(), numCh, numSamples);

    // El audio sale exactamente igual que entro: este plugin es solo
    // de referencia/medicion, no procesa la senal.
}

juce::AudioProcessorEditor* BarSpectrumAnalyzerAudioProcessor::createEditor()
{
    return new BarSpectrumAnalyzerAudioProcessorEditor (*this);
}

void BarSpectrumAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BarSpectrumAnalyzerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Requerido por JUCE para instanciar el plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BarSpectrumAnalyzerAudioProcessor();
}
