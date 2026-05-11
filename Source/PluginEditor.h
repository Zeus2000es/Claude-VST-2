#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AWCascadeEditor : public juce::AudioProcessorEditor
{
public:
    explicit AWCascadeEditor (AWCascadeProcessor&);
    ~AWCascadeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AWCascadeProcessor& processor;

    // Apex Limiter knobs
    juce::Slider apexLimitSlider,  apexDryWetSlider;
    juce::Label  apexLimitLabel,   apexDryWetLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment apexLimitAttach, apexDryWetAttach;

    // Even Drive knobs
    juce::Slider evenInputSlider, evenHighpassSlider, evenPresenceSlider,
                 evenOutputSlider, evenDryWetSlider;
    juce::Label  evenInputLabel, evenHighpassLabel, evenPresenceLabel,
                 evenOutputLabel, evenDryWetLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        evenInputAttach, evenHighpassAttach, evenPresenceAttach,
        evenOutputAttach, evenDryWetAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AWCascadeEditor)
};
