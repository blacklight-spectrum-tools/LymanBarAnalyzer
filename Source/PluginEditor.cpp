#include "PluginEditor.h"

BarSpectrumAnalyzerAudioProcessorEditor::BarSpectrumAnalyzerAudioProcessorEditor (BarSpectrumAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    referenceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    referenceSlider.setRange (minDb, maxDb, 0.1);
    referenceSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible (referenceSlider);

    referenceLabel.setText ("Referencia de compresion (dB)", juce::dontSendNotification);
    referenceLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (referenceLabel);

    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "referenceDb", referenceSlider);

    setSize (760, 420);
    startTimerHz (30);
}

BarSpectrumAnalyzerAudioProcessorEditor::~BarSpectrumAnalyzerAudioProcessorEditor()
{
    stopTimer();
}

juce::String BarSpectrumAnalyzerAudioProcessorEditor::frequencyLabel (float hz) const
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 1) + "k";
    return juce::String ((int) hz);
}

void BarSpectrumAnalyzerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top  = area.removeFromTop (30);
    referenceLabel.setBounds (top.removeFromLeft (220));
    referenceSlider.setBounds (top);
}

void BarSpectrumAnalyzerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto bounds = getLocalBounds().reduced (10);
    bounds.removeFromTop (30); // espacio reservado al slider de referencia

    const int   numBars = BarSpectrumAnalyzerAudioProcessor::numBars;
    const float barGap  = 3.0f;
    const float barWidth = ((float) bounds.getWidth() - barGap * (numBars - 1)) / (float) numBars;

    const float refDb = audioProcessor.apvts.getRawParameterValue ("referenceDb")->load();

    // Rejilla de dB cada 12 dB
    g.setColour (juce::Colours::grey.withAlpha (0.3f));
    for (int db = 0; db >= (int) minDb; db -= 12)
    {
        float y = juce::jmap ((float) db, minDb, maxDb,
                               (float) bounds.getBottom(), (float) bounds.getY());
        g.drawHorizontalLine ((int) y, (float) bounds.getX(), (float) bounds.getRight());
        g.drawText (juce::String (db), bounds.getX(), (int) y - 14, 30, 14,
                    juce::Justification::left);
    }

    // Barras de nivel por banda de frecuencia
    for (int i = 0; i < numBars; ++i)
    {
        float db    = audioProcessor.getBarLevelDb (i);
        float level = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));

        float x = (float) bounds.getX() + i * (barWidth + barGap);
        float barHeight = level * (float) bounds.getHeight();
        float y = (float) bounds.getBottom() - barHeight;

        bool overReference = db > refDb;
        juce::Colour colour = overReference ? juce::Colours::orangered
                                             : juce::Colours::mediumseagreen;

        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, y, barWidth, barHeight));

        if (i % 3 == 0)
        {
            float loF = audioProcessor.getBandFrequency (i);
            g.setColour (juce::Colours::lightgrey);
            g.setFont (10.0f);
            g.drawText (frequencyLabel (loF), (int) x, bounds.getBottom() + 2,
                        (int) (barWidth * 3), 14, juce::Justification::left);
        }
    }

    // Linea horizontal de referencia (umbral objetivo de compresion)
    float refY = juce::jmap (refDb, minDb, maxDb,
                              (float) bounds.getBottom(), (float) bounds.getY());
    g.setColour (juce::Colours::yellow);
    g.drawHorizontalLine ((int) refY, (float) bounds.getX(), (float) bounds.getRight());
}
