/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  13 Nov 2010 1:21:11 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_TRACKSTATESEDITORCOMPONENT_TRACKSTATESEDITORCOMPONENT_2F1DDE68__
#define __JUCER_HEADER_TRACKSTATESEDITORCOMPONENT_TRACKSTATESEDITORCOMPONENT_2F1DDE68__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
class TrackStatesTableComponent;
//[/Headers]

//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class TrackStatesEditorComponent : public Component {
public:
  //==============================================================================
  TrackStatesEditorComponent();
  ~TrackStatesEditorComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      //[/UserVariables]

      //==============================================================================
      TrackStatesTableComponent *component;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  TrackStatesEditorComponent(const TrackStatesEditorComponent &);
  const TrackStatesEditorComponent &
  operator=(const TrackStatesEditorComponent &);
};

#endif // __JUCER_HEADER_TRACKSTATESEDITORCOMPONENT_TRACKSTATESEDITORCOMPONENT_2F1DDE68__
