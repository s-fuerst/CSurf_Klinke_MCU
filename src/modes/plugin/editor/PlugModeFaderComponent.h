/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:29:22 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEFADERCOMPONENT_PLUGMODEFADERCOMPONENT_4644A0B1__
#define __JUCER_HEADER_PLUGMODEFADERCOMPONENT_PLUGMODEFADERCOMPONENT_4644A0B1__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
//[/Headers]

#include "PlugModeParamComponent.h"


//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeFaderComponent : public Component {
public:
  //==============================================================================
  PlugModeFaderComponent(PlugModeComponent *pMC, PMFader *pFader);
  ~PlugModeFaderComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateEverything() { m_params->updateEverything(); }
  PlugModeParamComponent *getParamComponent() { return m_params; }
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
      GroupComponent *m_faderGroup;
  PlugModeParamComponent *m_params;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeFaderComponent(const PlugModeFaderComponent &);
  const PlugModeFaderComponent &operator=(const PlugModeFaderComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEFADERCOMPONENT_PLUGMODEFADERCOMPONENT_4644A0B1__
