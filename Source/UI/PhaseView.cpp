#include "PhaseView.h"
#include "../PluginProcessor.h"
#include "../Palette.h"

PhaseView::PhaseView (BarSpectrumAnalyzerAudioProcessor& p)
    : phase (p.getPhaseAnalyzer()),
      bufL ((size_t) scopePoints, 0.0f),
      bufR ((size_t) scopePoints, 0.0f)
{
    resetButton.onClick = [this] { phase.requestReset(); };
    addAndMakeVisible (resetButton);
}

PhaseView::~PhaseView() = default;

void PhaseView::resized()
{
    auto area = getLocalBounds().reduced (20, 16).toFloat();

    // Goniometro: cuadrado a la izquierda (deja 22 px arriba para la etiqueta "M")
    const float labelSpace = 22.0f;
    const float side = juce::jmin (area.getHeight() - labelSpace, area.getWidth() * 0.55f);
    scopeArea = area.removeFromLeft (side).withTrimmedTop (labelSpace)
                                          .withSizeKeepingCentre (side, side);
    area.removeFromLeft (32.0f);

    // Columna derecha: ancho maximo de 560 px y centrada en vertical con el
    // goniometro, para que en ventanas grandes no quede pegada arriba.
    const float columnHeight = 272.0f + 90.0f;   // controles + texto de ayuda
    area = area.withWidth (juce::jmin (area.getWidth(), 560.0f));
    if (area.getHeight() > columnHeight)
    {
        const float top = juce::jlimit (area.getY(), area.getBottom() - columnHeight,
                                        scopeArea.getCentreY() - columnHeight * 0.5f);
        area = area.withTop (top).withHeight (columnHeight);
    }

    titleArea  = area.removeFromTop (20.0f);
    area.removeFromTop (10.0f);
    valueArea  = area.removeFromTop (40.0f);
    area.removeFromTop (6.0f);
    meterArea  = area.removeFromTop (26.0f);
    ticksArea  = area.removeFromTop (18.0f);
    area.removeFromTop (22.0f);
    statusArea = area.removeFromTop (30.0f);
    area.removeFromTop (16.0f);
    holdArea   = area.removeFromTop (22.0f);
    area.removeFromTop (12.0f);
    resetButton.setBounds (area.removeFromTop (30.0f).withWidth (150.0f).toNearestInt());
    area.removeFromTop (20.0f);
    hintArea   = area;
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
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawText (signal ? formatCorrelation (corr) : juce::String ("--"),
                valueArea, juce::Justification::centredLeft);

    drawCorrelationMeter (g, corr, signal);
    drawStatus (g, status);
    drawHold (g);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText ("Green: mono-safe.\n"
                      "Yellow: very wide, check how it sounds in mono.\n"
                      "Red: parts of the mix cancel out when summed to mono.\n"
                      "Min / max keep the extremes reached while playing.",
                      hintArea.toNearestInt(), juce::Justification::topLeft, 6);
}

void PhaseView::drawGoniometer (juce::Graphics& g)
{
    const auto  centre = scopeArea.getCentre();
    const float radius = scopeArea.getWidth() * 0.5f - 4.0f;

    // Fondo y guias: circulo, eje M (vertical), eje S (horizontal), diagonales L/R
    g.setColour (Palette::panel.withAlpha (0.5f));
    g.fillEllipse (scopeArea.reduced (4.0f));

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

    if (! phase.isSignalPresent())
    {
        g.setColour (Palette::idle);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No signal", scopeArea, juce::Justification::centred);
        return;
    }

    phase.copyScope (bufL.data(), bufR.data(), scopePoints);

    // Rotacion de 45 grados: mono (L = R) dibuja una linea vertical,
    // fase invertida (L = -R) una horizontal.
    float peak = 0.0f;
    for (int i = 0; i < scopePoints; ++i)
    {
        const float l = bufL[(size_t) i], r = bufR[(size_t) i];
        const float side = (r - l) * 0.7071f;
        const float mid  = (l + r) * 0.7071f;
        peak = juce::jmax (peak, std::sqrt (side * side + mid * mid));
    }

    // Ganancia automatica: la figura llena el circulo sin importar el volumen
    const float target = juce::jlimit (0.5f, 200.0f, 0.9f / juce::jmax (peak, 1.0e-4f));
    displayGain += (target < displayGain ? 0.5f : 0.05f) * (target - displayGain);

    g.setColour (Palette::barOver.withAlpha (0.55f));
    for (int i = 0; i < scopePoints; ++i)
    {
        const float l = bufL[(size_t) i], r = bufR[(size_t) i];
        float x = (r - l) * 0.7071f * displayGain;
        float y = (l + r) * 0.7071f * displayGain;

        const float len = std::sqrt (x * x + y * y);
        if (len > 1.0f) { x /= len; y /= len; }

        g.fillRect (centre.x + x * radius - 0.75f, centre.y - y * radius - 0.75f, 1.5f, 1.5f);
    }
}

void PhaseView::drawCorrelationMeter (juce::Graphics& g, float correlation, bool signal)
{
    const float h = meterArea.getHeight();

    // Zonas: rojo (-1 a 0), amarillo (0 a +0.3), verde (+0.3 a +1)
    const float x0    = meterX (-1.0f);
    const float xZero = meterX (PhaseAnalyzer::dangerBelow);
    const float xWarn = meterX (PhaseAnalyzer::warnBelow);
    const float x1    = meterX (1.0f);

    g.setColour (Palette::danger.withAlpha (0.22f));
    g.fillRect (x0, meterArea.getY(), xZero - x0, h);
    g.setColour (Palette::warn.withAlpha (0.22f));
    g.fillRect (xZero, meterArea.getY(), xWarn - xZero, h);
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

void PhaseView::drawStatus (juce::Graphics& g, PhaseAnalyzer::Status status)
{
    const auto colour = colourFor (status);
    const float ledSize = 18.0f;
    auto led = juce::Rectangle<float> (statusArea.getX(), statusArea.getCentreY() - ledSize * 0.5f,
                                       ledSize, ledSize);

    // Halo de la luz, para que se note encendida
    if (status != PhaseAnalyzer::Status::noSignal)
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (led.expanded (6.0f));
    }

    g.setColour (colour);
    g.fillEllipse (led);

    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (textFor (status),
                statusArea.withTrimmedLeft (ledSize + 14.0f),
                juce::Justification::centredLeft);
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
