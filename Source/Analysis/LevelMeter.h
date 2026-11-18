#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

/**
    Medidor de nivel: pico de muestra, RMS (~300 ms), LUFS de corto plazo
    (ventana de 3 s, ponderacion K segun ITU-R BS.1770) y foco de clip.

    process() corre en el hilo de audio; la interfaz solo lee atomicos.
    Solo mide: no toca la senal.
*/
class LevelMeter
{
public:
    static constexpr float floorDb = -100.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        computeKWeighting();

        for (auto& f : filters) f = {};
        for (auto& m : meanSquare) m = 0.0;
        for (auto& p : peak) p.store (0.0f);
        for (auto& r : rmsDb) r.store (floorDb);

        ring.fill (0.0);
        ringCount = 0;
        ringPos = 0;
        subBlockEnergy = 0.0;
        subBlockSamples = 0;
        lufsShort.store (floorDb);
        clip.store (false);
    }

    // --- Hilo de audio -----------------------------------------------------
    void process (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;

        const int used = juce::jmin (numChannels, 2);
        const double n = (double) numSamples;
        const double a = 1.0 - std::exp (-n / (rmsSeconds * sr));

        for (int c = 0; c < used; ++c)
        {
            const float* x = channels[c];
            auto& f = filters[(size_t) c];
            float pk = 0.0f;
            double sum = 0.0, kSum = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const double s = x[i];
                pk   = juce::jmax (pk, std::abs (x[i]));
                sum += s * s;

                // Ponderacion K: estante de agudos + pasa altos (dos biquads)
                const double y1 = pre.b0 * s + pre.b1 * f.x1 + pre.b2 * f.x2 - pre.a1 * f.y1 - pre.a2 * f.y2;
                f.x2 = f.x1; f.x1 = s; f.y2 = f.y1; f.y1 = y1;
                const double y2 = rlb.b0 * y1 + rlb.b1 * f.u1 + rlb.b2 * f.u2 - rlb.a1 * f.v1 - rlb.a2 * f.v2;
                f.u2 = f.u1; f.u1 = y1; f.v2 = f.v1; f.v1 = y2;

                kSum += y2 * y2;
            }

            // Pico acumulado hasta que la interfaz lo lea
            float prev = peak[(size_t) c].load (std::memory_order_relaxed);
            while (pk > prev && ! peak[(size_t) c].compare_exchange_weak (prev, pk)) {}

            if (pk >= 1.0f)
                clip.store (true);

            meanSquare[(size_t) c] += a * (sum / n - meanSquare[(size_t) c]);
            rmsDb[(size_t) c].store (toDb (meanSquare[(size_t) c]));

            subBlockEnergy += kSum;   // BS.1770: se suman las energias de los canales
        }

        // Mono: el mismo canal se muestra en L y R
        if (used == 1)
            rmsDb[1].store (rmsDb[0].load());

        subBlockSamples += numSamples;
        const int subBlockSize = (int) (0.1 * sr);   // bloques de 100 ms

        if (subBlockSamples >= subBlockSize)
        {
            ring[(size_t) ringPos] = subBlockEnergy / (double) subBlockSamples;
            ringPos = (ringPos + 1) % ringSize;
            ringCount = juce::jmin (ringCount + 1, ringSize);
            subBlockEnergy = 0.0;
            subBlockSamples = 0;

            double total = 0.0;
            for (int i = 0; i < ringCount; ++i)
                total += ring[(size_t) i];

            const double ms = total / (double) ringCount;
            lufsShort.store (ms > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : floorDb);
        }
    }

    // --- Interfaz ------------------------------------------------------------
    /** Pico lineal desde la ultima lectura (y lo reinicia). */
    float takePeak (int channel) noexcept       { return peak[(size_t) channel].exchange (0.0f); }
    float getRmsDb (int channel) const noexcept { return rmsDb[(size_t) channel].load(); }
    float getLufsShort() const noexcept         { return lufsShort.load(); }
    bool  hasClipped() const noexcept           { return clip.load(); }
    void  resetClip() noexcept                  { clip.store (false); }

private:
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0; };

    static float toDb (double ms) noexcept
    {
        return ms > 1.0e-10 ? (float) (10.0 * std::log10 (ms)) : floorDb;
    }

    // Coeficientes de BS.1770 para cualquier frecuencia de muestreo
    // (misma derivacion que usa libebur128).
    void computeKWeighting()
    {
        const double pi = juce::MathConstants<double>::pi;

        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K  = std::tan (pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            pre.b0 = (Vh + Vb * K / Q + K * K) / a0;
            pre.b1 = 2.0 * (K * K - Vh) / a0;
            pre.b2 = (Vh - Vb * K / Q + K * K) / a0;
            pre.a1 = 2.0 * (K * K - 1.0) / a0;
            pre.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K  = std::tan (pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            rlb.b0 = 1.0; rlb.b1 = -2.0; rlb.b2 = 1.0;
            rlb.a1 = 2.0 * (K * K - 1.0) / a0;
            rlb.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    static constexpr double rmsSeconds = 0.3;
    static constexpr int    ringSize   = 30;          // 30 x 100 ms = 3 s

    double sr = 44100.0;
    Biquad pre, rlb;
    std::array<State, 2>  filters {};
    std::array<double, 2> meanSquare {};

    std::array<double, ringSize> ring {};
    int ringCount = 0, ringPos = 0;
    double subBlockEnergy = 0.0;
    int subBlockSamples = 0;

    std::array<std::atomic<float>, 2> peak {};
    std::array<std::atomic<float>, 2> rmsDb {};
    std::atomic<float> lufsShort { floorDb };
    std::atomic<bool>  clip { false };
};
