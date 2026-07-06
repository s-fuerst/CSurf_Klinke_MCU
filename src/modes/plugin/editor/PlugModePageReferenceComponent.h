/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  26 Oct 2009 11:28:11 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEPAGEREFERENCECOMPONENT_PLUGMODEPAGEREFERENCECOMPONENT_A5D80AED__
#define __JUCER_HEADER_PLUGMODEPAGEREFERENCECOMPONENT_PLUGMODEPAGEREFERENCECOMPONENT_A5D80AED__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PlugAccess.h"
class PlugModePageComponent;
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModePageReferenceComponent : public Component,
                                       public Label::Listener,
                                       public ComboBox::Listener {
public:
  //==============================================================================
  PlugModePageReferenceComponent(PlugModePageComponent *pPMPC, PMPage *pPage);
  ~PlugModePageReferenceComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateEverything();
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void labelTextChanged(Label *labelThatHasChanged);
  void comboBoxChanged(ComboBox *comboBoxThatHasChanged);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PMPage *m_pPage;
  PlugModePageComponent *m_pPlugModePageComponent;
  //[/UserVariables]

  //==============================================================================
  GroupComponent *groupComponent2;
  Label *m_nameShort;
  Label *m_nameLong;
  GroupComponent *m_groupReference;
  ComboBox *m_referenceTo;
  Label *m_offset;
  Label *m_offsetLabel;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModePageReferenceComponent(const PlugModePageReferenceComponent &);
  const PlugModePageReferenceComponent &
  operator=(const PlugModePageReferenceComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEPAGEREFERENCECOMPONENT_PLUGMODEPAGEREFERENCECOMPONENT_A5D80AED__
