#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class BetaVocalAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    BetaVocalAudioProcessorEditor (BetaVocalAudioProcessor&);
    ~BetaVocalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::Colour& colour, const juce::String& suffix);

    BetaVocalAudioProcessor& audioProcessor;

    juce::Slider mudSlider;
    juce::Slider deEssSlider;
    juce::Slider compSlider;
    juce::Slider outputSlider;

    juce::Label mudLabel;
    juce::Label deEssLabel;
    juce::Label compLabel;
    juce::Label outputLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> mudAttachment;
    std::unique_ptr<SliderAttachment> deEssAttachment;
    std::unique_ptr<SliderAttachment> compAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BetaVocalAudioProcessorEditor)
};
