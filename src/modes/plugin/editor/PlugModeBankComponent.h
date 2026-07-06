/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:13:09 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEBANKCOMPONENT_PLUGMODEBANKCOMPONENT_59EA7095__
#define __JUCER_HEADER_PLUGMODEBANKCOMPONENT_PLUGMODEBANKCOMPONENT_59EA7095__

//[Headers]     -- You can add your own extra header files here --
#include "PlugAccess.h"
#include "JuceHeader.h"
#include "TabbedComponentWithCallback.h"
class PlugModeComponent;
class PlugModeSingleBankComponent;
//[/Headers]

//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeBankComponent : public Component, TabbedCallback {
public:
  //==============================================================================
  PlugModeBankComponent(PlugMap *pMap, PlugModeComponent *pMC);
  ~PlugModeBankComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeSingleBankComponent *getSingleBankComponent(int iBank);
  PlugModeSingleBankComponent *
  getSelectedBankComponent(); // can return NULL if Map Info is selected

  void selectedBankChanged(int iBank) {
    m_tabbedBanks->setCurrentTabIndex(iBank);
  }
  void updateBankNames();
  void updateEverything();
  void selectedTabHasChanged();

  void setTabVisible(bool shouldBeVisible);
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PlugMap *m_pMap;
  PlugModeComponent *m_pMainComponent;
  //[/UserVariables]

  //==============================================================================
  TabbedComponentWithCallback *m_tabbedBanks;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeBankComponent(const PlugModeBankComponent &);
  const PlugModeBankComponent &operator=(const PlugModeBankComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEBANKCOMPONENT_PLUGMODEBANKCOMPONENT_59EA7095__
