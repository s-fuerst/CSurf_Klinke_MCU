/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:29:22 pm

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
#include "PlugMap.h"
//[/Headers]

#include "PlugModeFaderComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeFaderComponent::PlugModeFaderComponent(PlugModeComponent *pMC,
                                               PMFader *pFader)
    : m_faderGroup(0), m_params(0) {
  addAndMakeVisible(m_faderGroup =
                        new GroupComponent(T("FaderGroup"), T("Fader")));
  m_faderGroup->setColour(GroupComponent::outlineColourId, Colours::black);

  addAndMakeVisible(m_params = new PlugModeParamComponent(pMC, pFader));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(600, 50);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

PlugModeFaderComponent::~PlugModeFaderComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_faderGroup);
  deleteAndZero(m_params);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeFaderComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeFaderComponent::resized() {
  m_faderGroup->setBounds(0, 0, 600, 50);
  m_params->setBounds(16, 13, 568, 28);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeFaderComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMFader* pFader"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="600"
		initialHeight="50">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GROUPCOMPONENT name="FaderGroup" id="a62034b231a86f21" memberName="m_faderGroup"
		virtualName="" explicitFocusOrder="0" pos="0 0 600 50" outlinecol="ff000000"
		title="Fader"/>
		<JUCERCOMP name="Params" id="78a5663e66a19b2a" memberName="m_params" virtualName=""
		explicitFocusOrder="0" pos="16 13 568 28" sourceFile="PlugModeParamComponent.cpp"
		constructorParams="pMC, pFader"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
