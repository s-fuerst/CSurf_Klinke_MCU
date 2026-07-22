/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

// ===========================================================================
//  JuceHeader.h — project-wide JUCE umbrella header (JUCE 8 upgrade branch)
// ===========================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

// JUCE 8 LookAndFeel_V4 renders with different default colours than JUCE 1.52.
// This helper forces readable classic colours on the global LookAndFeel.
// Call once on plugin init, before any JUCE window opens.
// Added in minimal steps to avoid breaking dialog interactivity.
inline void klinkeInitLookAndFeel()
{
    auto& lf = LookAndFeel::getDefaultLookAndFeel();

    // Keep text readable on JUCE 8 dialogs.
    lf.setColour (Label::textColourId,              Colours::black);
    lf.setColour (TextEditor::textColourId,          Colours::black);
    lf.setColour (ComboBox::textColourId,            Colours::black);
    lf.setColour (ListBox::textColourId,             Colours::black);
    lf.setColour (ToggleButton::textColourId,        Colours::black);
    lf.setColour (Slider::textBoxTextColourId,       Colours::black);
    lf.setColour (GroupComponent::textColourId,      Colours::black);
    lf.setColour (TextButton::textColourOffId,       Colours::black);
    lf.setColour (TextButton::textColourOnId,        Colours::black);
}
