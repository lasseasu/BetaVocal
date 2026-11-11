#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <atomic>

//==============================================================================
// Apuluokka: Tarkka Biquad-suodin (EQ-kaistoja ja sivuketjuja varten)
struct BiquadFilter
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    float processSample (float x) noexcept
    {
        const double out = b0 * (double) x + z1;
        z1 = b1 * (double) x - a1 * out + z2;
        z2 = b2 * (double) x - a2 * out;
        return (float) out;
    }

    void setHighPass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        b0 = ((1.0 + cosw) * 0.5) / a0;
        b1 = -(1.0 + cosw) / a0;
        b2 = ((1.0 + cosw) * 0.5) / a0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setBandPass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        b0 = alpha / a0;
        b1 = 0.0;
        b2 = -alpha / a0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setPeaking (double sampleRate, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha / A;

        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * cosw) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    void setHighShelf (double sampleRate, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cosw + twoSqrtAAlpha;

        b0 = (A * ((A + 1.0) + (A - 1.0) * cosw + twoSqrtAAlpha)) / a0;
        b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw)) / a0;
        b2 = (A * ((A + 1.0) + (A - 1.0) * cosw - twoSqrtAAlpha)) / a0;
        a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw)) / a0;
        a2 = ((A + 1.0) - (A - 1.0) * cosw - twoSqrtAAlpha) / a0;
    }
};

//==============================================================================
// Apuluokka: Verhokäyrän seuraaja (Envelope Follower) dynamiikkaa varten
struct EnvelopeDetector
{
    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double env = 0.0;

    void prepare (double sampleRate, double attackMs, double releaseMs) noexcept
    {
        attackCoeff  = std::exp (-1.0 / (0.001 * attackMs * sampleRate));
        releaseCoeff = std::exp (-1.0 / (0.001 * releaseMs * sampleRate));
        env = 0.0;
    }

    void reset() noexcept { env = 0.0; }

    double processSample (float input) noexcept
    {
        const double rect = std::abs ((double) input);
        if (rect > env)
            env = attackCoeff * env + (1.0 - attackCoeff) * rect;
        else
            env = releaseCoeff * env + (1.0 - releaseCoeff) * rect;

        return env;
    }
};

//==============================================================================
class BetaVocalAudioProcessor  : public juce::AudioProcessor
{
public:
    BetaVocalAudioProcessor();
    ~BetaVocalAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS-parametripuu
    juce::AudioProcessorValueTreeState apvts;

    // Mittariarvot käyttöliittymälle (dB vaimennus)
    std::atomic<float> dynEqReductionDb { 0.0f };
    std::atomic<float> deEssReductionDb { 0.0f };
    std::atomic<float> compReductionDb  { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateStaticFilters (float mudNormalized);

    double currentSampleRate = 48000.0;

    // 1. Kiinteä Beta 58A Pre-EQ (2x HPF = 24 dB/oct @ 80Hz, 4.8kHz notch, 10kHz air)
    BiquadFilter hpf1, hpf2;
    BiquadFilter betaPresenceDip;
    BiquadFilter betaAirShelf;

    // 2. Dynaaminen EQ (200 Hz läheisyysilmiö & 3 kHz riipivyys)
    BiquadFilter dynLowScFilter, dynLowBell;
    EnvelopeDetector dynLowEnv;

    BiquadFilter dynHighScFilter, dynHighBell;
    EnvelopeDetector dynHighEnv;

    // 3. De-Esser (6.8 kHz dynaaminen sibilanssileikkuri)
    BiquadFilter deEssScFilter, deEssBell;
    EnvelopeDetector deEssEnv;

    // 4. Mud Control (Säädettävä 315 Hz leikkuri)
    BiquadFilter mudBell;
    float lastMudValue = -1.0f;

    // 5. CLA-tyylinen pääkompressori
    EnvelopeDetector compEnv;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BetaVocalAudioProcessor)
};
