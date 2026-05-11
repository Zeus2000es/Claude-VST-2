/* ============================================================================
 *  The Press - PluginProcessor.cpp
 *  Chain: Apex Limiter → Even Drive → Velvet Clip  (or swapped)
 *  Original algorithms © airwindows (MIT license)
 * ============================================================================ */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AWCascadeProcessor::createParameterLayout()
{
    using PF   = juce::AudioParameterFloat;
    using PB   = juce::AudioParameterBool;
    using PID  = juce::ParameterID;
    using Attr = juce::AudioParameterFloatAttributes;
    using NR   = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Apex Limiter ----
    layout.add (std::make_unique<PF> (
        PID ("apexLimit", 1), "Apex Limit", NR (0.0f, 1.0f, 0.001f), 0.32f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            return juce::String ((int)(v * 100.0f)) + "%";
        })));

    layout.add (std::make_unique<PF> (
        PID ("apexDryWet", 1), "Apex Dry/Wet", NR (0.0f, 1.0f, 0.001f), 1.0f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            int w = juce::roundToInt (v * 100.0f);
            return juce::String (w) + "W/" + juce::String (100 - w) + "D";
        })));

    // ---- Even Drive ----
    layout.add (std::make_unique<PF> (
        PID ("evenInput", 1), "Drive Input", NR (0.0f, 1.0f, 0.001f), 0.5f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            const float g = std::pow (v * 2.0f, 2.0f);
            if (g < 1e-4f) return "-inf dB";
            return juce::String (20.0f * std::log10 (g), 1) + " dB";
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenHighpass", 1), "Drive Highpass", NR (0.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            const float fc = std::pow (v, 3.0f) * (44100.0f / (2.0f * juce::MathConstants<float>::pi));
            if (fc < 1.0f) return juce::String ("Off");
            if (fc < 1000.0f) return juce::String ((int) fc) + " Hz";
            return juce::String (fc / 1000.0f, 1) + " kHz";
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenPresence", 1), "Drive Presence", NR (0.0f, 1.0f, 0.001f), 0.5f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            const float val = (v - 0.5f) * 12.0f; // -6 to +6
            if (val >= 0.0f) return "+" + juce::String (val, 1);
            return juce::String (val, 1);
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenOutput", 1), "Drive Output", NR (0.0f, 1.0f, 0.001f), 1.0f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            if (v < 0.001f) return "-inf dB";
            return juce::String (20.0f * std::log10 (v), 1) + " dB";
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenDryWet", 1), "Drive Dry/Wet", NR (0.0f, 1.0f, 0.001f), 1.0f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            int w = juce::roundToInt (v * 100.0f);
            return juce::String (w) + "W/" + juce::String (100 - w) + "D";
        })));

    // ---- Swap order ----
    layout.add (std::make_unique<PB> (PID ("swapOrder", 1), "Swap Order", false));

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
    std::memset (sL, 0, sizeof (sL));   std::memset (sR, 0, sizeof (sR));
    m1L = m2L = m1R = m2R = 0.0;
    std::memset (biquadA, 0, sizeof (biquadA));
    std::memset (biquadB, 0, sizeof (biquadB));
    fpdL_acc = 1; while (fpdL_acc < 16386) fpdL_acc = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_acc = 1; while (fpdR_acc < 16386) fpdR_acc = (uint32_t)(rand() * (double)UINT32_MAX);

    iirSampleAL = iirSampleBL = prevSampleL_spi = 0.0;
    iirSampleAR = iirSampleBR = prevSampleR_spi = 0.0;
    flip_spi = true;
    fpdL_spi = 1; while (fpdL_spi < 16386) fpdL_spi = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_spi = 1; while (fpdR_spi < 16386) fpdR_spi = (uint32_t)(rand() * (double)UINT32_MAX);

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
    const bool   doSwap       = apvts.getRawParameterValue ("swapOrder")->load() > 0.5f;

    // ---- Apex Limiter per-block setup ----
    const double A         = (double) apvts.getRawParameterValue ("apexLimit") ->load();
    const double B         = (double) apvts.getRawParameterValue ("apexDryWet")->load();
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
    const double spiGain     = std::pow ((double) apvts.getRawParameterValue ("evenInput")   ->load() * 2.0, 2.0);
    const double spiIir      = std::pow ((double) apvts.getRawParameterValue ("evenHighpass")->load(), 3.0) / overallscale;
    const double spiPresence = (double) apvts.getRawParameterValue ("evenPresence")->load();
    const double spiOutput   = (double) apvts.getRawParameterValue ("evenOutput")  ->load();
    const double spiWet      = (double) apvts.getRawParameterValue ("evenDryWet")  ->load();

    // ---- Velvet Clip per-block setup ----
    int csSpacing = (int) std::floor (overallscale);
    if (csSpacing < 1) csSpacing = 1; if (csSpacing > 16) csSpacing = 16;

    // ---- Per-sample loop ----
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleL = (double) inL[i];
        double sampleR = (double) inR[i];

        // ============================================================
        //  APEX LIMITER — runs first unless swapped
        // ============================================================
        if (!doSwap)
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_acc * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_acc * 1.18e-17;
            const double dryL = sampleL, dryR = sampleR;

            double tmp = (sampleL * biquadA[2]) + biquadA[7];
            biquadA[7]  = (sampleL * biquadA[3]) - (tmp * biquadA[5]) + biquadA[8];
            biquadA[8]  = (sampleL * biquadA[4]) - (tmp * biquadA[6]);
            const double smL = tmp;
            tmp         = (sampleR * biquadA[2]) + biquadA[9];
            biquadA[9]  = (sampleR * biquadA[3]) - (tmp * biquadA[5]) + biquadA[10];
            biquadA[10] = (sampleR * biquadA[4]) - (tmp * biquadA[6]);
            const double smR = tmp;

            for (int c = spacing*2; c >= 0; --c) { sL[c+1] = sL[c]; sR[c+1] = sR[c]; }
            sL[0] = sampleL; sR[0] = sampleR;

            m1L = (sL[0]-sL[spacing]) * std::fabs (sL[0]-sL[spacing]);
            m2L = (sL[spacing]-sL[spacing*2]) * std::fabs (sL[spacing]-sL[spacing*2]);
            double sns = intensity*intensity*std::fabs(m1L-m2L); if (sns>1.0) sns=1.0;
            sampleL = sampleL*(1.0-sns) + smL*sns;
            m1R = (sR[0]-sR[spacing]) * std::fabs (sR[0]-sR[spacing]);
            m2R = (sR[spacing]-sR[spacing*2]) * std::fabs (sR[spacing]-sR[spacing*2]);
            sns = intensity*intensity*std::fabs(m1R-m2R); if (sns>1.0) sns=1.0;
            sampleR = sampleR*(1.0-sns) + smR*sns;

            tmp         = (sampleL * biquadB[2]) + biquadB[7];
            biquadB[7]  = (sampleL * biquadB[3]) - (tmp * biquadB[5]) + biquadB[8];
            biquadB[8]  = (sampleL * biquadB[4]) - (tmp * biquadB[6]);
            sampleL = tmp;
            tmp         = (sampleR * biquadB[2]) + biquadB[9];
            biquadB[9]  = (sampleR * biquadB[3]) - (tmp * biquadB[5]) + biquadB[10];
            biquadB[10] = (sampleR * biquadB[4]) - (tmp * biquadB[6]);
            sampleR = tmp;

            if (apexWet != 1.0) { sampleL=(sampleL*apexWet)+(dryL*(1.0-apexWet));
                                   sampleR=(sampleR*apexWet)+(dryR*(1.0-apexWet)); }
            fpdL_acc^=fpdL_acc<<13; fpdL_acc^=fpdL_acc>>17; fpdL_acc^=fpdL_acc<<5;
            fpdR_acc^=fpdR_acc<<13; fpdR_acc^=fpdR_acc>>17; fpdR_acc^=fpdR_acc<<5;
        }

        // ============================================================
        //  EVEN DRIVE — always here
        // ============================================================
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_spi * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_spi * 1.18e-17;
            const double drySpiL = sampleL, drySpiR = sampleR;

            if (spiGain != 1.0) { sampleL*=spiGain; sampleR*=spiGain;
                                   prevSampleL_spi*=spiGain; prevSampleR_spi*=spiGain; }
            if (flip_spi) {
                iirSampleAL=(iirSampleAL*(1.0-spiIir))+(sampleL*spiIir); sampleL-=iirSampleAL;
                iirSampleAR=(iirSampleAR*(1.0-spiIir))+(sampleR*spiIir); sampleR-=iirSampleAR;
            } else {
                iirSampleBL=(iirSampleBL*(1.0-spiIir))+(sampleL*spiIir); sampleL-=iirSampleBL;
                iirSampleBR=(iirSampleBR*(1.0-spiIir))+(sampleR*spiIir); sampleR-=iirSampleBR;
            }
            double pL=std::sin(sampleL*std::fabs(prevSampleL_spi))/(prevSampleL_spi==0.0?1.0:std::fabs(prevSampleL_spi));
            double pR=std::sin(sampleR*std::fabs(prevSampleR_spi))/(prevSampleR_spi==0.0?1.0:std::fabs(prevSampleR_spi));
            sampleL=std::sin(sampleL*std::fabs(sampleL))/(std::fabs(sampleL)==0.0?1.0:std::fabs(sampleL));
            sampleR=std::sin(sampleR*std::fabs(sampleR))/(std::fabs(sampleR)==0.0?1.0:std::fabs(sampleR));
            if (spiOutput<1.0) { sampleL*=spiOutput; sampleR*=spiOutput; pL*=spiOutput; pR*=spiOutput; }
            if (spiPresence>0.0) { sampleL=(sampleL*(1.0-spiPresence))+(pL*spiPresence);
                                    sampleR=(sampleR*(1.0-spiPresence))+(pR*spiPresence); }
            if (spiWet<1.0) { sampleL=(drySpiL*(1.0-spiWet))+(sampleL*spiWet);
                               sampleR=(drySpiR*(1.0-spiWet))+(sampleR*spiWet); }
            prevSampleL_spi=drySpiL; prevSampleR_spi=drySpiR;
            flip_spi=!flip_spi;
            fpdL_spi^=fpdL_spi<<13; fpdL_spi^=fpdL_spi>>17; fpdL_spi^=fpdL_spi<<5;
            fpdR_spi^=fpdR_spi<<13; fpdR_spi^=fpdR_spi>>17; fpdR_spi^=fpdR_spi<<5;
        }

        // ============================================================
        //  APEX LIMITER — runs second if swapped
        // ============================================================
        if (doSwap)
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_acc * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_acc * 1.18e-17;
            const double dryL = sampleL, dryR = sampleR;

            double tmp = (sampleL * biquadA[2]) + biquadA[7];
            biquadA[7]  = (sampleL * biquadA[3]) - (tmp * biquadA[5]) + biquadA[8];
            biquadA[8]  = (sampleL * biquadA[4]) - (tmp * biquadA[6]);
            const double smL = tmp;
            tmp         = (sampleR * biquadA[2]) + biquadA[9];
            biquadA[9]  = (sampleR * biquadA[3]) - (tmp * biquadA[5]) + biquadA[10];
            biquadA[10] = (sampleR * biquadA[4]) - (tmp * biquadA[6]);
            const double smR = tmp;

            for (int c = spacing*2; c >= 0; --c) { sL[c+1] = sL[c]; sR[c+1] = sR[c]; }
            sL[0] = sampleL; sR[0] = sampleR;

            m1L = (sL[0]-sL[spacing]) * std::fabs (sL[0]-sL[spacing]);
            m2L = (sL[spacing]-sL[spacing*2]) * std::fabs (sL[spacing]-sL[spacing*2]);
            double sns = intensity*intensity*std::fabs(m1L-m2L); if (sns>1.0) sns=1.0;
            sampleL = sampleL*(1.0-sns) + smL*sns;
            m1R = (sR[0]-sR[spacing]) * std::fabs (sR[0]-sR[spacing]);
            m2R = (sR[spacing]-sR[spacing*2]) * std::fabs (sR[spacing]-sR[spacing*2]);
            sns = intensity*intensity*std::fabs(m1R-m2R); if (sns>1.0) sns=1.0;
            sampleR = sampleR*(1.0-sns) + smR*sns;

            tmp         = (sampleL * biquadB[2]) + biquadB[7];
            biquadB[7]  = (sampleL * biquadB[3]) - (tmp * biquadB[5]) + biquadB[8];
            biquadB[8]  = (sampleL * biquadB[4]) - (tmp * biquadB[6]);
            sampleL = tmp;
            tmp         = (sampleR * biquadB[2]) + biquadB[9];
            biquadB[9]  = (sampleR * biquadB[3]) - (tmp * biquadB[5]) + biquadB[10];
            biquadB[10] = (sampleR * biquadB[4]) - (tmp * biquadB[6]);
            sampleR = tmp;

            if (apexWet != 1.0) { sampleL=(sampleL*apexWet)+(dryL*(1.0-apexWet));
                                   sampleR=(sampleR*apexWet)+(dryR*(1.0-apexWet)); }
            fpdL_acc^=fpdL_acc<<13; fpdL_acc^=fpdL_acc>>17; fpdL_acc^=fpdL_acc<<5;
            fpdR_acc^=fpdR_acc<<13; fpdR_acc^=fpdR_acc>>17; fpdR_acc^=fpdR_acc<<5;
        }

        // ============================================================
        //  VELVET CLIP — always last
        // ============================================================
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_cs * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_cs * 1.18e-17;
            { double ss=std::fabs(sampleL); if(ss<1.0)ss=1.0;else ss=1.0/ss;
              if(sampleL>1.57079633)sampleL=1.57079633; if(sampleL<-1.57079633)sampleL=-1.57079633;
              sampleL=std::sin(sampleL)*0.9549925859; sampleL=(sampleL*ss)+(lastSampleL_cs*(1.0-ss)); }
            { double ss=std::fabs(sampleR); if(ss<1.0)ss=1.0;else ss=1.0/ss;
              if(sampleR>1.57079633)sampleR=1.57079633; if(sampleR<-1.57079633)sampleR=-1.57079633;
              sampleR=std::sin(sampleR)*0.9549925859; sampleR=(sampleR*ss)+(lastSampleR_cs*(1.0-ss)); }
            intermediateL[csSpacing]=sampleL; sampleL=lastSampleL_cs;
            for(int x=csSpacing;x>0;--x) intermediateL[x-1]=intermediateL[x];
            lastSampleL_cs=intermediateL[0];
            intermediateR[csSpacing]=sampleR; sampleR=lastSampleR_cs;
            for(int x=csSpacing;x>0;--x) intermediateR[x-1]=intermediateR[x];
            lastSampleR_cs=intermediateR[0];
            fpdL_cs^=fpdL_cs<<13; fpdL_cs^=fpdL_cs>>17; fpdL_cs^=fpdL_cs<<5;
            fpdR_cs^=fpdR_cs<<13; fpdR_cs^=fpdR_cs>>17; fpdR_cs^=fpdR_cs<<5;
        }

        inL[i] = (float) sampleL;
        inR[i] = (float) sampleR;
    }
}

//==============================================================================
juce::AudioProcessorEditor* AWCascadeProcessor::createEditor() { return new AWCascadeEditor (*this); }

void AWCascadeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AWCascadeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AWCascadeProcessor(); }
