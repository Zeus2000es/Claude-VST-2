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

    // Acceleration2 controls
    juce::Slider accelLimitSlider;
    juce::Slider accelDryWetSlider;
    juce::Label  accelLimitLabel;
    juce::Label  accelDryWetLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment accelLimitAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment accelDryWetAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AWCascadeEditor)
};
