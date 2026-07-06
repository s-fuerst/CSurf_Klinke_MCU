/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:31:33 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODECHANNELCOMPONENT_PLUGMODECHANNELCOMPONENT_FAA54147__
#define __JUCER_HEADER_PLUGMODECHANNELCOMPONENT_PLUGMODECHANNELCOMPONENT_FAA54147__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugMap.h"
#include "PlugModeSingleChannelComponent.h"
#include "TabbedComponentWithCallback.h"
class PlugModeComponent;
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeChannelComponent : public Component, TabbedCallback {
public:
  //==============================================================================
  PlugModeChannelComponent(PlugModeComponent *pMC, PMPage *pPage);
  ~PlugModeChannelComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeSingleChannelComponent *getSingleChannelComponent(int iChannel) {
    return static_cast<PlugModeSingleChannelComponent *>(
        m_tabbedChannels->getTabContentComponent(iChannel));
  }
  PlugModeSingleChannelComponent *getSelectedChannelComponent() {
    return getSingleChannelComponent(m_tabbedChannels->getCurrentTabIndex());
  }
  void selectedChannelChanged(int iChannel);
  void selectedTabHasChanged() { updateEverything(); }
  void updateEverything();
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PlugModeComponent *m_pMainComponent;
  //[/UserVariables]

  //==============================================================================
  TabbedComponentWithCallback *m_tabbedChannels;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeChannelComponent(const PlugModeChannelComponent &);
  const PlugModeChannelComponent &operator=(const PlugModeChannelComponent &);
};

#endif // __JUCER_HEADER_PLUGMODECHANNELCOMPONENT_PLUGMODECHANNELCOMPONENT_FAA54147__
