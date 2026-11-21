#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

/**
    Medidor de nivel: pico de muestra, RMS (~300 ms), foco de clip, DC offset
    y sonoridad segun ITU-R BS.1770 / EBU R128:
      - Momentary (400 ms) y Short-term (3 s), con sus maximos.
      - Integrated: toda la cancion desde el ultimo reinicio, con compuertas
        absoluta (-70 LUFS) y relativa (-10 LU), bloques de 400 ms cada 100 ms.
      - LRA (EBU Tech 3342): percentiles 10-95 de los valores short-term,
        con compuerta absoluta y relativa (-20 LU).
    Integrated y LRA se guardan en histogramas de 0.1 LU (memoria fija, no
    crecen con la duracion).

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
        dcMean = { 0.0, 0.0 };
        dcLevel.store (0.0f);
        resetLoudness();
    }

    /** Reinicia integrado, LRA y maximos (desde la interfaz: se aplica en el
        hilo de audio en el siguiente bloque). */
    void requestLoudnessReset() noexcept { resetRequested.store (true); }

    // --- Hilo de audio -----------------------------------------------------
    void process (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;

        if (resetRequested.exchange (false))
            resetLoudness();

        // Se parte el bloque justo en los limites de 100 ms, asi la medicion
        // es exacta aunque el DAW mande bloques grandes.
        const int subBlockSize = juce::jmax (1, (int) std::lround (0.1 * sr));
        const float* part[2] = { nullptr, nullptr };
        int done = 0;
        while (done < numSamples)
        {
            const int n = juce::jmin (numSamples - done, subBlockSize - subBlockSamples);
            for (int c = 0; c < juce::jmin (numChannels, 2); ++c)
                part[c] = channels[c] + done;
            processChunk (part, numChannels, n);
            done += n;
        }
    }

private:
    void processChunk (const float* const* channels, int numChannels, int numSamples) noexcept
    {

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

            // DC offset: promedio muy lento (~3 s) de la senal sin ponderar
            double dcSum = 0.0;
            for (int i = 0; i < numSamples; ++i) dcSum += x[i];
            const double aDc = 1.0 - std::exp (-n / (3.0 * sr));   // ~3 s
            dcMean[(size_t) c] += aDc * (dcSum / n - dcMean[(size_t) c]);

            meanSquare[(size_t) c] += a * (sum / n - meanSquare[(size_t) c]);
            rmsDb[(size_t) c].store (toDb (meanSquare[(size_t) c]));

            subBlockEnergy += kSum;   // BS.1770: se suman las energias de los canales
        }

        // Mono: el mismo canal se muestra en L y R
        if (used == 1)
            rmsDb[1].store (rmsDb[0].load());

        dcLevel.store ((float) juce::jmax (std::abs (dcMean[0]), used > 1 ? std::abs (dcMean[1]) : 0.0));

        subBlockSamples += numSamples;
        const int subBlockSize = juce::jmax (1, (int) std::lround (0.1 * sr));   // bloques de 100 ms

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
            const float  shortL = toLufs (ms);
            lufsShort.store (shortL);

            // Momentary: los ultimos 4 bloques de 100 ms (400 ms)
            ++blocksSinceReset;
            secondsBlocks.store (blocksSinceReset);
            if (ringCount >= 4)
            {
                double m = 0.0;
                for (int i = 1; i <= 4; ++i)
                    m += ring[(size_t) ((ringPos - i + ringSize) % ringSize)];
                m *= 0.25;
                const float momL = toLufs (m);
                lufsMomentary.store (momL);

                if (blocksSinceReset >= 4)
                {
                    maxMomentary.store (juce::jmax (maxMomentary.load(), momL));
                    addToHistogram (integratedHist, m, momL);
                    lufsIntegrated.store (gatedLoudness (integratedHist, -10.0));
                }
            }

            // Short-term ventana completa (3 s): maximo y LRA
            if (ringCount >= ringSize && blocksSinceReset >= ringSize)
            {
                maxShort.store (juce::jmax (maxShort.load(), shortL));
                addToHistogram (shortHist, ms, shortL);
                loudnessRange.store (computeLra());
            }
        }
    }

public:
    // --- Interfaz ------------------------------------------------------------
    /** Pico lineal desde la ultima lectura (y lo reinicia). */
    float takePeak (int channel) noexcept       { return peak[(size_t) channel].exchange (0.0f); }
    float getRmsDb (int channel) const noexcept { return rmsDb[(size_t) channel].load(); }
    float getLufsShort() const noexcept         { return lufsShort.load(); }
    bool  hasClipped() const noexcept           { return clip.load(); }
    void  resetClip() noexcept                  { clip.store (false); }

    float getLufsMomentary() const noexcept     { return lufsMomentary.load(); }
    float getLufsIntegrated() const noexcept    { return lufsIntegrated.load(); }
    float getLoudnessRange() const noexcept     { return loudnessRange.load(); }
    float getMaxMomentary() const noexcept      { return maxMomentary.load(); }
    float getMaxShort() const noexcept          { return maxShort.load(); }
    /** Seconds measured for the integrated value (since the last reset). */
    double getIntegratedSeconds() const noexcept { return 0.1 * (double) secondsBlocks.load(); }

    /** Absolute DC offset (linear, largest of L/R). Above ~0.003 it matters. */
    float getDcOffset() const noexcept          { return dcLevel.load(); }
    // ~ -40 dBFS. Mas bajo daba falsas alarmas: un kick asimetrico (la onda
    // empieza hacia arriba y decae) promedia ~0.003 sin tener DC real.
    static constexpr float dcWarnLevel = 0.01f;

private:
    // --- Histogramas de sonoridad (0.1 LU por casilla, de -70 a +10 LUFS) ---
    static constexpr double histMin = -70.0, histStep = 0.1;
    static constexpr int    histBins = 800;
    struct Histogram
    {
        std::array<uint32_t, histBins> count {};
        std::array<double, histBins>   power {};
        uint64_t total = 0;
        void clear() noexcept { count.fill (0); power.fill (0.0); total = 0; }
    };

    static float toLufs (double ms) noexcept
    {
        return ms > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : floorDb;
    }

    static void addToHistogram (Histogram& h, double ms, float lufs) noexcept
    {
        if (lufs <= (float) histMin)    // compuerta absoluta de -70 LUFS
            return;
        const int bin = juce::jlimit (0, histBins - 1, (int) ((lufs - histMin) / histStep));
        ++h.count[(size_t) bin];
        h.power[(size_t) bin] += ms;
        ++h.total;
    }

    // Promedio de potencia de los bloques que pasan la compuerta relativa
    // (relGateLu por debajo del promedio con compuerta absoluta).
    static float gatedLoudness (const Histogram& h, double relGateLu) noexcept
    {
        if (h.total == 0) return floorDb;
        double p = 0.0;
        for (int i = 0; i < histBins; ++i) p += h.power[(size_t) i];
        const double gate = -0.691 + 10.0 * std::log10 (p / (double) h.total) + relGateLu;
        const int first = juce::jlimit (0, histBins - 1, (int) std::ceil ((gate - histMin) / histStep));
        double gp = 0.0; uint64_t gc = 0;
        for (int i = first; i < histBins; ++i) { gp += h.power[(size_t) i]; gc += h.count[(size_t) i]; }
        return gc > 0 ? toLufs (gp / (double) gc) : floorDb;
    }

    float computeLra() const noexcept
    {
        const auto& h = shortHist;
        if (h.total == 0) return 0.0f;
        double p = 0.0;
        for (int i = 0; i < histBins; ++i) p += h.power[(size_t) i];
        const double gate = -0.691 + 10.0 * std::log10 (p / (double) h.total) - 20.0;
        const int first = juce::jlimit (0, histBins - 1, (int) std::ceil ((gate - histMin) / histStep));
        uint64_t n = 0;
        for (int i = first; i < histBins; ++i) n += h.count[(size_t) i];
        if (n < 2) return 0.0f;
        auto percentile = [&] (double q)
        {
            const uint64_t target = (uint64_t) std::llround (q * (double) (n - 1));
            uint64_t acc = 0;
            for (int i = first; i < histBins; ++i)
            {
                acc += h.count[(size_t) i];
                if (acc > target) return histMin + (i + 0.5) * histStep;
            }
            return histMin + (histBins - 0.5) * histStep;
        };
        return (float) juce::jmax (0.0, percentile (0.95) - percentile (0.10));
    }

    void resetLoudness() noexcept
    {
        integratedHist.clear();
        shortHist.clear();
        blocksSinceReset = 0;
        secondsBlocks.store (0);
        lufsMomentary.store (floorDb);
        lufsIntegrated.store (floorDb);
        loudnessRange.store (0.0f);
        maxMomentary.store (floorDb);
        maxShort.store (floorDb);
    }

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

    std::array<double, 2> dcMean {};
    std::atomic<float> dcLevel { 0.0f };

    Histogram integratedHist, shortHist;
    int blocksSinceReset = 0;
    std::atomic<int>   secondsBlocks { 0 };
    std::atomic<bool>  resetRequested { false };
    std::atomic<float> lufsMomentary { floorDb };
    std::atomic<float> lufsIntegrated { floorDb };
    std::atomic<float> loudnessRange { 0.0f };
    std::atomic<float> maxMomentary { floorDb };
    std::atomic<float> maxShort { floorDb };
};
