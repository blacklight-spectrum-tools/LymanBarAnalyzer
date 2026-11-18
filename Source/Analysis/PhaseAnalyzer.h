#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

/**
    Analisis de fase estereo: correlacion L/R, minimo/maximo retenidos y un
    buffer circular con las ultimas muestras para dibujar el goniometro.

    process() corre en el hilo de audio; todo lo que lee la interfaz son
    atomicos, asi la interfaz nunca bloquea al audio. Solo mide: no toca
    la senal.
*/
class PhaseAnalyzer
{
public:
    static constexpr int scopeSize = 2048;          // potencia de 2

    // Umbrales de la luz de fase (correlacion de -1 a +1)
    static constexpr float warnBelow   = 0.3f;      // por debajo: amarillo
    static constexpr float dangerBelow = 0.0f;      // por debajo: rojo

    enum class Status { noSignal, ok, warn, danger };

    static Status statusFor (float correlation, bool signalPresent) noexcept
    {
        if (! signalPresent)             return Status::noSignal;
        if (correlation < dangerBelow)   return Status::danger;
        if (correlation < warnBelow)     return Status::warn;
        return Status::ok;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        sLR = sLL = sRR = 0.0;
        samplesSinceSignal = 1 << 30;
        correlation.store (1.0f);
        signal.store (false);
        applyReset();
        resetRequested.store (false);

        for (auto& s : scopeL) s.store (0.0f, std::memory_order_relaxed);
        for (auto& s : scopeR) s.store (0.0f, std::memory_order_relaxed);
        writePos.store (0);
    }

    // --- Hilo de audio -----------------------------------------------------
    void process (const float* left, const float* right, int numSamples) noexcept
    {
        if (resetRequested.exchange (false))
            applyReset();

        if (numSamples <= 0)
            return;

        double bLR = 0.0, bLL = 0.0, bRR = 0.0;
        int w = writePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i];
            const float r = right[i];
            bLR += (double) l * r;
            bLL += (double) l * l;
            bRR += (double) r * r;

            scopeL[(size_t) w].store (l, std::memory_order_relaxed);
            scopeR[(size_t) w].store (r, std::memory_order_relaxed);
            w = (w + 1) & (scopeSize - 1);
        }

        writePos.store (w, std::memory_order_release);

        // Promedio exponencial (~300 ms) para que la lectura no tiemble
        const double n = (double) numSamples;
        const double a = 1.0 - std::exp (-n / (smoothingSeconds * sr));
        sLR += a * (bLR / n - sLR);
        sLL += a * (bLL / n - sLL);
        sRR += a * (bRR / n - sRR);

        // Deteccion de senal con el bloque actual (no con el promedio, que
        // tarda segundos en caer). Se sostiene ~250 ms para que no parpadee.
        if ((bLL + bRR) / n > silenceThreshold)
            samplesSinceSignal = 0;
        else if (samplesSinceSignal < (1 << 30))         // tope para no desbordar
            samplesSinceSignal += numSamples;

        const bool present = samplesSinceSignal < (int) (signalHoldSeconds * sr);
        signal.store (present);

        if (! present)
            return;

        // Correlacion de Pearson sin restar la media (el audio no tiene DC).
        // Si un canal esta mas de 30 dB por debajo del otro (paneo duro) la
        // correlacion es 0: no hay nada que se cancele entre los dos lados.
        const double louder  = juce::jmax (sLL, sRR);
        const double quieter = juce::jmin (sLL, sRR);
        const double denom   = std::sqrt (sLL * sRR);
        const float c = (quieter > louder * 1.0e-3 && denom > 1.0e-12)
                          ? (float) juce::jlimit (-1.0, 1.0, sLR / denom)
                          : 0.0f;
        correlation.store (c);

        if (! holdValid.load())
        {
            minHold.store (c);
            maxHold.store (c);
            holdValid.store (true);
        }
        else
        {
            if (c < minHold.load()) minHold.store (c);
            if (c > maxHold.load()) maxHold.store (c);
        }
    }

    // --- Interfaz ------------------------------------------------------------
    float getCorrelation() const noexcept     { return correlation.load(); }
    bool  isSignalPresent() const noexcept    { return signal.load(); }
    bool  hasHold() const noexcept            { return holdValid.load(); }
    float getMinHold() const noexcept         { return minHold.load(); }
    float getMaxHold() const noexcept         { return maxHold.load(); }
    void  requestReset() noexcept             { resetRequested.store (true); }

    /** Copia las ultimas `count` muestras L/R (la mas vieja primero). */
    void copyScope (float* outL, float* outR, int count) const noexcept
    {
        count = juce::jlimit (0, scopeSize, count);
        const int end = writePos.load (std::memory_order_acquire);
        int idx = (end - count) & (scopeSize - 1);

        for (int i = 0; i < count; ++i)
        {
            outL[i] = scopeL[(size_t) idx].load (std::memory_order_relaxed);
            outR[i] = scopeR[(size_t) idx].load (std::memory_order_relaxed);
            idx = (idx + 1) & (scopeSize - 1);
        }
    }

private:
    void applyReset() noexcept
    {
        holdValid.store (false);
        minHold.store (1.0f);
        maxHold.store (-1.0f);
    }

    static constexpr double smoothingSeconds = 0.3;
    static constexpr double silenceThreshold  = 1.0e-7;  // ~ -70 dBFS
    static constexpr double signalHoldSeconds = 0.25;

    double sr = 44100.0;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;               // solo hilo de audio
    int samplesSinceSignal = 1 << 30;                     // solo hilo de audio

    std::atomic<float> correlation { 1.0f };
    std::atomic<bool>  signal { false };
    std::atomic<bool>  holdValid { false };
    std::atomic<float> minHold { 1.0f };
    std::atomic<float> maxHold { -1.0f };
    std::atomic<bool>  resetRequested { false };

    std::array<std::atomic<float>, scopeSize> scopeL {};
    std::array<std::atomic<float>, scopeSize> scopeR {};
    std::atomic<int> writePos { 0 };
};
