#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class AWCascadeEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit AWCascadeEditor (AWCascadeProcessor&);
    ~AWCascadeEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AWCascadeProcessor& processor;

    // Apex Limiter
    juce::Slider apexLimitSlider,  apexDryWetSlider;
    juce::Label  apexLimitLabel,   apexDryWetLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment apexLimitAttach, apexDryWetAttach;
    juce::TextButton apexBypassButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment apexBypassAttach;

    // Even Drive
    juce::Slider evenInputSlider, evenHighpassSlider, evenPresenceSlider,
                 evenOutputSlider, evenDryWetSlider;
    juce::Label  evenInputLabel, evenHighpassLabel, evenPresenceLabel,
                 evenOutputLabel, evenDryWetLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        evenInputAttach, evenHighpassAttach, evenPresenceAttach,
        evenOutputAttach, evenDryWetAttach;
    juce::TextButton evenBypassButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment evenBypassAttach;
    juce::TextButton doubleButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment doubleAttach;

    // Velvet Clip
    juce::Slider     velvetCeilingSlider;
    juce::Label      velvetCeilingLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment velvetCeilingAttach;
    juce::TextButton velvetBypassButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment velvetBypassAttach;
    juce::TextButton hardClipButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment hardClipAttach;

    // Swap
    juce::TextButton swapButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment swapAttach;

    // Oversampling
    juce::ComboBox oversampleBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment oversampleAttach;

    // Meter display values (UI thread only, decayed in timerCallback)
    float dispInL{0.0f}, dispInR{0.0f}, dispOutL{0.0f}, dispOutR{0.0f};

    void setupBypassButton (juce::TextButton& btn, bool bypassed);
    void placeApexKnobs (int knobY);
    void placeEvenKnobs (int knobY);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AWCascadeEditor)
};
