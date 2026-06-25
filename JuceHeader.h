// ===========================================================================
//  JuceHeader.h — project-wide JUCE umbrella header (JUCE 8 upgrade branch)
// ===========================================================================
//  Replaces the JUCE 1.52 omnibus headers (juce.h, juce_amalgamated.h,
//  src/juce_WithoutMacros.h) that the source previously included.
//
//  Why a hand-written umbrella instead of the Projucer-generated JuceHeader.h:
//  the GUI editor was removed from the Projucer in JUCE 8.0.1 and our
//  *Component.h files are Jucer 1.11-generated; this header gives us a stable,
//  version-controlled single point of include without depending on a generated
//  file whose generator no longer exists for our workflow.
//
//  juce_gui_basics pulls in the modules the plugin needs transitively:
//    juce_core (String, XmlElement, File, Time, ...)
//    juce_events (Timer, MessageManager, AsyncUpdater, ...)
//    juce_graphics (Font, Colour, Graphics, ...)
//    juce_data_structures (ValueTree, ...)
//    juce_gui_basics  (Component, Label, TextEditor, ComboBox, Slider, ...)
//
//  `using namespace juce;` restores the JUCE 1.52 behaviour where classes were
//  available unqualified (juce.h ended with `using namespace JUCE_NAMESPACE;`).
//  JUCE 8 keeps everything in juce:: and does NOT add the using-directive, so
//  without this line every unqualified Component/Label/... reference in the
//  existing source would fail to compile. Keeping it global minimises the
//  source diff for the upgrade experiment.
//
//  klinkeInitLookAndFeel() applies the dark colour scheme to the default
//  LookAndFeel (LookAndFeel_V4 in JUCE 8). MUST be called once before any
//  JUCE window is created. JUCE 8's default light theme otherwise renders
//  white text on white backgrounds for many components.
// ===========================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

inline void klinkeInitLookAndFeel() {
    auto& lf = dynamic_cast<LookAndFeel_V4&>(LookAndFeel::getDefaultLookAndFeel());
    lf.setColourScheme(LookAndFeel_V4::getDarkColourScheme());
}
