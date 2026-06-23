/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "TrackStatesTableComponent.h"
#include <boost/bind.hpp>
#include "csurf_mcu.h"

#define TS_COLUMN_ON_MCU 8
#define TS_COLUMN_TRACKNR 1
#define TS_COLUMN_TRACKNAME_REAPER 2
#define TS_COLUMN_TRACKNAME_MCU 3
#define TS_COLUMN_TCP 4
#define TS_COLUMN_MCP 5
#define TS_COLUMN_MCU 6
#define TS_COLUMN_ANCHOR 7
#define TS_COLUMN_QUICK_JUMP 9
#define TS_COLUMN_QUICK_NAME 10
#define TS_COLUMN_QUICK_ROOT 11

TableTrackState::TableTrackState() {
}

TableTrackState::TableTrackState(TrackState &other) : TrackState(other) {
  m_trackName = MediaTrackInfo::getTrackName(other.getMediaTrack(), false);
  m_colour = MediaTrackInfo::getTrackColor(other.getMediaTrack());
}

bool TableTrackState::operator==(TableTrackState &other) {
  TrackState otherTrackState = (TrackState)other;
  TrackState thisTrackState = (TrackState) * this;

  if (!(thisTrackState == otherTrackState))
    return false;
  if (m_colour != other.getColour())
    return false;
  if (m_trackName != other.getTrackName())
    return false;

  return true;
}

TrackStatesTableComponent::TrackStatesTableComponent() {
  m_pTracks = Tracks::instance();
  // Create our table component and add it to this component..
  addAndMakeVisible(m_table =
                        new TableListBox(JUCE_T("TrackStates table"), this));

  m_table->setHeaderHeight(24);

  // give it a border
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(2);

  m_table->getHeader().addColumn(String::empty, TS_COLUMN_ON_MCU, 15, 15, 15,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Nr."), TS_COLUMN_TRACKNR, 40, 40, 40,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Track Name"),
                                 TS_COLUMN_TRACKNAME_REAPER, 140, 140, 140,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Display"), TS_COLUMN_TRACKNAME_MCU, 60,
                                 60, 60,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("TCP"), TS_COLUMN_TCP, 30, 30, 30,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("MCP"), TS_COLUMN_MCP, 30, 30, 30,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("MCU"), TS_COLUMN_MCU, 30, 30, 30,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Anchor"), TS_COLUMN_ANCHOR, 80, 80, 80,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Jump Slot"), TS_COLUMN_QUICK_JUMP, 80,
                                 80, 80,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("J. Name"), TS_COLUMN_QUICK_NAME, 60,
                                 60, 60,
                                 TableHeaderComponent::notResizableOrSortable);
  m_table->getHeader().addColumn(JUCE_T("Root"), TS_COLUMN_QUICK_ROOT, 30, 30,
                                 30,
                                 TableHeaderComponent::notResizableOrSortable);

  // un-comment this line to have a go of stretch-to-fit mode
  // table->getHeader()->setStretchToFitActive (true);

  m_table->setMultipleSelectionEnabled(false);
  m_table->setAutoSizeMenuOptionShown(false);

  m_table->setSize(620, 556);
  m_table->setTopLeftPosition(0, 0);

  rebuildTrackStateAndMediaTrackVector();

  m_trackAddedConnection = m_pTracks->connect2TrackAddedSignal(
      boost::bind(&TrackStatesTableComponent::trackAddedOrRemoved, this, _1));
  m_trackRemovedConnection = m_pTracks->connect2TrackRemovedSignal(
      boost::bind(&TrackStatesTableComponent::trackAddedOrRemoved, this, _1));

  // extrem ugly hack
  m_frameConnection = Tracks::instance()->getMCU()->connect2FrameSignal(
      boost::bind(&TrackStatesTableComponent::frame, this, _1));
}

TrackStatesTableComponent::~TrackStatesTableComponent(void) {
  m_frameConnection.disconnect();

  deleteAllChildren();

  m_pTracks->disconnectTrackAdded(m_trackAddedConnection);
  m_pTracks->disconnectTrackRemoved(m_trackRemovedConnection);
}

// This is overloaded from TableListBoxModel, and must update any custom
// components that we're using
Component *TrackStatesTableComponent::refreshComponentForCell(
    int rowNumber, int columnId, bool isRowSelected,
    Component *existingComponentToUpdate) {
  if (columnId == TS_COLUMN_TRACKNAME_REAPER ||
      columnId == TS_COLUMN_TRACKNAME_MCU || columnId == TS_COLUMN_QUICK_NAME) {
    TableLabelTS *theLabel = (TableLabelTS *)existingComponentToUpdate;

    // If an existing component is being passed-in for updating, we'll re-use
    // it, but if not, we'll have to create one.
    if (theLabel == 0)
      theLabel = new TableLabelTS(*this);

    theLabel->setRowAndColumn(rowNumber, columnId);

    return theLabel;
  } else if (columnId == TS_COLUMN_ANCHOR) {
    AnchorColumnCustomComponent *accc =
        (AnchorColumnCustomComponent *)existingComponentToUpdate;

    if (accc == 0)
      accc = new AnchorColumnCustomComponent(*this);

    accc->setRowAndColumn(rowNumber, columnId);

    return accc;
  } else if (columnId == TS_COLUMN_QUICK_JUMP) {
    QuickJumpCustomComponent *accc =
        (QuickJumpCustomComponent *)existingComponentToUpdate;

    if (accc == 0)
      accc = new QuickJumpCustomComponent(*this);

    accc->setRowAndColumn(rowNumber, columnId);

    return accc;
  } else if (columnId == TS_COLUMN_MCP || columnId == TS_COLUMN_TCP ||
             columnId == TS_COLUMN_MCU || columnId == TS_COLUMN_QUICK_ROOT) {
    ButtonColumnCustomComponent *accc =
        (ButtonColumnCustomComponent *)existingComponentToUpdate;

    if (accc == 0)
      accc = new ButtonColumnCustomComponent(*this);

    accc->setRowAndColumn(rowNumber, columnId);

    return accc;
  } else {
    // for any other column, just return 0, as we'll be painting these columns
    // directly.
    jassert(existingComponentToUpdate == 0);
    return 0;
  }
}

int TrackStatesTableComponent::getNumRows() {
  return m_pTracks->getNumMediaTracksTotal();
}

void TrackStatesTableComponent::paintCell(Graphics &g, int rowNumber,
                                          int columnId, int width, int height,
                                          bool rowIsSelected) {
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultMonospacedFontName(), 13.0000f, Font::plain));

  MediaTrack *pMT = getRowMediaTrack(rowNumber);
  if (pMT) {
    if (columnId == TS_COLUMN_ON_MCU) {
      if (Tracks::instance()->getTrackStateForMediaTrack(pMT) &&
					Tracks::instance()->getTrackStateForMediaTrack(pMT)->isOnMCU()) {
        g.drawText(String(Tracks::instance()
                              ->getTrackStateForMediaTrack(pMT)
                              ->getOnMCUChannel()),
                   2, 1, width - 4, height, Justification::centredLeft, true);
      }
    }
  }

  if (columnId == TS_COLUMN_TRACKNR) {
    g.drawText(String::formatted(JUCE_T("%3d"), rowNumber + 1), 8, 1, width - 4,
               height, Justification::centredLeft, true);
  }
}

void TrackStatesTableComponent::rebuildTrackStateAndMediaTrackVector() {
  m_vecMediaTracks.clear();
  m_vecMediaTracks.resize(Tracks::instance()->getNumMediaTracksTotal());

  m_vecTrackStates.clear();
  m_vecTrackStates.resize(Tracks::instance()->getNumMediaTracksTotal());

  for (TrackIterator ti; !ti.end(); ++ti) {
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
    if (pTS == NULL) {
      // when multiple tracks are added at once (e.g. after loading a track
      // template) not for all tracks a TrackState exist while calling
      // trackAddedOrRemoved. That's not a problem, because trackkAddedOrRemoved
      // will be called multiple times. The last time all tracks will have a
      // TrackState.
      return;
    }

    int numTracksTotal = Tracks::instance()->getNumMediaTracksTotal();
    int trackNr = MediaTrackInfo::getTrackNr(*ti);

    m_vecMediaTracks[MediaTrackInfo::getTrackNr(*ti) - 1] = *ti;
    m_vecTrackStates[MediaTrackInfo::getTrackNr(*ti) - 1] = *pTS;
  }

  m_table->updateContent();
}

void TrackStatesTableComponent::paintRowBackground(Graphics &g, int rowNumber,
                                                   int width, int height,
                                                   bool rowIsSelected) {
  MediaTrack *pMT = getRowMediaTrack(rowNumber);
  if (pMT)
    g.fillAll(MediaTrackInfo::getTrackColor(pMT));
}

void TrackStatesTableComponent::frame(DWORD time) {
  for (TrackIterator ti; !ti.end(); ++ti) {
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
    if (pTS && pTS->getMediaTrack() != NULL) {
      TableTrackState tts = TableTrackState(*pTS);

      int row = MediaTrackInfo::getTrackNr(*ti) - 1;
      if (getRowMediaTrack(row) != NULL && !(m_vecTrackStates[row] == tts)) {
        m_vecTrackStates[row] = *pTS;
        m_table->flipRowSelection(row);
        m_table->flipRowSelection(row);
      }
    }
  }
}

MediaTrack *TrackStatesTableComponent::getRowMediaTrack(int row) {
  if (m_vecMediaTracks.size() < (unsigned)(row + 1))
    return NULL;

  return m_vecMediaTracks[row];
}

//==============================================================================

TableLabelTS::TableLabelTS(TrackStatesTableComponent &owner) : m_owner(owner) {
  addAndMakeVisible(m_label =
                        new Label(JUCE_T("Parameter Value"), String::empty));
  m_label->setFont(
      Font(Font::getDefaultMonospacedFontName(), 13.0000f, Font::plain));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(true, true, false);
  m_label->setColour(Label::backgroundColourId, Colours::white);
  m_label->setColour(Label::outlineColourId, Colours::black);
  m_label->setColour(TextEditor::textColourId, Colours::black);
  m_label->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_label->addListener(this);
}

void TableLabelTS::labelTextChanged(Label *labelThatHasChanged) {
  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  if (labelThatHasChanged == m_label) {
    String newName;
    switch (m_columnId) {
    case TS_COLUMN_TRACKNAME_REAPER:
      newName = labelThatHasChanged->getText();
      MediaTrackInfo::setTrackName(pMT, newName);
      break;
    case TS_COLUMN_TRACKNAME_MCU:
      newName = labelThatHasChanged->getText().substring(0, 6);
      Tracks::instance()->getTrackStateForMediaTrack(pMT)->setDisplayName(
          newName);
      m_label->setText(newName, false);
      break;
    case TS_COLUMN_QUICK_NAME:
      newName = labelThatHasChanged->getText().substring(0, 6);
      Tracks::instance()->getTrackStateForMediaTrack(pMT)->setQuickJumpName(
          newName);
      m_label->setText(newName, false);
      break;
    }
  }
}

void TableLabelTS::setRowAndColumn(const int newRow, const int columnId) {
  m_row = newRow;
  m_columnId = columnId;

  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  switch (m_columnId) {
  case TS_COLUMN_TRACKNAME_REAPER:
    m_label->setText(MediaTrackInfo::getTrackName(pMT, false), false);
    break;
  case TS_COLUMN_TRACKNAME_MCU:
    m_label->setText(
        Tracks::instance()->getTrackStateForMediaTrack(pMT)->getDisplayName(),
        false);
    break;
  case TS_COLUMN_QUICK_NAME:
    m_label->setText(
        Tracks::instance()->getTrackStateForMediaTrack(pMT)->getQuickJumpName(),
        false);
    //      m_label->setEnabled(Tracks::instance()->getTrackStateForMediaTrack(pMT)->getQuickJumpChannel());
    break;
  }
}

//==============================================================================

AnchorColumnCustomComponent::AnchorColumnCustomComponent(
    TrackStatesTableComponent &owner)
    : m_owner(owner) {
  addAndMakeVisible(m_comboBox = new ComboBox(String::empty));
  m_comboBox->addItem(JUCE_T("No Anchor"), -1);
  for (int i = 1; i <= 8; i++) {
    m_comboBox->addItem(JUCE_T("Channel ") + String(i), i);
  }

  m_comboBox->setSelectedId(-1);
  m_comboBox->addListener(this);
  m_comboBox->setWantsKeyboardFocus(true);
}

void AnchorColumnCustomComponent::setRowAndColumn(const int newRow,
                                                  const int newColumn) {
  m_row = newRow;
  m_columnId = newColumn;

  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  int anchorChannel =
      Tracks::instance()->getTrackStateForMediaTrack(pMT)->getAnchorChannel();
  m_comboBox->setSelectedId(anchorChannel == 0 ? -1 : anchorChannel, true);
}

void AnchorColumnCustomComponent::comboBoxChanged(
    ComboBox *comboBoxThatHasChanged) {
  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  int newAnchor = m_comboBox->getSelectedId();
  if (newAnchor == -1)
    newAnchor = 0;

  if (newAnchor > 0) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
      if (pTS && pTS->getAnchorChannel() == newAnchor &&
          pTS->getMediaTrack() != pMT) {
        pTS->setAnchorChannel(0);
      }
    }
  }

  Tracks::instance()->getTrackStateForMediaTrack(pMT)->setAnchorChannel(
      newAnchor);
  Tracks::instance()->buildGraph();
  //  m_owner.updateEverything();
}

//==============================================================================

QuickJumpCustomComponent::QuickJumpCustomComponent(
    TrackStatesTableComponent &owner)
    : m_owner(owner) {
  addAndMakeVisible(m_comboBox = new ComboBox(String::empty));
  m_comboBox->addItem(JUCE_T("Inactive"), -1);
  for (int i = 1; i <= 8; i++) {
    m_comboBox->addItem(JUCE_T("Pad ") + String(i), i);
  }

  m_comboBox->setSelectedId(-1);
  m_comboBox->addListener(this);
  m_comboBox->setWantsKeyboardFocus(true);
}

void QuickJumpCustomComponent::setRowAndColumn(const int newRow,
                                               const int newColumn) {
  m_row = newRow;
  m_columnId = newColumn;

  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  int quickJump = Tracks::instance()
                      ->getTrackStateForMediaTrack(pMT)
                      ->getQuickJumpChannel();
  m_comboBox->setSelectedId(quickJump == 0 ? -1 : quickJump, true);
}

void QuickJumpCustomComponent::comboBoxChanged(
    ComboBox *comboBoxThatHasChanged) {
  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  int newQuickJump = m_comboBox->getSelectedId();
  if (newQuickJump == -1)
    newQuickJump = 0;

  if (newQuickJump > 0) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
      if (pTS && pTS->getQuickJumpChannel() == newQuickJump) {
        pTS->setQuickJumpChannel(0);
      }
    }
  }

  Tracks::instance()->getTrackStateForMediaTrack(pMT)->setQuickJumpChannel(
      newQuickJump);
  //  m_owner.updateEverything();
}

//==============================================================================

ButtonColumnCustomComponent::ButtonColumnCustomComponent(
    TrackStatesTableComponent &owner)
    : m_owner(owner) {
  addAndMakeVisible(m_toggleButton = new ToggleButton(String::empty));

  m_toggleButton->addListener(this);
  m_toggleButton->setWantsKeyboardFocus(true);
}

void ButtonColumnCustomComponent::setRowAndColumn(const int newRow,
                                                  const int newColumn) {
  m_row = newRow;
  m_columnId = newColumn;

  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  switch (m_columnId) {
  case TS_COLUMN_MCP:
    m_toggleButton->setToggleState(MediaTrackInfo::isShownInMCP(pMT), false);
    break;
  case TS_COLUMN_TCP:
    m_toggleButton->setToggleState(MediaTrackInfo::isShownInTCP(pMT), false);
    break;
  case TS_COLUMN_MCU:
    m_toggleButton->setToggleState(
        Tracks::instance()->getTrackStateForMediaTrack(pMT)->isInSet(), false);
    break;
  case TS_COLUMN_QUICK_ROOT:
    m_toggleButton->setToggleState(
        Tracks::instance()->getTrackStateForMediaTrack(pMT)->useAsRootInQuick(),
        false);
    m_toggleButton->setEnabled(Tracks::instance()
                                       ->getTrackStateForMediaTrack(pMT)
                                       ->getQuickJumpChannel() != 0 &&
                               Tracks::instance()->hasChilds(pMT));
    break;
  }
}

void ButtonColumnCustomComponent::buttonClicked(Button *buttonThatWasClicked) {
  MediaTrack *pMT = m_owner.getRowMediaTrack(m_row);
  if (pMT == NULL)
    return;

  bool newState = buttonThatWasClicked->getToggleState();

  switch (m_columnId) {
  case TS_COLUMN_MCP:
    MediaTrackInfo::showInMCP(pMT, newState);
    TrackList_AdjustWindows(false);
    UpdateTimeline();
    break;
  case TS_COLUMN_TCP:
    MediaTrackInfo::showInTCP(pMT, newState);
    break;
  case TS_COLUMN_MCU:
    Tracks::instance()->getTrackStateForMediaTrack(pMT)->setIsInSet(newState);
    Tracks::instance()->buildGraph();
    break;
  case TS_COLUMN_QUICK_ROOT:
    Tracks::instance()->getTrackStateForMediaTrack(pMT)->setUseAsRootInQuick(
        newState);
  }
}
