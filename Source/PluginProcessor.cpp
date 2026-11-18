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

    // Bordes de banda espaciados logarítmicamente entre 20 Hz y 20 kHz,
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
        juce::ParameterID { "referenceDb", 1 }, "Referencia (dB)",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f));
    return { params.begin(), params.end() };
}

void BarSpectrumAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    fifoIndex  = 0;
    blockReady = false;
    std::fill (fifo.begin(), fifo.end(), 0.0f);
}

void BarSpectrumAnalyzerAudioProcessor::pushSampleForFFT (float sample)
{
    if (fifoIndex == fftSize)
    {
        if (! blockReady)
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            blockReady = true;
        }
        fifoIndex = 0;
    }
    fifo[(size_t) fifoIndex++] = sample;
}

void BarSpectrumAnalyzerAudioProcessor::computeBarsFromSpectrum()
{
    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double binWidth = currentSampleRate / (double) fftSize;

    // Ataque rápido, liberación más lenta -> se comporta como un medidor real.
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
            peak = juce::jmax (peak, fftData[(size_t) bin]);

        float db = juce::Decibels::gainToDecibels (peak / (float) fftSize, -100.0f);

        float prev   = barLevels[(size_t) barIdx].load();
        float coeff  = (db > prev) ? attack : release;
        float smooth = prev + coeff * (db - prev);
        barLevels[(size_t) barIdx].store (smooth);
    }
}

void BarSpectrumAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int s = 0; s < numSamples; ++s)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mono += buffer.getReadPointer (ch)[s];
        if (numCh > 0) mono /= (float) numCh;

        pushSampleForFFT (mono);
    }

    if (blockReady)
    {
        computeBarsFromSpectrum();
        blockReady = false;
    }

    // El audio sale exactamente igual que entró: este plugin es solo
    // de referencia/medición, no procesa la señal.
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
