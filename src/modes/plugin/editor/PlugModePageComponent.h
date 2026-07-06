/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:21:27 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEPAGECOMPONENT_PLUGMODEPAGECOMPONENT_1B35486A__
#define __JUCER_HEADER_PLUGMODEPAGECOMPONENT_PLUGMODEPAGECOMPONENT_1B35486A__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugMap.h"
#include "PlugModeSinglePageComponent.h"
class PlugModeComponent;
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModePageComponent : public Component, TabbedCallback {
public:
  //==============================================================================
  PlugModePageComponent(PlugModeComponent *pMC, PMBank *pBank);
  ~PlugModePageComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeSinglePageComponent *getSinglePageComponent(int iBank) {
    return static_cast<PlugModeSinglePageComponent *>(
        m_tabbedPages->getTabContentComponent(iBank));
  }
  PlugModeSinglePageComponent *getSelectedPageComponent() {
    return getSinglePageComponent(m_tabbedPages->getCurrentTabIndex());
  }
  void selectedPageChanged(int iPage) {
    m_tabbedPages->setCurrentTabIndex(iPage);
  }
  void selectedTabHasChanged();
  void updatePageNames();
  void updateEverything();

  void setTabVisible(bool shouldBeVisible);
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PMBank *m_pBank;
  PlugModeComponent *m_pMainComponent;
  //[/UserVariables]

  //==============================================================================
  TabbedComponentWithCallback *m_tabbedPages;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModePageComponent(const PlugModePageComponent &);
  const PlugModePageComponent &operator=(const PlugModePageComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEPAGECOMPONENT_PLUGMODEPAGECOMPONENT_1B35486A__
