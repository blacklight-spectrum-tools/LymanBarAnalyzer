#pragma once
#include <JuceHeader.h>
#include "../Palette.h"

/**
    Ayudas para que los controles sigan el tema de color en vivo.

    Los botones de segmento (pestanas, Tilt, View, Style...) y las etiquetas
    tenues se marcan una vez; al cambiar de tema, refreshTree() les vuelve a
    poner los colores del tema nuevo.
*/
namespace ThemeStyle
{
    inline void applySegment (juce::Component& b)
    {
        b.setColour (juce::TextButton::buttonColourId,   Palette::background);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::panel);
        b.setColour (juce::TextButton::textColourOffId,  Palette::textDim);
        b.setColour (juce::TextButton::textColourOnId,   Palette::barOver);
    }

    inline void segment (juce::Button& b)
    {
        b.getProperties().set ("blkSegment", true);
        applySegment (b);
    }

    inline void dimLabel (juce::Label& l)
    {
        l.getProperties().set ("blkDim", true);
        l.setColour (juce::Label::textColourId, Palette::textDim);
    }

    inline void refreshTree (juce::Component& root)
    {
        for (auto* c : root.getChildren())
        {
            if (c->getProperties().contains ("blkSegment"))
                applySegment (*c);
            if (c->getProperties().contains ("blkDim"))
                c->setColour (juce::Label::textColourId, Palette::textDim);
            refreshTree (*c);
        }
    }
}

/**
    Colores de los controles estandar de JUCE con la paleta del plugin,
    mas el menu del engrane y las ayudas (tooltips) con el estilo de la marca:
    panel oscuro con borde fino, titulos en versalitas, punto lila en la
    opcion activa y una linea lila arriba, como la marca.
*/
class BlacklightLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlacklightLookAndFeel()
    {
        Palette::ensureLoaded();
        refreshColours();
    }

    // Se vuelve a llamar al cambiar de tema
    void refreshColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Palette::background);

        setColour (juce::TextButton::buttonColourId,   Palette::panel);
        setColour (juce::TextButton::buttonOnColourId, Palette::grid);
        setColour (juce::TextButton::textColourOffId,  Palette::text);
        setColour (juce::TextButton::textColourOnId,   Palette::barOver);
        setColour (juce::ComboBox::outlineColourId,    Palette::grid);

        setColour (juce::Slider::thumbColourId,             Palette::barOver);
        setColour (juce::Slider::trackColourId,             Palette::barNormal);
        setColour (juce::Slider::backgroundColourId,        Palette::panel);
        setColour (juce::Slider::textBoxTextColourId,       Palette::text);
        setColour (juce::Slider::textBoxBackgroundColourId, Palette::panel);
        setColour (juce::Slider::textBoxOutlineColourId,    Palette::grid);
        setColour (juce::TextEditor::highlightColourId,     Palette::grid);
        setColour (juce::TextEditor::focusedOutlineColourId, Palette::barNormal);
        setColour (juce::CaretComponent::caretColourId,     Palette::barOver);

        setColour (juce::Label::textColourId, Palette::text);

        // Menu del engrane (el fondo opaco hace la ventana opaca: esquinas rectas)
        setColour (juce::PopupMenu::backgroundColourId,            Palette::background);
        setColour (juce::PopupMenu::textColourId,                  Palette::text);
        setColour (juce::PopupMenu::headerTextColourId,            Palette::textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::panel);
        setColour (juce::PopupMenu::highlightedTextColourId,       Palette::barOver);

        setColour (juce::TooltipWindow::backgroundColourId, Palette::background);
        setColour (juce::TooltipWindow::textColourId,       Palette::text);
        setColour (juce::TooltipWindow::outlineColourId,    Palette::grid);
    }

    //==========================================================================
    // Menu del engrane

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions (13.5f);
    }

    int getPopupMenuBorderSize() override { return 8; }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        const auto r = juce::Rectangle<int> (width, height);
        g.fillAll (Palette::background);
        g.setColour (Palette::grid);
        g.drawRect (r, 1);

        // Linea lila arriba: el "filamento" de la marca
        g.setColour (Palette::barOver.withAlpha (0.85f));
        g.fillRect (r.withHeight (2));
    }

    void drawPopupMenuColumnSeparatorWithOptions (juce::Graphics& g, const juce::Rectangle<int>& bounds,
                                                  const juce::PopupMenu::Options&) override
    {
        g.setColour (Palette::panel);
        g.fillRect (bounds.withSizeKeepingCentre (1, bounds.getHeight() - 16));
    }

    int getPopupMenuColumnSeparatorWidthWithOptions (const juce::PopupMenu::Options&) override { return 17; }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override
    {
        if (isSeparator)
        {
            idealWidth  = 50;
            idealHeight = 9;
            return;
        }

        juce::ignoreUnused (standardMenuItemHeight);
        idealHeight = itemHeight;
        idealWidth  = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 52;
    }

    void getIdealPopupMenuSectionHeaderSizeWithOptions (const juce::String& text, int,
                                                        int& idealWidth, int& idealHeight,
                                                        const juce::PopupMenu::Options&) override
    {
        idealHeight = 28;
        idealWidth  = juce::GlyphArrangement::getStringWidthInt (headerFont(), text.toUpperCase()) + 24;
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        auto r = area.reduced (10, 0).withTrimmedTop (10);
        g.setColour (Palette::textDim);
        g.setFont (headerFont());
        g.drawText (sectionName.toUpperCase(), r, juce::Justification::centredLeft, true);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColourToUse) override
    {
        juce::ignoreUnused (shortcutKeyText);

        if (isSeparator)
        {
            g.setColour (Palette::panel);
            g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
            return;
        }

        auto r = area.toFloat().reduced (2.0f, 1.0f);

        if (isHighlighted && isActive)
        {
            g.setColour (Palette::panel);
            g.fillRoundedRectangle (r, 4.0f);
        }

        // Punto lila en la opcion activa (o el icono del item, si trae)
        auto mark = r.removeFromLeft (26.0f);
        if (icon != nullptr)
        {
            icon->drawWithin (g, mark.withSizeKeepingCentre (12.0f, 12.0f),
                              juce::RectanglePlacement::centred, 1.0f);
        }
        else if (isTicked)
        {
            g.setColour (Palette::barOver);
            g.fillEllipse (mark.withSizeKeepingCentre (7.0f, 7.0f));
        }

        auto colour = textColourToUse != nullptr ? *textColourToUse : Palette::text;
        if (isTicked || (isHighlighted && isActive))
            colour = Palette::barOver;
        if (! isActive)
            colour = Palette::textDim.withMultipliedAlpha (0.6f);

        g.setColour (colour);
        g.setFont (getPopupMenuFont());
        g.drawFittedText (text, r.toNearestInt().withTrimmedRight (hasSubMenu ? 18 : 8),
                          juce::Justification::centredLeft, 1);

        if (hasSubMenu)
        {
            const float x = r.getRight() - 12.0f, cy = r.getCentreY();
            juce::Path p;
            p.startNewSubPath (x, cy - 4.0f);
            p.lineTo (x + 4.0f, cy);
            p.lineTo (x, cy + 4.0f);
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }
    }

    //==========================================================================
    // Ayudas al pasar el mouse

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const auto layout = tooltipLayout (tipText);
        const int w = (int) std::ceil (layout.getWidth())  + tipPadX * 2 + 3;
        const int h = (int) std::ceil (layout.getHeight()) + tipPadY * 2;

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 20,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8)  : screenPos.y + 18,
                                     w, h)
                 .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        const auto r = juce::Rectangle<int> (width, height);
        g.fillAll (Palette::background);
        g.setColour (Palette::grid);
        g.drawRect (r, 1);
        g.setColour (Palette::barOver.withAlpha (0.85f));
        g.fillRect (r.withWidth (2));               // filamento lila a la izquierda

        tooltipLayout (text).draw (g, r.toFloat().withTrimmedLeft ((float) tipPadX + 3.0f)
                                                 .withTrimmedRight ((float) tipPadX)
                                                 .reduced (0.0f, (float) tipPadY));
    }

private:
    static juce::Font headerFont()
    {
        return juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.12f);
    }

    static juce::TextLayout tooltipLayout (const juce::String& text)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::topLeft);
        s.setLineSpacing (2.0f);
        s.append (text, juce::FontOptions (13.0f), Palette::text);

        // Corta a tipMaxWidth y luego ajusta el ancho a la linea mas larga
        juce::TextLayout layout;
        layout.createLayout (s, (float) tipMaxWidth);

        float widest = 0.0f;
        for (auto& line : layout)
            widest = juce::jmax (widest, line.getLineBoundsX().getLength());

        layout.createLayout (s, std::ceil (widest) + 1.0f);
        return layout;
    }

    static constexpr int itemHeight  = 24;
    static constexpr int tipMaxWidth = 300;
    static constexpr int tipPadX = 10, tipPadY = 7;
};
