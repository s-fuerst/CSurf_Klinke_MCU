/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once

#include "JuceHeader.h"
#include "PlugMap.h"
#include <vector>
#include "csurf_mcu.h"
#include "Tracks.h"

class TableTrackState : public TrackState {
public:
  TableTrackState();
  TableTrackState(TrackState &trackState);

  bool operator==(TableTrackState &other);

  String getTrackName() { return m_trackName; }
  void setTrackName(String trackName) { m_trackName = trackName; }

  Colour getColour() { return m_colour; }
  void setColour(Colour colour) { m_colour = colour; }

private:
  String m_trackName; // should be only used for the TrackTable, maybe move it
                      // in a own subclass
  Colour m_colour; // should be only used for the TrackTable, maybe move it in a
                   // own subclass
};

//==============================================================================
/**
         This class shows how to implement a TableListBoxModel to show in a
   TableListBox.
*/
class TrackStatesTableComponent : public Component, public TableListBoxModel {
public:
  //==============================================================================
  TrackStatesTableComponent(void);
  ~TrackStatesTableComponent(void);
  //==============================================================================
  // This is overloaded from TableListBoxModel, and must return the total number
  // of rows in our table
  int getNumRows();

  void updateEverything() { m_table->updateContent(); }

  // This is overloaded from TableListBoxModel, and should fill in the
  // background of the whole row
  void paintRowBackground(Graphics &g, int rowNumber, int width, int height,
                          bool rowIsSelected);

  void paintCell(Graphics &g, int rowNumber, int columnId, int width,
                 int height, bool rowIsSelected);

  Component *refreshComponentForCell(int rowNumber, int columnId,
                                     bool isRowSelected,
                                     Component *existingComponentToUpdate);

  //==============================================================================

  void rebuildTrackStateAndMediaTrackVector();
  void trackAddedOrRemoved(MediaTrack *pMT) {
    rebuildTrackStateAndMediaTrackVector();
  }

  typedef std::vector<MediaTrack *> tVecMediaTracks;
  //      tVecMediaTracks getMediaTracks() { return m_vecMediaTracks; }
  MediaTrack *getRowMediaTrack(int row);

  void frame(DWORD time);

private:
  tVecMediaTracks m_vecMediaTracks;
  TableListBox *m_table; // the table component itself

  typedef std::vector<TableTrackState> tVecTrackStates;
  tVecTrackStates m_vecTrackStates;

  Tracks *m_pTracks;

  int m_trackAddedConnection;
  int m_trackRemovedConnection;

  connection m_frameConnection;
};

//==============================================================================

class TableLabelTS : public Component, LabelListener {
public:
  TableLabelTS(TrackStatesTableComponent &owner);

  ~TableLabelTS() { deleteAllChildren(); }

  void resized() { m_label->setBoundsInset(BorderSize(2)); }

  void setRowAndColumn(const int newRow, const int newColumn);

  void labelTextChanged(Label *labelThatHasChanged);

  Label *getLabel() { return m_label; }

private:
  TrackStatesTableComponent &m_owner;
  Label *m_label;
  int m_row, m_columnId;
};

//==============================================================================

class AnchorColumnCustomComponent : public Component, ComboBoxListener {
public:
  AnchorColumnCustomComponent(TrackStatesTableComponent &owner);

  ~AnchorColumnCustomComponent() { deleteAllChildren(); }

  void resized() { m_comboBox->setBoundsInset(BorderSize(2)); }

  void setRowAndColumn(const int newRow, const int newColumn);

  void comboBoxChanged(ComboBox *comboBoxThatHasChanged);

private:
  TrackStatesTableComponent &m_owner;
  ComboBox *m_comboBox;
  int m_row, m_columnId;
};

//==============================================================================

class QuickJumpCustomComponent : public Component, ComboBoxListener {
public:
  QuickJumpCustomComponent(TrackStatesTableComponent &owner);

  ~QuickJumpCustomComponent() { deleteAllChildren(); }

  void resized() { m_comboBox->setBoundsInset(BorderSize(2)); }

  void setRowAndColumn(const int newRow, const int newColumn);

  void comboBoxChanged(ComboBox *comboBoxThatHasChanged);

private:
  TrackStatesTableComponent &m_owner;
  ComboBox *m_comboBox;
  int m_row, m_columnId;
};

//==============================================================================

class ButtonColumnCustomComponent : public Component, ButtonListener {
public:
  ButtonColumnCustomComponent(TrackStatesTableComponent &owner);

  ~ButtonColumnCustomComponent() { deleteAllChildren(); }

  void resized() { m_toggleButton->setBoundsInset(BorderSize(2)); }

  void setRowAndColumn(const int newRow, const int newColumn);

  void buttonClicked(Button *buttonThatWasClicked);

private:
  TrackStatesTableComponent &m_owner;
  ToggleButton *m_toggleButton;
  int m_row, m_columnId;
};
