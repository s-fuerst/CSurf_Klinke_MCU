/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:33:00 pm

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

#include "PlugModeSinglePageComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeSinglePageComponent::PlugModeSinglePageComponent(
    PlugModeComponent *pMC, PlugModePageComponent *pPMPC, PMPage *pPage)
    : m_channelComponent(0), m_nameAndReference(0) {
  addAndMakeVisible(m_channelComponent =
                        new PlugModeChannelComponent(pMC, pPage));
  addAndMakeVisible(m_nameAndReference =
                        new PlugModePageReferenceComponent(pPMPC, pPage));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(608, 334);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

PlugModeSinglePageComponent::~PlugModeSinglePageComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_channelComponent);
  deleteAndZero(m_nameAndReference);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeSinglePageComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeSinglePageComponent::resized() {
  m_channelComponent->setBounds(2, 50, 608, 284);
  m_nameAndReference->setBounds(1, 2, 604, 50);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeSinglePageComponent::updateEverything() {
  m_channelComponent->updateEverything();
  m_nameAndReference->updateEverything();
}

void PlugModeSinglePageComponent::makeChannelComponentVisible(
    bool shouldBeVisible) {
  m_channelComponent->setVisible(shouldBeVisible);
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeSinglePageComponent"
		componentName="" parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PlugModePageComponent* pPMPC, PMPage* pPage"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="608"
		initialHeight="334">
		<BACKGROUND backgroundColour="ffffffff"/>
		<JUCERCOMP name="Channel Component" id="5cf548fd31a24209" memberName="m_channelComponent"
		virtualName="" explicitFocusOrder="0" pos="2 50 608 284" sourceFile="PlugModeChannelComponent.cpp"
		constructorParams="pMC, pPage"/>
		<JUCERCOMP name="Name and Reference" id="d57e3c7a8f531dff" memberName="m_nameAndReference"
		virtualName="" explicitFocusOrder="0" pos="1 2 604 50" sourceFile="PlugModePageReferenceComponent.cpp"
		constructorParams="pPMPC, pPage"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
