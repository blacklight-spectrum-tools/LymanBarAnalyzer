#include "PhaseView.h"
#include "../PluginProcessor.h"
#include "../Palette.h"

namespace
{
    const juce::Identifier gonioScaleId { "gonioScale" };   // 0 = Auto, 1 = Level
    constexpr float levelRangeDb = 48.0f;                   // escala Level: centro = -48 dB, borde = 0 dBFS
    constexpr float balanceRangeDb = 6.0f;                  // el medidor de balance va de -6 a +6 dB
}

PhaseView::PhaseView (BarSpectrumAnalyzerAudioProcessor& p)
    : apvts (p.apvts),
      phase (p.getPhaseAnalyzer()),
      stereo (p.getStereoAnalyzer()),
      bufL ((size_t) scopePoints, 0.0f),
      bufR ((size_t) scopePoints, 0.0f)
{
    resetButton.onClick = [this] { phase.requestReset(); };
    addAndMakeVisible (resetButton);

    // Escala del goniometro: AUTO (la figura llena el circulo) o LEVEL
    // (la distancia al centro muestra el nivel real; el borde es 0 dBFS)
    const char* const names[] = { "AUTO", "LEVEL" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = scaleButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.setColour (juce::TextButton::buttonColourId,   Palette::background);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::panel);
        b.setColour (juce::TextButton::textColourOffId,  Palette::textDim);
        b.setColour (juce::TextButton::textColourOnId,   Palette::barOver);
        b.setConnectedEdges (i == 0 ? juce::Button::ConnectedOnRight : juce::Button::ConnectedOnLeft);
        b.onClick = [this, i] { setScaleMode (i); };
        addAndMakeVisible (b);
    }

    setScaleMode (getScaleMode());
}

PhaseView::~PhaseView() = default;

int PhaseView::getScaleMode() const
{
    return juce::jlimit (0, 1, (int) apvts.state.getProperty (gonioScaleId, 0));
}

void PhaseView::setScaleMode (int mode)
{
    apvts.state.setProperty (gonioScaleId, mode, nullptr);
    for (int i = 0; i < 2; ++i)
        scaleButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    repaint();
}

void PhaseView::resized()
{
    auto area = getLocalBounds().reduced (20, 16).toFloat();

    // Columna izquierda: etiqueta "M" arriba, goniometro, y el switch de escala abajo
    const float labelSpace = 22.0f, switchSpace = 32.0f;
    const float side = juce::jmin (area.getHeight() - labelSpace - switchSpace, area.getWidth() * 0.55f);
    auto left = area.removeFromLeft (side);
    scopeArea = left.withTrimmedTop (labelSpace).withHeight (side);

    auto switchRow = juce::Rectangle<float> (scopeArea.getX(), scopeArea.getBottom() + 8.0f, 120.0f, 24.0f);
    scaleButtons[0].setBounds (switchRow.removeFromLeft (60.0f).toNearestInt());
    scaleButtons[1].setBounds (switchRow.toNearestInt());

    area.removeFromLeft (32.0f);
    area = area.withWidth (juce::jmin (area.getWidth(), 560.0f));

    // Columna derecha: correlacion, graves en mono y balance L/R.
    // Altura fija de los bloques; el texto de ayuda solo si sobra espacio.
    const float blocksHeight = 204.0f + 64.0f + 68.0f;
    const float hintHeight   = 64.0f;
    const bool  showHint     = area.getHeight() >= blocksHeight + 16.0f + hintHeight;
    const float columnHeight = blocksHeight + (showHint ? 16.0f + hintHeight : 0.0f);

    if (area.getHeight() > columnHeight)
    {
        const float top = juce::jlimit (area.getY(), area.getBottom() - columnHeight,
                                        scopeArea.getCentreY() - columnHeight * 0.5f);
        area = area.withTop (top).withHeight (columnHeight);
    }

    titleArea  = area.removeFromTop (18.0f);
    area.removeFromTop (6.0f);
    valueArea  = area.removeFromTop (36.0f);
    area.removeFromTop (4.0f);
    meterArea  = area.removeFromTop (24.0f);
    ticksArea  = area.removeFromTop (16.0f);
    area.removeFromTop (10.0f);
    statusArea = area.removeFromTop (26.0f);
    area.removeFromTop (8.0f);
    holdArea   = area.removeFromTop (20.0f);
    area.removeFromTop (8.0f);
    resetButton.setBounds (area.removeFromTop (28.0f).withWidth (150.0f).toNearestInt());

    area.removeFromTop (18.0f);
    lowTitleArea  = area.removeFromTop (16.0f);
    area.removeFromTop (4.0f);
    lowStatusArea = area.removeFromTop (26.0f);

    area.removeFromTop (16.0f);
    balTitleArea  = area.removeFromTop (16.0f);
    area.removeFromTop (6.0f);
    balMeterArea  = area.removeFromTop (16.0f);
    balTicksArea  = area.removeFromTop (14.0f);

    if (showHint)
    {
        area.removeFromTop (16.0f);
        hintArea = area.removeFromTop (hintHeight);
    }
    else
    {
        hintArea = {};
    }
}

float PhaseView::meterX (float correlation) const
{
    return juce::jmap (juce::jlimit (-1.0f, 1.0f, correlation), -1.0f, 1.0f,
                       meterArea.getX(), meterArea.getRight());
}

juce::Colour PhaseView::colourFor (PhaseAnalyzer::Status s)
{
    switch (s)
    {
        case PhaseAnalyzer::Status::ok:       return Palette::ok;
        case PhaseAnalyzer::Status::warn:     return Palette::warn;
        case PhaseAnalyzer::Status::danger:   return Palette::danger;
        case PhaseAnalyzer::Status::noSignal: break;
    }
    return Palette::idle;
}

juce::Colour PhaseView::colourFor (StereoAnalyzer::Status s)
{
    switch (s)
    {
        case StereoAnalyzer::Status::ok:       return Palette::ok;
        case StereoAnalyzer::Status::warn:     return Palette::warn;
        case StereoAnalyzer::Status::danger:   return Palette::danger;
        case StereoAnalyzer::Status::noSignal: break;
    }
    return Palette::idle;
}

juce::String PhaseView::textFor (PhaseAnalyzer::Status s)
{
    switch (s)
    {
        case PhaseAnalyzer::Status::ok:       return "Mono-safe";
        case PhaseAnalyzer::Status::warn:     return "Wide - check in mono";
        case PhaseAnalyzer::Status::danger:   return "Phase cancellation risk";
        case PhaseAnalyzer::Status::noSignal: break;
    }
    return "No signal";
}

juce::String PhaseView::formatCorrelation (float c)
{
    return (c >= 0.0f ? "+" : "") + juce::String (c, 2);
}

void PhaseView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    const bool  signal = phase.isSignalPresent();
    const float corr   = phase.getCorrelation();
    const auto  status = PhaseAnalyzer::statusFor (corr, signal);

    drawGoniometer (g);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("STEREO PHASE CORRELATION", titleArea, juce::Justification::centredLeft);

    g.setColour (signal ? Palette::text : Palette::idle);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText (signal ? formatCorrelation (corr) : juce::String ("--"),
                valueArea, juce::Justification::centredLeft);

    drawCorrelationMeter (g, corr, signal);
    drawLed (g, statusArea, colourFor (status), textFor (status), status != PhaseAnalyzer::Status::noSignal, 16.0f);
    drawHold (g);
    drawLowEnd (g);
    drawBalance (g);

    if (! hintArea.isEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawFittedText ("Green: mono-safe.  Yellow: very wide, check in mono.\n"
                          "Red: parts of the mix cancel out when summed to mono.\n"
                          "Low end should stay mono. Min / max keep the extremes.",
                          hintArea.toNearestInt(), juce::Justification::topLeft, 4);
    }
}

void PhaseView::drawGoniometer (juce::Graphics& g)
{
    const auto  centre = scopeArea.getCentre();
    const float radius = scopeArea.getWidth() * 0.5f - 4.0f;
    const bool  levelMode = getScaleMode() == 1;

    // Fondo y guias: circulo, eje M (vertical), eje S (horizontal), diagonales L/R
    g.setColour (Palette::panel.withAlpha (0.5f));
    g.fillEllipse (scopeArea.reduced (4.0f));

    // En modo LEVEL, anillos cada 12 dB (borde = 0 dBFS)
    if (levelMode)
    {
        g.setFont (juce::FontOptions (10.0f));
        for (int db = -12; db > -(int) levelRangeDb; db -= 12)
        {
            const float rr = radius * (1.0f + (float) db / levelRangeDb);
            g.setColour (Palette::grid.withAlpha (0.45f));
            g.drawEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
            g.setColour (Palette::textDim.withAlpha (0.8f));
            g.drawText (juce::String (db), juce::Rectangle<float> (centre.x + 3.0f, centre.y - rr, 28.0f, 12.0f),
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (Palette::grid);
    g.drawEllipse (scopeArea.reduced (4.0f), 1.5f);
    g.drawLine (centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);
    g.drawLine (centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);

    const float d = radius * 0.7071f;
    g.setColour (Palette::grid.withAlpha (0.5f));
    g.drawLine (centre.x - d, centre.y - d, centre.x + d, centre.y + d, 1.0f);
    g.drawLine (centre.x + d, centre.y - d, centre.x - d, centre.y + d, 1.0f);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("M", juce::Rectangle<float> (centre.x - 10.0f, scopeArea.getY() - 18.0f, 20.0f, 16.0f),
                juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (centre.x - d - 22.0f, centre.y - d - 22.0f, 16.0f, 16.0f),
                juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (centre.x + d + 6.0f, centre.y - d - 22.0f, 16.0f, 16.0f),
                juce::Justification::centred);

    // Persistencia ("fosforo"): los puntos se dibujan en una imagen que se
    // desvanece en ~0.5 s, asi la figura no parpadea en los huecos entre notas.
    const int imgW = juce::roundToInt (scopeArea.getWidth()), imgH = juce::roundToInt (scopeArea.getHeight());
    if (! trail.isValid() || trail.getWidth() != imgW || trail.getHeight() != imgH || trailMode != (int) levelMode)
    {
        trail = juce::Image (juce::Image::ARGB, juce::jmax (1, imgW), juce::jmax (1, imgH), true);
        trailMode = (int) levelMode;
    }

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTrailMs > 0.0 ? (float) juce::jmin (0.25, (now - lastTrailMs) / 1000.0) : 0.0f;
    lastTrailMs = now;
    trail.multiplyAllAlphas (std::pow (0.05f, dt / 0.5f));    // queda 5 % despues de 0.5 s

    if (! phase.isSignalPresent())
    {
        trail.clear (trail.getBounds());
        g.setColour (Palette::idle);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No signal", scopeArea, juce::Justification::centred);
        return;
    }

    phase.copyScope (bufL.data(), bufR.data(), scopePoints);

    float peak = 0.0f;
    for (int i = 0; i < scopePoints; ++i)
    {
        const float l = bufL[(size_t) i], r = bufR[(size_t) i];
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }

    // En un hueco entre notas no hay nada nuevo que dibujar: la imagen solo se
    // desvanece y la ganancia automatica se queda quieta (no se dispara).
    if (peak > 1.0e-3f)
    {
        juce::Graphics tg (trail);
        tg.setColour (Palette::barOver.withAlpha (0.55f));
        const float cx = (float) imgW * 0.5f, cy = (float) imgH * 0.5f;

        if (levelMode)
        {
            // Distancia al centro en escala logaritmica: borde = 0 dBFS, centro = -48 dB
            for (int i = 0; i < scopePoints; ++i)
            {
                const float l = bufL[(size_t) i], r = bufR[(size_t) i];
                const float sx = (r - l) * 0.5f, my = (l + r) * 0.5f;
                const float len = std::sqrt (sx * sx + my * my);
                if (len < 1.0e-6f)
                    continue;

                const float db = juce::Decibels::gainToDecibels (len, -levelRangeDb);
                const float rr = juce::jlimit (0.0f, 1.0f, 1.0f + db / levelRangeDb);
                tg.fillRect (cx + sx / len * rr * radius - 0.75f, cy - my / len * rr * radius - 0.75f, 1.5f, 1.5f);
            }
        }
        else
        {
            // Rotacion de 45 grados: mono (L = R) dibuja una linea vertical,
            // fase invertida (L = -R) una horizontal.
            float figure = 0.0f;
            for (int i = 0; i < scopePoints; ++i)
            {
                const float l = bufL[(size_t) i], r = bufR[(size_t) i];
                const float sx = (r - l) * 0.7071f, my = (l + r) * 0.7071f;
                figure = juce::jmax (figure, std::sqrt (sx * sx + my * my));
            }

            // Ganancia automatica: la figura llena el circulo sin importar el volumen
            const float target = juce::jlimit (0.5f, 200.0f, 0.9f / juce::jmax (figure, 1.0e-4f));
            displayGain += (target < displayGain ? 0.5f : 0.05f) * (target - displayGain);

            for (int i = 0; i < scopePoints; ++i)
            {
                const float l = bufL[(size_t) i], r = bufR[(size_t) i];
                float x = (r - l) * 0.7071f * displayGain;
                float y = (l + r) * 0.7071f * displayGain;

                const float len = std::sqrt (x * x + y * y);
                if (len > 1.0f) { x /= len; y /= len; }

                tg.fillRect (cx + x * radius - 0.75f, cy - y * radius - 0.75f, 1.5f, 1.5f);
            }
        }
    }

    g.drawImageAt (trail, juce::roundToInt (scopeArea.getX()), juce::roundToInt (scopeArea.getY()));
}

void PhaseView::drawCorrelationMeter (juce::Graphics& g, float correlation, bool signal)
{
    const float h = meterArea.getHeight();

    // Zonas: rojo (-1 a -0.3), amarillo (-0.3 a +0.3), verde (+0.3 a +1)
    const float x0    = meterX (-1.0f);
    const float xDang = meterX (PhaseAnalyzer::dangerBelow);
    const float xZero = meterX (0.0f);
    const float xWarn = meterX (PhaseAnalyzer::warnBelow);
    const float x1    = meterX (1.0f);

    g.setColour (Palette::danger.withAlpha (0.22f));
    g.fillRect (x0, meterArea.getY(), xDang - x0, h);
    g.setColour (Palette::warn.withAlpha (0.22f));
    g.fillRect (xDang, meterArea.getY(), xWarn - xDang, h);
    g.setColour (Palette::ok.withAlpha (0.22f));
    g.fillRect (xWarn, meterArea.getY(), x1 - xWarn, h);

    g.setColour (Palette::grid);
    g.drawRect (meterArea, 1.0f);
    g.drawVerticalLine ((int) xZero, meterArea.getY(), meterArea.getBottom());

    // Marcas de minimo y maximo retenidos
    if (phase.hasHold())
    {
        g.setColour (Palette::text.withAlpha (0.8f));
        for (float v : { phase.getMinHold(), phase.getMaxHold() })
        {
            const float x = meterX (v);
            juce::Path tri;
            tri.addTriangle (x - 5.0f, meterArea.getY() - 7.0f,
                             x + 5.0f, meterArea.getY() - 7.0f,
                             x,        meterArea.getY() - 1.0f);
            g.fillPath (tri);
        }
    }

    // Indicador del valor actual
    if (signal)
    {
        const float x = meterX (correlation);
        g.setColour (Palette::text);
        g.fillRect (x - 2.0f, meterArea.getY() - 2.0f, 4.0f, h + 4.0f);
    }

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("-1", juce::Rectangle<float> (x0, ticksArea.getY() + 2.0f, 24.0f, 14.0f),
                juce::Justification::centredLeft);
    g.drawText ("0",  juce::Rectangle<float> (xZero - 12.0f, ticksArea.getY() + 2.0f, 24.0f, 14.0f),
                juce::Justification::centred);
    g.drawText ("+1", juce::Rectangle<float> (x1 - 24.0f, ticksArea.getY() + 2.0f, 24.0f, 14.0f),
                juce::Justification::centredRight);
}

void PhaseView::drawLed (juce::Graphics& g, juce::Rectangle<float> row, juce::Colour colour,
                         const juce::String& text, bool lit, float textSize)
{
    const float ledSize = textSize + 2.0f;
    auto led = juce::Rectangle<float> (row.getX(), row.getCentreY() - ledSize * 0.5f, ledSize, ledSize);

    // Halo de la luz, para que se note encendida
    if (lit)
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (led.expanded (5.0f));
    }

    g.setColour (colour);
    g.fillEllipse (led);

    g.setFont (juce::FontOptions (textSize, juce::Font::bold));
    g.drawText (text, row.withTrimmedLeft (ledSize + 12.0f), juce::Justification::centredLeft);
}

void PhaseView::drawHold (juce::Graphics& g)
{
    g.setFont (juce::FontOptions (14.0f));

    if (! phase.hasHold())
    {
        g.setColour (Palette::textDim);
        g.drawText ("Min  --     Max  --", holdArea, juce::Justification::centredLeft);
        return;
    }

    const float mn = phase.getMinHold();
    const float mx = phase.getMaxHold();

    auto area = holdArea;
    auto minArea = area.removeFromLeft (110.0f);

    // Cada valor se pinta con el color de la zona a la que llego
    g.setColour (colourFor (PhaseAnalyzer::statusFor (mn, true)));
    g.drawText ("Min  " + formatCorrelation (mn), minArea, juce::Justification::centredLeft);

    g.setColour (colourFor (PhaseAnalyzer::statusFor (mx, true)));
    g.drawText ("Max  " + formatCorrelation (mx), area, juce::Justification::centredLeft);
}

void PhaseView::drawLowEnd (juce::Graphics& g)
{
    const bool  present = stereo.hasLowSignal();
    const float corr    = stereo.getLowCorrelation();
    const auto  status  = StereoAnalyzer::lowStatusFor (corr, present);

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.setColour (Palette::textDim);
    g.drawText ("LOW END  < 120 Hz", lowTitleArea, juce::Justification::centredLeft);
    g.setColour (present ? Palette::text : Palette::idle);
    g.drawText (present ? formatCorrelation (corr) : juce::String ("--"), lowTitleArea, juce::Justification::centredRight);

    juce::String text = "No low end";
    if (status == StereoAnalyzer::Status::ok)     text = "Mono";
    if (status == StereoAnalyzer::Status::warn)   text = "Slightly wide";
    if (status == StereoAnalyzer::Status::danger) text = "Stereo bass - make it mono";

    drawLed (g, lowStatusArea, colourFor (status), text, present, 14.0f);
}

void PhaseView::drawBalance (juce::Graphics& g)
{
    const bool  present = stereo.hasBalanceSignal();
    const float db      = stereo.getBalanceDb();
    const auto  status  = StereoAnalyzer::balanceStatusFor (db, present);

    juce::String value = "--";
    if (present)
        value = std::abs (db) < 0.1f ? juce::String ("Centred")
                                     : (db > 0.0f ? "R +" : "L +") + juce::String (std::abs (db), 1) + " dB";

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.setColour (Palette::textDim);
    g.drawText ("L/R BALANCE", balTitleArea, juce::Justification::centredLeft);
    g.setColour (colourFor (status));
    g.drawText (value, balTitleArea, juce::Justification::centredRight);

    // Medidor de -6 a +6 dB con zonas simetricas (verde al centro)
    auto toX = [&] (float v)
    {
        return juce::jmap (juce::jlimit (-balanceRangeDb, balanceRangeDb, v), -balanceRangeDb, balanceRangeDb,
                           balMeterArea.getX(), balMeterArea.getRight());
    };

    const float y = balMeterArea.getY(), h = balMeterArea.getHeight();
    g.setColour (Palette::danger.withAlpha (0.2f));
    g.fillRect (balMeterArea);
    g.setColour (Palette::warn.withAlpha (0.22f));
    g.fillRect (toX (-StereoAnalyzer::balanceLeaningDb), y,
                toX (StereoAnalyzer::balanceLeaningDb) - toX (-StereoAnalyzer::balanceLeaningDb), h);
    g.setColour (Palette::ok.withAlpha (0.25f));
    g.fillRect (toX (-StereoAnalyzer::balanceCentredDb), y,
                toX (StereoAnalyzer::balanceCentredDb) - toX (-StereoAnalyzer::balanceCentredDb), h);

    g.setColour (Palette::grid);
    g.drawRect (balMeterArea, 1.0f);
    g.drawVerticalLine ((int) toX (0.0f), y, y + h);

    if (present)
    {
        g.setColour (Palette::text);
        g.fillRect (toX (db) - 2.0f, y - 2.0f, 4.0f, h + 4.0f);
    }

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("L", balTicksArea.withWidth (20.0f), juce::Justification::centredLeft);
    g.drawText ("0", juce::Rectangle<float> (toX (0.0f) - 10.0f, balTicksArea.getY(), 20.0f, balTicksArea.getHeight()),
                juce::Justification::centred);
    g.drawText ("R", balTicksArea.withTrimmedLeft (balTicksArea.getWidth() - 20.0f), juce::Justification::centredRight);
}
