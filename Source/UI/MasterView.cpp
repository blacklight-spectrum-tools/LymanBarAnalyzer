#include "MasterView.h"
#include "../PluginProcessor.h"
#include "../Palette.h"

namespace
{
    constexpr float scaleMin = -24.0f, scaleMax = -6.0f;   // escala de sonoridad (LUFS)

    // Objetivos de normalizacion para musica (nov 2026). Spotify sube las
    // canciones mas bajitas (limitado por los picos); YouTube y Apple Music
    // solo bajan; las demas no publican si suben.
    struct PlatformInfo { const char* name; float target; int boost; };
    constexpr PlatformInfo platforms[] = {
        { "Spotify",        -14.0f, 1 },
        { "YouTube",        -14.0f, 0 },
        { "Apple Music",    -16.0f, 0 },
        { "Amazon / TIDAL", -14.0f, 2 },
        { "Deezer",         -15.0f, 2 },
    };

    juce::Colour tpColour (float db)
    {
        if (db > 0.0f)  return Palette::danger;
        if (db > -1.0f) return Palette::warn;
        return Palette::ok;
    }
}

MasterView::MasterView (BarSpectrumAnalyzerAudioProcessor& p)
    : level (p.getLevelMeter()),
      truePeak (p.getTruePeakMeter())
{
    resetButton.onClick = [this]
    {
        level.requestLoudnessReset();
        truePeak.requestReset();
    };
    resetButton.setTooltip ("Restart Integrated, LRA, maximums, True Peak and clip count");
    addAndMakeVisible (resetButton);
}

juce::String MasterView::formatLufs (float v)
{
    return v <= -69.9f ? juce::String ("--") : juce::String (v, 1);
}

juce::String MasterView::formatTime (double seconds)
{
    const int s = (int) seconds;
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}

void MasterView::resized()
{
    auto area = getLocalBounds().reduced (20, 14).toFloat();

    const float gap = 28.0f;
    auto left = area.removeFromLeft ((area.getWidth() - gap) * 0.52f);
    area.removeFromLeft (gap);
    auto right = area;

    loudArea = left;
    resetButton.setBounds (juce::Rectangle<float> (loudArea.getX(), loudArea.getBottom() - 28.0f, 96.0f, 28.0f).toNearestInt());

    tpArea     = right.removeFromTop (juce::jmin (112.0f, right.getHeight() * 0.3f));
    right.removeFromTop (10.0f);
    plrArea    = right.removeFromTop (juce::jmin (84.0f, right.getHeight() * 0.3f));
    right.removeFromTop (12.0f);
    streamArea = right;
}

void MasterView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
    drawLoudness (g);
    drawTruePeak (g);
    drawPlr (g);
    drawStreaming (g);
}

void MasterView::drawLed (juce::Graphics& g, juce::Rectangle<float> row, juce::Colour c, const juce::String& text)
{
    auto dot = row.removeFromLeft (row.getHeight()).reduced (row.getHeight() * 0.22f);
    g.setColour (c.withAlpha (0.25f));
    g.fillEllipse (dot.expanded (3.0f));
    g.setColour (c);
    g.fillEllipse (dot);
    row.removeFromLeft (8.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (text, row.toNearestInt(), juce::Justification::centredLeft, 1);
}

void MasterView::drawLoudness (juce::Graphics& g)
{
    auto a = loudArea;
    const float I       = level.getLufsIntegrated();
    const double secs   = level.getIntegratedSeconds();
    const bool  hasI    = I > -69.9f;

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawText ("LOUDNESS  (LUFS)", a.removeFromTop (16.0f), juce::Justification::centredLeft);
    a.removeFromTop (4.0f);

    // Integrado, grande
    auto bigRow = a.removeFromTop (54.0f);
    g.setColour (hasI ? Palette::text : Palette::idle);
    g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
    g.drawText (formatLufs (I), bigRow.removeFromLeft (150.0f), juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("INTEGRATED", bigRow.removeFromTop (26.0f), juce::Justification::bottomLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("whole song  " + formatTime (secs), bigRow, juce::Justification::topLeft);

    // Escala con los objetivos de las plataformas
    a.removeFromTop (16.0f);
    auto marks = a.removeFromTop (14.0f);
    auto bar   = a.removeFromTop (14.0f);
    auto ticks = a.removeFromTop (16.0f);
    auto xFor  = [&] (float v) { return juce::jmap (juce::jlimit (scaleMin, scaleMax, v), scaleMin, scaleMax, bar.getX(), bar.getRight()); };

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bar, 3.0f);

    // Zona "objetivo de streaming" (-16 a -14)
    g.setColour (Palette::ok.withAlpha (0.22f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xFor (-16.0f), bar.getY(), xFor (-14.0f), bar.getBottom()));

    g.setFont (juce::FontOptions (10.0f));
    for (float t = scaleMin; t <= scaleMax + 0.01f; t += 2.0f)
    {
        g.setColour (Palette::textDim);
        auto r = juce::Rectangle<float> (xFor (t) - 16.0f, ticks.getY(), 32.0f, ticks.getHeight());
        if (r.getX() < bar.getX())         r.setX (bar.getX());
        if (r.getRight() > bar.getRight()) r.setX (bar.getRight() - r.getWidth());
        const auto just = t <= scaleMin ? juce::Justification::centredLeft
                        : (t >= scaleMax ? juce::Justification::centredRight : juce::Justification::centred);
        g.drawText (juce::String ((int) t), r, just);
    }
    g.setColour (Palette::textDim);
    g.drawText ("Apple -16", juce::Rectangle<float> (xFor (-16.0f) - 60.0f, marks.getY(), 58.0f, marks.getHeight()), juce::Justification::centredRight);
    g.drawText ("-14 Spotify / YouTube", juce::Rectangle<float> (xFor (-14.0f) + 2.0f, marks.getY(), 130.0f, marks.getHeight()), juce::Justification::centredLeft);

    const float S = level.getLufsShort();
    if (S > scaleMin - 10.0f && S > -69.9f)
    {
        g.setColour (Palette::textDim);
        g.fillRect (xFor (S) - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
    }
    if (hasI)
    {
        g.setColour (Palette::barOver);
        g.fillRect (xFor (I) - 2.0f, bar.getY() - 4.0f, 4.0f, bar.getHeight() + 8.0f);
    }

    // Valores secundarios
    a.removeFromTop (16.0f);
    struct Item { const char* label; juce::String value; };
    const Item items[] = {
        { "MOMENTARY",  formatLufs (level.getLufsMomentary()) },
        { "SHORT-TERM", formatLufs (S) },
        { "LRA",        hasI && secs >= 3.0 ? juce::String (level.getLoudnessRange(), 1) + " LU" : juce::String ("--") },
        { "MAX S",      formatLufs (level.getMaxShort()) },
    };
    auto statRow = a.removeFromTop (42.0f);
    const float w = statRow.getWidth() / 4.0f;
    for (const auto& it : items)
    {
        auto cell = statRow.removeFromLeft (w);
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (it.label, cell.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText (it.value, cell, juce::Justification::centredLeft);
    }

    // Ayuda (si cabe, arriba del boton de reinicio)
    a.removeFromTop (12.0f);
    auto hint = a.withTrimmedBottom (36.0f);
    if (hint.getHeight() >= 30.0f)
    {
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawFittedText ("Integrated is what streaming platforms use. Press Reset, then play "
                          "the song from start to end.  LRA: how much the volume moves between "
                          "soft and loud parts.",
                          hint.removeFromTop (juce::jmin (hint.getHeight(), 48.0f)).toNearestInt(),
                          juce::Justification::topLeft, 3);
    }
}

void MasterView::drawTruePeak (juce::Graphics& g)
{
    auto a = tpArea;
    const float tp = truePeak.getMaxDb();
    const bool has = tp > -99.0f;

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawText ("TRUE PEAK  (max)", a.removeFromTop (16.0f), juce::Justification::centredLeft);
    a.removeFromTop (2.0f);

    auto row = a.removeFromTop (40.0f);
    g.setColour (has ? tpColour (tp) : Palette::idle);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText (has ? (tp > 0.0f ? "+" : "") + juce::String (tp, 1) + " dBTP" : juce::String ("--"), row, juce::Justification::centredLeft);

    const int clips = truePeak.getClipCount();
    g.setColour (clips > 0 ? Palette::danger : Palette::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("Clips: " + juce::String (clips), row, juce::Justification::centredRight);

    a.removeFromTop (4.0f);
    auto led = a.removeFromTop (24.0f);
    if (! has)            drawLed (g, led, Palette::idle,   "No signal");
    else if (tp > 0.0f)   drawLed (g, led, Palette::danger, "Clipping - lower the limiter ceiling");
    else if (tp > -1.0f)  drawLed (g, led, Palette::warn,   "Above -1 dBTP - may distort on streaming");
    else                  drawLed (g, led, Palette::ok,     "Safe for streaming (below -1 dBTP)");
}

void MasterView::drawPlr (juce::Graphics& g)
{
    auto a = plrArea;
    const float I  = level.getLufsIntegrated();
    const float tp = truePeak.getMaxDb();
    const bool  ok = I > -69.9f && tp > -99.0f && level.getIntegratedSeconds() >= 3.0;
    const float plr = tp - I;

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawText ("PLR  (dynamics: True Peak - Integrated)", a.removeFromTop (16.0f), juce::Justification::centredLeft);
    a.removeFromTop (2.0f);

    auto row = a.removeFromTop (30.0f);
    g.setColour (ok ? Palette::text : Palette::idle);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (ok ? juce::String (plr, 1) + " dB" : juce::String ("--"), row, juce::Justification::centredLeft);

    a.removeFromTop (2.0f);
    auto led = a.removeFromTop (24.0f);
    if (! ok)             drawLed (g, led, Palette::idle,   "Play a few seconds");
    else if (plr >= 9.0f) drawLed (g, led, Palette::ok,     "Healthy dynamics");
    else if (plr >= 7.0f) drawLed (g, led, Palette::warn,   "Dense - check the punch");
    else                  drawLed (g, led, Palette::danger, "Over-compressed - little punch left");
}

void MasterView::drawStreaming (juce::Graphics& g)
{
    auto a = streamArea;
    const float I  = level.getLufsIntegrated();
    const float tp = truePeak.getMaxDb();
    const bool  hasI = I > -69.9f && level.getIntegratedSeconds() >= 3.0;

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawText ("STREAMING  (what each platform does)", a.removeFromTop (16.0f), juce::Justification::centredLeft);
    a.removeFromTop (4.0f);

    const int count = (int) std::size (platforms);
    const float rowH = juce::jlimit (16.0f, 22.0f, a.getHeight() / (float) count);

    for (const auto& pl : platforms)
    {
        if (a.getHeight() < rowH) break;
        auto row = a.removeFromTop (rowH);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (Palette::text);
        g.drawText (pl.name, row.removeFromLeft (112.0f), juce::Justification::centredLeft);
        g.setColour (Palette::textDim);
        g.drawText (juce::String ((int) pl.target), row.removeFromLeft (34.0f), juce::Justification::centredLeft);

        juce::String text = "--";
        juce::Colour colour = Palette::textDim;
        if (hasI)
        {
            const float diff = pl.target - I;   // negativo: te bajan
            if (diff < -0.05f)
            {
                text   = "turned down " + juce::String (diff, 1) + " dB";
                colour = diff < -4.0f ? Palette::warn : Palette::text;
            }
            else if (diff <= 0.05f)
            {
                text = "on target";
                colour = Palette::ok;
            }
            else if (pl.boost == 1)
            {
                // Spotify sube, pero sin pasar de -1 dBTP
                const float room  = juce::jmax (0.0f, -1.0f - tp);
                const float boost = juce::jmin (diff, room);
                if (boost < 0.05f)
                    text = "no boost: peaks above -1 dBTP";
                else
                    text = "turned up +" + juce::String (boost, 1) + " dB" + (boost < diff - 0.05f ? " (limited by peaks)" : "");
            }
            else if (pl.boost == 0)
            {
                text = "plays " + juce::String (diff, 1) + " dB quieter (no boost)";
            }
            else
            {
                text = "may play quieter";
            }
        }
        g.setColour (colour);
        g.drawFittedText (text, row.toNearestInt(), juce::Justification::centredLeft, 1);
    }
}
