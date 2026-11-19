#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

/**
    Indicadores de imagen estereo:
      - Graves en mono: correlacion L/R solo de la banda por debajo de 120 Hz
        (filtro pasa bajos de 8o orden, -48 dB/oct). El grave deberia estar
        casi en +1. Si la banda grave esta mas de 25 dB por debajo del resto
        del sonido (por ejemplo, un bajo con pasa altos), se reporta "sin
        graves": lo que queda ahi es solo fuga de frecuencias mas altas.
      - Balance L/R: diferencia de nivel promedio (ventana lenta, ~2 s) entre
        izquierda y derecha, para saber si la mezcla se carga hacia un lado.

    process() corre en el hilo de audio; la interfaz solo lee atomicos.
    Solo mide: no toca la senal.
*/
class StereoAnalyzer
{
public:
    static constexpr double lowCutoffHz = 120.0;

    // Umbrales de la luz de graves (correlacion de la banda grave)
    static constexpr float lowMonoAbove   = 0.9f;   // verde
    static constexpr float lowSlightAbove = 0.7f;   // amarillo; debajo, rojo

    // Umbrales del balance (dB de diferencia entre L y R)
    static constexpr float balanceCentredDb = 1.0f;   // verde
    static constexpr float balanceLeaningDb = 3.0f;   // amarillo; arriba, rojo

    enum class Status { noSignal, ok, warn, danger };

    static Status lowStatusFor (float correlation, bool signalPresent) noexcept
    {
        if (! signalPresent)                return Status::noSignal;
        if (correlation >= lowMonoAbove)    return Status::ok;
        if (correlation >= lowSlightAbove)  return Status::warn;
        return Status::danger;
    }

    static Status balanceStatusFor (float balanceDb, bool signalPresent) noexcept
    {
        if (! signalPresent)                          return Status::noSignal;
        const float a = std::abs (balanceDb);
        if (a < balanceCentredDb)                     return Status::ok;
        if (a < balanceLeaningDb)                     return Status::warn;
        return Status::danger;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        computeLowPass();
        for (auto& s : states) s = {};
        lLR = lLL = lRR = 0.0;
        fLL = fRR = 0.0;
        msL = msR = 0.0;
        lowCorrelation.store (1.0f);
        lowSignal.store (false);
        balanceDb.store (0.0f);
        balanceSignal.store (false);
    }

    // --- Hilo de audio -----------------------------------------------------
    void process (const float* left, const float* right, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        double bLR = 0.0, bLL = 0.0, bRR = 0.0, fullL = 0.0, fullR = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double l = left[i], r = right[i];
            fullL += l * l;
            fullR += r * r;

            const double lowL = lowPass (0, l);
            const double lowR = lowPass (4, r);
            bLR += lowL * lowR;
            bLL += lowL * lowL;
            bRR += lowR * lowR;
        }

        const double n = (double) numSamples;

        // Graves: promedio de ~500 ms
        const double aLow = 1.0 - std::exp (-n / (0.5 * sr));
        lLR += aLow * (bLR / n - lLR);
        lLL += aLow * (bLL / n - lLL);
        lRR += aLow * (bRR / n - lRR);
        fLL += aLow * (fullL / n - fLL);
        fRR += aLow * (fullR / n - fRR);

        // Hay graves si superan el piso absoluto Y no estan muy por debajo
        // del sonido completo (evita falsas alarmas por fuga del filtro).
        const double lowEnergy  = lLL + lRR;
        const double fullEnergy = fLL + fRR;
        const bool lowPresent = lowEnergy > lowThreshold && lowEnergy > fullEnergy * lowRelative;
        lowSignal.store (lowPresent);

        if (lowPresent)
        {
            const double louder  = juce::jmax (lLL, lRR);
            const double quieter = juce::jmin (lLL, lRR);
            const double denom   = std::sqrt (lLL * lRR);
            lowCorrelation.store ((quieter > louder * 1.0e-3 && denom > 1.0e-15)
                                    ? (float) juce::jlimit (-1.0, 1.0, lLR / denom)
                                    : 0.0f);
        }

        // Balance: promedio lento de ~2 s para ver la tendencia, no cada golpe
        const double aBal = 1.0 - std::exp (-n / (2.0 * sr));
        msL += aBal * (fullL / n - msL);
        msR += aBal * (fullR / n - msR);

        const bool balPresent = (msL + msR) > balanceThreshold;
        balanceSignal.store (balPresent);

        if (balPresent)
        {
            // Positivo: la derecha esta mas fuerte. Limitado a +/-12 dB.
            const double db = 10.0 * std::log10 ((msR + 1.0e-12) / (msL + 1.0e-12));
            balanceDb.store ((float) juce::jlimit (-12.0, 12.0, db));
        }
    }

    // --- Interfaz ------------------------------------------------------------
    float getLowCorrelation() const noexcept { return lowCorrelation.load(); }
    bool  hasLowSignal() const noexcept      { return lowSignal.load(); }
    float getBalanceDb() const noexcept      { return balanceDb.load(); }
    bool  hasBalanceSignal() const noexcept  { return balanceSignal.load(); }

private:
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    double run (State& s, double x) const noexcept
    {
        const double y = lp.b0 * x + lp.b1 * s.x1 + lp.b2 * s.x2 - lp.a1 * s.y1 - lp.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }

    // Cuatro Butterworth de 2o orden en serie: 8o orden, -48 dB/oct
    double lowPass (int first, double x) noexcept
    {
        for (int i = 0; i < 4; ++i)
            x = run (states[(size_t) (first + i)], x);
        return x;
    }

    void computeLowPass()
    {
        const double w0    = juce::MathConstants<double>::twoPi * lowCutoffHz / sr;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * 0.7071067811865476);
        const double a0    = 1.0 + alpha;
        lp.b0 = ((1.0 - cosw) * 0.5) / a0;
        lp.b1 = (1.0 - cosw) / a0;
        lp.b2 = lp.b0;
        lp.a1 = (-2.0 * cosw) / a0;
        lp.a2 = (1.0 - alpha) / a0;
    }

    static constexpr double lowThreshold     = 1.0e-6;   // ~ -60 dBFS en la banda grave
    static constexpr double lowRelative      = 0.00316;  // -25 dB respecto al sonido completo
    static constexpr double balanceThreshold = 1.0e-7;   // ~ -70 dBFS

    double sr = 44100.0;
    Biquad lp;
    std::array<State, 8> states {};          // 4 etapas para L (0-3) y 4 para R (4-7)
    double lLR = 0.0, lLL = 0.0, lRR = 0.0;  // solo hilo de audio
    double fLL = 0.0, fRR = 0.0;             // energia de banda completa, mismo promedio
    double msL = 0.0, msR = 0.0;

    std::atomic<float> lowCorrelation { 1.0f };
    std::atomic<bool>  lowSignal { false };
    std::atomic<float> balanceDb { 0.0f };
    std::atomic<bool>  balanceSignal { false };
};
