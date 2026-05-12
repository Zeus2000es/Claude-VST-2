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
    using PC   = juce::AudioParameterChoice;
    using PID  = juce::ParameterID;
    using Attr = juce::AudioParameterFloatAttributes;
    using NR   = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Apex Limiter ----
    layout.add (std::make_unique<PF> (
        PID ("apexLimit", 1), "Apex Limit", NR (0.0f, 1.0f, 0.001f), 0.32f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            return juce::String ((int)(v * 100.0f)) + "%";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 100.0f);
        })));

    layout.add (std::make_unique<PF> (
        PID ("apexDryWet", 1), "Apex Dry/Wet", NR (0.0f, 1.0f, 0.001f), 1.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            int w = juce::roundToInt (v * 100.0f);
            return juce::String (w) + "W/" + juce::String (100 - w) + "D";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 100.0f);
        })));

    layout.add (std::make_unique<PB> (PID ("bypassApex", 1), "Bypass Apex", false));

    // ---- Even Drive ----
    layout.add (std::make_unique<PF> (
        PID ("evenInput", 1), "Drive Input", NR (-24.0f, 24.0f, 0.1f), 0.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            if (v >= 0.0f) return "+" + juce::String (v, 1) + " dB";
            return juce::String (v, 1) + " dB";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            return juce::jlimit (-24.0f, 24.0f, s.getFloatValue());
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenHighpass", 1), "Drive Highpass", NR (0.0f, 1.0f, 0.001f), 0.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            const float fc = std::pow (v, 3.0f) * (44100.0f / (2.0f * juce::MathConstants<float>::pi));
            if (fc < 1.0f) return juce::String ("Off");
            if (fc < 1000.0f) return juce::String ((int) fc) + " Hz";
            return juce::String (fc / 1000.0f, 2) + " kHz";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            const juce::String t = s.trim();
            if (t.equalsIgnoreCase ("Off") || t.equalsIgnoreCase ("0")) return 0.0f;
            float hz = t.containsIgnoreCase ("k") ? t.getFloatValue() * 1000.0f
                                                   : t.getFloatValue();
            if (hz < 1.0f) return 0.0f;
            return juce::jlimit (0.0f, 1.0f,
                std::cbrt (hz * 2.0f * juce::MathConstants<float>::pi / 44100.0f));
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenPresence", 1), "Drive Presence", NR (0.0f, 1.0f, 0.001f), 0.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            if (v < 0.001f) return "Off";
            return juce::String ((int)(v * 100.0f)) + "%";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            if (s.trim().equalsIgnoreCase ("Off")) return 0.0f;
            return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 100.0f);
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenOutput", 1), "Drive Output", NR (-24.0f, 24.0f, 0.1f), 0.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            if (v >= 0.0f) return "+" + juce::String (v, 1) + " dB";
            return juce::String (v, 1) + " dB";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            return juce::jlimit (-24.0f, 24.0f, s.getFloatValue());
        })));

    layout.add (std::make_unique<PF> (
        PID ("evenDryWet", 1), "Drive Dry/Wet", NR (0.0f, 1.0f, 0.001f), 1.0f,
        Attr()
        .withStringFromValueFunction ([] (float v, int) -> juce::String {
            int w = juce::roundToInt (v * 100.0f);
            return juce::String (w) + "W/" + juce::String (100 - w) + "D";
        })
        .withValueFromStringFunction ([] (const juce::String& s) -> float {
            return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 100.0f);
        })));

    layout.add (std::make_unique<PB> (PID ("bypassEven",   1), "Bypass Even",   false));
    layout.add (std::make_unique<PB> (PID ("doubleEffect", 1), "Double Effect", false));

    // ---- Velvet Clip ----
    layout.add (std::make_unique<PF> (
        PID ("velvetCeiling", 1), "Velvet Ceiling", NR (-6.0f, -0.1f, 0.01f), -0.4f,
        Attr().withStringFromValueFunction ([] (float v, int) -> juce::String {
            return juce::String (v, 1) + " dB";
        })));

    layout.add (std::make_unique<PB> (PID ("bypassVelvet", 1), "Bypass Velvet", false));
    layout.add (std::make_unique<PB> (PID ("hardClip",    1), "Hard Clip",     true));

    // ---- Swap order ----
    layout.add (std::make_unique<PB> (PID ("swapOrder", 1), "Swap Order", false));

    // ---- Oversampling ----
    layout.add (std::make_unique<PC> (
        PID ("oversample", 1), "Oversample",
        juce::StringArray { "Off", "2x", "4x" }, 1));

    return layout;
}

AWCascadeProcessor::AWCascadeProcessor()
    : AudioProcessor (BusesProperties()
                         .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      os2x (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true),
      os4x (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)
{
    prepareToPlay (44100.0, 512);
}

AWCascadeProcessor::~AWCascadeProcessor() {}

//==============================================================================
void AWCascadeProcessor::prepareToPlay (double /*sampleRate*/, int samplesPerBlock)
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

    lastSampleL_co3 = lastSampleR_co3 = 0.0;
    std::memset (intermediateL_co3, 0, sizeof (intermediateL_co3));
    std::memset (intermediateR_co3, 0, sizeof (intermediateR_co3));
    std::memset (slewL_co3, 0, sizeof (slewL_co3));
    std::memset (slewR_co3, 0, sizeof (slewR_co3));
    wasPosClipL_co3 = wasNegClipL_co3 = false;
    wasPosClipR_co3 = wasNegClipR_co3 = false;
    fpdL_co3 = 1; while (fpdL_co3 < 16386) fpdL_co3 = (uint32_t)(rand() * (double)UINT32_MAX);
    fpdR_co3 = 1; while (fpdR_co3 < 16386) fpdR_co3 = (uint32_t)(rand() * (double)UINT32_MAX);

    meterInL = meterInR = meterOutL = meterOutR = 0.0f;

    os2x.initProcessing ((size_t) samplesPerBlock);
    os4x.initProcessing ((size_t) samplesPerBlock);
}

void AWCascadeProcessor::releaseResources()
{
    os2x.reset();
    os4x.reset();
}

bool AWCascadeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

//==============================================================================
void AWCascadeProcessor::processChain (float* inL, float* inR,
                                       int numSamples, double actualSR)
{
    const double overallscale = actualSR / 44100.0;

    const bool doSwap       = apvts.getRawParameterValue ("swapOrder")->load()    > 0.5f;
    const bool bypassApex   = apvts.getRawParameterValue ("bypassApex")->load()   > 0.5f;
    const bool bypassEven    = apvts.getRawParameterValue ("bypassEven")->load()    > 0.5f;
    const bool doubleEffect  = apvts.getRawParameterValue ("doubleEffect")->load() > 0.5f;
    const bool bypassVelvet = apvts.getRawParameterValue ("bypassVelvet")->load() > 0.5f;
    const bool hardClip     = apvts.getRawParameterValue ("hardClip")->load()     > 0.5f;

    // ---- Apex Limiter per-block setup ----
    const double A         = (double) apvts.getRawParameterValue ("apexLimit") ->load();
    const double B         = (double) apvts.getRawParameterValue ("apexDryWet")->load();
    const double intensity = std::pow (A, 3.0) * 32.0;
    const double apexWet   = B;
    int spacing = (int)(1.73 * overallscale) + 1;
    if (spacing > 16) spacing = 16;

    biquadA[0] = (20000.0 * (1.0 - (A * 0.618033988749894848204586))) / actualSR;
    biquadB[0] = 20000.0 / actualSR;
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
    const double spiGain     = std::pow (10.0, (double) apvts.getRawParameterValue ("evenInput")   ->load() / 20.0);
    const double spiIir      = std::pow ((double) apvts.getRawParameterValue ("evenHighpass")->load(), 3.0) / overallscale;
    const double spiPresence = (double) apvts.getRawParameterValue ("evenPresence")->load();
    const double spiOutput   = std::pow (10.0, (double) apvts.getRawParameterValue ("evenOutput")  ->load() / 20.0);
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
        //  APEX LIMITER — first position (not swapped)
        // ============================================================
        if (!doSwap && !bypassApex)
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
        //  EVEN DRIVE
        // ============================================================
        if (!bypassEven)
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_spi * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_spi * 1.18e-17;
            const double drySpiL = sampleL, drySpiR = sampleR;

            // driveScale keeps the sin(x*|x|)/|x| nonlinearity in its musical range
            // regardless of input gain. Compensated after saturation so 0dB = transparent.
            const double driveScale = 0.25;
            sampleL *= spiGain * driveScale; sampleR *= spiGain * driveScale;
            prevSampleL_spi *= spiGain * driveScale; prevSampleR_spi *= spiGain * driveScale;
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
            // compensate driveScale so output level matches expectation
            sampleL /= driveScale; sampleR /= driveScale;
            pL /= driveScale; pR /= driveScale;

            // Double effect: second pass through same nonlinearity — more harmonics, same level
            if (doubleEffect)
            {
                sampleL *= driveScale; sampleR *= driveScale;
                sampleL = std::sin(sampleL*std::fabs(sampleL))/(std::fabs(sampleL)==0.0?1.0:std::fabs(sampleL));
                sampleR = std::sin(sampleR*std::fabs(sampleR))/(std::fabs(sampleR)==0.0?1.0:std::fabs(sampleR));
                sampleL /= driveScale; sampleR /= driveScale;
                pL *= driveScale;
                pL = std::sin(pL*std::fabs(pL))/(std::fabs(pL)==0.0?1.0:std::fabs(pL));
                pL /= driveScale;
                pR *= driveScale;
                pR = std::sin(pR*std::fabs(pR))/(std::fabs(pR)==0.0?1.0:std::fabs(pR));
                pR /= driveScale;
            }

            sampleL*=spiOutput; sampleR*=spiOutput; pL*=spiOutput; pR*=spiOutput;
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
        //  APEX LIMITER — second position (swapped)
        // ============================================================
        if (doSwap && !bypassApex)
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
        //  VELVET CLIP (ClipSoftly) — exact airwindows algorithm
        // ============================================================
        if (!bypassVelvet)
        {
            if (std::fabs (sampleL) < 1.18e-23) sampleL = fpdL_cs * 1.18e-17;
            if (std::fabs (sampleR) < 1.18e-23) sampleR = fpdR_cs * 1.18e-17;

            double ssL = std::fabs (sampleL); if (ssL < 1.0) ssL = 1.0; else ssL = 1.0 / ssL;
            if (sampleL >  1.57079633) sampleL =  1.57079633;
            if (sampleL < -1.57079633) sampleL = -1.57079633;
            sampleL = std::sin (sampleL) * 0.9549925859;
            sampleL = (sampleL * ssL) + (lastSampleL_cs * (1.0 - ssL));
            intermediateL[csSpacing] = sampleL; sampleL = lastSampleL_cs;
            for (int x = csSpacing; x > 0; --x) intermediateL[x-1] = intermediateL[x];
            lastSampleL_cs = intermediateL[0];

            double ssR = std::fabs (sampleR); if (ssR < 1.0) ssR = 1.0; else ssR = 1.0 / ssR;
            if (sampleR >  1.57079633) sampleR =  1.57079633;
            if (sampleR < -1.57079633) sampleR = -1.57079633;
            sampleR = std::sin (sampleR) * 0.9549925859;
            sampleR = (sampleR * ssR) + (lastSampleR_cs * (1.0 - ssR));
            intermediateR[csSpacing] = sampleR; sampleR = lastSampleR_cs;
            for (int x = csSpacing; x > 0; --x) intermediateR[x-1] = intermediateR[x];
            lastSampleR_cs = intermediateR[0];

            fpdL_cs ^= fpdL_cs << 13; fpdL_cs ^= fpdL_cs >> 17; fpdL_cs ^= fpdL_cs << 5;
            fpdR_cs ^= fpdR_cs << 13; fpdR_cs ^= fpdR_cs >> 17; fpdR_cs ^= fpdR_cs << 5;
        }

        // ============================================================
        //  HARD CLIP (ClipOnly3) — exact airwindows algorithm
        // ============================================================
        if (hardClip)
        {
            // --- Left channel ---
            const double noiseL = 1.0 - ((double(fpdL_co3) / (double)UINT32_MAX) * 0.076);
            if (wasPosClipL_co3) {
                if (sampleL < lastSampleL_co3) lastSampleL_co3 = (0.9085097 * noiseL) + (sampleL * (1.0 - noiseL));
                else lastSampleL_co3 = 0.94;
            }
            wasPosClipL_co3 = false;
            if (sampleL > 0.9085097) { wasPosClipL_co3 = true; sampleL = (0.9085097 * noiseL) + (lastSampleL_co3 * (1.0 - noiseL)); }
            if (wasNegClipL_co3) {
                if (sampleL > lastSampleL_co3) lastSampleL_co3 = (-0.9085097 * noiseL) + (sampleL * (1.0 - noiseL));
                else lastSampleL_co3 = -0.94;
            }
            wasNegClipL_co3 = false;
            if (sampleL < -0.9085097) { wasNegClipL_co3 = true; sampleL = (-0.9085097 * noiseL) + (lastSampleL_co3 * (1.0 - noiseL)); }
            slewL_co3[csSpacing * 2] = std::fabs (lastSampleL_co3 - sampleL);
            for (int x = csSpacing * 2; x > 0; x--) slewL_co3[x-1] = slewL_co3[x];
            intermediateL_co3[csSpacing] = sampleL; sampleL = lastSampleL_co3;
            for (int x = csSpacing; x > 0; x--) intermediateL_co3[x-1] = intermediateL_co3[x];
            lastSampleL_co3 = intermediateL_co3[0];
            if (wasPosClipL_co3 || wasNegClipL_co3) {
                for (int x = csSpacing; x > 0; x--) lastSampleL_co3 += intermediateL_co3[x];
                lastSampleL_co3 /= (double) csSpacing;
            }
            double finalSlewL = 0.0;
            for (int x = csSpacing * 2; x >= 0; x--) if (finalSlewL < slewL_co3[x]) finalSlewL = slewL_co3[x];
            const double postclipL = 0.94 / (1.0 + (finalSlewL * 1.3986013));
            if (sampleL >  postclipL) sampleL =  postclipL;
            if (sampleL < -postclipL) sampleL = -postclipL;

            // --- Right channel ---
            const double noiseR = 1.0 - ((double(fpdR_co3) / (double)UINT32_MAX) * 0.076);
            if (wasPosClipR_co3) {
                if (sampleR < lastSampleR_co3) lastSampleR_co3 = (0.9085097 * noiseR) + (sampleR * (1.0 - noiseR));
                else lastSampleR_co3 = 0.94;
            }
            wasPosClipR_co3 = false;
            if (sampleR > 0.9085097) { wasPosClipR_co3 = true; sampleR = (0.9085097 * noiseR) + (lastSampleR_co3 * (1.0 - noiseR)); }
            if (wasNegClipR_co3) {
                if (sampleR > lastSampleR_co3) lastSampleR_co3 = (-0.9085097 * noiseR) + (sampleR * (1.0 - noiseR));
                else lastSampleR_co3 = -0.94;
            }
            wasNegClipR_co3 = false;
            if (sampleR < -0.9085097) { wasNegClipR_co3 = true; sampleR = (-0.9085097 * noiseR) + (lastSampleR_co3 * (1.0 - noiseR)); }
            slewR_co3[csSpacing * 2] = std::fabs (lastSampleR_co3 - sampleR);
            for (int x = csSpacing * 2; x > 0; x--) slewR_co3[x-1] = slewR_co3[x];
            intermediateR_co3[csSpacing] = sampleR; sampleR = lastSampleR_co3;
            for (int x = csSpacing; x > 0; x--) intermediateR_co3[x-1] = intermediateR_co3[x];
            lastSampleR_co3 = intermediateR_co3[0];
            if (wasPosClipR_co3 || wasNegClipR_co3) {
                for (int x = csSpacing; x > 0; x--) lastSampleR_co3 += intermediateR_co3[x];
                lastSampleR_co3 /= (double) csSpacing;
            }
            double finalSlewR = 0.0;
            for (int x = csSpacing * 2; x >= 0; x--) if (finalSlewR < slewR_co3[x]) finalSlewR = slewR_co3[x];
            const double postclipR = 0.94 / (1.0 + (finalSlewR * 1.3986013));
            if (sampleR >  postclipR) sampleR =  postclipR;
            if (sampleR < -postclipR) sampleR = -postclipR;

            fpdL_co3 ^= fpdL_co3 << 13; fpdL_co3 ^= fpdL_co3 >> 17; fpdL_co3 ^= fpdL_co3 << 5;
            fpdR_co3 ^= fpdR_co3 << 13; fpdR_co3 ^= fpdR_co3 >> 17; fpdR_co3 ^= fpdR_co3 << 5;
        }

        inL[i] = (float) sampleL;
        inR[i] = (float) sampleR;
    }
}

//==============================================================================
void AWCascadeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    // Measure input peaks
    float inPeakL = 0.0f, inPeakR = 0.0f;
    const float* rdL = buffer.getReadPointer (0);
    const float* rdR = buffer.getReadPointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        inPeakL = std::max (inPeakL, std::fabs (rdL[i]));
        inPeakR = std::max (inPeakR, std::fabs (rdR[i]));
    }
    meterInL.store (inPeakL);
    meterInR.store (inPeakR);

    // Route through oversampler (or direct)
    const int osMode = (int) apvts.getRawParameterValue ("oversample")->load();

    if (osMode == 0)
    {
        processChain (buffer.getWritePointer (0), buffer.getWritePointer (1),
                      numSamples, getSampleRate());
    }
    else
    {
        auto block = juce::dsp::AudioBlock<float> (buffer);
        if (osMode == 1)
        {
            auto upBlock = os2x.processSamplesUp (block);
            processChain (upBlock.getChannelPointer (0), upBlock.getChannelPointer (1),
                          (int) upBlock.getNumSamples(), getSampleRate() * 2.0);
            os2x.processSamplesDown (block);
        }
        else
        {
            auto upBlock = os4x.processSamplesUp (block);
            processChain (upBlock.getChannelPointer (0), upBlock.getChannelPointer (1),
                          (int) upBlock.getNumSamples(), getSampleRate() * 4.0);
            os4x.processSamplesDown (block);
        }
    }

    // Hard ceiling clamp — runs after all DSP and OS downsample.
    // Catches IIR downsample ringing and enforces velvetCeiling absolutely.
    // Activates whenever VelvetClip or HardClip is engaged.
    {
        const bool bvOn = apvts.getRawParameterValue ("bypassVelvet")->load() < 0.5f;
        const bool hcOn = apvts.getRawParameterValue ("hardClip")->load()     > 0.5f;
        if (bvOn || hcOn)
        {
            const float cl = (float) std::pow (10.0,
                (double) apvts.getRawParameterValue ("velvetCeiling")->load() / 20.0);
            float* wL = buffer.getWritePointer (0);
            float* wR = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                if (wL[i] >  cl) wL[i] =  cl;
                if (wL[i] < -cl) wL[i] = -cl;
                if (wR[i] >  cl) wR[i] =  cl;
                if (wR[i] < -cl) wR[i] = -cl;
            }
        }
    }

    // Measure output peaks
    float outPeakL = 0.0f, outPeakR = 0.0f;
    rdL = buffer.getReadPointer (0);
    rdR = buffer.getReadPointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        outPeakL = std::max (outPeakL, std::fabs (rdL[i]));
        outPeakR = std::max (outPeakR, std::fabs (rdR[i]));
    }
    meterOutL.store (outPeakL);
    meterOutR.store (outPeakR);
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
