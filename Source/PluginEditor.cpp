#include "PluginEditor.h"

namespace
{
    constexpr int kWidth        = 420;
    constexpr int kHeight       = 320;
    constexpr int kPadding      = 20;
    constexpr int kLabelH       = 18;
    constexpr int kSliderH      = 60;
    constexpr int kSectionGap   = 14;

    const juce::Colour kBg      { 0xff1a1a2e };
    const juce::Colour kPanel   { 0xff16213e };
    const juce::Colour kAccent  { 0xff0f3460 };
    const juce::Colour kKnob    { 0xffe94560 };
    const juce::Colour kText    { 0xffe0e0e0 };
    const juce::Colour kSubText { 0xff888888 };
}

//==============================================================================
AWCascadeEditor::AWCascadeEditor (AWCascadeProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      accelLimitAttach  (p.apvts, "accelLimit",  accelLimitSlider),
      accelDryWetAttach (p.apvts, "accelDryWet", accelDryWetSlider)
{
    setSize (kWidth, kHeight);

    auto setupSlider = [&] (juce::Slider& s, juce::Label& lbl, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        s.setColour (juce::Slider::rotarySliderFillColourId, kKnob);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, kAccent);
        s.setColour (juce::Slider::thumbColourId, kKnob);
        s.setColour (juce::Slider::textBoxTextColourId, kText);
        s.setColour (juce::Slider::textBoxBackgroundColourId, kPanel);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        lbl.setText (name, juce::dontSendNotification);
        lbl.setFont (juce::Font (13.0f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, kText);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
    };

    setupSlider (accelLimitSlider,  accelLimitLabel,  "Limit");
    setupSlider (accelDryWetSlider, accelDryWetLabel, "Dry / Wet");
}

AWCascadeEditor::~AWCascadeEditor() {}

//==============================================================================
void AWCascadeEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (kBg);

    // Title bar
    g.setColour (kAccent);
    g.fillRect (0, 0, kWidth, 44);

    g.setColour (kText);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("AW Cascade", 0, 0, kWidth, 44, juce::Justification::centred);

    // Section header — Acceleration2
    const int sec1Y = 56;
    g.setColour (kPanel);
    g.fillRoundedRectangle (kPadding, sec1Y, kWidth - kPadding * 2, kSliderH + kLabelH * 2 + 10, 6.0f);
    g.setColour (kKnob);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("ACCELERATION2", kPadding + 8, sec1Y + 4, 140, 14, juce::Justification::left);

    // Section header — Spiral
    const int sec2Y = sec1Y + kSliderH + kLabelH * 2 + 10 + kSectionGap;
    g.setColour (kPanel);
    g.fillRoundedRectangle (kPadding, sec2Y, kWidth - kPadding * 2, 48, 6.0f);
    g.setColour (kKnob);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("SPIRAL", kPadding + 8, sec2Y + 8, 80, 14, juce::Justification::left);
    g.setColour (kSubText);
    g.setFont (juce::Font (12.0f));
    g.drawText ("sin(x\xc2\xb7|x|)/|x|  \xe2\x80\x94  warm saturation (no params)",
                kPadding + 8, sec2Y + 24, kWidth - kPadding * 2 - 16, 16,
                juce::Justification::left);

    // Section header — ClipSoftly
    const int sec3Y = sec2Y + 48 + kSectionGap;
    g.setColour (kPanel);
    g.fillRoundedRectangle (kPadding, sec3Y, kWidth - kPadding * 2, 48, 6.0f);
    g.setColour (kKnob);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("CLIPSOFTLY", kPadding + 8, sec3Y + 8, 100, 14, juce::Justification::left);
    g.setColour (kSubText);
    g.setFont (juce::Font (12.0f));
    g.drawText ("sin-based soft clip with adaptive speed blend (no params)",
                kPadding + 8, sec3Y + 24, kWidth - kPadding * 2 - 16, 16,
                juce::Justification::left);
}

void AWCascadeEditor::resized()
{
    const int sec1Y   = 56;
    const int innerY  = sec1Y + 20;
    const int halfW   = (kWidth - kPadding * 2) / 2;

    // Acceleration2 — two rotary knobs side by side
    accelLimitLabel.setBounds  (kPadding,          innerY, halfW, kLabelH);
    accelLimitSlider.setBounds (kPadding,          innerY + kLabelH, halfW, kSliderH);

    accelDryWetLabel.setBounds (kPadding + halfW,  innerY, halfW, kLabelH);
    accelDryWetSlider.setBounds(kPadding + halfW,  innerY + kLabelH, halfW, kSliderH);
}
