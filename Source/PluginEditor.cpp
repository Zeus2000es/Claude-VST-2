#include "PluginEditor.h"

namespace
{
    constexpr int kW          = 480;
    constexpr int kH          = 548;
    constexpr int kPad        = 16;
    constexpr int kTitleH     = 46;
    constexpr int kKnobW      = 76;
    constexpr int kKnobH      = 72;
    constexpr int kLabelH     = 16;
    constexpr int kBioH       = 28;
    constexpr int kSecHead    = 22;
    constexpr int kGap        = 10;

    const juce::Colour kBg     { 0xff141420 };
    const juce::Colour kPanel  { 0xff1e1e30 };
    const juce::Colour kBorder { 0xff2a2a45 };
    const juce::Colour kAccent { 0xffe94560 };
    const juce::Colour kText   { 0xffe8e8e8 };
    const juce::Colour kBio    { 0xff9090a8 };
    const juce::Colour kTag    { 0xffcc3355 };
}

static void setupKnob (juce::Slider& s, juce::Label& lbl, const juce::String& name,
                       juce::Component* parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffe94560));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2a45));
    s.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffe94560));
    s.setColour (juce::Slider::textBoxTextColourId,         juce::Colour (0xffe8e8e8));
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xff1e1e30));
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    parent->addAndMakeVisible (s);

    lbl.setText (name, juce::dontSendNotification);
    lbl.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    lbl.setColour (juce::Label::textColourId, kBio);
    lbl.setJustificationType (juce::Justification::centred);
    parent->addAndMakeVisible (lbl);
}

//==============================================================================
AWCascadeEditor::AWCascadeEditor (AWCascadeProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      apexLimitAttach    (p.apvts, "apexLimit",    apexLimitSlider),
      apexDryWetAttach   (p.apvts, "apexDryWet",   apexDryWetSlider),
      evenInputAttach    (p.apvts, "evenInput",     evenInputSlider),
      evenHighpassAttach (p.apvts, "evenHighpass",  evenHighpassSlider),
      evenPresenceAttach (p.apvts, "evenPresence",  evenPresenceSlider),
      evenOutputAttach   (p.apvts, "evenOutput",    evenOutputSlider),
      evenDryWetAttach   (p.apvts, "evenDryWet",    evenDryWetSlider)
{
    setupKnob (apexLimitSlider,    apexLimitLabel,    "Limit",    this);
    setupKnob (apexDryWetSlider,   apexDryWetLabel,   "Dry/Wet",  this);
    setupKnob (evenInputSlider,    evenInputLabel,    "Input",    this);
    setupKnob (evenHighpassSlider, evenHighpassLabel, "Highpass", this);
    setupKnob (evenPresenceSlider, evenPresenceLabel, "Presence", this);
    setupKnob (evenOutputSlider,   evenOutputLabel,   "Output",   this);
    setupKnob (evenDryWetSlider,   evenDryWetLabel,   "Dry/Wet",  this);

    setSize (kW, kH);
}

AWCascadeEditor::~AWCascadeEditor() {}

//==============================================================================
void AWCascadeEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // ---- Title bar ----
    g.setColour (kPanel);
    g.fillRect (0, 0, kW, kTitleH);
    g.setColour (kBorder);
    g.drawLine (0, kTitleH, kW, kTitleH, 1.0f);
    g.setColour (kText);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("The Press", 0, 0, kW, kTitleH, juce::Justification::centred);

    int y = kTitleH + kGap;

    auto drawSection = [&] (const juce::String& tag, const juce::String& bio, int h)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (kPad, y, kW - kPad*2, h, 6.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (kPad, y, kW - kPad*2, h, 6.0f, 1.0f);

        // Tag badge
        int tagW = g.getCurrentFont().getStringWidth (tag) + 20;
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        tagW = g.getCurrentFont().getStringWidth (tag) + 16;
        g.setColour (kTag);
        g.fillRoundedRectangle (kPad + 10, y + 8, tagW, 16, 3.0f);
        g.setColour (juce::Colours::white);
        g.drawText (tag, kPad + 10, y + 8, tagW, 16, juce::Justification::centred);

        // Bio text
        g.setFont (juce::FontOptions (11.5f));
        g.setColour (kBio);
        g.drawText (bio, kPad + 14, y + 28, kW - kPad*2 - 28, kBioH,
                    juce::Justification::centredLeft, true);
    };

    // Apex Limiter section
    int apexH = kBioH + kSecHead + kKnobH + kLabelH + 20;
    drawSection ("APEX LIMITER", "Tracks waveform velocity and softens only the sharpest transient edges.", apexH);
    y += apexH + kGap;

    // Even Drive section
    int evenH = kBioH + kSecHead + kKnobH + kLabelH + 20;
    drawSection ("EVEN DRIVE", "Generates even-order harmonics, analogue weight, and leaves transients untouched.", evenH);
    y += evenH + kGap;

    // Velvet Clip section
    int clipH = kBioH + kSecHead + 14;
    drawSection ("VELVET CLIP", "Transparent soft ceiling \xe2\x80\x94 Catches output peaks smoothly.", clipH);

    // Footer
    g.setFont (juce::FontOptions (10.5f));
    g.setColour (kBio);
    g.drawText ("Made by Zeus", 0, kH - 18, kW, 16, juce::Justification::centred);
}

void AWCascadeEditor::resized()
{
    int y = kTitleH + kGap;

    // ---- Apex Limiter knobs ----
    int apexH    = kBioH + kSecHead + kKnobH + kLabelH + 20;
    int knobY    = y + kSecHead + kBioH + 4;
    int halfW    = (kW - kPad*2) / 2;

    apexLimitLabel .setBounds (kPad,          knobY,          halfW, kLabelH);
    apexLimitSlider.setBounds (kPad,          knobY + kLabelH, halfW, kKnobH);
    apexDryWetLabel .setBounds(kPad + halfW,  knobY,          halfW, kLabelH);
    apexDryWetSlider.setBounds(kPad + halfW,  knobY + kLabelH, halfW, kKnobH);
    y += apexH + kGap;

    // ---- Even Drive knobs ----
    int evenH  = kBioH + kSecHead + kKnobH + kLabelH + 20;
    knobY      = y + kSecHead + kBioH + 4;
    int inner  = kW - kPad*2;
    int slot   = inner / 5;

    auto placeKnob = [&] (juce::Label& lbl, juce::Slider& sl, int idx)
    {
        int x = kPad + idx * slot;
        lbl.setBounds (x, knobY,          slot, kLabelH);
        sl .setBounds (x, knobY + kLabelH, slot, kKnobH);
    };
    placeKnob (evenInputLabel,    evenInputSlider,    0);
    placeKnob (evenHighpassLabel, evenHighpassSlider, 1);
    placeKnob (evenPresenceLabel, evenPresenceSlider, 2);
    placeKnob (evenOutputLabel,   evenOutputSlider,   3);
    placeKnob (evenDryWetLabel,   evenDryWetSlider,   4);
}
