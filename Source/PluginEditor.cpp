#include "PluginEditor.h"

namespace
{
    constexpr int kW      = 480;
    constexpr int kH      = 578;
    constexpr int kPad    = 16;
    constexpr int kTitleH = 58;
    constexpr int kKnobH  = 72;
    constexpr int kLabelH = 16;
    constexpr int kBioH   = 26;
    constexpr int kSecHd  = 22;
    constexpr int kGap    = 10;
    constexpr int kSGap   = 6;
    constexpr int kSecH   = kSecHd + kBioH + kLabelH + kKnobH + 20; // 156
    constexpr int kClipH  = kSecHd + kBioH + 6 + 20 + 8;            // 82

    // White & red high-contrast palette
    const juce::Colour kBg      (0xfff5f0f0);
    const juce::Colour kPanel   (0xffffffff);
    const juce::Colour kBorder  (0xffcc2200);
    const juce::Colour kAccent  (0xffcc2200);
    const juce::Colour kText    (0xff111111);
    const juce::Colour kBio     (0xff884444);
}

//==============================================================================
static void setupKnob (juce::Slider& s, juce::Label& lbl,
                       const juce::String& name, juce::Component* parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffcc2200));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xffe8c8c0));
    s.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffaa1100));
    s.setColour (juce::Slider::textBoxTextColourId,         juce::Colour (0xff111111));
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xffffffff));
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    parent->addAndMakeVisible (s);

    lbl.setText (name, juce::dontSendNotification);
    lbl.setFont (juce::FontOptions (10.5f));
    lbl.setColour (juce::Label::textColourId, juce::Colour (0xff884444));
    lbl.setJustificationType (juce::Justification::centred);
    parent->addAndMakeVisible (lbl);
}

//==============================================================================
void AWCascadeEditor::setupBypassButton (juce::TextButton& btn, bool bypassed)
{
    btn.setClickingTogglesState (true);
    btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xfff0e8e8));
    btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffeeee));
    btn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff229922));
    btn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffaa6644));
    btn.setButtonText (bypassed ? "BYPASS" : "ON");
    addAndMakeVisible (btn);
}

//==============================================================================
AWCascadeEditor::AWCascadeEditor (AWCascadeProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      apexLimitAttach    (p.apvts, "apexLimit",      apexLimitSlider),
      apexDryWetAttach   (p.apvts, "apexDryWet",     apexDryWetSlider),
      apexBypassAttach   (p.apvts, "bypassApex",     apexBypassButton),
      evenInputAttach    (p.apvts, "evenInput",       evenInputSlider),
      evenHighpassAttach (p.apvts, "evenHighpass",    evenHighpassSlider),
      evenPresenceAttach (p.apvts, "evenPresence",    evenPresenceSlider),
      evenOutputAttach   (p.apvts, "evenOutput",      evenOutputSlider),
      evenDryWetAttach   (p.apvts, "evenDryWet",      evenDryWetSlider),
      evenBypassAttach   (p.apvts, "bypassEven",      evenBypassButton),
      doubleAttach       (p.apvts, "doubleEffect",    doubleButton),
      velvetCeilingAttach(p.apvts, "velvetCeiling",   velvetCeilingSlider),
      velvetBypassAttach (p.apvts, "bypassVelvet",    velvetBypassButton),
      hardClipAttach     (p.apvts, "hardClip",        hardClipButton),
      swapAttach         (p.apvts, "swapOrder",       swapButton),
      oversampleAttach   (p.apvts, "oversample",      oversampleBox)
{
    setupKnob (apexLimitSlider,    apexLimitLabel,    "Limit",    this);
    setupKnob (apexDryWetSlider,   apexDryWetLabel,   "Dry/Wet",  this);
    setupKnob (evenInputSlider,    evenInputLabel,    "Input",    this);
    setupKnob (evenHighpassSlider, evenHighpassLabel, "Highpass", this);
    setupKnob (evenPresenceSlider, evenPresenceLabel, "Presence", this);
    setupKnob (evenOutputSlider,   evenOutputLabel,   "Output",   this);
    setupKnob (evenDryWetSlider,   evenDryWetLabel,   "Dry/Wet",  this);

    // Bypass buttons
    const bool initBypassApex   = p.apvts.getRawParameterValue ("bypassApex")  ->load() > 0.5f;
    const bool initBypassEven   = p.apvts.getRawParameterValue ("bypassEven")  ->load() > 0.5f;
    const bool initBypassVelvet = p.apvts.getRawParameterValue ("bypassVelvet")->load() > 0.5f;
    setupBypassButton (apexBypassButton,   initBypassApex);
    setupBypassButton (evenBypassButton,   initBypassEven);
    setupBypassButton (velvetBypassButton, initBypassVelvet);
    apexBypassButton.onClick   = [this] {
        apexBypassButton.setButtonText   (apexBypassButton.getToggleState()   ? "BYPASS" : "ON"); };
    evenBypassButton.onClick   = [this] {
        evenBypassButton.setButtonText   (evenBypassButton.getToggleState()   ? "BYPASS" : "ON"); };
    velvetBypassButton.onClick = [this] {
        velvetBypassButton.setButtonText (velvetBypassButton.getToggleState() ? "BYPASS" : "ON"); };

    // Hard clip button (default ON)
    hardClipButton.setClickingTogglesState (true);
    hardClipButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xfff0e8e8));
    hardClipButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfff0e8e8));
    hardClipButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffaaaaaa));
    hardClipButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff229922));
    const bool initHardClip = p.apvts.getRawParameterValue ("hardClip")->load() > 0.5f;
    hardClipButton.setButtonText (initHardClip ? "HARD ON" : "HARD OFF");
    hardClipButton.onClick = [this] {
        const bool on = hardClipButton.getToggleState();
        hardClipButton.setButtonText (on ? "HARD ON" : "HARD OFF");
        velvetCeilingSlider.setEnabled (on);
        velvetCeilingLabel .setEnabled (on);
    };
    addAndMakeVisible (hardClipButton);
    velvetCeilingSlider.setEnabled (initHardClip);
    velvetCeilingLabel .setEnabled (initHardClip);

    // Double effect button
    doubleButton.setClickingTogglesState (true);
    doubleButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xfff0e8e8));
    doubleButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    doubleButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffaaaaaa));
    doubleButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffffffff));
    const bool initDouble = p.apvts.getRawParameterValue ("doubleEffect")->load() > 0.5f;
    doubleButton.setButtonText (initDouble ? "x2 ON" : "x2");
    doubleButton.onClick = [this] {
        doubleButton.setButtonText (doubleButton.getToggleState() ? "x2 ON" : "x2"); };
    addAndMakeVisible (doubleButton);

    // Velvet ceiling slider (horizontal)
    velvetCeilingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    velvetCeilingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 16);
    velvetCeilingSlider.setColour (juce::Slider::trackColourId,            juce::Colour (0xffcc2200));
    velvetCeilingSlider.setColour (juce::Slider::backgroundColourId,       juce::Colour (0xffe8c8c0));
    velvetCeilingSlider.setColour (juce::Slider::thumbColourId,             juce::Colour (0xffaa1100));
    velvetCeilingSlider.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xff111111));
    velvetCeilingSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xffffffff));
    velvetCeilingSlider.setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    addAndMakeVisible (velvetCeilingSlider);

    velvetCeilingLabel.setText ("Ceiling", juce::dontSendNotification);
    velvetCeilingLabel.setFont (juce::FontOptions (10.0f));
    velvetCeilingLabel.setColour (juce::Label::textColourId, juce::Colour (0xff884444));
    velvetCeilingLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (velvetCeilingLabel);

    // Swap button
    swapButton.setClickingTogglesState (true);
    swapButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xfff0e8e8));
    swapButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    swapButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff883322));
    swapButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffffffff));
    addAndMakeVisible (swapButton);

    const bool initSwap = p.apvts.getRawParameterValue ("swapOrder")->load() > 0.5f;
    swapButton.setButtonText (initSwap ? "EVEN  ->  APEX" : "APEX  ->  EVEN");
    swapButton.onClick = [this] {
        swapButton.setButtonText (swapButton.getToggleState() ? "EVEN  ->  APEX"
                                                               : "APEX  ->  EVEN");
        resized();
        repaint();
    };

    // Oversampling ComboBox
    oversampleBox.addItem ("Off", 1);
    oversampleBox.addItem ("2x",  2);
    oversampleBox.addItem ("4x",  3);
    oversampleBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffffffff));
    oversampleBox.setColour (juce::ComboBox::textColourId,       juce::Colour (0xff111111));
    oversampleBox.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xffcc2200));
    oversampleBox.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xffcc2200));
    addAndMakeVisible (oversampleBox);

    setSize (kW, kH);
    startTimerHz (30);
}

AWCascadeEditor::~AWCascadeEditor()
{
    stopTimer();
}

//==============================================================================
void AWCascadeEditor::timerCallback()
{
    // Sync bypass button text (handles state restore / external automation)
    apexBypassButton.setButtonText   (apexBypassButton.getToggleState()   ? "BYPASS" : "ON");
    evenBypassButton.setButtonText   (evenBypassButton.getToggleState()   ? "BYPASS" : "ON");
    velvetBypassButton.setButtonText (velvetBypassButton.getToggleState() ? "BYPASS" : "ON");
    doubleButton.setButtonText       (doubleButton.getToggleState()       ? "x2 ON"  : "x2");
    hardClipButton.setButtonText (hardClipButton.getToggleState() ? "HARD ON" : "HARD OFF");
    const bool hcOn = hardClipButton.getToggleState();
    velvetCeilingSlider.setEnabled (hcOn);
    velvetCeilingLabel .setEnabled (hcOn);

    // Peak-hold meter decay (attack = instant, release = ~0.85 per frame at 30Hz ≈ 9dB/s)
    auto decay = [](float cur, float incoming) -> float {
        return incoming > cur ? incoming : cur * 0.85f;
    };
    dispInL  = decay (dispInL,  processor.meterInL.load());
    dispInR  = decay (dispInR,  processor.meterInR.load());
    dispOutL = decay (dispOutL, processor.meterOutL.load());
    dispOutR = decay (dispOutR, processor.meterOutR.load());

    repaint (0, 0, kW, kTitleH);
}

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

    // ---- Title bar ----
    g.setColour (kPanel);
    g.fillRect (0, 0, kW, kTitleH);
    g.setColour (kAccent);
    g.fillRect (0, kTitleH - 2, kW, 2);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("The Press", 0, 4, kW, 26, juce::Justification::centred);

    // ---- Meters ----
    auto drawMeter = [&] (float level, int x, int y, int maxW, int h)
    {
        const float levelDb = level < 1e-5f ? -100.0f : 20.0f * std::log10 (level);
        const float norm    = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
        const int   barW    = (int)(norm * maxW);

        g.setColour (juce::Colour (0xffe8c8c0));
        g.fillRect (x, y, maxW, h);
        if (barW > 0)
        {
            const juce::Colour col = levelDb > -6.0f  ? juce::Colour (0xffff2200)
                                   : levelDb > -18.0f ? juce::Colour (0xffcc2200)
                                                       : juce::Colour (0xffaa4433);
            g.setColour (col);
            g.fillRect (x, y, barW, h);
        }
    };

    const int mW  = kW / 2 - kPad - 10;
    const int mXi = kPad + 4;
    const int mXo = kW / 2 + 6;
    // Labels sit below the title text, bars below the labels
    const int mLabelY = 34;
    const int mY1     = 44;
    const int mY2     = 50;

    g.setFont (juce::FontOptions (8.0f));
    g.setColour (juce::Colour (0xff884444));
    g.drawText ("IN",  mXi, mLabelY, 20, 8, juce::Justification::left);
    g.drawText ("OUT", mXo, mLabelY, 24, 8, juce::Justification::left);

    drawMeter (dispInL,  mXi, mY1, mW, 4);
    drawMeter (dispInR,  mXi, mY2, mW, 4);
    drawMeter (dispOutL, mXo, mY1, mW, 4);
    drawMeter (dispOutR, mXo, mY2, mW, 4);

    // ---- Section panels ----
    const bool swapped = swapButton.getToggleState();

    struct SecInfo { juce::String tag, bio; };
    const SecInfo sec1 = swapped
        ? SecInfo { "EVEN DRIVE",   "Generates even-order harmonics, analogue weight, and leaves transients untouched." }
        : SecInfo { "APEX LIMITER", "Tracks waveform velocity and softens only the sharpest transient edges." };
    const SecInfo sec2 = swapped
        ? SecInfo { "APEX LIMITER", "Tracks waveform velocity and softens only the sharpest transient edges." }
        : SecInfo { "EVEN DRIVE",   "Generates even-order harmonics, analogue weight, and leaves transients untouched." };

    auto drawSection = [&] (const SecInfo& s, int y, int h)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle ((float)kPad, (float)y, (float)(kW - kPad*2), (float)h, 5.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle ((float)kPad, (float)y, (float)(kW - kPad*2), (float)h, 5.0f, 1.0f);

        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        const int tagW = 110;
        g.setColour (kAccent);
        g.fillRoundedRectangle ((float)(kPad + 8), (float)(y + 7), (float)tagW, 16.0f, 3.0f);
        g.setColour (kText);
        g.drawText (s.tag, kPad + 8, y + 7, tagW, 16, juce::Justification::centred);

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (kBio);
        g.drawText (s.bio, kPad + 10, y + 27, kW - kPad*2 - 20, kBioH,
                    juce::Justification::centredLeft, true);
    };

    const int y1    = kTitleH + kGap;
    const int ySwap = y1 + kSecH + kSGap;
    const int y2    = ySwap + 36 + kSGap;
    const int yClip = y2 + kSecH + kSGap;
    const int yOs   = yClip + kClipH + kSGap;

    drawSection (sec1, y1, kSecH);
    drawSection (sec2, y2, kSecH);

    // ---- Velvet Clip panel ----
    g.setColour (kPanel);
    g.fillRoundedRectangle ((float)kPad, (float)yClip, (float)(kW - kPad*2), (float)kClipH, 5.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle ((float)kPad, (float)yClip, (float)(kW - kPad*2), (float)kClipH, 5.0f, 1.0f);
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

    // ---- Oversampling row ----
    g.setFont (juce::FontOptions (10.5f));
    g.setColour (kText);
    g.drawText ("Oversample:", 0, yOs + 5, kW / 2 + 20, 18, juce::Justification::centredRight);

    // ---- Footer ----
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colour (0xffaa4433));
    g.drawText ("Made by Zeus", 0, kH - 16, kW, 14, juce::Justification::centred);
}

void AWCascadeEditor::resized()
{
    const bool swapped = swapButton.getToggleState();

    const int y1    = kTitleH + kGap;
    const int ySwap = y1 + kSecH + kSGap;
    const int y2    = ySwap + 36 + kSGap;
    const int yClip = y2 + kSecH + kSGap;
    const int yOs   = yClip + kClipH + kSGap;

    swapButton.setBounds ((kW - 188) / 2, ySwap + 4, 188, 27);

    const int relY    = kSecHd + kBioH + 4;
    const int bypassX = kW - kPad - 8 - 60;

    if (!swapped)
    {
        placeApexKnobs (y1 + relY);
        apexBypassButton.setBounds (bypassX, y1 + 5, 60, 16);
        placeEvenKnobs (y2 + relY);
        evenBypassButton.setBounds (bypassX,      y2 + 5, 60, 16);
        doubleButton    .setBounds (bypassX - 46, y2 + 5, 42, 16);
    }
    else
    {
        placeEvenKnobs (y1 + relY);
        evenBypassButton.setBounds (bypassX,      y1 + 5, 60, 16);
        doubleButton    .setBounds (bypassX - 46, y1 + 5, 42, 16);
        placeApexKnobs (y2 + relY);
        apexBypassButton.setBounds (bypassX, y2 + 5, 60, 16);
    }

    // Velvet section
    velvetBypassButton.setBounds (bypassX,      yClip + 5, 60, 16);
    hardClipButton    .setBounds (bypassX - 72, yClip + 5, 68, 16);
    const int ceilX = kPad + 10;
    const int ceilY = yClip + kSecHd + kBioH + 6;
    velvetCeilingLabel.setBounds  (ceilX, ceilY, 52, 16);
    velvetCeilingSlider.setBounds (ceilX + 54, ceilY, kW - kPad*2 - 20 - 54, 20);

    // Oversampling ComboBox
    oversampleBox.setBounds (kW / 2 + 24, yOs + 3, 90, 22);
}
