/* ============================================================================
 *  AW Cascade - PluginProcessor.cpp
 *  Chain: Apex Limiter → Even Drive → Velvet Clip
 *  Original algorithms © airwindows (MIT license)
 * ============================================================================ */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AWCascadeProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Apex Limiter (Acceleration2)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "apexLimit",  "Apex Limit",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.32f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "apexDryWet", "Apex Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // Even Drive (Spiral2)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "evenInput",    "Drive Input",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "evenHighpass", "Drive Highpass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "evenPresence", "Drive Presence",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "evenOutput",   "Drive Output",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "evenDryWet",   "Drive Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return layout;
}

AWCascadeProcessor::AWCascadeProcessor()
    : AudioProcessor (BusesProperties()
                         .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    prepareToPlay (44100.0, 512);
}

AWCascadeProcessor::~AWCascadeProcessor() {}

//==============================================================================
void AWCascadeProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Apex Limiter
    std::memset (sL, 0, sizeof (sL));
    std::memset (sR, 0, sizeof (sR));
    m1L = m2L = m1R = m2R = 0.0;
    std::memset (biquadA, 0, sizeof (biquadA));
    std::memset (biquadB, 0, sizeof (biquadB));
    fpdL_acc = 1; while (fpdL_acc < 16386) fpdL_acc = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_acc = 1; while (fpdR_acc < 16386) fpdR_acc = (uint32_t)(rand() * (double)UINT32_MAX);

    // Even Drive
    iirSampleAL = iirSampleBL = prevSampleL_spi = 0.0;
    iirSampleAR = iirSampleBR = prevSampleR_spi = 0.0;
    flip_spi = true;
    fpdL_spi = 1; while (fpdL_spi < 16386) fpdL_spi = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_spi = 1; while (fpdR_spi < 16386) fpdR_spi = (uint32_t)(rand() * (double)UINT32_MAX);

    // Velvet Clip
    lastSampleL_cs = lastSampleR_cs = 0.0;
    std::memset (intermediateL, 0, sizeof (intermediateL));
    std::memset (intermediateR, 0, sizeof (intermediateR));
    fpdL_cs = 1; while (fpdL_cs < 16386) fpdL_cs = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_cs = 1; while (fpdR_cs < 16386) fpdR_cs = (uint32_t)(rand() * (double)UINT32_MAX);
}

void AWCascadeProcessor::releaseResources() {}

bool AWCascadeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

//==============================================================================
void AWCascadeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    float* inL = buffer.getWritePointer (0);
    float* inR = buffer.getWritePointer (1);

    const double sr           = getSampleRate();
    const double overallscale = sr / 44100.0;

    // ---- Apex Limiter per-block setup ----
    const double A    = (double)apvts.getRawParameterValue ("apexLimit") ->load();
    const double B    = (double)apvts.getRawParameterValue ("apexDryWet")->load();
    const double intensity = std::pow (A, 3.0) * 32.0;
    const double apexWet   = B;
    int spacing = (int)(1.73 * overallscale) + 1;
    if (spacing > 16) spacing = 16;

    biquadA[0] = (20000.0 * (1.0 - (A * 0.618033988749894848204586))) / sr;
    biquadB[0] = 20000.0 / sr;
    biquadA[1] = biquadB[1] = 0.7071;
    {
        double K    = std::tan (juce::MathConstants<double>::pi * biquadA[0]);
        double norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
        biquadA[2] = K*K*norm; biquadA[3] = 2.0*biquadA[2]; biquadA[4] = biquadA[2];
        biquadA[5] = 2.0*(K*K-1.0)*norm; biquadA[6] = (1.0-K/biquadA[1]+K*K)*norm;

        K    = std::tan (juce::MathConstants<double>::pi * biquadB[0]);
        norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
        biquadB[2] = K*K*norm; biquadB[3] = 2.0*biquadB[2]; biquadB[4] = biquadB[2];
        biquadB[5] = 2.0*(K*K-1.0)*norm; biquadB[6] = (1.0-K/biquadB[1]+K*K)*norm;
    }

    // ---- Even Drive per-block setup ----
    const double spiGain      = std::pow ((double)apvts.getRawParameterValue ("evenInput")   ->load() * 2.0, 2.0);
    const double spiIir       = std::pow ((double)apvts.getRawParameterValue ("evenHighpass")->load(), 3.0) / overallscale;
    const double spiPresence  = (double)apvts.getRawParameterValue ("evenPresence")->load();
    const double spiOutput    = (double)apvts.getRawParameterValue ("evenOutput")  ->load();
    const double spiWet       = (double)apvts.getRawParameterValue ("evenDryWet")  ->load();

    // ---- Velvet Clip per-block setup ----
    int csSpacing = (int)std::floor (overallscale);
    if (csSpacing < 1) csSpacing = 1; if (csSpacing > 16) csSpacing = 16;

    // ---- Per-sample loop ----
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleL = (double)inL[i];
        double sampleR = (double)inR[i];

        // ============================================================
        //  1. APEX LIMITER (Acceleration2)
        // ============================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_acc * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_acc * 1.18e-17;
        const double dryApexL = sampleL, dryApexR = sampleR;

        double tmp = (sampleL * biquadA[2]) + biquadA[7];
        biquadA[7]  = (sampleL * biquadA[3]) - (tmp * biquadA[5]) + biquadA[8];
        biquadA[8]  = (sampleL * biquadA[4]) - (tmp * biquadA[6]);
        const double smoothL = tmp;

        tmp         = (sampleR * biquadA[2]) + biquadA[9];
        biquadA[9]  = (sampleR * biquadA[3]) - (tmp * biquadA[5]) + biquadA[10];
        biquadA[10] = (sampleR * biquadA[4]) - (tmp * biquadA[6]);
        const double smoothR = tmp;

        for (int c = spacing*2; c >= 0; --c) { sL[c+1] = sL[c]; sR[c+1] = sR[c]; }
        sL[0] = sampleL; sR[0] = sampleR;

        m1L = (sL[0]-sL[spacing]) * std::fabs (sL[0]-sL[spacing]);
        m2L = (sL[spacing]-sL[spacing*2]) * std::fabs (sL[spacing]-sL[spacing*2]);
        double senseL = intensity * intensity * std::fabs (m1L - m2L);
        if (senseL > 1.0) senseL = 1.0;
        sampleL = (sampleL * (1.0 - senseL)) + (smoothL * senseL);

        m1R = (sR[0]-sR[spacing]) * std::fabs (sR[0]-sR[spacing]);
        m2R = (sR[spacing]-sR[spacing*2]) * std::fabs (sR[spacing]-sR[spacing*2]);
        double senseR = intensity * intensity * std::fabs (m1R - m2R);
        if (senseR > 1.0) senseR = 1.0;
        sampleR = (sampleR * (1.0 - senseR)) + (smoothR * senseR);

        tmp         = (sampleL * biquadB[2]) + biquadB[7];
        biquadB[7]  = (sampleL * biquadB[3]) - (tmp * biquadB[5]) + biquadB[8];
        biquadB[8]  = (sampleL * biquadB[4]) - (tmp * biquadB[6]);
        sampleL = tmp;
        tmp         = (sampleR * biquadB[2]) + biquadB[9];
        biquadB[9]  = (sampleR * biquadB[3]) - (tmp * biquadB[5]) + biquadB[10];
        biquadB[10] = (sampleR * biquadB[4]) - (tmp * biquadB[6]);
        sampleR = tmp;

        if (apexWet != 1.0) {
            sampleL = (sampleL * apexWet) + (dryApexL * (1.0 - apexWet));
            sampleR = (sampleR * apexWet) + (dryApexR * (1.0 - apexWet));
        }
        fpdL_acc ^= fpdL_acc << 13; fpdL_acc ^= fpdL_acc >> 17; fpdL_acc ^= fpdL_acc << 5;
        fpdR_acc ^= fpdR_acc << 13; fpdR_acc ^= fpdR_acc >> 17; fpdR_acc ^= fpdR_acc << 5;

        // ============================================================
        //  2. EVEN DRIVE (Spiral2)
        // ============================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_spi * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_spi * 1.18e-17;
        const double drySpiL = sampleL, drySpiR = sampleR;

        if (spiGain != 1.0) {
            sampleL *= spiGain; sampleR *= spiGain;
            prevSampleL_spi *= spiGain; prevSampleR_spi *= spiGain;
        }

        if (flip_spi) {
            iirSampleAL = (iirSampleAL * (1.0 - spiIir)) + (sampleL * spiIir);
            iirSampleAR = (iirSampleAR * (1.0 - spiIir)) + (sampleR * spiIir);
            sampleL -= iirSampleAL; sampleR -= iirSampleAR;
        } else {
            iirSampleBL = (iirSampleBL * (1.0 - spiIir)) + (sampleL * spiIir);
            iirSampleBR = (iirSampleBR * (1.0 - spiIir)) + (sampleR * spiIir);
            sampleL -= iirSampleBL; sampleR -= iirSampleBR;
        }

        double presL = std::sin (sampleL * std::fabs (prevSampleL_spi)) /
                       (prevSampleL_spi == 0.0 ? 1.0 : std::fabs (prevSampleL_spi));
        double presR = std::sin (sampleR * std::fabs (prevSampleR_spi)) /
                       (prevSampleR_spi == 0.0 ? 1.0 : std::fabs (prevSampleR_spi));

        sampleL = std::sin (sampleL * std::fabs (sampleL)) /
                  (std::fabs (sampleL) == 0.0 ? 1.0 : std::fabs (sampleL));
        sampleR = std::sin (sampleR * std::fabs (sampleR)) /
                  (std::fabs (sampleR) == 0.0 ? 1.0 : std::fabs (sampleR));

        if (spiOutput < 1.0) {
            sampleL *= spiOutput; sampleR *= spiOutput;
            presL   *= spiOutput; presR   *= spiOutput;
        }
        if (spiPresence > 0.0) {
            sampleL = (sampleL * (1.0 - spiPresence)) + (presL * spiPresence);
            sampleR = (sampleR * (1.0 - spiPresence)) + (presR * spiPresence);
        }
        if (spiWet < 1.0) {
            sampleL = (drySpiL * (1.0 - spiWet)) + (sampleL * spiWet);
            sampleR = (drySpiR * (1.0 - spiWet)) + (sampleR * spiWet);
        }

        prevSampleL_spi = drySpiL;
        prevSampleR_spi = drySpiR;
        flip_spi = !flip_spi;

        fpdL_spi ^= fpdL_spi << 13; fpdL_spi ^= fpdL_spi >> 17; fpdL_spi ^= fpdL_spi << 5;
        fpdR_spi ^= fpdR_spi << 13; fpdR_spi ^= fpdR_spi >> 17; fpdR_spi ^= fpdR_spi << 5;

        // ============================================================
        //  3. VELVET CLIP (ClipSoftly)
        // ============================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_cs * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_cs * 1.18e-17;

        {
            double ss = std::fabs (sampleL);
            if (ss < 1.0) ss = 1.0; else ss = 1.0 / ss;
            if (sampleL >  1.57079633) sampleL =  1.57079633;
            if (sampleL < -1.57079633) sampleL = -1.57079633;
            sampleL = std::sin (sampleL) * 0.9549925859;
            sampleL = (sampleL * ss) + (lastSampleL_cs * (1.0 - ss));
        }
        {
            double ss = std::fabs (sampleR);
            if (ss < 1.0) ss = 1.0; else ss = 1.0 / ss;
            if (sampleR >  1.57079633) sampleR =  1.57079633;
            if (sampleR < -1.57079633) sampleR = -1.57079633;
            sampleR = std::sin (sampleR) * 0.9549925859;
            sampleR = (sampleR * ss) + (lastSampleR_cs * (1.0 - ss));
        }

        intermediateL[csSpacing] = sampleL;
        sampleL = lastSampleL_cs;
        for (int x = csSpacing; x > 0; --x) intermediateL[x-1] = intermediateL[x];
        lastSampleL_cs = intermediateL[0];

        intermediateR[csSpacing] = sampleR;
        sampleR = lastSampleR_cs;
        for (int x = csSpacing; x > 0; --x) intermediateR[x-1] = intermediateR[x];
        lastSampleR_cs = intermediateR[0];

        fpdL_cs ^= fpdL_cs << 13; fpdL_cs ^= fpdL_cs >> 17; fpdL_cs ^= fpdL_cs << 5;
        fpdR_cs ^= fpdR_cs << 13; fpdR_cs ^= fpdR_cs >> 17; fpdR_cs ^= fpdR_cs << 5;

        inL[i] = (float)sampleL;
        inR[i] = (float)sampleR;
    }
}

//==============================================================================
juce::AudioProcessorEditor* AWCascadeProcessor::createEditor()
{
    return new AWCascadeEditor (*this);
}

void AWCascadeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AWCascadeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AWCascadeProcessor();
}
