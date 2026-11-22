#pragma once
#include <JuceHeader.h>
#include "../Palette.h"

/**
    Boton de escucha Mono / Side (la unica funcion de Lyman que toca el audio).

    - Mantener presionado = escuchar; al soltar regresa solo a estereo.
    - Clic derecho o Shift + clic = se queda fijo (otro igual lo suelta).
    - El modo (Mono o Side) se elige en el engrane.
    - Mueve el parametro "listen", asi que tambien se puede ligar a un pad MIDI.
*/
class ListenButton : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit ListenButton (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        setTooltip ("Hold to listen in mono (or Side, from the settings menu). "
                    "Right-click or Shift+click to lock it. Turns itself off on export.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool isOn() const        { return apvts.getRawParameterValue ("listen")->load() > 0.5f; }
    bool isSideMode() const  { return apvts.getRawParameterValue ("listenMode")->load() > 0.5f; }
    bool isLatched() const   { return latched; }

    // El editor lo llama ~30 veces por segundo: redibuja solo si algo cambio
    // (por ejemplo, si lo prendio un pad MIDI)
    void refresh()
    {
        const int now = (isOn() ? 1 : 0) | (isSideMode() ? 2 : 0) | (latched ? 4 : 0);
        if (now != lastShown)
        {
            lastShown = now;
            if (! isOn()) latched = false;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = isOn();
        const float radius = 4.0f;

        g.setColour (on ? Palette::warn : Palette::panel);
        g.fillRoundedRectangle (r, radius);
        g.setColour (on ? Palette::warn : (isMouseOver() ? Palette::barNormal : Palette::grid));
        g.drawRoundedRectangle (r, radius, 1.0f);

        g.setColour (on ? Palette::background : Palette::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.08f));
        g.drawText (isSideMode() ? "SIDE" : "MONO", r, juce::Justification::centred);

        // Candado chiquito cuando esta fijo
        if (latched && on)
            g.fillRoundedRectangle (r.getRight() - 9.0f, r.getY() + 4.0f, 4.0f, 4.0f, 1.0f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || e.mods.isShiftDown())
        {
            latched = ! (latched && isOn());
            set (latched);
            return;
        }
        latched = false;
        set (true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! latched && ! e.mods.isPopupMenu())
            set (false);
    }

private:
    void set (bool on)
    {
        if (auto* p = apvts.getParameter ("listen"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
        refresh();
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    bool latched = false;
    int  lastShown = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ListenButton)
};
