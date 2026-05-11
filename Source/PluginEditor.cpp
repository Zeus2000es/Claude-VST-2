#include "PluginEditor.h"

namespace
{
    constexpr int kW      = 480;
    constexpr int kH      = 512;
    constexpr int kPad    = 16;
    constexpr int kTitleH = 46;
    constexpr int kKnobH  = 72;
    constexpr int kLabelH = 16;
    constexpr int kBioH   = 26;
    constexpr int kSecHd  = 22;
    constexpr int kGap    = 10;
    constexpr int kSGap   = 6;
    constexpr int kSecH   = kSecHd + kBioH + kLabelH + kKnobH + 20; // 156

    // Red & black high-contrast palette
    const juce::Colour kBg      (0xff080808);
    const juce::Colour kPanel   (0xff1a0000);
    const juce::Colour kBorder  (0xff5c0000);
    const juce::Colour kAccent  (0xffcc0000);
    const juce::Colour kKnob    (0xffdd1111);
    const juce::Colour kText    (0xffffffff);
    const juce::Colour kBio     (0xffcc9090);
    const juce::Colour kTag     (0xffff2222);
}

//==============================================================================
static void setupKnob (juce::Slider& s, juce::Label& lbl,
                       const juce::String& name, juce::Component* parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffdd1111));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff5c0000));
    s.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffff4444));
    s.setColour (juce::Slider::textBoxTextColourId,         juce::Colour (0xffffffff));
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xff1a0000));
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    parent->addAndMakeVisible (s);

    lbl.setText (name, juce::dontSendNotification);
    lbl.setFont (juce::FontOptions (10.5f));
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
      evenDryWetAttach   (p.apvts, "evenDryWet",    evenDryWetSlider),
      swapAttach         (p.apvts, "swapOrder",     swapButton)
{
    setupKnob (apexLimitSlider,    apexLimitLabel,    "Limit",    this);
    setupKnob (apexDryWetSlider,   apexDryWetLabel,   "Dry/Wet",  this);
    setupKnob (evenInputSlider,    evenInputLabel,    "Input",    this);
    setupKnob (evenHighpassSlider, evenHighpassLabel, "Highpass", this);
    setupKnob (evenPresenceSlider, evenPresenceLabel, "Presence", this);
    setupKnob (evenOutputSlider,   evenOutputLabel,   "Output",   this);
    setupKnob (evenDryWetSlider,   evenDryWetLabel,   "Dry/Wet",  this);

    swapButton.setClickingTogglesState (true);
    swapButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1a0000));
    swapButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff6b0000));
    swapButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffcc7777));
    swapButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffffffff));
    addAndMakeVisible (swapButton);

    // Sync button text with initial parameter state
    const bool initSwap = p.apvts.getRawParameterValue ("swapOrder")->load() > 0.5f;
    swapButton.setButtonText (initSwap ? "EVEN  ->  APEX" : "APEX  ->  EVEN");

    swapButton.onClick = [this] {
        swapButton.setButtonText (swapButton.getToggleState() ? "EVEN  ->  APEX"
                                                               : "APEX  ->  EVEN");
        resized();
        repaint();
    };

    setSize (kW, kH);
}

AWCascadeEditor::~AWCascadeEditor() {}

//==============================================================================
void AWCascadeEditor::placeApexKnobs (int knobY)
{
    const int halfW = (kW - kPad * 2) / 2;
    apexLimitLabel .setBounds (kPad,         knobY,           halfW, kLabelH);
    apexLimitSlider.setBounds (kPad,         knobY + kLabelH, halfW, kKnobH);
    apexDryWetLabel .setBounds(kPad + halfW, knobY,           halfW, kLabelH);
    apexDryWetSlider.setBounds(kPad + halfW, knobY + kLabelH, halfW, kKnobH);
}

void AWCascadeEditor::placeEvenKnobs (int knobY)
{
    const int slot = (kW - kPad * 2) / 5;
    auto place = [&] (juce::Label& lbl, juce::Slider& sl, int idx) {
        const int x = kPad + idx * slot;
        lbl.setBounds (x, knobY,           slot, kLabelH);
        sl .setBounds (x, knobY + kLabelH, slot, kKnobH);
    };
    place (evenInputLabel,    evenInputSlider,    0);
    place (evenHighpassLabel, evenHighpassSlider, 1);
    place (evenPresenceLabel, evenPresenceSlider, 2);
    place (evenOutputLabel,   evenOutputSlider,   3);
    place (evenDryWetLabel,   evenDryWetSlider,   4);
}

//==============================================================================
void AWCascadeEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Title bar
    g.setColour (kPanel);
    g.fillRect (0, 0, kW, kTitleH);
    g.setColour (kAccent);
    g.fillRect (0, kTitleH - 2, kW, 2);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("The Press", 0, 0, kW, kTitleH, juce::Justification::centred);

    const bool swapped = swapButton.getToggleState();

    // Section header data based on swap
    struct SecInfo { juce::String tag, bio; };
    const SecInfo sec1 = swapped
        ? SecInfo { "EVEN DRIVE",    "Generates even-order harmonics, analogue weight, and leaves transients untouched." }
        : SecInfo { "APEX LIMITER",  "Tracks waveform velocity and softens only the sharpest transient edges." };
    const SecInfo sec2 = swapped
        ? SecInfo { "APEX LIMITER",  "Tracks waveform velocity and softens only the sharpest transient edges." }
        : SecInfo { "EVEN DRIVE",    "Generates even-order harmonics, analogue weight, and leaves transients untouched." };

    auto drawSection = [&] (const SecInfo& s, int y, int h)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle ((float)kPad, (float)y, (float)(kW - kPad*2), (float)h, 5.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle ((float)kPad, (float)y, (float)(kW - kPad*2), (float)h, 5.0f, 1.0f);

        // Tag badge
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        const int tagW = 110;
        g.setColour (kAccent);
        g.fillRoundedRectangle ((float)(kPad + 8), (float)(y + 7), (float)tagW, 16.0f, 3.0f);
        g.setColour (kText);
        g.drawText (s.tag, kPad + 8, y + 7, tagW, 16, juce::Justification::centred);

        // Bio
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (kBio);
        g.drawText (s.bio, kPad + 10, y + 27, kW - kPad*2 - 20, kBioH,
                    juce::Justification::centredLeft, true);
    };

    const int y1     = kTitleH + kGap;
    const int ySwap  = y1 + kSecH + kSGap;
    const int y2     = ySwap + 36 + kSGap;
    const int yClip  = y2 + kSecH + kSGap;

    drawSection (sec1, y1,    kSecH);
    drawSection (sec2, y2,    kSecH);

    // Velvet Clip section
    const int clipH = kSecHd + kBioH + 14;
    g.setColour (kPanel);
    g.fillRoundedRectangle ((float)kPad, (float)yClip, (float)(kW - kPad*2), (float)clipH, 5.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle ((float)kPad, (float)yClip, (float)(kW - kPad*2), (float)clipH, 5.0f, 1.0f);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.setColour (kAccent);
    g.fillRoundedRectangle ((float)(kPad + 8), (float)(yClip + 7), 110.0f, 16.0f, 3.0f);
    g.setColour (kText);
    g.drawText ("VELVET CLIP", kPad + 8, yClip + 7, 110, 16, juce::Justification::centred);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kBio);
    g.drawText ("Transparent soft ceiling. Catches output peaks.",
                kPad + 10, yClip + 27, kW - kPad*2 - 20, kBioH,
                juce::Justification::centredLeft);

    // Footer
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colour (0xff661111));
    g.drawText ("Made by Zeus", 0, kH - 16, kW, 14, juce::Justification::centred);
}

void AWCascadeEditor::resized()
{
    const bool swapped = swapButton.getToggleState();

    const int y1    = kTitleH + kGap;
    const int ySwap = y1 + kSecH + kSGap;
    const int y2    = ySwap + 36 + kSGap;

    // Swap button centered between sections
    swapButton.setBounds ((kW - 188) / 2, ySwap + 4, 188, 27);

    // Knob offset inside section (below header + bio)
    const int relY = kSecHd + kBioH + 4;

    if (!swapped) {
        placeApexKnobs (y1 + relY);
        placeEvenKnobs (y2 + relY);
    } else {
        placeEvenKnobs (y1 + relY);
        placeApexKnobs (y2 + relY);
    }
}
