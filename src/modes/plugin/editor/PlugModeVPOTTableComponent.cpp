/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugModeVPOTTableComponent.h"
#include "std_helper.h"


#define VPOT_COLUMN_VALUE 1
#define VPOT_COLUMN_SHORTNAME 2
#define VPOT_COLUMN_LONGNAME 3

PlugModeVPOTTableComponent::PlugModeVPOTTableComponent(PMVPot::tSteps *pStepMap)
    : m_pStepMap(pStepMap) {
  // Create our table component and add it to this component..
  addAndMakeVisible(m_table = new TableListBox(String("demo table"), this));

  m_table->setHeaderHeight(24);

  // give it a border
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(2);

  m_table->getHeader().addColumn(String("Value"), VPOT_COLUMN_VALUE, 100, 100,
                                 100,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(String("Short Name"), VPOT_COLUMN_SHORTNAME,
                                 118, 118, 118,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(String("Long Name"), VPOT_COLUMN_LONGNAME, 271,
                                 271, 271,
                                 TableHeaderComponent::notResizableOrSortable);

  // un-comment this line to have a go of stretch-to-fit mode
  // table->getHeader()->setStretchToFitActive (true);

  m_table->setMultipleSelectionEnabled(false);
  m_table->setAutoSizeMenuOptionShown(false);

  m_table->setSize(512, 138);
  m_table->setTopLeftPosition(0, 0);
}

// This is overloaded from TableListBoxModel, and must update any custom
// components that we're using
Component *PlugModeVPOTTableComponent::refreshComponentForCell(
    int rowNumber, int columnId, bool isRowSelected,
    Component *existingComponentToUpdate) {
  if (columnId == VPOT_COLUMN_SHORTNAME ||
      columnId == VPOT_COLUMN_LONGNAME) // If it's the ratings column, we'll
                                        // return our custom component..
  {
    TableLabel *theLabel = (TableLabel *)existingComponentToUpdate;

    // If an existing component is being passed-in for updating, we'll re-use
    // it, but if not, we'll have to create one.
    if (theLabel == 0)
      theLabel = new TableLabel(*this, m_pStepMap);

    theLabel->setRowAndColumn(rowNumber, columnId);

    return theLabel;
  } else {
    // for any other column, just return 0, as we'll be painting these columns
    // directly.

    jassert(existingComponentToUpdate == 0);
    return 0;
  }
}

PlugModeVPOTTableComponent::~PlugModeVPOTTableComponent(void) {
  deleteAllChildren();
}

int PlugModeVPOTTableComponent::getNumRows() { return static_cast<int>(m_pStepMap->size()); }

void PlugModeVPOTTableComponent::paintCell(Graphics &g, int rowNumber,
                                           int columnId, int width, int height,
                                           bool rowIsSelected) {
  g.setColour(Colours::black);
  g.setFont(Font(FontOptions(Font::getDefaultMonospacedFontName(), 13.0000f, Font::plain)));

  if (m_pStepMap->empty())
    return;

  double key = getNthKeyFromMap(rowNumber, m_pStepMap);

  if (columnId == VPOT_COLUMN_VALUE) {
    g.drawText(String::formatted(String("%f"), key), 2, 1, width - 4, height,
               Justification::centredLeft, true);
  }
}

TableLabel::TableLabel(PlugModeVPOTTableComponent &owner, PMVPot::tSteps *pMap)
    : m_owner(owner), m_pMap(pMap) {
  addAndMakeVisible(m_label =
                        new Label(String("Parameter Value"), String()));
  m_label->setFont(
      Font(FontOptions(Font::getDefaultMonospacedFontName(), 13.0000f, Font::plain)));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(true, true, false);
  m_label->setColour(Label::backgroundColourId, Colours::white);
  m_label->setColour(Label::outlineColourId, Colours::black);
  m_label->setColour(TextEditor::textColourId, Colours::black);
  m_label->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_label->addListener(this);
}

void TableLabel::labelTextChanged(Label *labelThatHasChanged) {
  if (labelThatHasChanged == m_label) {
    double key = getNthKeyFromMap(m_row, m_pMap);
    PMVPot::tStepsValue value = getNthValueFromMap(m_row, m_pMap);

    String newName;
    switch (m_columnId) {
    case VPOT_COLUMN_SHORTNAME:
      newName = labelThatHasChanged->getText().substring(0, 6);
      if (value.get<0>() == value.get<1>()) {
        (*m_pMap)[key] = boost::tuple<String, String>(newName, newName);
        m_owner.updateEverything();
      } else // if the long name was the same as the short, than change it also
        (*m_pMap)[key] = boost::tuple<String, String>(newName, value.get<1>());

      m_label->setText(newName, dontSendNotification);
      break;
    case VPOT_COLUMN_LONGNAME:
      newName = labelThatHasChanged->getText().substring(0, 17);
      (*m_pMap)[key] = boost::tuple<String, String>(value.get<0>(), newName);
      m_label->setText(newName, dontSendNotification);
      break;
    }
  }
}

void TableLabel::setRowAndColumn(const int newRow, const int columnId) {
  m_row = newRow;
  m_columnId = columnId;

  PMVPot::tStepsValue value = getNthValueFromMap(m_row, m_pMap);

  switch (m_columnId) {
  case VPOT_COLUMN_SHORTNAME:
    m_label->setText(getNthValueFromMap(m_row, m_pMap).get<0>(), dontSendNotification);
    break;
  case VPOT_COLUMN_LONGNAME:
    m_label->setText(getNthValueFromMap(m_row, m_pMap).get<1>(), dontSendNotification);
    break;
  }
}
