/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  20 Dec 2009 1:17:38 am

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
#include "JuceHeader.h"
//[/Headers]

#include "PlugMapSaveDialog.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
class PlugMapManager;
//[/MiscUserDefs]

//==============================================================================
PlugMapSaveDialog::PlugMapSaveDialog(PlugMapManager *pPMM)
    : Component(String("PlugMapSaveDialog")), m_okButton(0), m_cancelButton(0),
      m_labelExisting(0), m_mapList(0), m_labelNameOfNewMap(0), m_mapName(0),
      m_fileDialog(0), m_labelConflict(0), m_labelExisting2(0),
      m_mapInstalled(0) {
  addAndMakeVisible(m_okButton = new TextButton(String("Ok")));
  m_okButton->setExplicitFocusOrder(2);
  m_okButton->addListener(this);

  addAndMakeVisible(m_cancelButton = new TextButton(String("Cancel")));
  m_cancelButton->setExplicitFocusOrder(3);
  m_cancelButton->addListener(this);

  addAndMakeVisible(m_labelExisting =
                        new Label(String("Existing"), String("Existing User-Maps:\n")));
  m_labelExisting->setFont(Font(FontOptions(15.0000f, Font::plain)));
  m_labelExisting->setJustificationType(Justification::centredLeft);
  m_labelExisting->setEditable(false, false, false);
  m_labelExisting->setColour(TextEditor::textColourId, Colours::black);
  m_labelExisting->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_mapList = new TextEditor(String("Map List")));
  m_mapList->setTooltip(
      String("The list of plugin maps that are created by the user (the are stored "
        "in the \"My Documents/MCU/PlugMaps/\" directory)."));
  m_mapList->setExplicitFocusOrder(100);
  m_mapList->setMultiLine(true);
  m_mapList->setReturnKeyStartsNewLine(true);
  m_mapList->setReadOnly(true);
  m_mapList->setScrollbarsShown(true);
  m_mapList->setCaretVisible(false);
  m_mapList->setPopupMenuEnabled(false);
  m_mapList->setColour(TextEditor::textColourId, Colours::black);
  m_mapList->setColour(TextEditor::highlightedTextColourId, Colours::white);
  m_mapList->setColour(TextEditor::highlightColourId, Colour(0xff4f6f9f));
  m_mapList->setColour(TextEditor::backgroundColourId, Colours::white);
  m_mapList->setColour(TextEditor::outlineColourId, Colours::black);
  m_mapList->setColour(TextEditor::shadowColourId, Colour(0x0));
  m_mapList->setText(String());

  addAndMakeVisible(m_labelNameOfNewMap =
                        new Label(String("Existing"), String("Name of new Map:\n")));
  m_labelNameOfNewMap->setFont(Font(FontOptions(15.0000f, Font::plain)));
  m_labelNameOfNewMap->setJustificationType(Justification::centredLeft);
  m_labelNameOfNewMap->setEditable(false, false, false);
  m_labelNameOfNewMap->setColour(TextEditor::textColourId, Colours::black);
  m_labelNameOfNewMap->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_mapName = new Label(String("Map Name"), String()));
  m_mapName->setTooltip(String("The name of the map that should be created."));
  m_mapName->setExplicitFocusOrder(1);
  m_mapName->setFont(Font(FontOptions(15.0000f, Font::plain)));
  m_mapName->setJustificationType(Justification::centredLeft);
  m_mapName->setEditable(true, true, false);
  m_mapName->setColour(Label::outlineColourId, Colours::black);
  m_mapName->setColour(TextEditor::textColourId, Colours::black);
  m_mapName->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_mapName->addListener(this);

  addAndMakeVisible(m_fileDialog = new TextButton(String("FileDialog")));
  m_fileDialog->setTooltip(
      String("Rescan the \"My Documents/MCU/PlugMaps/\" directory."));
  m_fileDialog->setExplicitFocusOrder(2);
  m_fileDialog->setButtonText(String("Rescan"));
  m_fileDialog->addListener(this);

  addAndMakeVisible(m_labelConflict = new Label(String("Conflict"), String()));
  m_labelConflict->setFont(Font(FontOptions(15.0000f, Font::plain)));
  m_labelConflict->setJustificationType(Justification::centredLeft);
  m_labelConflict->setEditable(false, false, false);
  m_labelConflict->setColour(Label::textColourId, Colour(0xffbc0000));
  m_labelConflict->setColour(TextEditor::textColourId, Colours::black);
  m_labelConflict->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_labelExisting2 =
                        new Label(String("Existing"), String("Factory Maps:\n")));
  m_labelExisting2->setFont(Font(FontOptions(15.0000f, Font::plain)));
  m_labelExisting2->setJustificationType(Justification::centredLeft);
  m_labelExisting2->setEditable(false, false, false);
  m_labelExisting2->setColour(TextEditor::textColourId, Colours::black);
  m_labelExisting2->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_mapInstalled = new TextEditor(String("Map List")));
  m_mapInstalled->setTooltip(
      String("The list of plugin maps that came with the distribution. They can\'t "
        "cause name conflicts, so there is no need to modify them."));
  m_mapInstalled->setExplicitFocusOrder(100);
  m_mapInstalled->setMultiLine(true);
  m_mapInstalled->setReturnKeyStartsNewLine(true);
  m_mapInstalled->setReadOnly(true);
  m_mapInstalled->setScrollbarsShown(true);
  m_mapInstalled->setCaretVisible(false);
  m_mapInstalled->setPopupMenuEnabled(false);
  m_mapInstalled->setColour(TextEditor::textColourId, Colours::black);
  m_mapInstalled->setColour(TextEditor::highlightedTextColourId, Colours::white);
  m_mapInstalled->setColour(TextEditor::highlightColourId, Colour(0xff4f6f9f));
  m_mapInstalled->setColour(TextEditor::backgroundColourId, Colours::white);
  m_mapInstalled->setColour(TextEditor::outlineColourId, Colours::black);
  m_mapInstalled->setColour(TextEditor::shadowColourId, Colour(0x0));
  m_mapInstalled->setText(String());

  //[UserPreSize]
  m_mapList->setText(pPMM->getUserMapsAsString());
  m_mapInstalled->setText(pPMM->getInstalledMapsAsString());
  //[/UserPreSize]

  setSize(450, 550);

  //[Constructor] You can add your own custom stuff here..
  m_pPMM = pPMM;
  //		startThread(2);
  //[/Constructor]
}

PlugMapSaveDialog::~PlugMapSaveDialog() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_okButton);
  deleteAndZero(m_cancelButton);
  deleteAndZero(m_labelExisting);
  deleteAndZero(m_mapList);
  deleteAndZero(m_labelNameOfNewMap);
  deleteAndZero(m_mapName);
  deleteAndZero(m_fileDialog);
  deleteAndZero(m_labelConflict);
  deleteAndZero(m_labelExisting2);
  deleteAndZero(m_mapInstalled);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugMapSaveDialog::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugMapSaveDialog::resized() {
  m_okButton->setBounds(80, 515, 100, 24);
  m_cancelButton->setBounds(270, 515, 100, 24);
  m_labelExisting->setBounds(18, 180, 150, 24);
  m_mapList->setBounds(20, 204, 410, 220);
  m_labelNameOfNewMap->setBounds(18, 425, 150, 24);
  m_mapName->setBounds(20, 449, 410, 24);
  m_fileDialog->setBounds(328, 180, 100, 24);
  m_labelConflict->setBounds(18, 476, 414, 24);
  m_labelExisting2->setBounds(18, 3, 150, 24);
  m_mapInstalled->setBounds(20, 27, 410, 140);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void PlugMapSaveDialog::buttonClicked(Button *buttonThatWasClicked) {
  //[UserbuttonClicked_Pre]
  //[/UserbuttonClicked_Pre]

  if (buttonThatWasClicked == m_okButton) {
    //[UserButtonCode_m_okButton] -- add your button handler code here..
    if (m_pPMM->isValidAdditionalName(m_mapName->getText())) {
      if (m_pPMM->writeMapFile(m_mapName->getText()))
        getParentComponent()->exitModalState(SAVEMAP_OK);
    } else {
      AlertWindow::showMessageBox(
          AlertWindow::WarningIcon, String("Name is invalid"),
          String("The name \'" + m_mapName->getText() +
                 "\' collide with the existing map \'" +
                 m_pPMM->nameIsInvalidBecauseOf() + "\'."));
    }

    //[/UserButtonCode_m_okButton]
  } else if (buttonThatWasClicked == m_cancelButton) {
    //[UserButtonCode_m_cancelButton] -- add your button handler code here..
    getParentComponent()->exitModalState(SAVEMAP_CANCEL);
    //[/UserButtonCode_m_cancelButton]
  } else if (buttonThatWasClicked == m_fileDialog) {
    //[UserButtonCode_m_fileDialog] -- add your button handler code here..
    // 				FileChooser* pChooser = new FileChooser ("Administrate your user
    // maps...", m_pPMM->getUserMapsLocation(), "*.xml");
    //
    // 				if (pChooser->browseForFileToOpen())
    // 				{
    // 					File mooseFile (pChooser->getResult());
    //
    // 					m_mapName->setText(mooseFile.getFileNameWithoutExtension(),
    // false);
    // 				}
    // 				delete(pChooser);

    m_pPMM->rescanDirs();
    m_mapList->setText(m_pPMM->getUserMapsAsString());
    //[/UserButtonCode_m_fileDialog]
  }

  //[UserbuttonClicked_Post]
  //[/UserbuttonClicked_Post]
}

void PlugMapSaveDialog::labelTextChanged(Label *labelThatHasChanged) {
  //[UserlabelTextChanged_Pre]
  //[/UserlabelTextChanged_Pre]

  if (labelThatHasChanged == m_mapName) {
    //[UserLabelCode_m_mapName] -- add your label text handling code here..
    //[/UserLabelCode_m_mapName]
  }

  //[UserlabelTextChanged_Post]
  //[/UserlabelTextChanged_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugMapSaveDialog" componentName="PlugMapSaveDialog"
		parentClasses="public Component, Thread" constructorParams="PlugMapManager* pPMM"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="450"
		initialHeight="550">
		<BACKGROUND backgroundColour="ffffffff"/>
		<TEXTBUTTON name="Ok" id="65d3243c6c5e024f" memberName="m_okButton" virtualName=""
		explicitFocusOrder="2" pos="80 515 100 24" buttonText="Ok" connectedEdges="0"
		needsCallback="1" radioGroupId="0"/>
		<TEXTBUTTON name="Cancel" id="86b9083ec120b355" memberName="m_cancelButton"
		virtualName="" explicitFocusOrder="3" pos="270 515 100 24" buttonText="Cancel"
		connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<LABEL name="Existing" id="5065bec996158c3d" memberName="m_labelExisting"
		virtualName="" explicitFocusOrder="0" pos="18 180 150 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Existing User-Maps:&#10;" editableSingleClick="0"
		editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		<TEXTEDITOR name="Map List" id="210ec44d18823998" memberName="m_mapList"
		virtualName="" explicitFocusOrder="100" pos="20 204 410 220"
		tooltip="The list of plugin maps that are created by the user (the are stored in the &quot;My Documents/MCU/PlugMaps/&quot; directory)."
		outlinecol="ff000000" shadowcol="0" initialText="" multiline="1"
		retKeyStartsLine="1" readonly="1" scrollbars="1" caret="0" popupmenu="0"/>
		<LABEL name="Existing" id="cdbd34dbb5ecac89" memberName="m_labelNameOfNewMap"
		virtualName="" explicitFocusOrder="0" pos="18 425 150 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Name of new Map:&#10;" editableSingleClick="0"
		editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		<LABEL name="Map Name" id="2bba7de3b8466c9e" memberName="m_mapName"
		virtualName="" explicitFocusOrder="1" pos="20 449 410 24" tooltip="The name of the map that should be created."
		outlineCol="ff000000" edTextCol="ff000000" edBkgCol="0" labelText=""
		editableSingleClick="1" editableDoubleClick="1" focusDiscardsChanges="0"
		fontname="Default font" fontsize="15" bold="0" italic="0" justification="33"/>
		<TEXTBUTTON name="FileDialog" id="19e07d4854a5df84" memberName="m_fileDialog"
		virtualName="" explicitFocusOrder="2" pos="328 180 100 24" tooltip="Rescan the &quot;My Documents/MCU/PlugMaps/&quot; directory."
		buttonText="Rescan" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<LABEL name="Conflict" id="d1e39cefcb422ed3" memberName="m_labelConflict"
		virtualName="" explicitFocusOrder="0" pos="18 476 414 24" textCol="ffbc0000"
		edTextCol="ff000000" edBkgCol="0" labelText="" editableSingleClick="0"
		editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		<LABEL name="Existing" id="87bb0ef8a4768f43" memberName="m_labelExisting2"
		virtualName="" explicitFocusOrder="0" pos="18 3 150 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Factory Maps:&#10;" editableSingleClick="0"
		editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		<TEXTEDITOR name="Map List" id="b4113d119c9a8549" memberName="m_mapInstalled"
		virtualName="" explicitFocusOrder="100" pos="20 27 410 140" tooltip="The list of plugin maps that came with the distribution. They can't cause name conflicts, so there is no need to modify them."
		outlinecol="ff000000" shadowcol="0" initialText="" multiline="1"
		retKeyStartsLine="1" readonly="1" scrollbars="1" caret="0" popupmenu="0"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
