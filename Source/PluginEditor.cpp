#include "PluginEditor.h"
#include "Palette.h"
#include "UI/Logo.h"

namespace
{
    constexpr int defaultWidth  = 820, defaultHeight = 500;
    constexpr int minWidth      = 640, minHeight     = 420;
    constexpr int maxWidth      = 1920, maxHeight    = 1200;

    const juce::Identifier uiWidthId  { "uiWidth" };
    const juce::Identifier uiHeightId { "uiHeight" };
}

BarSpectrumAnalyzerAudioProcessorEditor::BarSpectrumAnalyzerAudioProcessorEditor (BarSpectrumAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      spectrumView (p),
      phaseView (p)
{
    setLookAndFeel (&lookAndFeel);

    // Pestanas: botones que se comportan como grupo de radio
    int index = 0;
    for (auto* tab : { &spectrumTab, &phaseTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1001);
        tab->setColour (juce::TextButton::buttonColourId,   Palette::background);
        tab->setColour (juce::TextButton::buttonOnColourId, Palette::panel);
        tab->setColour (juce::TextButton::textColourOffId,  Palette::textDim);
        tab->setColour (juce::TextButton::textColourOnId,   Palette::barOver);
        tab->onClick = [this, index] { showTab (index); };
        addAndMakeVisible (tab);
        ++index;
    }

    addChildComponent (spectrumView);
    addChildComponent (phaseView);

    showTab (juce::jlimit (0, 1, audioProcessor.lastTab.load()));

    // Ventana redimensionable (esquina inferior derecha o desde el DAW).
    // El tamano se guarda con el proyecto y se recupera al reabrir.
    const auto& state = audioProcessor.apvts.state;
    const int w = juce::jlimit (minWidth,  maxWidth,  (int) state.getProperty (uiWidthId,  defaultWidth));
    const int h = juce::jlimit (minHeight, maxHeight, (int) state.getProperty (uiHeightId, defaultHeight));

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (w, h);

    startTimerHz (30);
}

BarSpectrumAnalyzerAudioProcessorEditor::~BarSpectrumAnalyzerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BarSpectrumAnalyzerAudioProcessorEditor::showTab (int index)
{
    currentTab = index;
    audioProcessor.lastTab.store (index);

    spectrumTab.setToggleState (index == 0, juce::dontSendNotification);
    phaseTab.setToggleState    (index == 1, juce::dontSendNotification);

    spectrumView.setVisible (index == 0);
    phaseView.setVisible    (index == 1);
}

void BarSpectrumAnalyzerAudioProcessorEditor::timerCallback()
{
    // Solo se redibuja la pestana visible
    if (currentTab == 0) spectrumView.repaint();
    else                 phaseView.repaint();
}

void BarSpectrumAnalyzerAudioProcessorEditor::resized()
{
    auto area   = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (12, 8);

    spectrumTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (6);
    phaseTab.setBounds (header.removeFromLeft (110));

    spectrumView.setBounds (area);
    phaseView.setBounds (area);

    audioProcessor.apvts.state.setProperty (uiWidthId,  getWidth(),  nullptr);
    audioProcessor.apvts.state.setProperty (uiHeightId, getHeight(), nullptr);
}

void BarSpectrumAnalyzerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Logo a la derecha de la cabecera: marca + "BLACKLIGHT / SPECTRUM TOOLS"
    auto header = getLocalBounds().removeFromTop (headerHeight).toFloat().reduced (16.0f, 8.0f);
    auto text   = header.removeFromRight (128.0f);
    header.removeFromRight (10.0f);
    Logo::drawMark (g, header.removeFromRight (48.0f).withSizeKeepingCentre (48.0f, 18.0f), Palette::barOver);

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.14f));
    g.drawText ("BLACKLIGHT", text.removeFromTop (17.0f), juce::Justification::bottomLeft);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (9.0f).withKerningFactor (0.28f));
    g.drawText ("SPECTRUM TOOLS", text, juce::Justification::topLeft);

    g.setColour (Palette::panel);
    g.fillRect (0, headerHeight - 1, getWidth(), 1);
}
