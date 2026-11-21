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
    for (auto* arr : { &curveMid, &curveSide })
        for (auto& c : *arr)
            c.store (-100.0f);

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

    // Curva: FFT 8 veces mas grande que la de las barras. Se calcula cada ~86 ms
    // (cada ~170 ms arriba de 66 kHz): con el promedio no se nota y ahorra CPU.
    const int curveOrder = order + 3;
    if (curveFft == nullptr || (1 << curveOrder) != curveFft->getSize())
    {
        curveFftSize = 1 << curveOrder;
        curveFft     = std::make_unique<juce::dsp::FFT> (curveOrder);
        curveWindow  = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) curveFftSize, juce::dsp::WindowingFunction<float>::hann);
    }
    curveEvery = currentSampleRate > 66000.0 ? 4 : 2;

    // Ventana de cada punto de la curva (~1/24 de octava), en divisiones
    const double curveBin = currentSampleRate / (double) curveFftSize;
    const int    lastCurveBin = curveFftSize / 2 - 1;
    for (int p = 0; p < numCurvePoints; ++p)
    {
        const double f = getCurveFrequency (p);
        curveLoBin[(size_t) p] = juce::jlimit (2, lastCurveBin - 1, (int) std::ceil  (f * std::pow (2.0, -1.0 / 48.0) / curveBin));
        curveHiBin[(size_t) p] = juce::jlimit (2, lastCurveBin - 1, (int) std::floor (f * std::pow (2.0,  1.0 / 48.0) / curveBin));
        curvePos[(size_t) p]   = juce::jlimit (2.0, (double) lastCurveBin - 1.0, f / curveBin);
    }
    for (auto& r : curveRing)
        r.assign ((size_t) curveFftSize, 0.0f);
    curveData.assign ((size_t) curveFftSize * 2, 0.0f);
    avgPowerMid.assign  ((size_t) curveFftSize / 2 + 1, 0.0f);
    avgPowerSide.assign ((size_t) curveFftSize / 2 + 1, 0.0f);
    curveWrite = curveFrameCounter = 0;
    curveActive = false;
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

    // Estilo de la grafica: barras (por defecto) o curva
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "style", 1 }, "Style",
        juce::StringArray { "Bars", "Curve" }, 0));

    // Promedio de la curva (como el "Avg time" de SPAN). Las barras no se
    // promedian: siguen mostrando los golpes tal cual para decidir.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "curveSpeed", 1 }, "Curve speed",
        juce::StringArray { "Fast", "Normal", "Slow" }, 1));

    // Rango vertical de la grafica (0 dB arriba)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "range", 1 }, "Range",
        juce::StringArray { "60 dB", "72 dB", "84 dB" }, 0));

    // Peak hold: normal (quieto 1 s y luego cae) o infinito (el maximo de
    // toda la cancion, hasta reiniciar con doble clic o desde el engrane)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "peakHold", 1 }, "Peak hold",
        juce::StringArray { "Normal", "Infinite" }, 0));
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

    curveRing[0][(size_t) curveWrite] = mid;
    curveRing[1][(size_t) curveWrite] = side;
    if (++curveWrite == curveFftSize)
        curveWrite = 0;
}

float BarSpectrumAnalyzerAudioProcessor::averagingCoefficient (int hopSamples) const
{
    // Constante de tiempo del promedio: Fast 0.25 s, Normal 1 s, Slow 3 s
    static constexpr double tau[] = { 0.25, 1.0, 3.0 };
    const int speed = juce::jlimit (0, 2, (int) std::round (apvts.getRawParameterValue ("curveSpeed")->load()));
    return (float) (1.0 - std::exp (-(double) hopSamples / (tau[speed] * currentSampleRate)));
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

void BarSpectrumAnalyzerAudioProcessor::computeCurveSpectrum (bool restart)
{
    // restart: la curva estaba apagada -> se arranca con el cuadro actual
    // en vez de mezclar con un promedio viejo.
    const float alpha = restart ? 1.0f : averagingCoefficient (fftSize * curveEvery);

    for (int c = 0; c < 2; ++c)
    {
        // La historia del anillo en orden (la muestra mas vieja primero), sin
        // su promedio (el DC no debe colarse en los graves de la curva)
        const auto& ring = curveRing[(size_t) c];
        const int older = curveFftSize - curveWrite;
        std::copy (ring.begin() + curveWrite, ring.end(), curveData.begin());
        std::copy (ring.begin(), ring.begin() + curveWrite, curveData.begin() + older);
        double mean = 0.0;
        for (int i = 0; i < curveFftSize; ++i)
            mean += curveData[(size_t) i];
        mean /= (double) curveFftSize;
        for (int i = 0; i < curveFftSize; ++i)
            curveData[(size_t) i] -= (float) mean;
        std::fill (curveData.begin() + curveFftSize, curveData.end(), 0.0f);

        curveWindow->multiplyWithWindowingTable (curveData.data(), (size_t) curveFftSize);
        curveFft->performFrequencyOnlyForwardTransform (curveData.data());

        auto& avgPower = c == 0 ? avgPowerMid : avgPowerSide;
        for (int bin = 0; bin <= curveFftSize / 2; ++bin)
        {
            float& avg = avgPower[(size_t) bin];
            const float pw = curveData[(size_t) bin] * curveData[(size_t) bin];
            avg += alpha * (pw - avg);
            if (avg < 1.0e-20f) avg = 0.0f;
        }

        computeCurve (avgPower, c == 0 ? curveMid : curveSide);
    }
}

void BarSpectrumAnalyzerAudioProcessor::computeCurve (const std::vector<float>& avgPower,
                                                      std::array<std::atomic<float>, numCurvePoints>& curve)
{
    // Cada punto toma el maximo del espectro promediado en una ventana del
    // ancho de un punto (~1/24 de octava), con interpolacion parabolica para
    // que un tono entre dos divisiones lea su nivel real. Si la ventana es
    // mas angosta que una division (graves), se evalua la parabola que pasa
    // por las tres divisiones mas cercanas.
    // Calibracion: igual que las barras, un seno de X dBFS lee X dB.
    const int    lastBin  = curveFftSize / 2 - 1;
    const float  scale2   = (2.0f / (float) curveFftSize) * (2.0f / (float) curveFftSize);
    auto dbOf = [&] (int k) { return 10.0f * std::log10 (juce::jmax (1.0e-10f, avgPower[(size_t) k] * scale2)); };

    for (int p = 0; p < numCurvePoints; ++p)
    {
        const int b0 = curveLoBin[(size_t) p], b1 = curveHiBin[(size_t) p];
        float db;

        if (b1 >= b0)
        {
            int best = b0;
            for (int bin = b0 + 1; bin <= b1; ++bin)
                if (avgPower[(size_t) bin] > avgPower[(size_t) best]) best = bin;

            db = dbOf (best);
            const float a = dbOf (best - 1), c = dbOf (best + 1), den = a - 2.0f * db + c;
            if (a <= db && c <= db && den < 0.0f)
            {
                const float off = juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / den);
                db -= 0.25f * (a - c) * off;
            }
        }
        else
        {
            // Entre dos divisiones: linea recta en dB, y cerca de un maximo
            // la parabola de ese maximo (asi el pico de una nota queda en su
            // frecuencia y nivel reales, sin inventar picos al lado).
            const double pos = curvePos[(size_t) p];
            const int    k0  = juce::jlimit (2, lastBin - 2, (int) std::floor (pos));
            const float  y0 = dbOf (k0), y1 = dbOf (k0 + 1);
            db = y0 + (y1 - y0) * (float) (pos - k0);

            for (int m : { k0, k0 + 1 })
            {
                const float a = dbOf (m - 1), b = dbOf (m), c = dbOf (m + 1), den = a - 2.0f * b + c;
                if (a <= b && c <= b && den < 0.0f)
                {
                    const float d = (float) (pos - m);
                    db = juce::jmax (db, b + 0.5f * d * (c - a) + 0.5f * d * d * den);
                }
            }
        }

        curve[(size_t) p].store (juce::jmax (-100.0f, db));
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

        // La curva solo se calcula si hay una ventana abierta en estilo Curve
        const bool wantCurve = editorsOpen.load() > 0
                            && apvts.getRawParameterValue ("style")->load() > 0.5f;
        if (wantCurve && ++curveFrameCounter >= curveEvery)
        {
            curveFrameCounter = 0;
            computeCurveSpectrum (! curveActive);
            curveActive = true;
        }
        else if (! wantCurve)
        {
            curveActive = false;
        }
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
