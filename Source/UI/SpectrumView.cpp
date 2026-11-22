#include "SpectrumView.h"
#include "../PluginProcessor.h"
#include "../Palette.h"
#include "BlacklightLookAndFeel.h"

namespace
{
    constexpr float tiltSlopes[] = { 0.0f, 3.0f, 4.5f };   // dB por octava, pivote 1 kHz
    const char* const tiltNames[] = { "0", "3 dB", "4.5 dB" };
    const char* const viewNames[] = { "Sum", "M/S" };
    const char* const styleNames[] = { "Bars", "Curve" };

    constexpr double holdMs          = 1000.0;   // peak hold: tiempo quieto
    constexpr float  holdFallDbPerS  = 15.0f;    // y luego cae a esta velocidad
    constexpr float  meterFallDbPerS = 24.0f;
    constexpr double numericHoldMs   = 2000.0;   // el numero de pico se sostiene 2 s
}

SpectrumView::SpectrumView (BarSpectrumAnalyzerAudioProcessor& p)
    : audioProcessor (p)
{
    static_assert (numBars == BarSpectrumAnalyzerAudioProcessor::numBars, "numero de barras");

    holdDb.fill (-100.0f);
    holdTime.fill (0.0);
    curveHoldMid.assign  ((size_t) BarSpectrumAnalyzerAudioProcessor::numCurvePoints, -100.0f);
    curveHoldSide.assign ((size_t) BarSpectrumAnalyzerAudioProcessor::numCurvePoints, -100.0f);
    frozenCurveMid.assign  ((size_t) BarSpectrumAnalyzerAudioProcessor::numCurvePoints, -100.0f);
    frozenCurveSide.assign ((size_t) BarSpectrumAnalyzerAudioProcessor::numCurvePoints, -100.0f);

    referenceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    referenceSlider.setRange (-60.0, 0.0, 0.1);
    referenceSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    addAndMakeVisible (referenceSlider);

    referenceLabel.setText ("Compression reference (dB)", juce::dontSendNotification);
    referenceLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (referenceLabel);

    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "referenceDb", referenceSlider);

    // Tilt: tres botones tipo segmento, ligados al parametro "tilt"
    tiltLabel.setText ("TILT", juce::dontSendNotification);
    tiltLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    ThemeStyle::dimLabel (tiltLabel);
    tiltLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tiltLabel);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = tiltButtons[(size_t) i];
        b.setButtonText (tiltNames[i]);
        styleSegment (b, i, 3);
        b.onClick = [this, i] { if (tiltAttachment != nullptr) tiltAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    if (auto* tiltParam = audioProcessor.apvts.getParameter ("tilt"))
    {
        tiltAttachment = std::make_unique<juce::ParameterAttachment> (
            *tiltParam, [this] (float v) { setTiltButtons ((int) std::round (v)); });
        tiltAttachment->sendInitialUpdate();
    }

    // Vista: Sum (solo Mid, como siempre) o M/S (Mid + Side superpuestos)
    viewLabel.setText ("VIEW", juce::dontSendNotification);
    viewLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    ThemeStyle::dimLabel (viewLabel);
    viewLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (viewLabel);

    for (int i = 0; i < 2; ++i)
    {
        auto& b = viewButtons[(size_t) i];
        b.setButtonText (viewNames[i]);
        styleSegment (b, i, 2);
        b.onClick = [this, i] { if (viewAttachment != nullptr) viewAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    if (auto* viewParam = audioProcessor.apvts.getParameter ("view"))
    {
        viewAttachment = std::make_unique<juce::ParameterAttachment> (
            *viewParam, [this] (float v) { setViewButtons ((int) std::round (v)); });
        viewAttachment->sendInitialUpdate();
    }

    // Estilo: barras, curva o ambas (solo cambia el dibujo)
    styleLabel.setText ("STYLE", juce::dontSendNotification);
    styleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    ThemeStyle::dimLabel (styleLabel);
    styleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < 2; ++i)
    {
        auto& b = styleButtons[(size_t) i];
        b.setButtonText (styleNames[i]);
        styleSegment (b, i, 2);
        b.onClick = [this, i] { if (styleAttachment != nullptr) styleAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    if (auto* styleParam = audioProcessor.apvts.getParameter ("style"))
    {
        styleAttachment = std::make_unique<juce::ParameterAttachment> (
            *styleParam, [this] (float v) { setStyleButtons ((int) std::round (v)); });
        styleAttachment->sendInitialUpdate();
    }

    styleSegment (freezeButton, 0, 1);
    freezeButton.setTooltip ("Freeze the display to study it (meters keep running)");
    freezeButton.onClick = [this] { setFrozen (! frozen); };
    addAndMakeVisible (freezeButton);
}

void SpectrumView::setFrozen (bool shouldFreeze)
{
    using P = BarSpectrumAnalyzerAudioProcessor;
    if (shouldFreeze && ! frozen)
    {
        for (int i = 0; i < numBars; ++i)
        {
            frozenBars[(size_t) i]     = audioProcessor.getBarLevelDb (i);
            frozenSideBars[(size_t) i] = audioProcessor.getSideLevelDb (i);
            frozenPhaseRisk[(size_t) i] = audioProcessor.isBandPhaseRisk (i);
        }
        for (int i = 0; i < P::numCurvePoints; ++i)
        {
            frozenCurveMid[(size_t) i]  = audioProcessor.getCurveDb (i, false);
            frozenCurveSide[(size_t) i] = audioProcessor.getCurveDb (i, true);
        }
    }
    else if (! shouldFreeze && frozen)
    {
        // Al soltar, el hold normal vuelve a contar desde ahora
        holdTime.fill (juce::Time::getMillisecondCounterHiRes());
    }
    frozen = shouldFreeze;
    freezeButton.setToggleState (frozen, juce::dontSendNotification);
    repaint();
}

void SpectrumView::resetPeaks()
{
    holdDb.fill (-100.0f);
    std::fill (curveHoldMid.begin(),  curveHoldMid.end(),  -100.0f);
    std::fill (curveHoldSide.begin(), curveHoldSide.end(), -100.0f);
    repaint();
}

float SpectrumView::barDb (int i) const     { return frozen ? frozenBars[(size_t) i]     : audioProcessor.getBarLevelDb (i); }
float SpectrumView::sideBarDb (int i) const { return frozen ? frozenSideBars[(size_t) i] : audioProcessor.getSideLevelDb (i); }
bool SpectrumView::phaseRisk (int i) const { return frozen ? frozenPhaseRisk[(size_t) i] : audioProcessor.isBandPhaseRisk (i); }

int SpectrumView::bandAt (float hz) const
{
    for (int i = 0; i < numBars; ++i)
        if (hz < audioProcessor.getBandFrequency (i + 1))
            return i;
    return numBars - 1;
}

float SpectrumView::curveDb (int i, bool side) const
{
    if (frozen)
        return side ? frozenCurveSide[(size_t) i] : frozenCurveMid[(size_t) i];
    return audioProcessor.getCurveDb (i, side);
}

juce::String SpectrumView::noteName (float hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi  = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int    n     = (int) std::lround (midi);
    const int    cents = (int) std::lround ((midi - n) * 100.0);
    const int    octave = n / 12 - 1;
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (octave);
    if (cents != 0)
        s << " " << (cents > 0 ? "+" : "") << cents << juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));
    return s;
}

void GearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto c = r.getCentre();
    const float outer = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const float inner = outer * 0.74f;
    constexpr int teeth = 8;

    juce::Path gear;
    for (int k = 0; k < teeth; ++k)
    {
        const float a  = juce::MathConstants<float>::twoPi * (float) k / (float) teeth;
        const float hw = juce::MathConstants<float>::pi / (float) teeth;
        const float angles[] = { a - hw * 0.45f, a + hw * 0.45f, a + hw * 0.75f, a + 2.0f * hw - hw * 0.75f };
        const float radii[]  = { outer, outer, inner, inner };
        for (int i = 0; i < 4; ++i)
        {
            const juce::Point<float> pt (c.x + radii[i] * std::sin (angles[i]), c.y - radii[i] * std::cos (angles[i]));
            if (k == 0 && i == 0) gear.startNewSubPath (pt); else gear.lineTo (pt);
        }
    }
    gear.closeSubPath();
    gear.addEllipse (c.x - outer * 0.32f, c.y - outer * 0.32f, outer * 0.64f, outer * 0.64f);
    gear.setUsingNonZeroWinding (false);

    g.setColour (down ? Palette::barOver : highlighted ? Palette::text : Palette::textDim);
    g.fillPath (gear);
}

namespace
{
    // Corazon dibujado a mano (sin depender de que la fuente tenga el simbolo)
    std::unique_ptr<juce::Drawable> heartIcon()
    {
        juce::Path p;
        p.startNewSubPath (6.0f, 11.0f);
        p.cubicTo (-1.5f, 5.5f, 0.5f, -0.5f, 6.0f, 3.0f);
        p.cubicTo (11.5f, -0.5f, 13.5f, 5.5f, 6.0f, 11.0f);
        p.closeSubPath();

        auto d = std::make_unique<juce::DrawablePath>();
        d->setPath (p);
        d->setFill (Palette::barOver);
        return d;
    }

    const char* const supportUrl = "https://ko-fi.com/blacklightspectrumtools";
}

void SpectrumView::showSettingsMenu (juce::Component& target, juce::Component* parent)
{
    auto& apvts = audioProcessor.apvts;
    auto current = [&apvts] (const char* id) { return (int) std::round (apvts.getRawParameterValue (id)->load()); };

    // Dos columnas: a la izquierda la lectura, a la derecha escucha y colores
    juce::PopupMenu menu;
    menu.setLookAndFeel (&target.getLookAndFeel());

    menu.addSectionHeader ("Curve speed");
    const char* speeds[] = { "Fast (0.25 s)", "Normal (1 s)", "Slow (3 s)" };
    for (int i = 0; i < 3; ++i)
        menu.addItem (1 + i, speeds[i], true, current ("curveSpeed") == i);

    menu.addSectionHeader ("Vertical range");
    const char* ranges[] = { "60 dB", "72 dB", "84 dB" };
    for (int i = 0; i < 3; ++i)
        menu.addItem (11 + i, ranges[i], true, current ("range") == i);

    menu.addSectionHeader ("Peak hold");
    menu.addItem (21, "Normal (1 s)", true, current ("peakHold") == 0);
    menu.addItem (22, "Infinite (whole song)", true, current ("peakHold") == 1);
    menu.addItem (23, "Reset peaks (double-click)");

    menu.addColumnBreak();

    menu.addSectionHeader ("Listen button");
    menu.addItem (31, "Mono", true, current ("listenMode") == 0);
    menu.addItem (32, "Side only", true, current ("listenMode") == 1);

    const auto& look = Palette::settings();
    menu.addSectionHeader ("Colours");
    menu.addItem (41, "Blacklight", true, look.theme == Palette::blacklightTheme);
    menu.addItem (42, "Graphite (neutral)", true, look.theme == Palette::graphiteTheme);
    menu.addItem (43, "Dim (dark room)", true, look.theme == Palette::dimTheme);
    menu.addItem (44, "Colour-blind safe alerts", true, look.colourBlind);

    menu.addSeparator();
    juce::PopupMenu::Item support ("Support Lyman (Ko-fi)");
    support.setID (51).setImage (heartIcon());
    menu.addItem (std::move (support));

    auto setChoice = [&apvts] (const char* id, int index)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
            param->endChangeGesture();
        }
    };

    juce::Component::SafePointer<SpectrumView> safeThis (this);
    auto options = juce::PopupMenu::Options().withTargetComponent (&target);
    if (parent != nullptr)
        options = options.withParentComponent (parent);

    menu.showMenuAsync (options,
                        [setChoice, safeThis] (int result)
                        {
                            const auto& s = Palette::settings();

                            if (result >= 1 && result <= 3)        setChoice ("curveSpeed", result - 1);
                            else if (result >= 11 && result <= 13) setChoice ("range", result - 11);
                            else if (result == 21 || result == 22)
                            {
                                setChoice ("peakHold", result - 21);
                                if (safeThis != nullptr) safeThis->resetPeaks();
                            }
                            else if (result == 23 && safeThis != nullptr) safeThis->resetPeaks();
                            else if (result == 31 || result == 32)       setChoice ("listenMode", result - 31);
                            else if (result >= 41 && result <= 43)       Palette::setTheme (result - 41, s.colourBlind);
                            else if (result == 44)                       Palette::setTheme (s.theme, ! s.colourBlind);
                            else if (result == 51)                       juce::URL (supportUrl).launchInDefaultBrowser();
                        });
}

void SpectrumView::setStyleButtons (int index)
{
    for (int i = 0; i < 2; ++i)
        styleButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
}

void SpectrumView::styleSegment (juce::TextButton& b, int index, int count)
{
    b.setClickingTogglesState (false);
    ThemeStyle::segment (b);
    b.setConnectedEdges ((index > 0 ? juce::Button::ConnectedOnLeft : 0)
                       | (index < count - 1 ? juce::Button::ConnectedOnRight : 0));
}

void SpectrumView::setViewButtons (int index)
{
    for (int i = 0; i < 2; ++i)
        viewButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
}

SpectrumView::~SpectrumView() = default;

void SpectrumView::setTiltButtons (int index)
{
    for (int i = 0; i < 3; ++i)
        tiltButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
}

juce::String SpectrumView::frequencyLabel (float hz)
{
    if (hz >= 1000.0f)
        return juce::String ((int) (hz / 1000.0f)) + "k";
    return juce::String ((int) hz);
}

juce::String SpectrumView::formatDb (float db)
{
    if (db <= -70.0f) return "--";
    return (db > 0.0f ? "+" : "") + juce::String (db, 1);
}

void SpectrumView::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    // Fila 1: referencia + lecturas (Peak / RMS / LUFS) a la derecha
    auto row1 = area.removeFromTop (30);
    referenceLabel.setBounds (row1.removeFromLeft (210));
    readoutArea = row1.removeFromRight (row1.getWidth() >= 560 ? 300 : 200).toFloat();
    row1.removeFromRight (12);
    referenceSlider.setBounds (row1);

    // Fila 2: TILT / VIEW / STYLE
    area.removeFromTop (8);
    auto row2 = area.removeFromTop (26);
    tiltLabel.setBounds (row2.removeFromLeft (34));
    for (auto& b : tiltButtons)
        b.setBounds (row2.removeFromLeft (52));
    row2.removeFromLeft (14);
    viewLabel.setBounds (row2.removeFromLeft (38));
    for (auto& b : viewButtons)
        b.setBounds (row2.removeFromLeft (48));
    row2.removeFromLeft (14);
    styleLabel.setBounds (row2.removeFromLeft (44));
    for (auto& b : styleButtons)
        b.setBounds (row2.removeFromLeft (54));
    freezeButton.setBounds (row2.removeFromRight (64));

    area.removeFromTop (14);

    // Medidor de nivel a la derecha, con su foco de clip debajo
    auto plot = area.toFloat();
    auto meterColumn = plot.removeFromRight (40.0f);
    plot.removeFromRight (14.0f);

    plot.removeFromBottom (22.0f);   // etiquetas de frecuencia
    plot.removeFromLeft (32.0f);     // etiquetas de dB
    plotArea  = plot;

    meterArea = meterColumn.withY (plot.getY()).withHeight (plot.getHeight());
    clipArea  = juce::Rectangle<float> (meterColumn.getX(), plot.getBottom() + 5.0f,
                                        meterColumn.getWidth(), 15.0f);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    // Clic en el foco de clip (o en el medidor) para apagarlo
    if (clipArea.expanded (2.0f).contains (e.position) || meterArea.contains (e.position))
        audioProcessor.getLevelMeter().resetClip();
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doble clic en la grafica: reiniciar los picos (sobre todo en hold infinito)
    if (plotArea.contains (e.position))
        resetPeaks();
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    if (plotArea.contains (e.position)) hoverPos = e.position;
    else                                hoverPos.reset();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e) { mouseMove (e); }
void SpectrumView::mouseExit (const juce::MouseEvent&)   { hoverPos.reset(); }

void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastPaintMs > 0.0 ? (float) juce::jmin (0.2, (now - lastPaintMs) / 1000.0) : 0.0f;
    lastPaintMs = now;
    frameDt = dt;

    const auto plot = plotArea;
    const float barGap   = juce::jmax (1.0f, plot.getWidth() * 0.004f);
    const float barWidth = (plot.getWidth() - barGap * (float) (numBars - 1)) / (float) numBars;

    static constexpr float ranges[] = { -60.0f, -72.0f, -84.0f };
    minDb = ranges[juce::jlimit (0, 2, (int) std::round (audioProcessor.apvts.getRawParameterValue ("range")->load()))];

    const float refDb = audioProcessor.apvts.getRawParameterValue ("referenceDb")->load();
    const int   tiltIndex = juce::jlimit (0, 2, (int) std::round (audioProcessor.apvts.getRawParameterValue ("tilt")->load()));
    const float tilt = tiltSlopes[tiltIndex];
    const bool  showSide = audioProcessor.apvts.getRawParameterValue ("view")->load() > 0.5f;
    const bool  showBars = audioProcessor.apvts.getRawParameterValue ("style")->load() < 0.5f;
    const bool  infiniteHold = audioProcessor.apvts.getRawParameterValue ("peakHold")->load() > 0.5f;

    auto dbToY = [&] (float db)
    {
        return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY());
    };

    // Las bandas son logaritmicas de 20 Hz a 20 kHz
    auto freqToX = [&] (float hz)
    {
        const float t = std::log (hz / 20.0f) / std::log (1000.0f);
        return plot.getX() + t * plot.getWidth();
    };

    g.setFont (juce::FontOptions (11.0f));

    // Rejilla de dB cada 12 dB
    for (int db = 0; db >= (int) minDb; db -= 12)
    {
        const float y = dbToY ((float) db);
        g.setColour (Palette::grid.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Palette::textDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getX() - 34.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // Barras, peak hold y dB por encima de la referencia
    const bool compactNumbers = barWidth < 21.0f;

    // Curva: sombras (Mid, y Side encima en M/S) y "+dB" por zona
    if (! showBars)
    {
        drawCurve (g, false, tilt, refDb);
        if (showSide)
            drawCurve (g, true, tilt, refDb);
    }

    for (int i = 0; i < numBars && showBars; ++i)
    {
        // Centro de la banda (media geometrica) para aplicar el tilt
        const float centreHz = std::sqrt (audioProcessor.getBandFrequency (i) * audioProcessor.getBandFrequency (i + 1));
        const float tiltOff = tilt * std::log2 (centreHz / 1000.0f);
        const float raw = barDb (i);
        const float db  = raw + tiltOff;

        const float x   = plot.getX() + (float) i * (barWidth + barGap);
        const float top = dbToY (db);
        const bool over = db > refDb;

        g.setColour (over ? Palette::barOver : Palette::barNormal);
        g.fillRoundedRectangle (juce::Rectangle<float> (x, top, barWidth, plot.getBottom() - top),
                                juce::jmin (2.0f, barWidth * 0.25f));

        // Si el tilt empuja una banda con sonido real por debajo de la grafica,
        // una flechita lo indica: "aqui hay sonido, solo que queda abajo".
        if (db < minDb && raw > -80.0f)
        {
            const float cx = x + barWidth * 0.5f, by = plot.getBottom() - 3.0f;
            const float hw = juce::jmin (5.0f, barWidth * 0.35f);
            juce::Path arrow;
            arrow.addTriangle (cx - hw, by - hw, cx + hw, by - hw, cx, by);
            g.setColour (Palette::barNormal);
            g.fillPath (arrow);
        }

        // Peak hold: normal = quieto 1 s y luego baja; infinito = no baja.
        // Congelado: no cambia. Se guarda sin tilt y se dibuja con tilt.
        auto& hold = holdDb[(size_t) i];
        if (! frozen)
        {
            if (raw >= hold)
            {
                hold = raw;
                holdTime[(size_t) i] = now;
            }
            else if (! infiniteHold && now - holdTime[(size_t) i] > holdMs)
            {
                hold = juce::jmax (raw, hold - holdFallDbPerS * dt);
            }
        }
        const float holdShown = hold + tiltOff;

        if (holdShown > minDb + 1.0f)
        {
            g.setColour (Palette::text.withAlpha (0.9f));
            g.fillRect (x, dbToY (holdShown) - 1.0f, barWidth, 2.0f);
        }

        // Vista M/S: el canal Side como barra angosta cian encima de la Mid
        if (showSide)
        {
            const float sideDb  = sideBarDb (i) + tiltOff;
            const float sideTop = dbToY (sideDb);
            const float sideW   = juce::jmax (2.0f, barWidth * 0.5f);

            if (sideDb > minDb)
            {
                g.setColour (Palette::side.withAlpha (0.9f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x + (barWidth - sideW) * 0.5f, sideTop,
                                                                sideW, plot.getBottom() - sideTop),
                                        juce::jmin (2.0f, sideW * 0.25f));
            }
        }

        // Fase por banda: punto rojo arriba de todo lo de esta barra
        if (phaseRisk (i))
        {
            float yTop = juce::jmin (top, dbToY (holdShown));
            if (showSide)
                yTop = juce::jmin (yTop, dbToY (sideBarDb (i) + tiltOff));
            const float dotY = juce::jmax (plot.getY() + 5.0f, yTop - (over ? 24.0f : 9.0f));
            g.setColour (Palette::danger);
            g.fillEllipse (x + barWidth * 0.5f - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
        }

        // Cuantos dB pasa la referencia (el dato para el threshold del compresor)
        if (over)
        {
            const float amount = db - refDb;
            const auto text = "+" + (compactNumbers ? juce::String (juce::roundToInt (amount))
                                                    : juce::String (amount, 1));
            const float textY = juce::jmax (plot.getY(), juce::jmin (top, dbToY (holdShown)) - 16.0f);

            g.setColour (Palette::barOver);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (text, juce::Rectangle<float> (x - 6.0f, textY, barWidth + 12.0f, 13.0f),
                        juce::Justification::centred);
            g.setFont (juce::FontOptions (11.0f));
        }
    }

    // Eje de frecuencias: valores redondos en su posicion real. Si una
    // etiqueta no cabe junto a la anterior se omite (nunca se corta).
    const float ticks[] = { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                            1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const float labelW  = 34.0f;
    float lastRight = -1.0e9f;

    for (float hz : ticks)
    {
        const float x = freqToX (hz);
        auto box = juce::Rectangle<float> (x - labelW * 0.5f, plot.getBottom() + 6.0f, labelW, 14.0f);

        if (box.getX() < plot.getX() - 4.0f)          box.setX (plot.getX() - 4.0f);
        if (box.getRight() > plot.getRight() + 4.0f)  box.setX (plot.getRight() + 4.0f - labelW);

        if (box.getX() < lastRight + 6.0f)
            continue;

        g.setColour (Palette::grid);
        g.drawVerticalLine ((int) x, plot.getBottom(), plot.getBottom() + 4.0f);
        g.setColour (Palette::textDim);
        g.drawText (frequencyLabel (hz), box, juce::Justification::centred);
        lastRight = box.getRight();
    }

    // Linea horizontal de referencia (umbral objetivo de compresion)
    g.setColour (Palette::referenceLine);
    g.drawHorizontalLine ((int) dbToY (refDb), plot.getX(), plot.getRight());

    // Leyenda de la vista M/S
    if (showSide)
    {
        auto legend = juce::Rectangle<float> (plot.getRight() - 118.0f, plot.getY() + 6.0f, 118.0f, 14.0f);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.setColour (Palette::barNormal);
        g.fillRect (legend.getX(), legend.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.drawText ("MID", legend.withTrimmedLeft (12.0f).withWidth (40.0f), juce::Justification::centredLeft);
        g.setColour (Palette::side);
        g.fillRect (legend.getX() + 56.0f, legend.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.drawText ("SIDE", legend.withTrimmedLeft (68.0f), juce::Justification::centredLeft);
    }

    // Fase por banda: tira roja sobre el eje en las bandas que se cancelarian
    // en mono (en barras y en curva, en Sum y en M/S)
    for (int i = 0; i < numBars; ++i)
    {
        if (! phaseRisk (i))
            continue;
        float x0, x1;
        if (showBars)
        {
            x0 = plot.getX() + (float) i * (barWidth + barGap);
            x1 = x0 + barWidth;
        }
        else
        {
            x0 = freqToX (audioProcessor.getBandFrequency (i));
            x1 = freqToX (audioProcessor.getBandFrequency (i + 1));
        }
        g.setColour (Palette::danger);
        g.fillRect (juce::Rectangle<float> (x0, plot.getBottom() - 4.0f, x1 - x0, 4.0f));
    }

    // Congelado: aviso en la esquina de la grafica
    if (frozen)
    {
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.setColour (Palette::barOver);
        g.drawText ("FROZEN", juce::Rectangle<float> (plot.getX() + 8.0f, plot.getY() + 6.0f, 80.0f, 14.0f),
                    juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (11.0f));
    }

    drawHover (g, tilt, refDb, showBars, showSide, barWidth, barGap);
    drawLevelMeter (g);
    drawReadouts (g);
}

void SpectrumView::drawHover (juce::Graphics& g, float tilt, float refDb, bool showBars, bool showSide,
                              float barWidth, float barGap)
{
    using P = BarSpectrumAnalyzerAudioProcessor;
    if (! hoverPos.has_value() || ! plotArea.contains (*hoverPos))
        return;

    const auto plot = plotArea;
    auto dbToY = [&] (float db)
    {
        return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY());
    };

    float x = hoverPos->x;
    juce::StringArray lines;
    float midShown = -200.0f, sideShown = -200.0f;

    if (showBars)
    {
        // Barras: la banda bajo el mouse
        const int i = juce::jlimit (0, numBars - 1, (int) ((x - plot.getX()) / (barWidth + barGap)));
        const float loHz = audioProcessor.getBandFrequency (i), hiHz = audioProcessor.getBandFrequency (i + 1);
        const float tiltOff = tilt * std::log2 (std::sqrt (loHz * hiHz) / 1000.0f);
        const float bx = plot.getX() + (float) i * (barWidth + barGap);
        x = bx + barWidth * 0.5f;
        midShown  = barDb (i) + tiltOff;
        sideShown = sideBarDb (i) + tiltOff;

        g.setColour (Palette::text.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (bx - 1.0f, plot.getY(), barWidth + 2.0f, plot.getHeight()));

        const juce::String dash (juce::CharPointer_UTF8 ("\xe2\x80\x93"));
        lines.add (hiHz < 1000.0f ? juce::String (juce::roundToInt (loHz)) + dash + juce::String (juce::roundToInt (hiHz)) + " Hz"
                                  : juce::String (loHz / 1000.0f, loHz < 10000.0f ? 2 : 1) + dash
                                      + juce::String (hiHz / 1000.0f, hiHz < 10000.0f ? 2 : 1) + " kHz");
    }
    else
    {
        // Curva: el valor de la curva en la frecuencia exacta del mouse
        const float t   = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        const float hz  = 20.0f * std::pow (1000.0f, t);
        const float pos = t * (float) (P::numCurvePoints - 1);
        const int   k   = juce::jlimit (0, P::numCurvePoints - 2, (int) pos);
        const float f   = pos - (float) k;
        const float tiltOff = tilt * std::log2 (hz / 1000.0f);
        auto valueAt = [&] (bool side) { return curveDb (k, side) + (curveDb (k + 1, side) - curveDb (k, side)) * f; };
        midShown  = valueAt (false) + tiltOff;
        sideShown = valueAt (true) + tiltOff;

        const juce::String hzText = hz < 100.0f ? juce::String (hz, 1) + " Hz"
                                  : hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz"
                                                 : juce::String (hz / 1000.0f, 2) + " kHz";
        lines.add (hzText + "   " + noteName (hz));
    }

    // Lineas: vertical en la frecuencia; horizontal desde el punto de la
    // senal hasta la escala de dB (se lee el nivel real, no el del mouse)
    const float midY = dbToY (midShown);
    g.setColour (Palette::text.withAlpha (0.35f));
    g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    if (midShown > minDb)
        g.drawHorizontalLine ((int) midY, plot.getX(), x);

    auto dot = [&] (float y, juce::Colour c)
    {
        g.setColour (Palette::background);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (c);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    };
    if (showSide && sideShown > minDb) dot (dbToY (sideShown), Palette::side);
    if (midShown > minDb)              dot (midY, Palette::barOver);

    auto dbText = [] (float v) { return v <= -99.0f ? juce::String ("--") : juce::String (v, 1) + " dB"; };
    lines.add ((showSide ? "Mid  " : "") + dbText (midShown));
    if (showSide)
        lines.add ("Side " + dbText (sideShown));

    // M/S: distancia Side-Mid ya restada, y que significa. En una banda,
    // M^2 - S^2 = L*R, asi que la correlacion de esa zona sale de los dos
    // niveles; mismos limites y palabras que la pestana Phase.
    juce::String gapLine, verdict;
    juce::Colour verdictColour = Palette::textDim;
    if (showSide && midShown > -99.0f && sideShown > -99.0f)
    {
        const float gap = midShown - sideShown;          // el tilt se cancela
        gapLine = "Side " + juce::String (std::abs (gap), 1) + (gap >= 0.0f ? " dB below Mid" : " dB above Mid");

        const float ratio = std::pow (10.0f, -gap / 10.0f);  // potencia Side / Mid
        const float corr  = (1.0f - ratio) / (1.0f + ratio);
        if (corr >= 0.9f)                            { verdict = "Mostly mono";              verdictColour = Palette::ok; }
        else if (corr >= PhaseAnalyzer::warnBelow)   { verdict = "Stereo, mono-safe";        verdictColour = Palette::ok; }
        else if (corr >= PhaseAnalyzer::dangerBelow) { verdict = "Wide - check in mono";     verdictColour = Palette::warn; }
        else                                         { verdict = "Phase cancellation risk";  verdictColour = Palette::danger; }
    }

    // Banda marcada por el aviso de fase: veredicto sostenido + que revisar
    const int   hoverBand = showBars ? juce::jlimit (0, numBars - 1, (int) ((hoverPos->x - plot.getX()) / (barWidth + barGap)))
                                     : bandAt (20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (hoverPos->x - plot.getX()) / plot.getWidth())));
    const bool  bandRisk  = phaseRisk (hoverBand);
    if (bandRisk)
    {
        verdict = "Phase cancellation risk";
        verdictColour = Palette::danger;
    }

    juce::String refLine;
    if (midShown > -99.0f)
    {
        const float diff = midShown - refDb;
        refLine = (diff >= 0.0f ? "+" + juce::String (diff, 1) + " over ref" : juce::String (diff, 1) + " below ref");
    }
    if (tilt > 0.0f)
        lines.add ("(tilt " + juce::String (tilt, 1) + " dB/oct)");

    // Cuadro junto al cursor, siempre dentro de la grafica
    g.setFont (juce::FontOptions (11.5f));
    const int rows = lines.size() + (refLine.isNotEmpty() ? 1 : 0) + (gapLine.isNotEmpty() ? 1 : 0)
                   + (verdict.isNotEmpty() ? 1 : 0) + (bandRisk ? 1 : 0);
    const float w = (gapLine.isNotEmpty() || bandRisk) ? 212.0f : 150.0f, h = 8.0f + 15.0f * (float) rows;
    float bx = hoverPos->x + 14.0f, by = hoverPos->y + 14.0f;
    if (bx + w > plot.getRight()) bx = hoverPos->x - 14.0f - w;
    if (by + h > plot.getBottom()) by = hoverPos->y - 14.0f - h;
    const auto box = juce::Rectangle<float> (bx, juce::jmax (plot.getY(), by), w, h);

    g.setColour (Palette::panel.withAlpha (0.94f));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (Palette::grid);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    auto row = box.reduced (8.0f, 4.0f).withHeight (15.0f);
    for (int i = 0; i < lines.size(); ++i)
    {
        const bool isSide = lines[i].startsWith ("Side");
        g.setColour (i == 0 ? Palette::text : isSide ? Palette::side : lines[i].startsWith ("(") ? Palette::textDim : Palette::barOver);
        g.drawText (lines[i], row, juce::Justification::centredLeft);
        row.translate (0.0f, 15.0f);
    }
    if (gapLine.isNotEmpty())
    {
        g.setColour (Palette::text);
        g.drawText (gapLine, row, juce::Justification::centredLeft);
        row.translate (0.0f, 15.0f);
    }
    if (verdict.isNotEmpty())
    {
        g.setColour (verdictColour);
        g.drawText (verdict, row, juce::Justification::centredLeft);
        row.translate (0.0f, 15.0f);
    }
    if (bandRisk)
    {
        g.setColour (Palette::textDim);
        g.drawText ("Check: polarity, L/R delay, widener", row, juce::Justification::centredLeft);
        row.translate (0.0f, 15.0f);
    }
    if (refLine.isNotEmpty())
    {
        g.setColour (midShown > refDb ? Palette::barOver : Palette::textDim);
        g.drawText (refLine, row, juce::Justification::centredLeft);
    }
    g.setFont (juce::FontOptions (11.0f));
}

void SpectrumView::drawCurve (juce::Graphics& g, bool side, float tilt, float refDb)
{
    using P = BarSpectrumAnalyzerAudioProcessor;
    const auto plot = plotArea;

    auto dbToY = [&] (float db)
    {
        return juce::jmap (juce::jlimit (minDb - 6.0f, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY());
    };
    auto freqToX = [&] (float hz)
    {
        return plot.getX() + std::log (hz / 20.0f) / std::log (1000.0f) * plot.getWidth();
    };

    // Puntos de la curva (con tilt). La linea pasa EXACTAMENTE por cada
    // punto medido: curvas tipo Catmull-Rom con los puntos de control
    // limitados entre sus vecinos, asi nunca dibuja picos que no existen.
    std::array<float, P::numCurvePoints> dbs;
    std::array<juce::Point<float>, P::numCurvePoints> pts;
    for (int i = 0; i < P::numCurvePoints; ++i)
    {
        const float hz = P::getCurveFrequency (i);
        dbs[(size_t) i] = curveDb (i, side) + tilt * std::log2 (hz / 1000.0f);
        pts[(size_t) i] = { freqToX (hz), juce::jmin (plot.getBottom(), dbToY (dbs[(size_t) i])) };
    }

    juce::Path line;
    line.startNewSubPath (pts[0]);
    for (int i = 1; i < P::numCurvePoints; ++i)
    {
        const auto p0 = pts[(size_t) juce::jmax (0, i - 2)], p1 = pts[(size_t) i - 1];
        const auto p2 = pts[(size_t) i], p3 = pts[(size_t) juce::jmin (P::numCurvePoints - 1, i + 1)];
        const float lo = juce::jmin (p1.y, p2.y), hi = juce::jmax (p1.y, p2.y);
        juce::Point<float> c1 (p1.x + (p2.x - p0.x) / 6.0f, juce::jlimit (lo, hi, p1.y + (p2.y - p0.y) / 6.0f));
        juce::Point<float> c2 (p2.x - (p3.x - p1.x) / 6.0f, juce::jlimit (lo, hi, p2.y - (p3.y - p1.y) / 6.0f));
        line.cubicTo (c1, c2, p2);
    }

    juce::Path area (line);
    area.lineTo (plot.getRight(), plot.getBottom());
    area.lineTo (plot.getX(), plot.getBottom());
    area.closeSubPath();

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // Sombra: relleno que se desvanece hacia abajo y un borde suave
    const auto base = side ? Palette::side : Palette::barNormal;
    g.setGradientFill (juce::ColourGradient (base.withAlpha (side ? 0.40f : 0.70f), 0.0f, plot.getY(),
                                             base.withAlpha (side ? 0.08f : 0.18f), 0.0f, plot.getBottom(), false));
    g.fillPath (area);

    if (! side)
    {
        // Lo que pasa la referencia, resaltado (el dato para comprimir)
        const float refY = dbToY (refDb);
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float>::leftTopRightBottom (plot.getX(), plot.getY(), plot.getRight(), refY).toNearestInt());
        g.setColour (Palette::barOver.withAlpha (0.85f));
        g.fillPath (area);
        g.restoreState();
    }

    g.setColour ((side ? Palette::side : Palette::barOver).withAlpha (0.9f));
    g.strokePath (line, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Hold infinito: linea fina con el maximo de la curva en toda la cancion
    if (audioProcessor.apvts.getRawParameterValue ("peakHold")->load() > 0.5f)
    {
        auto& holds = side ? curveHoldSide : curveHoldMid;
        juce::Path maxLine;
        for (int i = 0; i < P::numCurvePoints; ++i)
        {
            const float hz = P::getCurveFrequency (i);
            if (! frozen)
                holds[(size_t) i] = juce::jmax (holds[(size_t) i], curveDb (i, side));
            const juce::Point<float> pt (freqToX (hz), juce::jmin (plot.getBottom(), dbToY (holds[(size_t) i] + tilt * std::log2 (hz / 1000.0f))));
            if (i == 0) maxLine.startNewSubPath (pt); else maxLine.lineTo (pt);
        }
        g.setColour ((side ? Palette::side : Palette::text).withAlpha (0.55f));
        g.strokePath (maxLine, juce::PathStrokeType (0.8f));
    }
    g.restoreState();

    // "+dB" una vez por cada zona que pasa la referencia, en su punto mas alto
    if (! side)
    {
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        float lastRight = -1.0e9f;
        int labels = 0;
        for (int i = 0; i < P::numCurvePoints && labels < 8; )
        {
            if (dbs[(size_t) i] <= refDb) { ++i; continue; }
            int best = i;
            while (i < P::numCurvePoints && dbs[(size_t) i] > refDb)
            {
                if (dbs[(size_t) i] > dbs[(size_t) best]) best = i;
                ++i;
            }
            const float amount = dbs[(size_t) best] - refDb;
            if (amount < 0.5f) continue;
            const auto p = pts[(size_t) best];
            auto box = juce::Rectangle<float> (p.x - 20.0f, juce::jmax (plot.getY(), p.y - 17.0f), 40.0f, 13.0f);
            if (box.getX() < lastRight + 2.0f) continue;
            g.setColour (Palette::barOver);
            g.drawText ("+" + juce::String (amount, 1), box, juce::Justification::centred);
            lastRight = box.getRight();
            ++labels;
        }
        g.setFont (juce::FontOptions (11.0f));
    }
}

void SpectrumView::drawLevelMeter (juce::Graphics& g)
{
    auto& meter = audioProcessor.getLevelMeter();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = frameDt;   // segundos desde el cuadro anterior

    // Medidor segmentado: dos columnas (L y R) de rayas horizontales,
    // RMS como relleno y el pico como una raya brillante.
    const float colGap = 4.0f;
    const float colW   = (meterArea.getWidth() - colGap) * 0.5f;
    const float pitch  = 4.0f, segH = 3.0f;

    auto dbToY = [&] (float db)
    {
        return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, meterArea.getBottom(), meterArea.getY());
    };

    auto dbForY = [&] (float y)
    {
        return juce::jmap (y, meterArea.getBottom(), meterArea.getY(), minDb, maxDb);
    };

    // Zonas de intensidad: morado (bajo), lila (-12 a -6), amarillo (-6 a -1),
    // rojo (arriba de -1 dB, el limite que piden las plataformas).
    auto zoneColour = [] (float db)
    {
        if (db > -1.0f)  return Palette::danger;
        if (db > -6.0f)  return Palette::warn;
        if (db > -12.0f) return Palette::barOver;
        return Palette::barNormal;
    };

    float freshPeak = -100.0f;

    for (int c = 0; c < 2; ++c)
    {
        const float pkDb = juce::Decibels::gainToDecibels (meter.takePeak (c), -100.0f);
        freshPeak = juce::jmax (freshPeak, pkDb);

        auto& shown = meterPeakDb[(size_t) c];
        shown = pkDb > shown ? pkDb : juce::jmax (pkDb, shown - meterFallDbPerS * dt);

        const float rms  = meter.getRmsDb (c);
        const float x    = meterArea.getX() + (float) c * (colW + colGap);
        const float rmsY = dbToY (rms);
        const float pkY  = dbToY (shown);

        for (float y = meterArea.getBottom() - segH; y >= meterArea.getY(); y -= pitch)
        {
            const bool lit  = y >= rmsY;
            const auto zone = zoneColour (dbForY (y + segH * 0.5f));
            // Apagadas, las zonas de -6 y -1 dB se ven tenues: siempre se sabe
            // donde empieza lo caliente.
            g.setColour (lit ? zone
                             : (zone == Palette::warn || zone == Palette::danger ? zone.withAlpha (0.16f)
                                                                                 : Palette::panel.withAlpha (0.6f)));
            g.fillRect (x, y, colW, segH);
        }

        if (shown > minDb)
        {
            const auto zone = zoneColour (shown);
            g.setColour (zone == Palette::barNormal ? Palette::text : zone);
            g.fillRect (x, pkY - 1.0f, colW, 2.0f);
        }
    }

    // El numero de pico se sostiene 2 s y despues sigue al pico actual
    if (freshPeak >= numericPeakDb || now - numericPeakTime > numericHoldMs)
    {
        numericPeakDb   = freshPeak;
        numericPeakTime = now;
    }

    // Foco de clip: se queda encendido hasta darle clic
    const bool clipped = meter.hasClipped();
    g.setColour (clipped ? Palette::danger : Palette::panel);
    g.fillRoundedRectangle (clipArea, 3.0f);
    g.setColour (clipped ? Palette::background : Palette::textDim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("CLIP", clipArea, juce::Justification::centred);
}

void SpectrumView::drawReadouts (juce::Graphics& g)
{
    auto& meter = audioProcessor.getLevelMeter();
    const float rms  = juce::jmax (meter.getRmsDb (0), meter.getRmsDb (1));
    const float lufs = meter.getLufsShort();

    struct Item { const char* label; juce::String value; };
    const Item items[] = { { "PEAK",   formatDb (numericPeakDb) },
                           { "RMS",    formatDb (rms) },
                           { "LUFS S", formatDb (lufs) } };

    // Lecturas alineadas a la derecha: etiqueta tenue + valor.
    // Si la ventana es angosta se omite RMS (Peak y LUFS son los esenciales).
    auto area = readoutArea;
    const bool roomForAll = area.getWidth() >= 300.0f;

    for (int i = 2; i >= 0; --i)
    {
        if (i == 1 && ! roomForAll)
            continue;

        auto slot = area.removeFromRight (i == 2 ? 104.0f : 86.0f);
        auto valueBox = slot.removeFromRight (46.0f);

        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (items[i].value, valueBox, juce::Justification::centredRight);

        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (items[i].label, slot.withTrimmedRight (6.0f), juce::Justification::centredRight);

        area.removeFromRight (8.0f);
    }
}
