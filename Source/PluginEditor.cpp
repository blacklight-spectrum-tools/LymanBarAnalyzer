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
      listenButton (p.apvts),
      spectrumView (p),
      phaseView (p),
      masterView (p)
{
    setLookAndFeel (&lookAndFeel);
    ++audioProcessor.editorsOpen;

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

    settingsButton.setTooltip ("Settings: curve speed, vertical range, peak hold");
    settingsButton.onClick = [this] { spectrumView.showSettingsMenu (settingsButton); };
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (listenButton);

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
    --audioProcessor.editorsOpen;
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

    // Boton de escucha (puede prenderlo un pad MIDI) y su aviso grande
    listenButton.refresh();
    if (listenButton.isOn() != listenShown)
    {
        listenShown = listenButton.isOn();
        repaint();
    }

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

    // Pestanas un poco mas angostas en ventanas chicas, para que quepan el
    // engrane y el aviso de DC
    const int tabW = getWidth() < 720 ? 96 : 110;
    spectrumTab.setBounds (header.removeFromLeft (tabW));
    header.removeFromLeft (6);
    phaseTab.setBounds (header.removeFromLeft (tabW));
    header.removeFromLeft (6);
    masterTab.setBounds (header.removeFromLeft (tabW));
    header.removeFromLeft (12);
    compactLogo = getWidth() < 760;
    header.removeFromRight (compactLogo ? 60 : 200); // logo
    settingsButton.setBounds (header.removeFromRight (28).withSizeKeepingCentre (28, 28));
    header.removeFromRight (6);
    listenButton.setBounds (header.removeFromRight (60).withSizeKeepingCentre (60, 26));
    header.removeFromRight (10);
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
    if (compactLogo)
    {
        Logo::drawMark (g, header.removeFromRight (48.0f).withSizeKeepingCentre (48.0f, 18.0f), Palette::barOver);
    }
    auto text   = header.removeFromRight (128.0f);
    header.removeFromRight (10.0f);
    if (! compactLogo)
        Logo::drawMark (g, header.removeFromRight (48.0f).withSizeKeepingCentre (48.0f, 18.0f), Palette::barOver);

    if (! compactLogo)
    {
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.14f));
        g.drawText ("BLACKLIGHT", text.removeFromTop (17.0f), juce::Justification::bottomLeft);

        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (9.0f).withKerningFactor (0.28f));
        g.drawText ("SPECTRUM TOOLS", text, juce::Justification::topLeft);
    }

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

void BarSpectrumAnalyzerAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // Aviso grande mientras se escucha en Mono / Side: es lo unico que cambia
    // el sonido, tiene que ser imposible no verlo.
    if (! listenButton.isOn())
        return;

    const juce::String text = listenButton.isSideMode() ? "SIDE ONLY - listening only, turns off on export"
                                                        : "MONO CHECK - listening only, turns off on export";
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const float w = juce::jmin ((float) getWidth() - 40.0f, 380.0f);
    const auto pill = juce::Rectangle<float> (((float) getWidth() - w) * 0.5f, (float) getHeight() - 30.0f, w, 22.0f);
    g.setColour (Palette::warn);
    g.fillRoundedRectangle (pill, 11.0f);
    g.setColour (Palette::background);
    g.drawFittedText (text, pill.toNearestInt(), juce::Justification::centred, 1);
}
