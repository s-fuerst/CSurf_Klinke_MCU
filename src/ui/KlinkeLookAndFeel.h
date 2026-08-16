/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

// KlinkeLookAndFeel.h — minimal LookAndFeel_V4 override for JUCE 8
// Fixes white-on-white text without affecting dialog interactivity.
// Applied per-window, not globally.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class KlinkeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KlinkeLookAndFeel()
        : LookAndFeel_V4 (LookAndFeel_V4::getLightColourScheme())
    {
        // Keep text editors readable even when a component does not assign
        // explicit colours. This also covers the temporary editor JUCE creates
        // for editable Labels.
        setColour (juce::TextEditor::textColourId, juce::Colours::black);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff4f6f9f));
        setColour (juce::Label::textColourId, juce::Colours::black);
        setColour (juce::Label::textWhenEditingColourId, juce::Colours::black);
        setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        // Mapping names are editable Labels. Set the Label colour IDs,
        // rather than TextEditor IDs, so this also affects the temporary
        // editor JUCE creates while a name is being edited.
        if (label.isEditableOnSingleClick() || label.isEditable())
        {
            label.setColour (juce::Label::textColourId, juce::Colours::black);
            label.setColour (juce::Label::textWhenEditingColourId, juce::Colours::black);
        }
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
