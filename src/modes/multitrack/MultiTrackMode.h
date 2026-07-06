/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "MultiTrackMeterBridge.h"
#include "CCSMode.h"
#include "Tracks.h"

class MediaTrack;
class SelectedTrack;
class Display;
class Options;
class TrackStatesEditorComponent;
class MultiTrackSelector;

#define MIDIOUT                                                                \
  midi_Output *midiout = m_pCCSManager->getMCU()->GetMidiOutput();             \
  if (midiout == NULL)                                                         \
    return;

class MultiTrackMode : public CCSMode {
public:
  MultiTrackMode(CCSManager *pManager);
  virtual ~MultiTrackMode(void);

  void activate();

  bool buttonFaderBanks(int button, bool pressed);
  bool buttonFlip(bool pressed);
  bool buttonGView(bool pressed);

  bool buttonRec(int channel, bool pressed);
  bool buttonMute(int channel, bool pressed);
  bool buttonSolo(int channel, bool pressed);
  bool buttonSoloDC(int channel);
  bool buttonSelect(int channel, bool pressed);

  bool buttonSelectLong(int channel);

  bool fader(int channel, int value);
  //      bool someFadersTouched(bool touched); // is called when the first
  //      fader is touched or the last fader touch is released
  void updateRecLEDs();
  void updateSoloLEDs();
  void updateMuteLEDs();
  void updateSelectLEDs();
  void updateFlipLED();
  void updateGlobalViewLED();
  void updateAssignmentDisplay();

  void updateFaders();
  virtual void updateVPOTs();

  virtual void updateDisplay();

  virtual void trackListChange();
  virtual void trackVolume(int id, double volume);
  virtual void trackPan(int id, double pan);
  virtual void trackMute(MediaTrack *trackid, bool mute) {}
  virtual void trackSolo(MediaTrack *trackid, bool solo) { updateSoloLEDs(); }
  virtual void trackRecArm(MediaTrack *trackid, bool recarm) {}

  static bool getFlipMode() { return s_flipmode; }
  static bool getMCPMode() { return s_mcpmode; }

  virtual void frameUpdate();

  virtual Options *getOptions() { return Tracks::instance()->getOptions(); }
  virtual Options *get2ndOptions() {
    return Tracks::instance()->get2ndOptions();
  }

  virtual Component **createEditorComponent();
  virtual void deleteEditorComponent();

protected:
  void toggleShowInMixer(MediaTrack *tr);
  MediaTrack *getMediaTrackForChannel(int channel);

  TrackStatesEditorComponent *m_pTrackStatesEditor;

  MultiTrackSelector *m_pSelector;

	
  Display *m_pDisplay;
  static bool s_flipmode;
  static bool s_mcpmode;

  int m_lastSelectedTrackNr;
};
