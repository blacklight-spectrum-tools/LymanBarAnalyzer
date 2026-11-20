#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

/**
    True Peak (ITU-R BS.1770, anexo 2): el pico real de la onda, incluido el
    que queda ENTRE muestras. Es el que distorsiona al convertir a MP3/AAC o
    en el conversor de la tarjeta de sonido. El pico de muestra puede leer
    hasta ~3 dB menos.

    La senal se sobremuestrea (x4 a 44.1/48 kHz, x2 a 88.2/96 kHz, x1 desde
    176.4 kHz) con un filtro de interpolacion polifasico (sinc con ventana de
    Kaiser, 16 coeficientes por fase) y se busca el maximo.

    Ademas cuenta los "clips": cada vez que el True Peak pasa de 0 dBTP
    (un evento nuevo si estuvo 50 ms por debajo).

    process() corre en el hilo de audio; la interfaz solo lee atomicos.
    Solo mide: no toca la senal.
*/
class TruePeakMeter
{
public:
    static constexpr int tapsPerPhase = 16;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        factor = sr < 80000.0 ? 4 : (sr < 160000.0 ? 2 : 1);
        design();
        for (auto& h : history) h.fill (0.0f);
        histPos = 0;
        quietSamples = holdSamples = (int) (0.05 * sr);
        reset();
    }

    void requestReset() noexcept { resetRequested.store (true); }

    // --- Hilo de audio ---------------------------------------------------
    void process (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;
        if (resetRequested.exchange (false))
            reset();

        const int used = juce::jmin (numChannels, 2);
        float blockMax = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float samplePeak = 0.0f;
            histPos = (histPos + 1) & histMask;

            for (int c = 0; c < used; ++c)
            {
                const float x = channels[c][i];
                history[(size_t) c][(size_t) histPos] = x;
                samplePeak = juce::jmax (samplePeak, std::abs (x));

                if (factor > 1)
                    for (int ph = 1; ph < factor; ++ph)   // fase 0 = la muestra misma
                    {
                        const float* h = coeffs[(size_t) ph].data();
                        float acc = 0.0f;
                        for (int k = 0; k < tapsPerPhase; ++k)
                            acc += h[k] * history[(size_t) c][(size_t) ((histPos - k) & histMask)];
                        samplePeak = juce::jmax (samplePeak, std::abs (acc));
                    }
            }

            blockMax = juce::jmax (blockMax, samplePeak);

            // Contador de clips: eventos por encima de 0 dBTP
            if (samplePeak > 1.0f)
            {
                if (quietSamples >= holdSamples)
                    clipEvents.fetch_add (1);
                quietSamples = 0;
            }
            else if (quietSamples < holdSamples)
            {
                ++quietSamples;
            }
        }

        float prev = recent.load (std::memory_order_relaxed);
        while (blockMax > prev && ! recent.compare_exchange_weak (prev, blockMax)) {}
        if (blockMax > maxLinear.load()) maxLinear.store (blockMax);
    }

    // --- Interfaz ----------------------------------------------------------
    /** True Peak since the last read (linear), then resets it. */
    float takeRecent() noexcept            { return recent.exchange (0.0f); }
    /** Highest True Peak since the last reset, in dBTP. */
    float getMaxDb() const noexcept        { return juce::Decibels::gainToDecibels (maxLinear.load(), -100.0f); }
    int   getClipCount() const noexcept    { return clipEvents.load(); }
    int   getOversampling() const noexcept { return factor; }

private:
    void reset() noexcept
    {
        recent.store (0.0f);
        maxLinear.store (0.0f);
        clipEvents.store (0);
        quietSamples = holdSamples;
    }

    // Filtro de interpolacion: sinc con corte en la frecuencia de Nyquist
    // original, ventana de Kaiser (beta 8). Cada fase se normaliza a ganancia
    // 1 en continua.
    void design()
    {
        // Centro entero: la fase 0 cae justo en las muestras originales
        // (es la muestra misma) y las fases 1..factor-1 en 1/4, 2/4, 3/4.
        const int total = tapsPerPhase * factor;
        const double centre = (double) (tapsPerPhase / 2 * factor);
        const double beta = 8.0;
        auto bessel0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 30; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
            return sum;
        };
        std::vector<double> h ((size_t) total);
        for (int n = 0; n < total; ++n)
        {
            const double t = (n - centre) / (double) factor;
            const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
            const double r = (n - centre) / centre;
            const double w = bessel0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / bessel0 (beta);
            h[(size_t) n] = sinc * w;
        }
        for (int ph = 0; ph < 4; ++ph)
            coeffs[(size_t) ph].fill (0.0f);
        for (int ph = 0; ph < factor; ++ph)
        {
            double sum = 0.0;
            for (int k = 0; k < tapsPerPhase; ++k) sum += h[(size_t) (k * factor + ph)];
            for (int k = 0; k < tapsPerPhase; ++k)
                coeffs[(size_t) ph][(size_t) k] = (float) (h[(size_t) (k * factor + ph)] / sum);
        }
    }

    static constexpr int histSize = 32, histMask = histSize - 1;

    double sr = 44100.0;
    int factor = 4;
    std::array<std::array<float, tapsPerPhase>, 4> coeffs {};
    std::array<std::array<float, histSize>, 2> history {};
    int histPos = 0;
    int quietSamples = 0, holdSamples = 2400;

    std::atomic<bool>  resetRequested { false };
    std::atomic<float> recent { 0.0f };
    std::atomic<float> maxLinear { 0.0f };
    std::atomic<int>   clipEvents { 0 };
};
