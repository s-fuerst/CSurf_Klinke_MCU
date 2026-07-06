/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:20:42 pm

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
#include "PlugModeSinglePageComponent.h"
//[/Headers]

#include "PlugModeSingleBankComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeSingleBankComponent::PlugModeSingleBankComponent(
    PlugModeComponent *pMC, PlugModeBankComponent *pPMBC, PMBank *pBank)
    : m_nameAndReference(0), m_pageComponent(0) {
  addAndMakeVisible(m_nameAndReference =
                        new PlugModeBankReferenceComponent(pPMBC, pBank));
  addAndMakeVisible(m_pageComponent = new PlugModePageComponent(pMC, pBank));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(616, 424);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

PlugModeSingleBankComponent::~PlugModeSingleBankComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_nameAndReference);
  deleteAndZero(m_pageComponent);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeSingleBankComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeSingleBankComponent::resized() {
  m_nameAndReference->setBounds(4, 2, 608, 56);
  m_pageComponent->setBounds(2, 51, 612, 376);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeSingleBankComponent::updateEverything() {
  m_nameAndReference->updateEverything();
  m_pageComponent->updateEverything();
}

void PlugModeSingleBankComponent::makePageComponentVisible(
    bool shouldBeVisible) {
  m_pageComponent->setVisible(shouldBeVisible);
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeSingleBankComponent"
		componentName="" parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PlugModeBankComponent* pPMBC, PMBank* pBank"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="616"
		initialHeight="424">
		<BACKGROUND backgroundColour="ffffffff"/>
		<JUCERCOMP name="Name and Reference" id="aeefa72a2cebe2b9" memberName="m_nameAndReference"
		virtualName="" explicitFocusOrder="0" pos="4 2 608 56" sourceFile="PlugModeBankReferenceComponent.cpp"
		constructorParams="pPMBC, pBank"/>
		<JUCERCOMP name="Page Component" id="93f6ea040c5b9ca9" memberName="m_pageComponent"
		virtualName="" explicitFocusOrder="0" pos="2 51 612 376" sourceFile="PlugModePageComponent.cpp"
		constructorParams="pMC, pBank"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
