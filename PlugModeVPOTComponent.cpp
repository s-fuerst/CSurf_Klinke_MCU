/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  30 Nov 2009 11:04:35 pm

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
#include "PlugAccess.h"
#include <boost/tuple/tuple.hpp>
#include "std_helper.h"
#include "PlugModeComponent.h"
//[/Headers]

#include "PlugModeVPOTComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeVPOTComponent::PlugModeVPOTComponent(PlugModeComponent *pMC,
                                             PMVPot *pVPot)
    : m_vpotGroup(0), m_tableComponent(0), m_params(0), m_copyButton(0),
      m_pasteButton(0), m_clearButton(0) {
  addAndMakeVisible(m_vpotGroup =
                        new GroupComponent(T("VPotGroup"), T("V-Pot")));
  m_vpotGroup->setColour(GroupComponent::outlineColourId, Colours::black);

  addAndMakeVisible(m_tableComponent =
                        new PlugModeVPOTTableComponent(pVPot->getStepsMap()));
  m_tableComponent->setName(T("Parameter Table"));

  addAndMakeVisible(m_params = new PlugModeParamComponent(pMC, pVPot));
  addAndMakeVisible(m_copyButton = new TextButton(T("Copy Button")));
  m_copyButton->setTooltip(T("Copy the table to the (local) clipboard"));
  m_copyButton->setButtonText(T("Copy"));
  m_copyButton->addListener(this);

  addAndMakeVisible(m_pasteButton = new TextButton(T("Paste Button")));
  m_pasteButton->setTooltip(T("Paste the table"));
  m_pasteButton->setButtonText(T("Paste"));
  m_pasteButton->addListener(this);

  addAndMakeVisible(m_clearButton = new TextButton(T("Clear Button")));
  m_clearButton->setTooltip(T("Clear the table"));
  m_clearButton->setButtonText(T("Clear"));
  m_clearButton->addListener(this);

  //[UserPreSize]
  //[/UserPreSize]

  setSize(600, 200);

  //[Constructor] You can add your own custom stuff here..
  m_pVPot = pVPot;
  m_pMainComponent = pMC;
  //[/Constructor]
}

PlugModeVPOTComponent::~PlugModeVPOTComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_vpotGroup);
  deleteAndZero(m_tableComponent);
  deleteAndZero(m_params);
  deleteAndZero(m_copyButton);
  deleteAndZero(m_pasteButton);
  deleteAndZero(m_clearButton);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeVPOTComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeVPOTComponent::resized() {
  m_vpotGroup->setBounds(0, -1, 600, 200);
  m_tableComponent->setBounds(24, 47, 512, 140);
  m_params->setBounds(16, 13, 568, 28);
  m_copyButton->setBounds(544, 48, 45, 24);
  m_pasteButton->setBounds(545, 81, 45, 24);
  m_clearButton->setBounds(545, 160, 45, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void PlugModeVPOTComponent::buttonClicked(Button *buttonThatWasClicked) {
  //[UserbuttonClicked_Pre]
  //[/UserbuttonClicked_Pre]

  if (buttonThatWasClicked == m_copyButton) {
    //[UserButtonCode_m_copyButton] -- add your button handler code here..
    *(m_pMainComponent->getStepTableClipBoard()) = *(m_pVPot->getStepsMap());
    updateEverything();
    //[/UserButtonCode_m_copyButton]
  } else if (buttonThatWasClicked == m_pasteButton) {
    //[UserButtonCode_m_pasteButton] -- add your button handler code here..
    *(m_pVPot->getStepsMap()) = *(m_pMainComponent->getStepTableClipBoard());
    updateEverything();
    //[/UserButtonCode_m_pasteButton]
  } else if (buttonThatWasClicked == m_clearButton) {
    //[UserButtonCode_m_clearButton] -- add your button handler code here..
    m_pVPot->getStepsMap()->clear();
    updateEverything();
    //[/UserButtonCode_m_clearButton]
  }

  //[UserbuttonClicked_Post]
  //[/UserbuttonClicked_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeVPOTComponent::changeParamId(int paramId, double value,
                                          String paramName) {
  m_params->changeParamId(paramId);
  PMVPot::tSteps *pSteps = m_pVPot->getStepsMap();

  (*pSteps)[value] = boost::tuple<String, String>(
      PlugAccess::shortNameFromCString(paramName.toCString()),
      PlugAccess::longNameFromCString(paramName.toCString()));

  int pos = findIndexFromKeyInMap(value, pSteps);
  m_tableComponent->setLastChangedRow(0);
  m_tableComponent->updateEverything();
  m_tableComponent->setLastChangedRow(pos);

  updateEverything();
}

void PlugModeVPOTComponent::updateEverything() {
  m_params->updateEverything();
  m_tableComponent->updateEverything();

  m_pasteButton->setEnabled(m_pMainComponent->getStepTableClipBoard()->size() >
                            0);
  m_clearButton->setEnabled(m_pVPot->getStepsMap()->size() > 0);
  m_copyButton->setEnabled(m_pVPot->getStepsMap()->size() > 0);
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeVPOTComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMVPot* pVPot"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="600"
		initialHeight="200">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GROUPCOMPONENT name="VPotGroup" id="a62034b231a86f21" memberName="m_vpotGroup"
		virtualName="" explicitFocusOrder="0" pos="0 -1 600 200" outlinecol="ff000000"
		title="V-Pot"/>
		<GENERICCOMPONENT name="Parameter Table" id="ebb1778879e1c2e8" memberName="m_tableComponent"
		virtualName="" explicitFocusOrder="0" pos="24 47 512 140" class="PlugModeVPOTTableComponent"
		params="pVPot-&gt;getStepsMap()"/>
		<JUCERCOMP name="" id="abd31dd755db70c3" memberName="m_params" virtualName=""
		explicitFocusOrder="0" pos="16 13 568 28" sourceFile="PlugModeParamComponent.cpp"
		constructorParams="pMC, pVPot"/>
		<TEXTBUTTON name="Copy Button" id="9d62c1239105b71b" memberName="m_copyButton"
		virtualName="" explicitFocusOrder="0" pos="544 48 45 24" tooltip="Copy the table to the (local) clipboard"
		buttonText="Copy" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<TEXTBUTTON name="Paste Button" id="b6c56fa987b43ff8" memberName="m_pasteButton"
		virtualName="" explicitFocusOrder="0" pos="545 81 45 24" tooltip="Paste the table"
		buttonText="Paste" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<TEXTBUTTON name="Clear Button" id="2c0e67cb2bc91131" memberName="m_clearButton"
		virtualName="" explicitFocusOrder="0" pos="545 160 45 24" tooltip="Clear the table"
		buttonText="Clear" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
