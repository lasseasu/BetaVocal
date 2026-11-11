#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BetaVocalAudioProcessorEditor::BetaVocalAudioProcessorEditor (BetaVocalAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Kosketusnäyttöystävällinen värikoodaus SuperRackia varten
    setupSlider (mudSlider,    mudLabel,    "MUD CUT",  juce::Colour (0xffe67e22), " %");
    setupSlider (deEssSlider,  deEssLabel,  "DE-ESS",   juce::Colour (0xff00bcd4), " %");
    setupSlider (compSlider,   compLabel,   "COMPRESS", juce::Colour (0xff9b59b6), " %");
    setupSlider (outputSlider, outputLabel, "OUTPUT",   juce::Colour (0xff2ecc71), " dB");

    mudAttachment    = std::make_unique<SliderAttachment> (audioProcessor.apvts, "mud",    mudSlider);
    deEssAttachment  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "deess",  deEssSlider);
    compAttachment   = std::make_unique<SliderAttachment> (audioProcessor.apvts, "comp",   compSlider);
    outputAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "output", outputSlider);

    setSize (520, 420);
    startTimerHz (30); // Päivittää vaimennusmittarit 30 kertaa sekunnissa
}

BetaVocalAudioProcessorEditor::~BetaVocalAudioProcessorEditor()
{
    stopTimer();
}

void BetaVocalAudioProcessorEditor::setupSlider (juce::Slider& slider,
                                                 juce::Label& label,
                                                 const juce::String& name,
                                                 const juce::Colour& colour,
                                                 const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 26);
    slider.setColour (juce::Slider::thumbColourId, colour);
    slider.setColour (juce::Slider::trackColourId, colour.withAlpha (0.35f));
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff16191e));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (label);
}

//==============================================================================
void BetaVocalAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Tumma keikkaystävällinen tausta
    g.fillAll (juce::Colour (0xff1e2229));

    // Yläpalkki
    g.setColour (juce::Colour (0xff14171c));
    g.fillRect (0, 0, getWidth(), 54);

    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("BETA 58A VOCAL STRIP", 20, 8, 300, 24, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff8a94a6));
    g.setFont (12.5f);
    g.drawText ("Pre-EQ (80Hz HPF + 4.8k Dip + Air)  |  Auto Dyn-EQ (200Hz & 3kHz)",
                20, 30, 480, 18, juce::Justification::centredLeft);

    // Reaaliaikaiset Gain Reduction (GR) -mittarit yläoikealla
    const float dynGr   = audioProcessor.dynEqReductionDb.load();
    const float deEssGr = audioProcessor.deEssReductionDb.load();
    const float compGr  = audioProcessor.compReductionDb.load();

    auto drawMeter = [&] (int x, const juce::String& title, float grDb, juce::Colour col)
    {
        g.setColour (juce::Colour (0xff2a2f3a));
        g.fillRoundedRectangle ((float) x, 10.0f, 55.0f, 34.0f, 4.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, grDb / 12.0f);
        g.setColour (col);
        g.fillRoundedRectangle ((float) x + 4.0f, 34.0f, 47.0f * norm, 6.0f, 2.0f);

        g.setColour (juce::Colours::white);
        g.setFont (10.5f);
        g.drawText (title, x, 12, 55, 12, juce::Justification::centred);
        g.drawText ("-" + juce::String (grDb, 1) + "dB", x, 22, 55, 12, juce::Justification::centred);
    };

    drawMeter (330, "DYN EQ", dynGr,   juce::Colour (0xfff1c40f));
    drawMeter (392, "DE-ESS", deEssGr, juce::Colour (0xff00bcd4));
    drawMeter (454, "COMP",   compGr,  juce::Colour (0xff9b59b6));

    // Kehykset liukusäätimille
    const int margin = 20;
    const int stripW = (getWidth() - margin * 2) / 4;
    for (int i = 0; i < 4; ++i)
    {
        const int x = margin + i * stripW + 6;
        g.setColour (juce::Colour (0xff262b34));
        g.fillRoundedRectangle ((float) x, 68.0f, (float) (stripW - 12), (float) (getHeight() - 86), 8.0f);
    }
}

void BetaVocalAudioProcessorEditor::resized()
{
    const int margin = 20;
    const int stripW = (getWidth() - margin * 2) / 4;
    const int topY   = 76;
    const int labelH = 26;
    const int sliderH = getHeight() - topY - labelH - 26;

    juce::Label*  labels[4]  = { &mudLabel, &deEssLabel, &compLabel, &outputLabel };
    juce::Slider* sliders[4] = { &mudSlider, &deEssSlider, &compSlider, &outputSlider };

    for (int i = 0; i < 4; ++i)
    {
        const int x = margin + i * stripW;
        labels[i]->setBounds (x, topY, stripW, labelH);
        sliders[i]->setBounds (x, topY + labelH, stripW, sliderH);
    }
}

void BetaVocalAudioProcessorEditor::timerCallback()
{
    repaint (325, 8, 190, 42); // Päivittää vain yläpalkin mittarialueen
}
