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
    tooltipWindow.setLookAndFeel (&lookAndFeel);
    themeVersion = Palette::version();
    ++audioProcessor.editorsOpen;

    // Pestanas: botones que se comportan como grupo de radio
    int index = 0;
    for (auto* tab : { &spectrumTab, &phaseTab, &masterTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1001);
        ThemeStyle::segment (*tab);
        tab->onClick = [this, index] { showTab (index); };
        addAndMakeVisible (tab);
        ++index;
    }

    settingsButton.setTooltip ("Settings: curve speed, range, peak hold, listen mode, colours");
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
    listenShown = listenButton.isOn();
    masterTab.setButtonText (masterView.showsMaster() ? "Master" : "Loudness");
    startTimerHz (30);
}

BarSpectrumAnalyzerAudioProcessorEditor::~BarSpectrumAnalyzerAudioProcessorEditor()
{
    stopTimer();
    --audioProcessor.editorsOpen;
    tooltipWindow.setLookAndFeel (nullptr);
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
    // Cambio de tema (desde esta ventana o desde otra instancia de Lyman)
    if (themeVersion != Palette::version())
    {
        themeVersion = Palette::version();
        lookAndFeel.refreshColours();
        ThemeStyle::refreshTree (*this);
        sendLookAndFeelChange();
        repaint();
    }

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

    // En que pista esta (el DAW puede mandar o cambiar el nombre en cualquier
    // momento). En un canal, la pestana Master se llama "Loudness".
    if (++roleTicks >= 10)
    {
        roleTicks = 0;
        masterView.refreshRole();
        masterTab.setButtonText (masterView.showsMaster() ? "Master" : "Loudness");
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

    // Mientras se escucha en Mono / Side, el texto del logo se vuelve el aviso
    // (misma tipografia, en amarillo): no tapa nada de la grafica.
    if (! compactLogo)
    {
        const bool listening = listenShown;
        g.setColour (listening ? Palette::warn : Palette::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.14f));
        g.drawText (listening ? (listenButton.isSideMode() ? "SIDE ONLY" : "MONO CHECK") : "BLACKLIGHT",
                    text.removeFromTop (17.0f), juce::Justification::bottomLeft);

        g.setColour (listening ? Palette::warn.withAlpha (0.8f) : Palette::textDim);
        g.setFont (juce::FontOptions (9.0f).withKerningFactor (0.28f));
        g.drawText (listening ? "OFF ON EXPORT" : "SPECTRUM TOOLS", text, juce::Justification::topLeft);
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
    // Mientras se escucha en Mono / Side (lo unico que cambia el sonido), la
    // ventana entera lleva un marco amarillo: imposible no verlo, y no tapa
    // ninguna lectura. El texto del aviso va en el lugar del logo.
    if (! listenButton.isOn())
        return;

    g.setColour (Palette::warn);
    g.drawRect (getLocalBounds(), 2);
}
