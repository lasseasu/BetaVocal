#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BetaVocalAudioProcessor::BetaVocalAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                      .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

BetaVocalAudioProcessor::~BetaVocalAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BetaVocalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Yhteensopiva sekä vanhempien (JUCE 5/6) että uudempien (JUCE 7/8) versioiden kanssa
   #if JUCE_MAJOR_VERSION >= 7
    auto makeId = [] (const char* id) { return juce::ParameterID { id, 1 }; };
   #else
    auto makeId = [] (const char* id) { return juce::String (id); };
   #endif

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        makeId ("mud"),
        "Mud Cut",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        makeId ("deess"),
        "De-Esser",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        40.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        makeId ("comp"),
        "Compress",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        55.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        makeId ("output"),
        "Output",
        juce::NormalisableRange<float> (-18.0f, 12.0f, 0.1f),
        0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String BetaVocalAudioProcessor::getName() const { return JucePlugin_Name; }
bool BetaVocalAudioProcessor::acceptsMidi() const { return false; }
bool BetaVocalAudioProcessor::producesMidi() const { return false; }
bool BetaVocalAudioProcessor::isMidiEffect() const { return false; }
double BetaVocalAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int BetaVocalAudioProcessor::getNumPrograms() { return 1; }
int BetaVocalAudioProcessor::getCurrentProgram() { return 0; }
void BetaVocalAudioProcessor::setCurrentProgram (int) {}
const juce::String BetaVocalAudioProcessor::getProgramName (int) { return {}; }
void BetaVocalAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void BetaVocalAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;

    hpf1.reset();
    hpf2.reset();
    betaPresenceDip.reset();
    betaAirShelf.reset();

    dynLowScFilter.reset();
    dynLowBell.reset();
    dynHighScFilter.reset();
    dynHighBell.reset();

    deEssScFilter.reset();
    deEssBell.reset();
    mudBell.reset();

    // 1. Beta 58A kiinteä esikorjaus
    hpf1.setHighPass (currentSampleRate, 80.0, 0.541196);
    hpf2.setHighPass (currentSampleRate, 80.0, 1.306563);
    betaPresenceDip.setPeaking (currentSampleRate, 4800.0, 1.2, -1.8);
    betaAirShelf.setHighShelf (currentSampleRate, 10000.0, 0.707, 2.0);

    // 2. Dynaamisen EQ:n sivuketjusuotimet ja alkutila
    dynLowScFilter.setBandPass (currentSampleRate, 200.0, 1.5);
    dynLowBell.setPeaking (currentSampleRate, 200.0, 1.2, 0.0);
    dynLowEnv.prepare (currentSampleRate, 12.0, 120.0);

    dynHighScFilter.setBandPass (currentSampleRate, 3000.0, 2.0);
    dynHighBell.setPeaking (currentSampleRate, 3000.0, 1.8, 0.0);
    dynHighEnv.prepare (currentSampleRate, 3.0, 65.0);

    // 3. De-Esserin sivuketju ja alkutila (6.8 kHz)
    deEssScFilter.setBandPass (currentSampleRate, 6800.0, 2.2);
    deEssBell.setPeaking (currentSampleRate, 6800.0, 1.6, 0.0);
    deEssEnv.prepare (currentSampleRate, 1.0, 45.0);

    // 4. Pääkompressorin verhokäyrä
    compEnv.prepare (currentSampleRate, 2.5, 180.0);

    lastMudValue = -1.0f;
    const float initialMud = *apvts.getRawParameterValue ("mud") / 100.0f;
    updateStaticFilters (initialMud);
}

void BetaVocalAudioProcessor::releaseResources()
{
}

void BetaVocalAudioProcessor::updateStaticFilters (float mudNormalized)
{
    if (std::abs (mudNormalized - lastMudValue) > 0.0005f)
    {
        lastMudValue = mudNormalized;
        const double mudGainDb = -15.0 * (double) mudNormalized;
        const double mudQ      = 0.9 + 0.9 * (double) mudNormalized;
        mudBell.setPeaking (currentSampleRate, 315.0, mudQ, mudGainDb);
    }
}

bool BetaVocalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return (mainIn == juce::AudioChannelSet::mono()
         || mainIn == juce::AudioChannelSet::stereo());
}

//==============================================================================
void BetaVocalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (totalNumInputChannels == 0 || numSamples == 0)
        return;

    const float mudNorm    = *apvts.getRawParameterValue ("mud") / 100.0f;
    const float deEssNorm  = *apvts.getRawParameterValue ("deess") / 100.0f;
    const float compNorm   = *apvts.getRawParameterValue ("comp") / 100.0f;
    const float outputDb   = *apvts.getRawParameterValue ("output");
    const float outputGain = juce::Decibels::decibelsToGain (outputDb);

    updateStaticFilters (mudNorm);

    const double compThreshDb = -6.0 - 26.0 * (double) compNorm;
    const double compRatio    = 1.0 + 4.0 * (double) compNorm;
    const double makeupDb     = 12.5 * (double) compNorm;
    const float  makeupGain   = (float) juce::Decibels::decibelsToGain ((float) makeupDb);
    const float  satAmount    = 0.15f * compNorm;

    const double deEssThreshDb = -10.0 - 26.0 * (double) deEssNorm;
    const double deEssMaxCutDb = -12.0 * (double) deEssNorm;

    float* channelData = buffer.getWritePointer (0);

    float maxDynEqCut = 0.0f;
    float maxDeEssCut = 0.0f;
    float maxCompCut  = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float x = channelData[sample];

        // --- 1. BETA 58A KIINTEÄ PRE-EQ ---
        x = hpf1.processSample (x);
        x = hpf2.processSample (x);
        x = betaPresenceDip.processSample (x);
        x = betaAirShelf.processSample (x);

        // --- 2. ESISÄÄDETTY DYNAAMINEN EQ (200 Hz & 3 kHz) ---
        const float scLow  = dynLowScFilter.processSample (x);
        const float scHigh = dynHighScFilter.processSample (x);

        const double envLowDb  = juce::Decibels::gainToDecibels ((float) dynLowEnv.processSample (scLow), -96.0f);
        const double envHighDb = juce::Decibels::gainToDecibels ((float) dynHighEnv.processSample (scHigh), -96.0f);

        double lowCutDb = 0.0;
        if (envLowDb > -26.0)
            lowCutDb = juce::jlimit (-6.0, 0.0, -((envLowDb - (-26.0)) * 0.5));

        double highCutDb = 0.0;
        if (envHighDb > -24.0)
            highCutDb = juce::jlimit (-4.5, 0.0, -((envHighDb - (-24.0)) * 0.45));

        if ((sample & 15) == 0)
        {
            dynLowBell.setPeaking (currentSampleRate, 200.0, 1.2, lowCutDb);
            dynHighBell.setPeaking (currentSampleRate, 3000.0, 1.8, highCutDb);
        }

        x = dynLowBell.processSample (x);
        x = dynHighBell.processSample (x);

        maxDynEqCut = juce::jmax (maxDynEqCut, (float) std::abs (lowCutDb + highCutDb));

        // --- 3. DE-ESSER ---
        const float scDeEss = deEssScFilter.processSample (x);
        const double envDeEssDb = juce::Decibels::gainToDecibels ((float) deEssEnv.processSample (scDeEss), -96.0f);

        double deEssCutDb = 0.0;
        if (deEssNorm > 0.001f && envDeEssDb > deEssThreshDb)
        {
            deEssCutDb = juce::jlimit (deEssMaxCutDb, 0.0, -((envDeEssDb - deEssThreshDb) * 0.85));
        }

        if ((sample & 15) == 0)
            deEssBell.setPeaking (currentSampleRate, 6800.0, 1.6, deEssCutDb);

        x = deEssBell.processSample (x);
        maxDeEssCut = juce::jmax (maxDeEssCut, (float) std::abs (deEssCutDb));

        // --- 4. MUD CONTROL (315 Hz) ---
        x = mudBell.processSample (x);

        // --- 5. KOMPRESSORI + SATURAATIO ---
        const double envCompDb = juce::Decibels::gainToDecibels ((float) compEnv.processSample (x), -96.0f);
        double compGainRedDb = 0.0;

        if (compNorm > 0.001f && envCompDb > compThreshDb)
        {
            const double overDb = envCompDb - compThreshDb;
            compGainRedDb = overDb * (1.0 - (1.0 / compRatio));
        }

        const float compGainLin = juce::Decibels::decibelsToGain ((float) -compGainRedDb);
        x = x * compGainLin * makeupGain;

        if (satAmount > 0.001f)
        {
            const float drive = 1.0f + satAmount * 1.5f;
            x = std::tanh (x * drive) / std::tanh (drive);
        }

        maxCompCut = juce::jmax (maxCompCut, (float) compGainRedDb);

        // --- 6. MASTER OUTPUT & LIMITTERI ---
        x *= outputGain;
        x = juce::jlimit (-0.99f, 0.99f, x);

        channelData[sample] = x;
    }

    for (int ch = 1; ch < totalNumOutputChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    dynEqReductionDb.store (maxDynEqCut);
    deEssReductionDb.store (maxDeEssCut);
    compReductionDb.store  (maxCompCut);
}

//==============================================================================
bool BetaVocalAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* BetaVocalAudioProcessor::createEditor()
{
    return new BetaVocalAudioProcessorEditor (*this);
}

//==============================================================================
void BetaVocalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BetaVocalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BetaVocalAudioProcessor();
}
