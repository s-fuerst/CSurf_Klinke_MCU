/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  29 Aug 2010 9:53:47 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODECOMPONENT_PLUGMODECOMPONENT_A9A481D3__
#define __JUCER_HEADER_PLUGMODECOMPONENT_PLUGMODECOMPONENT_A9A481D3__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugAccess.h"
#include "PlugModeSingleBankComponent.h"
class PluginWatcher;
//[/Headers]

#include "PlugModeBankComponent.h"


//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeComponent : public Component, public ButtonListener {
public:
  //==============================================================================
  PlugModeComponent(PlugAccess *pPA);
  ~PlugModeComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void selectedBankChanged(int iBank);
  void selectedPageChanged(int iPage);
  void selectedChannelChanged(int iChannel, bool fader);
	void changePlug(PlugAccess* pPA);
  void updateEverything();
  void updateLearnStatus();
  PlugAccess *getPlugAccess() { return m_pPlugAccess; }
  bool isUseParamName() { return m_useParamName->getToggleState(); }

  void selectedPluginChanged(MediaTrack *pMediaTrack, int iSlot);

  void watchedPluginParameterChanged(MediaTrack *pMediaTrack, int iSlot,
                                     int iParameter, double dValue,
                                     String strValue);

  void paramComponentPressed(PlugModeParamComponent *pPC);

  PMVPot::tSteps *getStepTableClipBoard() { return &m_stepTableClipBoard; }
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void buttonClicked(Button *buttonThatWasClicked);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PlugModeSingleBankComponent *
      getBankComponent(int iBank) {
    return m_bankComponent->getSingleBankComponent(iBank);
  }
  PlugAccess *m_pPlugAccess;
  bool m_learnFader;
  int m_paramChangedConnectionId;
  PMVPot::tSteps m_stepTableClipBoard;
  //[/UserVariables]

  //==============================================================================
  GroupComponent *m_groupFile;
  Label *m_mappingFile;
  TextButton *m_save;
  ToggleButton *m_autosave;
  PlugModeBankComponent *m_bankComponent;
  ToggleButton *m_learn;
  TextButton *m_saveAs;
  ToggleButton *m_useParamName;
  TextButton *m_clear;
  ToggleButton *m_local;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeComponent(const PlugModeComponent &);
  const PlugModeComponent &operator=(const PlugModeComponent &);
};

#endif // __JUCER_HEADER_PLUGMODECOMPONENT_PLUGMODECOMPONENT_A9A481D3__
