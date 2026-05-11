/* ============================================================================
 *  AW Cascade - PluginProcessor.cpp
 *  Airwindows DSP ported to JUCE VST3
 *  Chain: Acceleration2 → Spiral → ClipSoftly
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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "accelLimit", "Acceleration Limit",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.32f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "accelDryWet", "Acceleration Dry/Wet",
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
    // Acceleration2
    std::memset (sL, 0, sizeof (sL));
    std::memset (sR, 0, sizeof (sR));
    m1L = m2L = m1R = m2R = 0.0;
    std::memset (biquadA, 0, sizeof (biquadA));
    std::memset (biquadB, 0, sizeof (biquadB));
    fpdL_acc = 1; while (fpdL_acc < 16386) fpdL_acc = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_acc = 1; while (fpdR_acc < 16386) fpdR_acc = (uint32_t)(rand() * (double)UINT32_MAX);

    // Spiral
    fpdL_spi = 1; while (fpdL_spi < 16386) fpdL_spi = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_spi = 1; while (fpdR_spi < 16386) fpdR_spi = (uint32_t)(rand() * (double)UINT32_MAX);

    // ClipSoftly
    lastSampleL_cs = lastSampleR_cs = 0.0;
    std::memset (intermediateL, 0, sizeof (intermediateL));
    std::memset (intermediateR, 0, sizeof (intermediateR));
    fpdL_cs = 1; while (fpdL_cs < 16386) fpdL_cs = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_cs = 1; while (fpdR_cs < 16386) fpdR_cs = (uint32_t)(rand() * (double)UINT32_MAX);
}

void AWCascadeProcessor::releaseResources() {}

bool AWCascadeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
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

    const double sr = getSampleRate();
    const double overallscale = sr / 44100.0;

    // ---- Acceleration2 per-block setup ----
    const double A = (double) apvts.getRawParameterValue ("accelLimit")->load();
    const double B = (double) apvts.getRawParameterValue ("accelDryWet")->load();

    const double intensity = std::pow (A, 3.0) * 32.0;
    const double wet = B;

    int spacing = (int)(1.73 * overallscale) + 1;
    if (spacing > 16) spacing = 16;

    biquadA[0] = (20000.0 * (1.0 - (A * 0.618033988749894848204586))) / sr;
    biquadB[0] = 20000.0 / sr;
    biquadA[1] = biquadB[1] = 0.7071;

    {
        double K    = std::tan (juce::MathConstants<double>::pi * biquadA[0]);
        double norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
        biquadA[2]  = K * K * norm;
        biquadA[3]  = 2.0 * biquadA[2];
        biquadA[4]  = biquadA[2];
        biquadA[5]  = 2.0 * (K * K - 1.0) * norm;
        biquadA[6]  = (1.0 - K / biquadA[1] + K * K) * norm;

        K    = std::tan (juce::MathConstants<double>::pi * biquadB[0]);
        norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
        biquadB[2]  = K * K * norm;
        biquadB[3]  = 2.0 * biquadB[2];
        biquadB[4]  = biquadB[2];
        biquadB[5]  = 2.0 * (K * K - 1.0) * norm;
        biquadB[6]  = (1.0 - K / biquadB[1] + K * K) * norm;
    }

    // ---- ClipSoftly per-block setup ----
    int csSpacing = (int)std::floor (overallscale);
    if (csSpacing < 1)  csSpacing = 1;
    if (csSpacing > 16) csSpacing = 16;

    // ---- Per-sample loop ----
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleL = (double)inL[i];
        double sampleR = (double)inR[i];

        // ================================================================
        //  1. Acceleration2
        // ================================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_acc * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_acc * 1.18e-17;

        const double dryL = sampleL;
        const double dryR = sampleR;

        // Biquad A — pre-smoothing filter
        double tmp = (sampleL * biquadA[2]) + biquadA[7];
        biquadA[7] = (sampleL * biquadA[3]) - (tmp * biquadA[5]) + biquadA[8];
        biquadA[8] = (sampleL * biquadA[4]) - (tmp * biquadA[6]);
        const double smoothL = tmp;

        tmp        = (sampleR * biquadA[2]) + biquadA[9];
        biquadA[9] = (sampleR * biquadA[3]) - (tmp * biquadA[5]) + biquadA[10];
        biquadA[10]= (sampleR * biquadA[4]) - (tmp * biquadA[6]);
        const double smoothR = tmp;

        // Shift sample history
        for (int c = spacing * 2; c >= 0; --c) { sL[c+1] = sL[c]; sR[c+1] = sR[c]; }
        sL[0] = sampleL;
        sR[0] = sampleR;

        // Detect acceleration (change-of-change) and blend
        m1L = (sL[0] - sL[spacing])         * std::fabs (sL[0] - sL[spacing]);
        m2L = (sL[spacing] - sL[spacing*2]) * std::fabs (sL[spacing] - sL[spacing*2]);
        double senseL = intensity * intensity * std::fabs (m1L - m2L);
        if (senseL > 1.0) senseL = 1.0;
        sampleL = (sampleL * (1.0 - senseL)) + (smoothL * senseL);

        m1R = (sR[0] - sR[spacing])         * std::fabs (sR[0] - sR[spacing]);
        m2R = (sR[spacing] - sR[spacing*2]) * std::fabs (sR[spacing] - sR[spacing*2]);
        double senseR = intensity * intensity * std::fabs (m1R - m2R);
        if (senseR > 1.0) senseR = 1.0;
        sampleR = (sampleR * (1.0 - senseR)) + (smoothR * senseR);

        // Biquad B — post-smoothing filter
        tmp        = (sampleL * biquadB[2]) + biquadB[7];
        biquadB[7] = (sampleL * biquadB[3]) - (tmp * biquadB[5]) + biquadB[8];
        biquadB[8] = (sampleL * biquadB[4]) - (tmp * biquadB[6]);
        sampleL    = tmp;

        tmp         = (sampleR * biquadB[2]) + biquadB[9];
        biquadB[9]  = (sampleR * biquadB[3]) - (tmp * biquadB[5]) + biquadB[10];
        biquadB[10] = (sampleR * biquadB[4]) - (tmp * biquadB[6]);
        sampleR     = tmp;

        // Dry/wet
        if (wet != 1.0)
        {
            sampleL = (sampleL * wet) + (dryL * (1.0 - wet));
            sampleR = (sampleR * wet) + (dryR * (1.0 - wet));
        }

        // Advance dither seeds
        fpdL_acc ^= fpdL_acc << 13; fpdL_acc ^= fpdL_acc >> 17; fpdL_acc ^= fpdL_acc << 5;
        fpdR_acc ^= fpdR_acc << 13; fpdR_acc ^= fpdR_acc >> 17; fpdR_acc ^= fpdR_acc << 5;

        // ================================================================
        //  2. Spiral  — sin(x*|x|) / |x|  warm saturation
        // ================================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_spi * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_spi * 1.18e-17;

        {
            const double absL = std::fabs (sampleL);
            const double absR = std::fabs (sampleR);
            sampleL = std::sin (sampleL * absL) / (absL == 0.0 ? 1.0 : absL);
            sampleR = std::sin (sampleR * absR) / (absR == 0.0 ? 1.0 : absR);
        }

        fpdL_spi ^= fpdL_spi << 13; fpdL_spi ^= fpdL_spi >> 17; fpdL_spi ^= fpdL_spi << 5;
        fpdR_spi ^= fpdR_spi << 13; fpdR_spi ^= fpdR_spi >> 17; fpdR_spi ^= fpdR_spi << 5;

        // ================================================================
        //  3. ClipSoftly — sin-based soft clip with speed-adaptive blend
        // ================================================================
        if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_cs * 1.18e-17;
        if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_cs * 1.18e-17;

        {
            double softSpeed = std::fabs (sampleL);
            if (softSpeed < 1.0) softSpeed = 1.0; else softSpeed = 1.0 / softSpeed;
            if (sampleL >  1.57079633) sampleL =  1.57079633;
            if (sampleL < -1.57079633) sampleL = -1.57079633;
            sampleL = std::sin (sampleL) * 0.9549925859;
            sampleL = (sampleL * softSpeed) + (lastSampleL_cs * (1.0 - softSpeed));
        }
        {
            double softSpeed = std::fabs (sampleR);
            if (softSpeed < 1.0) softSpeed = 1.0; else softSpeed = 1.0 / softSpeed;
            if (sampleR >  1.57079633) sampleR =  1.57079633;
            if (sampleR < -1.57079633) sampleR = -1.57079633;
            sampleR = std::sin (sampleR) * 0.9549925859;
            sampleR = (sampleR * softSpeed) + (lastSampleR_cs * (1.0 - softSpeed));
        }

        // Latency-compensation buffer (sample-rate scaling)
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

        // ================================================================
        //  Write output
        // ================================================================
        inL[i] = (float)sampleL;
        inR[i] = (float)sampleR;
    }
}

//==============================================================================
juce::AudioProcessorEditor* AWCascadeProcessor::createEditor()
{
    return new AWCascadeEditor (*this);
}

//==============================================================================
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

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AWCascadeProcessor();
}
