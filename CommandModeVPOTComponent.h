/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  28 Sep 2009 4:00:13 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_COMMANDMODEVPOTCOMPONENT_COMMANDMODEVPOTCOMPONENT_449361AD__
#define __JUCER_HEADER_COMMANDMODEVPOTCOMPONENT_COMMANDMODEVPOTCOMPONENT_449361AD__

//[Headers]     -- You can add your own extra header files here --
#include "csurf_mcu.h"
#include "JuceHeader.h"
#include "CommandMode.h"
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class CommandModeVPOTComponent : public Component,
                                 public LabelListener,
                                 public ButtonListener,
                                 public SliderListener {
public:
  //==============================================================================
  CommandModeVPOTComponent(CommandMode::Page *pPage, int shift, int channel);
  ~CommandModeVPOTComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void labelTextChanged(Label *labelThatHasChanged);
  void buttonClicked(Button *buttonThatWasClicked);
  void sliderValueChanged(Slider *sliderThatWasMoved);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      CommandMode::Page *m_pPage;
  int m_shift;
  int m_channel;
  //[/UserVariables]

  //==============================================================================
  GroupComponent *groupComponent;
  Label *actionLabel;
  ToggleButton *relativeButton;
  Slider *normalSpeedSlider;
  Slider *pressedSpeedSlider;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  CommandModeVPOTComponent(const CommandModeVPOTComponent &);
  const CommandModeVPOTComponent &operator=(const CommandModeVPOTComponent &);
};

#endif // __JUCER_HEADER_COMMANDMODEVPOTCOMPONENT_COMMANDMODEVPOTCOMPONENT_449361AD__
