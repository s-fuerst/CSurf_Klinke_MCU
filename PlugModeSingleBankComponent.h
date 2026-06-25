/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:20:42 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODESINGLEBANKCOMPONENT_PLUGMODESINGLEBANKCOMPONENT_257734C__
#define __JUCER_HEADER_PLUGMODESINGLEBANKCOMPONENT_PLUGMODESINGLEBANKCOMPONENT_257734C__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugModeSinglePageComponent.h"
//[/Headers]

#include "PlugModeBankReferenceComponent.h"
#include "PlugModePageComponent.h"


//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeSingleBankComponent : public Component {
public:
  //==============================================================================
  PlugModeSingleBankComponent(PlugModeComponent *pMC,
                              PlugModeBankComponent *pPMBC, PMBank *pBank);
  ~PlugModeSingleBankComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeBankReferenceComponent *getReferenceComponent() {
    return m_nameAndReference;
  }
  PlugModeSinglePageComponent *getPageComponent(int iPage) {
    return m_pageComponent->getSinglePageComponent(iPage);
  }
  PlugModeSinglePageComponent *getSelectedPageComponent() {
    return m_pageComponent->getSelectedPageComponent();
  }
  PlugModePageComponent *getPageComponent() { return m_pageComponent; }
  void updateEverything();
  void makePageComponentVisible(bool shouldBeVisible);
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
      PlugModeBankReferenceComponent *m_nameAndReference;
  PlugModePageComponent *m_pageComponent;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeSingleBankComponent(const PlugModeSingleBankComponent &);
  const PlugModeSingleBankComponent &
  operator=(const PlugModeSingleBankComponent &);
};

#endif // __JUCER_HEADER_PLUGMODESINGLEBANKCOMPONENT_PLUGMODESINGLEBANKCOMPONENT_257734C__
