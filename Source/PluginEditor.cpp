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
      phaseView (p),
      masterView (p)
{
    setLookAndFeel (&lookAndFeel);

    // Pestanas: botones que se comportan como grupo de radio
    int index = 0;
    for (auto* tab : { &spectrumTab, &phaseTab, &masterTab })
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
    addChildComponent (masterView);

    showTab (juce::jlimit (0, 2, audioProcessor.lastTab.load()));

    // Ventana redimensionable (esquina inferior derecha o desde el DAW).
    // El tamano se guarda con el proyecto y se recupera al reabrir.
    const auto& state = audioProcessor.apvts.state;
    const int w = juce::jlimit (minWidth,  maxWidth,  (int) state.getProperty (uiWidthId,  defaultWidth));
    const int h = juce::jlimit (minHeight, maxHeight, (int) state.getProperty (uiHeightId, defaultHeight));

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (w, h);

    dcShown = audioProcessor.getLevelMeter().getDcOffset() > LevelMeter::dcWarnLevel;
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
    masterTab.setToggleState   (index == 2, juce::dontSendNotification);

    spectrumView.setVisible (index == 0);
    phaseView.setVisible    (index == 1);
    masterView.setVisible   (index == 2);
}

void BarSpectrumAnalyzerAudioProcessorEditor::timerCallback()
{
    // Solo se redibuja la pestana visible
    if (currentTab == 0)      spectrumView.repaint();
    else if (currentTab == 1) phaseView.repaint();
    else                      masterView.repaint();

    // Aviso de DC offset en la cabecera (visible en cualquier pestana)
    const bool dc = audioProcessor.getLevelMeter().getDcOffset() > LevelMeter::dcWarnLevel;
    if (dc != dcShown)
    {
        dcShown = dc;
        repaint (dcArea);
    }
}

void BarSpectrumAnalyzerAudioProcessorEditor::resized()
{
    auto area   = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (12, 8);

    spectrumTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (6);
    phaseTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (6);
    masterTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (12);
    header.removeFromRight (200);                   // logo
    dcArea = header.withSizeKeepingCentre (juce::jmin (header.getWidth(), 210), header.getHeight());

    spectrumView.setBounds (area);
    phaseView.setBounds (area);
    masterView.setBounds (area);

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

    // Aviso de DC offset: la onda esta corrida del cero (suele venir de un
    // sample o de un plugin). Come headroom y hace "clics" al cortar.
    if (dcShown && dcArea.getWidth() > 60)
    {
        auto pill = dcArea.toFloat().reduced (0.0f, 2.0f);
        g.setColour (Palette::warn.withAlpha (0.16f));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (Palette::warn);
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawFittedText (dcArea.getWidth() > 170 ? "DC OFFSET - high-pass at ~20 Hz" : "DC OFFSET",
                          pill.toNearestInt(), juce::Justification::centred, 1);
    }
}
