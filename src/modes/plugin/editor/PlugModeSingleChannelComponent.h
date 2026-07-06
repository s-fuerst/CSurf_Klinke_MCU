/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:27:55 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODESINGLECHANNELCOMPONENT_PLUGMODESINGLECHANNELCOMPONENT_2C59FFEA__
#define __JUCER_HEADER_PLUGMODESINGLECHANNELCOMPONENT_PLUGMODESINGLECHANNELCOMPONENT_2C59FFEA__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
//[/Headers]

#include "PlugModeVPOTComponent.h"
#include "PlugModeFaderComponent.h"


//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeSingleChannelComponent : public Component {
public:
  //==============================================================================
  PlugModeSingleChannelComponent(PlugModeComponent *pMC, PMFader *pFader,
                                 PMVPot *pVPot);
  ~PlugModeSingleChannelComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  PlugModeVPOTComponent *getVPOT() { return m_vpot; }
  PlugModeFaderComponent *getFader() { return m_fader; }
  void updateEverything();
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
      PlugModeVPOTComponent *m_vpot;
  PlugModeFaderComponent *m_fader;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeSingleChannelComponent(const PlugModeSingleChannelComponent &);
  const PlugModeSingleChannelComponent &
  operator=(const PlugModeSingleChannelComponent &);
};

#endif // __JUCER_HEADER_PLUGMODESINGLECHANNELCOMPONENT_PLUGMODESINGLECHANNELCOMPONENT_2C59FFEA__
