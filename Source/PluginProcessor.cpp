#include "PluginProcessor.h"
#include "PluginEditor.h"

BarSpectrumAnalyzerAudioProcessor::BarSpectrumAnalyzerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
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

    // Bandas graves (borde superior <= lowSplitHz): banco de filtros
    numLowBands = 0;
    while (numLowBands < numBars && bandEdgesHz[(size_t) numLowBands + 1] <= lowSplitHz + 0.5f)
        ++numLowBands;

    setupAnalysis (44100.0);
}

void BarSpectrumAnalyzerAudioProcessor::setupAnalysis (double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    int order = 11;
    if (currentSampleRate > 130000.0)     order = 13;
    else if (currentSampleRate > 66000.0) order = 12;

    if (fft == nullptr || (1 << order) != fft->getSize())
    {
        fftSize = 1 << order;
        fft     = std::make_unique<juce::dsp::FFT> (order);
        window  = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
    }

    for (auto* ch : { &midChannel, &sideChannel })
    {
        ch->fifo.assign ((size_t) fftSize, 0.0f);
        ch->data.assign ((size_t) fftSize * 2, 0.0f);
    }
    fifoIndex  = 0;
    blockReady = false;

    lowBank.prepare (currentSampleRate, bandEdgesHz.data(), numLowBands);
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
    setupAnalysis (sampleRate);
    phaseAnalyzer.prepare (sampleRate);
    stereoAnalyzer.prepare (sampleRate);
    levelMeter.prepare (sampleRate);
    truePeak.prepare (sampleRate);
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

void BarSpectrumAnalyzerAudioProcessor::computeBars (SpectrumChannel& ch, int lowChannel,
                                                     std::array<std::atomic<float>, numBars>& levels)
{
    auto& data = ch.data;
    window->multiplyWithWindowingTable (data.data(), (size_t) fftSize);
    fft->performFrequencyOnlyForwardTransform (data.data());

    const double binWidth = currentSampleRate / (double) fftSize;
    const int    lastBin  = fftSize / 2 - 1;

    // Calibracion: la ventana Hann de JUCE esta normalizada (ganancia 1), asi
    // que un seno de amplitud A da una magnitud A * N / 2. Multiplicando por
    // 2 / N, un seno de 0 dBFS lee 0 dB en su barra.
    const float scale = 2.0f / (float) fftSize;

    // Ataque instantaneo, liberacion mas lenta -> se comporta como un medidor
    // real. (Con ataque suavizado, un kick corto se leia 6-14 dB por debajo
    // de su nivel: la barra caia entre golpes y no alcanzaba a subir.)
    const float attack  = 1.0f;
    const float release = 0.15f;

    for (int barIdx = 0; barIdx < numBars; ++barIdx)
    {
        float db;

        if (barIdx < numLowBands)
        {
            // Graves: banco de filtros (separa bien bandas de pocos Hz)
            db = juce::Decibels::gainToDecibels ((float) lowBank.getAmplitude (barIdx, lowChannel), -100.0f);
        }
        else
        {
            const double loF = bandEdgesHz[(size_t) barIdx];
            const double hiF = bandEdgesHz[(size_t) barIdx + 1];

            // Solo los cajones cuyo centro cae dentro de la banda; si ninguno
            // cae, el mas cercano al centro de la banda.
            int loBin = juce::jlimit (1, lastBin, (int) std::ceil  (loF / binWidth));
            int hiBin = juce::jlimit (1, lastBin, (int) std::floor (hiF / binWidth));
            if (hiBin < loBin)
                loBin = hiBin = juce::jlimit (1, lastBin, (int) std::lround (std::sqrt (loF * hiF) / binWidth));

            int   best = loBin;
            float peak = data[(size_t) loBin];
            for (int bin = loBin + 1; bin <= hiBin; ++bin)
                if (data[(size_t) bin] > peak) { peak = data[(size_t) bin]; best = bin; }

            db = juce::Decibels::gainToDecibels (peak * scale, -100.0f);

            // Interpolacion parabolica (en dB) cuando el maximo es un pico real:
            // corrige la perdida de hasta 1.4 dB cuando el tono cae entre dos
            // cajones. Si el pico verdadero esta fuera de la banda, no se usa.
            if (best > 1 && best < lastBin && db > -100.0f)
            {
                const float a = juce::Decibels::gainToDecibels (data[(size_t) best - 1] * scale, -140.0f);
                const float c = juce::Decibels::gainToDecibels (data[(size_t) best + 1] * scale, -140.0f);
                const float den = a - 2.0f * db + c;
                if (a <= db && c <= db && den < 0.0f)
                {
                    const float pOff = juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / den);
                    db -= 0.25f * (a - c) * pOff;
                }
            }
        }

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
    const int    chans = numCh > 1 ? 2 : 1;

    // Si otro plugin manda NaN o infinito, un solo sample asi dejaria los
    // medidores trabados para siempre (los promedios se vuelven NaN). Esos
    // samples se analizan como silencio, en una copia: el audio no se toca.
    bool finite = true;
    for (int s = 0; s < numSamples && finite; ++s)
        finite = std::isfinite (left[s]) && std::isfinite (right[s]);

    if (finite)
    {
        analyse (left, right, chans, numSamples);
    }
    else
    {
        constexpr int chunk = 256;
        float cl[chunk], cr[chunk];
        for (int start = 0; start < numSamples; start += chunk)
        {
            const int n = juce::jmin (chunk, numSamples - start);
            for (int i = 0; i < n; ++i)
            {
                const float l = left[start + i], r = right[start + i];
                cl[i] = std::isfinite (l) ? l : 0.0f;
                cr[i] = std::isfinite (r) ? r : 0.0f;
            }
            analyse (cl, cr, chans, n);
        }
    }

    // El audio sale exactamente igual que entro: este plugin es solo
    // de referencia/medicion, no procesa la senal.
}

void BarSpectrumAnalyzerAudioProcessor::analyse (const float* left, const float* right, int numChannels, int numSamples)
{
    for (int s = 0; s < numSamples; ++s)
    {
        const float mid  = 0.5f * (left[s] + right[s]);
        const float side = 0.5f * (left[s] - right[s]);
        pushSamplesForFFT (mid, side);
        lowBank.process (mid, side);
    }

    if (blockReady)
    {
        computeBars (midChannel, 0, barLevels);
        computeBars (sideChannel, 1, sideLevels);
        blockReady = false;
    }

    phaseAnalyzer.process (left, right, numSamples);
    stereoAnalyzer.process (left, right, numSamples);

    const float* chans[2] = { left, right };
    levelMeter.process (chans, numChannels, numSamples);
    truePeak.process (chans, numChannels, numSamples);
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
