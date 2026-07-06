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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "PlugModeSingleChannelComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeSingleChannelComponent::PlugModeSingleChannelComponent(
    PlugModeComponent *pMC, PMFader *pFader, PMVPot *pVPot)
    : m_vpot(0), m_fader(0) {
  addAndMakeVisible(m_vpot = new PlugModeVPOTComponent(pMC, pVPot));
  addAndMakeVisible(m_fader = new PlugModeFaderComponent(pMC, pFader));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(600, 250);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

PlugModeSingleChannelComponent::~PlugModeSingleChannelComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_vpot);
  deleteAndZero(m_fader);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeSingleChannelComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeSingleChannelComponent::resized() {
  m_vpot->setBounds(0, 0, 600, 200);
  m_fader->setBounds(0, 200, 600, 50);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeSingleChannelComponent::updateEverything() {
  m_vpot->updateEverything();
  m_fader->updateEverything();
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeSingleChannelComponent"
		componentName="" parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMFader* pFader, PMVPot* pVPot"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="600"
		initialHeight="250">
		<BACKGROUND backgroundColour="ffffffff"/>
		<JUCERCOMP name="VPOT" id="924be7a7708ffd9d" memberName="m_vpot" virtualName=""
		explicitFocusOrder="0" pos="0 0 600 200" sourceFile="PlugModeVPOTComponent.cpp"
		constructorParams="pMC, pVPot"/>
		<JUCERCOMP name="Fader" id="2802f90c3fe62ac7" memberName="m_fader" virtualName=""
		explicitFocusOrder="0" pos="0 200 600 50" sourceFile="PlugModeFaderComponent.cpp"
		constructorParams="pMC, pFader"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
