#include "SpectrumView.h"
#include "../PluginProcessor.h"
#include "../Palette.h"

namespace
{
    constexpr float tiltSlopes[] = { 0.0f, 3.0f, 4.5f };   // dB por octava, pivote 1 kHz
    const char* const tiltNames[] = { "0", "3 dB", "4.5 dB" };
    const char* const viewNames[] = { "Sum", "M/S" };

    constexpr double holdMs          = 1000.0;   // peak hold: tiempo quieto
    constexpr float  holdFallDbPerS  = 15.0f;    // y luego cae a esta velocidad
    constexpr float  meterFallDbPerS = 24.0f;
    constexpr double numericHoldMs   = 2000.0;   // el numero de pico se sostiene 2 s
}

SpectrumView::SpectrumView (BarSpectrumAnalyzerAudioProcessor& p)
    : audioProcessor (p)
{
    static_assert (numBars == BarSpectrumAnalyzerAudioProcessor::numBars, "numero de barras");

    holdDb.fill (minDb);
    holdTime.fill (0.0);

    referenceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    referenceSlider.setRange (minDb, maxDb, 0.1);
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
    tiltLabel.setColour (juce::Label::textColourId, Palette::textDim);
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
    viewLabel.setColour (juce::Label::textColourId, Palette::textDim);
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
}

void SpectrumView::styleSegment (juce::TextButton& b, int index, int count)
{
    b.setClickingTogglesState (false);
    b.setColour (juce::TextButton::buttonColourId,   Palette::background);
    b.setColour (juce::TextButton::buttonOnColourId, Palette::panel);
    b.setColour (juce::TextButton::textColourOffId,  Palette::textDim);
    b.setColour (juce::TextButton::textColourOnId,   Palette::barOver);
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

    auto row1 = area.removeFromTop (30);
    referenceLabel.setBounds (row1.removeFromLeft (210));
    referenceSlider.setBounds (row1);

    area.removeFromTop (8);
    auto row2 = area.removeFromTop (26);
    tiltLabel.setBounds (row2.removeFromLeft (36));
    for (auto& b : tiltButtons)
        b.setBounds (row2.removeFromLeft (56));
    row2.removeFromLeft (14);
    viewLabel.setBounds (row2.removeFromLeft (40));
    for (auto& b : viewButtons)
        b.setBounds (row2.removeFromLeft (52));
    row2.removeFromLeft (14);
    readoutArea = row2.toFloat();

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

    const float refDb = audioProcessor.apvts.getRawParameterValue ("referenceDb")->load();
    const int   tiltIndex = juce::jlimit (0, 2, (int) std::round (audioProcessor.apvts.getRawParameterValue ("tilt")->load()));
    const float tilt = tiltSlopes[tiltIndex];
    const bool  showSide = audioProcessor.apvts.getRawParameterValue ("view")->load() > 0.5f;

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

    for (int i = 0; i < numBars; ++i)
    {
        // Centro de la banda (media geometrica) para aplicar el tilt
        const float centreHz = std::sqrt (audioProcessor.getBandFrequency (i) * audioProcessor.getBandFrequency (i + 1));
        const float db = audioProcessor.getBarLevelDb (i) + tilt * std::log2 (centreHz / 1000.0f);

        const float x   = plot.getX() + (float) i * (barWidth + barGap);
        const float top = dbToY (db);
        const bool over = db > refDb;

        g.setColour (over ? Palette::barOver : Palette::barNormal);
        g.fillRoundedRectangle (juce::Rectangle<float> (x, top, barWidth, plot.getBottom() - top),
                                juce::jmin (2.0f, barWidth * 0.25f));

        // Si el tilt empuja una banda con sonido real por debajo de la grafica,
        // una flechita lo indica: "aqui hay sonido, solo que queda abajo".
        if (db < minDb && audioProcessor.getBarLevelDb (i) > -80.0f)
        {
            const float cx = x + barWidth * 0.5f, by = plot.getBottom() - 3.0f;
            const float hw = juce::jmin (5.0f, barWidth * 0.35f);
            juce::Path arrow;
            arrow.addTriangle (cx - hw, by - hw, cx + hw, by - hw, cx, by);
            g.setColour (Palette::barNormal);
            g.fillPath (arrow);
        }

        // Peak hold: se queda quieto 1 s y luego baja
        auto& hold = holdDb[(size_t) i];
        if (db >= hold)
        {
            hold = db;
            holdTime[(size_t) i] = now;
        }
        else if (now - holdTime[(size_t) i] > holdMs)
        {
            hold = juce::jmax (db, hold - holdFallDbPerS * dt);
        }

        if (hold > minDb + 1.0f)
        {
            g.setColour (Palette::text.withAlpha (0.9f));
            g.fillRect (x, dbToY (hold) - 1.0f, barWidth, 2.0f);
        }

        // Vista M/S: el canal Side como barra angosta cian encima de la Mid
        if (showSide)
        {
            const float sideDb  = audioProcessor.getSideLevelDb (i) + tilt * std::log2 (centreHz / 1000.0f);
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

        // Cuantos dB pasa la referencia (el dato para el threshold del compresor)
        if (over)
        {
            const float amount = db - refDb;
            const auto text = "+" + (compactNumbers ? juce::String (juce::roundToInt (amount))
                                                    : juce::String (amount, 1));
            const float textY = juce::jmax (plot.getY(), juce::jmin (top, dbToY (hold)) - 16.0f);

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

    drawLevelMeter (g);
    drawReadouts (g);
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
