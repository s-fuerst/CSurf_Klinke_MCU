/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  15 Dec 2009 12:19:27 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEPARAMCOMPONENT_PLUGMODEPARAMCOMPONENT_5238419A__
#define __JUCER_HEADER_PLUGMODEPARAMCOMPONENT_PLUGMODEPARAMCOMPONENT_5238419A__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugMap.h"
class PlugModeComponent;
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeParamComponent : public Component,
                               public Label::Listener,
                               public ComboBox::Listener {
public:
  //==============================================================================
  PlugModeParamComponent(PlugModeComponent *pMC, PMParam *pParam);
  ~PlugModeParamComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateEverything();
  void changeParamId(int paramId);
  void setLearn(bool learn);
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void labelTextChanged(Label *labelThatHasChanged);
  void comboBoxChanged(ComboBox *comboBoxThatHasChanged);
  void mouseDown(const MouseEvent &e);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      void
      updateParameterList();
      void fillDiscreteStepsFromFX(int paramId);

  PMParam *m_pParam;

  PlugModeComponent *m_pMainComponent;
  //[/UserVariables]

  //==============================================================================
  Label *m_nameShort;
  ComboBox *m_parameter;
  Label *m_nameLong;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeParamComponent(const PlugModeParamComponent &);
  const PlugModeParamComponent &operator=(const PlugModeParamComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEPARAMCOMPONENT_PLUGMODEPARAMCOMPONENT_5238419A__
