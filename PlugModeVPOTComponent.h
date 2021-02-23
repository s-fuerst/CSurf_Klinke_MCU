/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  30 Nov 2009 11:04:35 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEVPOTCOMPONENT_PLUGMODEVPOTCOMPONENT_859C1EC8__
#define __JUCER_HEADER_PLUGMODEVPOTCOMPONENT_PLUGMODEVPOTCOMPONENT_859C1EC8__

//[Headers]     -- You can add your own extra header files here --
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
//#include <src/juce_DefineMacros.h>
#include "PlugMap.h"
#include "PlugModeVPOTTableComponent.h"
//[/Headers]

#include "PlugModeParamComponent.h"


//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeVPOTComponent : public Component, public ButtonListener {
public:
  //==============================================================================
  PlugModeVPOTComponent(PlugModeComponent *pMC, PMVPot *pVPot);
  ~PlugModeVPOTComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateEverything();
  PlugModeParamComponent *getParamComponent() { return m_params; }
  void changeParamId(int paramId, double value, String paramName);
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void buttonClicked(Button *buttonThatWasClicked);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PlugModeComponent *m_pMainComponent;
  PMVPot *m_pVPot;
  //[/UserVariables]

  //==============================================================================
  GroupComponent *m_vpotGroup;
  PlugModeVPOTTableComponent *m_tableComponent;
  PlugModeParamComponent *m_params;
  TextButton *m_copyButton;
  TextButton *m_pasteButton;
  TextButton *m_clearButton;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeVPOTComponent(const PlugModeVPOTComponent &);
  const PlugModeVPOTComponent &operator=(const PlugModeVPOTComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEVPOTCOMPONENT_PLUGMODEVPOTCOMPONENT_859C1EC8__
