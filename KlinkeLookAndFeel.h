// KlinkeLookAndFeel.h — minimal LookAndFeel_V4 override for JUCE 8
// Fixes white-on-white text without affecting dialog interactivity.
// Applied per-window, not globally.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class KlinkeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KlinkeLookAndFeel()
        : LookAndFeel_V4 (LookAndFeel_V4::getLightColourScheme()) {}

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        // JUCE 8 V4 sometimes draws editable labels with the wrong text colour
        // against white backgrounds. Force black text.
        if (label.isEditableOnSingleClick() || label.isEditable())
            g.setColour (juce::Colours::black);
        LookAndFeel_V4::drawLabel (g, label);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        // Force a visible tick colour so checkboxes show up on light backgrounds.
        g.setColour (button.findColour (juce::ToggleButton::tickColourId, true));
        LookAndFeel_V4::drawToggleButton (g, button,
                                          shouldDrawButtonAsHighlighted,
                                          shouldDrawButtonAsDown);
    }
};
