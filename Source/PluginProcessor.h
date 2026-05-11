#pragma once

#include <JuceHeader.h>

class AWCascadeProcessor : public juce::AudioProcessor
{
public:
    AWCascadeProcessor();
    ~AWCascadeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()  const override { return false; }
    bool  producesMidi() const override { return false; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                             { return 1; }
    int getCurrentProgram() override                          { return 0; }
    void setCurrentProgram (int) override                     {}
    const juce::String getProgramName (int) override          { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // ---- Apex Limiter (Acceleration2) ----
    double sL[34], sR[34];
    double m1L, m2L, m1R, m2R;
    double biquadA[11], biquadB[11];
    uint32_t fpdL_acc, fpdR_acc;

    // ---- Even Drive (Spiral2) ----
    double iirSampleAL, iirSampleBL, prevSampleL_spi;
    double iirSampleAR, iirSampleBR, prevSampleR_spi;
    bool   flip_spi;
    uint32_t fpdL_spi, fpdR_spi;

    // ---- Velvet Clip (ClipSoftly) ----
    double lastSampleL_cs, lastSampleR_cs;
    double intermediateL[17], intermediateR[17];
    uint32_t fpdL_cs, fpdR_cs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AWCascadeProcessor)
};
