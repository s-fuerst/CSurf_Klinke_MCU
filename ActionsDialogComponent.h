/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  16 Jan 2011 11:09:09 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_ACTIONSDIALOGCOMPONENT_ACTIONSDIALOGCOMPONENT_722363D1__
#define __JUCER_HEADER_ACTIONSDIALOGCOMPONENT_ACTIONSDIALOGCOMPONENT_722363D1__

//[Headers]     -- You can add your own extra header files here --
#include <src/juce_WithoutMacros.h>
class ActionsDisplay;
//[/Headers]

//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class ActionsDialogComponent : public Component,
                               public LabelListener,
                               public ButtonListener {
public:
  //==============================================================================
  ActionsDialogComponent(ActionsDisplay *pAD);
  ~ActionsDialogComponent();

  //==============================================================================
  void paint(Graphics &g);
  void resized();
  void labelTextChanged(Label *labelThatHasChanged);
  void buttonClicked(Button *buttonThatWasClicked);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private : void
                fillActionLabels();

  //==============================================================================
  ToggleButton *m_shift;
  ToggleButton *m_option;
  ToggleButton *m_control;
  ToggleButton *m_alt;
  Label *m_labelAction[8];

  ActionsDisplay *m_pActionDisplay;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  ActionsDialogComponent(const ActionsDialogComponent &);
  const ActionsDialogComponent &operator=(const ActionsDialogComponent &);
};

#endif // __JUCER_HEADER_ACTIONSDIALOGCOMPONENT_ACTIONSDIALOGCOMPONENT_722363D1__
