#pragma once
#include <JuceHeader.h>
#include <array>
#include <complex>
#include <vector>

/**
    Banco de filtros para las barras graves (20 Hz a ~240 Hz).

    Con una FFT de 2048 muestras cada "cajon" mide ~23 Hz, mas ancho que las
    bandas graves (20-26 Hz, 26-33 Hz...), asi que una nota encendia igual a
    varias barras vecinas. Aqui cada banda grave tiene su propio filtro pasa
    banda Butterworth de 6o orden (como los analizadores RTA de tercio de
    octava): separa bien las bandas y responde rapido a los golpes.

    Cada banda guarda una envolvente de pico: un seno de amplitud A lee A
    (0 dBFS = 0 dB), igual que las barras de la FFT ya calibradas.

    Para ahorrar procesador, la senal se filtra (pasa bajos de 6o orden en
    1 kHz) y se analiza a ~6 kHz en vez de a la frecuencia del proyecto: las
    bandas llegan solo hasta ~240 Hz, asi que no se pierde nada.

    process() corre en el hilo de audio. Solo mide: no toca la senal.
*/
class LowBandBank
{
public:
    static constexpr int maxBands = 16;

    // edgesHz: bordes de banda (numBands + 1 valores), en orden creciente.
    void prepare (double sampleRate, const float* edgesHz, int bandsToUse)
    {
        inputRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        factor    = juce::jmax (1, (int) std::floor (inputRate / 6000.0));
        sr        = inputRate / factor;
        designDecimator();
        numBands = juce::jlimit (0, maxBands, bandsToUse);
        for (int b = 0; b < numBands; ++b)
        {
            design (bands[(size_t) b], edgesHz[b], edgesHz[b + 1]);
            // La envolvente cae ~1 dB por ciclo de la frecuencia mas baja
            // de la banda: sostiene el pico entre ciclos sin quedarse pegada.
            bands[(size_t) b].decay = std::exp (std::log (std::pow (10.0, -1.0 / 20.0)) * edgesHz[b] / sr);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : decState)
            for (auto& st : ch) st = {};
        counter = 0;
        for (auto& b : bands)
            for (auto& ch : b.ch)
            {
                for (auto& s : ch.state) s = {};
                ch.env = 0.0;
            }
    }

    int getNumBands() const noexcept { return numBands; }

    // --- Hilo de audio ---------------------------------------------------
    void process (float mid, float side) noexcept
    {
        // Pasa bajos antirreplica (en cada muestra) y diezmado
        double in[2] = { (double) mid, (double) side };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k)
            {
                const auto& q = dec[(size_t) k];
                auto& st = decState[(size_t) c][(size_t) k];
                const double y = q.b0 * in[c] + q.b1 * st.x1 + q.b2 * st.x2 - q.a1 * st.y1 - q.a2 * st.y2;
                st.x2 = st.x1; st.x1 = in[c]; st.y2 = st.y1; st.y1 = y;
                in[c] = y;
            }
        if (++counter < factor)
            return;
        counter = 0;

        for (int b = 0; b < numBands; ++b)
        {
            auto& band = bands[(size_t) b];
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = band.ch[(size_t) c];
                double x = in[c];
                for (int k = 0; k < 3; ++k)
                {
                    const auto& q = band.sec[(size_t) k];
                    auto& s = ch.state[(size_t) k];
                    const double y = q.b0 * x + q.b1 * s.x1 + q.b2 * s.x2 - q.a1 * s.y1 - q.a2 * s.y2;
                    s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
                    x = y;
                }
                const double a = std::abs (x);
                ch.env = a > ch.env ? a : ch.env * band.decay;
            }
        }
    }

    // channel: 0 = Mid, 1 = Side. Devuelve la amplitud de pico actual.
    double getAmplitude (int band, int channel) const noexcept
    {
        return bands[(size_t) band].ch[(size_t) channel].env;
    }

private:
    struct Section { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State   { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    struct Channel { std::array<State, 3> state {}; double env = 0.0; };
    struct Band
    {
        std::array<Section, 3> sec {};
        std::array<Channel, 2> ch {};
        double decay = 0.999;
    };

    // Pasa banda Butterworth: prototipo pasa bajos de 3er orden ->
    // transformacion a pasa banda (6o orden) -> transformada bilineal con
    // pre-distorsion de los bordes. Tres secciones de 2o orden.
    void design (Band& band, double fLo, double fHi)
    {
        using cd = std::complex<double>;
        const double K   = 2.0 * sr;
        const double wLo = K * std::tan (juce::MathConstants<double>::pi * fLo / sr);
        const double wHi = K * std::tan (juce::MathConstants<double>::pi * juce::jmin (fHi, 0.45 * sr) / sr);
        const double w0  = std::sqrt (wLo * wHi);
        const double B   = wHi - wLo;

        const cd proto[3] = { std::polar (1.0, juce::MathConstants<double>::pi * 2.0 / 3.0),
                              cd (-1.0, 0.0),
                              std::polar (1.0, juce::MathConstants<double>::pi * 4.0 / 3.0) };

        // Polos del pasa banda con parte imaginaria positiva (uno por seccion)
        std::vector<cd> poles;
        for (const auto& p : proto)
        {
            const cd pb = p * B;
            const cd root = std::sqrt (pb * pb - 4.0 * w0 * w0);
            for (const cd s : { (pb + root) * 0.5, (pb - root) * 0.5 })
                if (s.imag() > 0.0) poles.push_back (s);
        }
        while (poles.size() < 3) poles.push_back (poles.back());   // no deberia pasar

        for (int k = 0; k < 3; ++k)
        {
            const double a1 = -2.0 * poles[(size_t) k].real();
            const double a0 = std::norm (poles[(size_t) k]);
            const double d0 = K * K + a1 * K + a0;
            auto& q = band.sec[(size_t) k];
            q.b0 =  K / d0;
            q.b1 =  0.0;
            q.b2 = -K / d0;
            q.a1 = (2.0 * a0 - 2.0 * K * K) / d0;
            q.a2 = (K * K - a1 * K + a0) / d0;
        }

        // Ganancia 1 (0 dB) en el centro de la banda
        const double wc = 2.0 * std::atan (w0 / K);
        const cd z1 = std::polar (1.0, -wc), z2 = z1 * z1;
        cd h (1.0, 0.0);
        for (const auto& q : band.sec)
            h *= (q.b0 + q.b1 * z1 + q.b2 * z2) / (1.0 + q.a1 * z1 + q.a2 * z2);
        const double g = 1.0 / std::abs (h);
        band.sec[0].b0 *= g; band.sec[0].b1 *= g; band.sec[0].b2 *= g;
    }

    // Butterworth pasa bajos de 6o orden en 1 kHz (tres secciones RBJ)
    void designDecimator()
    {
        const double fc = juce::jmin (1000.0, 0.45 * inputRate);
        const double qs[3] = { 0.5176380902, 0.7071067812, 1.9318516526 };
        for (int k = 0; k < 3; ++k)
        {
            const double w0 = juce::MathConstants<double>::twoPi * fc / inputRate;
            const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * qs[k]), a0 = 1.0 + alpha;
            auto& q = dec[(size_t) k];
            q.b0 = (1.0 - cw) * 0.5 / a0; q.b1 = (1.0 - cw) / a0; q.b2 = q.b0;
            q.a1 = -2.0 * cw / a0;        q.a2 = (1.0 - alpha) / a0;
        }
    }

    double inputRate = 44100.0;
    double sr = 44100.0;          // frecuencia de analisis (diezmada)
    int factor = 1, counter = 0;
    std::array<Section, 3> dec {};
    std::array<std::array<State, 3>, 2> decState {};

    int numBands = 0;
    std::array<Band, maxBands> bands {};
};
