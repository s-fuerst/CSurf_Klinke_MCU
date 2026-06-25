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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "CommandModeVPOTComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
CommandModeVPOTComponent::CommandModeVPOTComponent(CommandMode::Page *pPage,
                                                   int shift, int channel)
    : Component(String("CommandModeVPOT")), groupComponent(0), actionLabel(0),
      relativeButton(0), normalSpeedSlider(0), pressedSpeedSlider(0) {
  addAndMakeVisible(groupComponent = new GroupComponent(String("group"), String("1")));
  groupComponent->setColour(GroupComponent::outlineColourId, Colours::black);

  addAndMakeVisible(actionLabel = new Label(String("Action"), String("Action")));
  actionLabel->setTooltip(
      String("A short (max. 6 char long) description of the assigned action."));
  actionLabel->setExplicitFocusOrder(10);
  actionLabel->setFont(
      Font(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain));
  actionLabel->setJustificationType(Justification::centredLeft);
  actionLabel->setEditable(true, true, false);
  actionLabel->setColour(Label::backgroundColourId, Colours::white);
  actionLabel->setColour(Label::outlineColourId, Colours::black);
  actionLabel->setColour(TextEditor::textColourId, Colours::black);
  actionLabel->setColour(TextEditor::backgroundColourId, Colour(0x0));
  actionLabel->addListener(this);

  addAndMakeVisible(relativeButton = new ToggleButton(String("Relative")));
  relativeButton->setTooltip(
      String("When enabled, the VPOT sends \"relative 2 mode\" messages. This "
        "allows you to assign \"(midi cc only)\" actions to the VPOT, but you "
        "must select \"Relative 2\" in the \"MIDI CC:\" combo box. If this "
        "button is not selected, you must select \"Absolute\"."));
  relativeButton->setExplicitFocusOrder(11);
  relativeButton->setButtonText(String());
  relativeButton->addListener(this);

  addAndMakeVisible(normalSpeedSlider = new Slider(String("NormalSpeed")));
  normalSpeedSlider->setTooltip(
      String("The amount of change when the VPOT is rotated but not pressed. See "
        "also the tooltip of the button above."));
  normalSpeedSlider->setExplicitFocusOrder(12);
  normalSpeedSlider->setRange(1, 20, 1);
  normalSpeedSlider->setSliderStyle(Slider::LinearBar);
  normalSpeedSlider->setTextBoxStyle(Slider::TextBoxLeft, false, 80, 20);
  normalSpeedSlider->setColour(Slider::thumbColourId, Colour(0xffe0f0ff));
  normalSpeedSlider->setColour(Slider::trackColourId, Colour(0x0));
  normalSpeedSlider->addListener(this);

  addAndMakeVisible(pressedSpeedSlider = new Slider(String("PressedSpeed")));
  pressedSpeedSlider->setTooltip(
      String("The amount of change when the VPOT is rotated while pressed. See also "
        "the tooltip of the button above."));
  pressedSpeedSlider->setExplicitFocusOrder(13);
  pressedSpeedSlider->setRange(1, 30, 1);
  pressedSpeedSlider->setSliderStyle(Slider::LinearBar);
  pressedSpeedSlider->setTextBoxStyle(Slider::TextBoxLeft, false, 80, 20);
  pressedSpeedSlider->setColour(Slider::thumbColourId, Colour(0xffe0f0ff));
  pressedSpeedSlider->addListener(this);

  //[UserPreSize]
  m_pPage = pPage;
  m_shift = shift;
  m_channel = channel;
  groupComponent->setText((shift ? String("Shift ") : String()) +
                          String::formatted(String("%d"), channel + 1));
  actionLabel->setText(m_pPage->getCommandName(shift, channel), dontSendNotification);
  relativeButton->setToggleState(m_pPage->m_bRelative[shift][channel], false);
  normalSpeedSlider->setEnabled(m_pPage->m_bRelative[shift][channel]);
  normalSpeedSlider->setValue(m_pPage->m_iNormalSpeed[shift][channel], dontSendNotification);
  pressedSpeedSlider->setEnabled(m_pPage->m_bRelative[shift][channel]);
  pressedSpeedSlider->setValue(m_pPage->m_iPressedSpeed[shift][channel], dontSendNotification);

  // actionLabel->setMinimumHorizontalScale(0.98);
  //[/UserPreSize]

  setSize(80, 150);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

CommandModeVPOTComponent::~CommandModeVPOTComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(groupComponent);
  deleteAndZero(actionLabel);
  deleteAndZero(relativeButton);
  deleteAndZero(normalSpeedSlider);
  deleteAndZero(pressedSpeedSlider);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void CommandModeVPOTComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void CommandModeVPOTComponent::resized() {
  groupComponent->setBounds(0, 0, 80, 150);
  actionLabel->setBounds(10, 20, 60, 24);
  relativeButton->setBounds(29, 49, 24, 24);
  normalSpeedSlider->setBounds(10, 80, 60, 24);
  pressedSpeedSlider->setBounds(10, 110, 60, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void CommandModeVPOTComponent::labelTextChanged(Label *labelThatHasChanged) {
  //[UserlabelTextChanged_Pre]
  //[/UserlabelTextChanged_Pre]

  if (labelThatHasChanged == actionLabel) {
    //[UserLabelCode_actionLabel] -- add your label text handling code here..
    actionLabel->setText(actionLabel->getText().substring(0, 6), dontSendNotification);
    m_pPage->setCommandName(m_shift, m_channel, actionLabel->getText());
    //[/UserLabelCode_actionLabel]
  }

  //[UserlabelTextChanged_Post]
  //[/UserlabelTextChanged_Post]
}

void CommandModeVPOTComponent::buttonClicked(Button *buttonThatWasClicked) {
  //[UserbuttonClicked_Pre]
  //[/UserbuttonClicked_Pre]

  if (buttonThatWasClicked == relativeButton) {
    //[UserButtonCode_relativeButton] -- add your button handler code here..
    m_pPage->m_bRelative[m_shift][m_channel] = relativeButton->getToggleState();
    normalSpeedSlider->setEnabled(relativeButton->getToggleState());
    pressedSpeedSlider->setEnabled(relativeButton->getToggleState());
    //[/UserButtonCode_relativeButton]
  }

  //[UserbuttonClicked_Post]
  //[/UserbuttonClicked_Post]
}

void CommandModeVPOTComponent::sliderValueChanged(Slider *sliderThatWasMoved) {
  //[UsersliderValueChanged_Pre]
  //[/UsersliderValueChanged_Pre]

  if (sliderThatWasMoved == normalSpeedSlider) {
    //[UserSliderCode_normalSpeedSlider] -- add your slider handling code here..
    m_pPage->m_iNormalSpeed[m_shift][m_channel] =
        (int)(normalSpeedSlider->getValue() + 0.5);
    //[/UserSliderCode_normalSpeedSlider]
  } else if (sliderThatWasMoved == pressedSpeedSlider) {
    //[UserSliderCode_pressedSpeedSlider] -- add your slider handling code
    //here..
    m_pPage->m_iPressedSpeed[m_shift][m_channel] =
        (int)(pressedSpeedSlider->getValue() + 0.5);
    //[/UserSliderCode_pressedSpeedSlider]
  }

  //[UsersliderValueChanged_Post]
  //[/UsersliderValueChanged_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="CommandModeVPOTComponent"
		componentName="CommandModeVPOT" parentClasses="public Component"
		constructorParams="CommandMode::Page* pPage, int shift, int channel"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="80"
		initialHeight="150">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GROUPCOMPONENT name="group" id="c4882edbf506f11c" memberName="groupComponent"
		virtualName="" explicitFocusOrder="0" pos="0 0 80 150" outlinecol="ff000000"
		title="1"/>
		<LABEL name="Action" id="c61bbc2cfa171336" memberName="actionLabel"
		virtualName="" explicitFocusOrder="10" pos="10 20 60 24" tooltip="A short (max. 6 char long) description of the assigned action."
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="Action" editableSingleClick="1" editableDoubleClick="1"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		<TOGGLEBUTTON name="Relative" id="3b816a37da75b1a2" memberName="relativeButton"
		virtualName="" explicitFocusOrder="11" pos="29 49 24 24" tooltip="When enabled, the VPOT sends &quot;relative 2 mode&quot; messages. This allows you to assign &quot;(midi cc only)&quot; actions to the VPOT, but you must select &quot;Relative 2&quot; in the &quot;MIDI CC:&quot; combo box. If this button is not selected, you must select &quot;Absolute&quot;."
		buttonText="" connectedEdges="0" needsCallback="1" radioGroupId="0"
		state="0"/>
		<SLIDER name="NormalSpeed" id="5e46c1794e96c89c" memberName="normalSpeedSlider"
		virtualName="" explicitFocusOrder="12" pos="10 80 60 24" tooltip="The amount of change when the VPOT is rotated but not pressed. See also the tooltip of the button above."
		thumbcol="ffe0f0ff" trackcol="0" min="1" max="20" int="1" style="LinearBar"
		textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="80"
		textBoxHeight="20" skewFactor="1"/>
		<SLIDER name="PressedSpeed" id="5c9c9c1a8a9408d5" memberName="pressedSpeedSlider"
		virtualName="" explicitFocusOrder="13" pos="10 110 60 24" tooltip="The amount of change when the VPOT is rotated while pressed. See also the tooltip of the button above."
		thumbcol="ffe0f0ff" min="1" max="30" int="1" style="LinearBar"
		textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="80"
		textBoxHeight="20" skewFactor="1"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
