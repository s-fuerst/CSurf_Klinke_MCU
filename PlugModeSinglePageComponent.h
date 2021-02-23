/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:33:00 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODESINGLEPAGECOMPONENT_PLUGMODESINGLEPAGECOMPONENT_B9F0057B__
#define __JUCER_HEADER_PLUGMODESINGLEPAGECOMPONENT_PLUGMODESINGLEPAGECOMPONENT_B9F0057B__

//[Headers]     -- You can add your own extra header files here --
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "PlugModeSingleChannelComponent.h"

class PlugModeFaderComponent;
class PlugModeVPOTComponent;
//[/Headers]

#include "PlugModeChannelComponent.h"
#include "PlugModePageReferenceComponent.h"

//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeSinglePageComponent : public Component {
public:
  //==============================================================================
  PlugModeSinglePageComponent(PlugModeComponent *pMC,
                              PlugModePageComponent *pPMPC, PMPage *pPage);
  ~PlugModeSinglePageComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeChannelComponent *getChannelComponent() { return m_channelComponent; }
  PlugModeSingleChannelComponent *getSelectedChannelComponent() {
    return m_channelComponent->getSelectedChannelComponent();
  }
  PlugModeFaderComponent *getFaderComponent(int iFader) {
    return m_channelComponent->getSingleChannelComponent(iFader)->getFader();
  }
  PlugModeVPOTComponent *getVPOTComponent(int iFader) {
    return m_channelComponent->getSingleChannelComponent(iFader)->getVPOT();
  }
  void updateEverything();
  void makeChannelComponentVisible(bool shouldBeVisible);
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
      PlugModeChannelComponent *m_channelComponent;
  PlugModePageReferenceComponent *m_nameAndReference;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeSinglePageComponent(const PlugModeSinglePageComponent &);
  const PlugModeSinglePageComponent &
  operator=(const PlugModeSinglePageComponent &);
};

#endif // __JUCER_HEADER_PLUGMODESINGLEPAGECOMPONENT_PLUGMODESINGLEPAGECOMPONENT_B9F0057B__
