#pragma once
#include <JuceHeader.h>
#include "../Palette.h"

/** Colores de los controles estandar de JUCE con la paleta del plugin. */
class BlacklightLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlacklightLookAndFeel()
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

        setColour (juce::Label::textColourId, Palette::text);

        // Menu del engrane
        setColour (juce::PopupMenu::backgroundColourId,            Palette::panel);
        setColour (juce::PopupMenu::textColourId,                  Palette::text);
        setColour (juce::PopupMenu::headerTextColourId,            Palette::textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::grid);
        setColour (juce::PopupMenu::highlightedTextColourId,       Palette::barOver);

        setColour (juce::TooltipWindow::backgroundColourId, Palette::panel);
        setColour (juce::TooltipWindow::textColourId,       Palette::text);
        setColour (juce::TooltipWindow::outlineColourId,    Palette::grid);
    }
};
