/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  13 Nov 2010 1:21:11 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "TrackStatesTableComponent.h"
//[/Headers]

#include "TrackStatesEditorComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
TrackStatesEditorComponent::TrackStatesEditorComponent() : component(0) {
  addAndMakeVisible(component = new TrackStatesTableComponent());
  component->setName(T("new component"));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(668, 604);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

TrackStatesEditorComponent::~TrackStatesEditorComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(component);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void TrackStatesEditorComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void TrackStatesEditorComponent::resized() {
  component->setBounds(24, 25, 620, 556);
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

		<JUCER_COMPONENT documentType="Component" className="TrackStatesEditorComponent"
		componentName="" parentClasses="public Component" constructorParams=""
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="668"
		initialHeight="604">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GENERICCOMPONENT name="new component" id="a9bbb362a8b53a6" memberName="component"
		virtualName="" explicitFocusOrder="0" pos="24 25 620 556" class="TrackStatesTableComponent"
		params=""/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
