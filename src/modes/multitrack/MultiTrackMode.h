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

  void activate() override;

  bool buttonFaderBanks(int button, bool pressed) override;
  bool buttonFlip(bool pressed) override;
  bool buttonGView(bool pressed) override;

  bool buttonRec(int channel, bool pressed) override;
  bool buttonMute(int channel, bool pressed) override;
  bool buttonSolo(int channel, bool pressed) override;
  bool buttonSoloDC(int channel) override;
  bool buttonSelect(int channel, bool pressed) override;

  bool buttonSelectLong(int channel) override;

  bool fader(int channel, int value) override;
  //      bool someFadersTouched(bool touched); // is called when the first
  //      fader is touched or the last fader touch is released
  void updateRecLEDs() override;
  void updateSoloLEDs() override;
  void updateMuteLEDs() override;
  void updateSelectLEDs() override;
  void updateFlipLED() override;
  void updateGlobalViewLED() override;
  void updateAssignmentDisplay() override;

  void updateFaders() override;
  virtual void updateVPOTs() override;

  virtual void updateDisplay() override;

  virtual void trackListChange() override;
  virtual void trackVolume(int id, double volume);
  virtual void trackPan(int id, double pan);
  virtual void trackMute(MediaTrack *trackid, bool mute) {}
  virtual void trackSolo(MediaTrack *trackid, bool solo) { updateSoloLEDs(); }
  virtual void trackRecArm(MediaTrack *trackid, bool recarm) {}

  static bool getFlipMode() { return s_flipmode; }
  static bool getMCPMode() { return s_mcpmode; }

  virtual void frameUpdate() override;

  // Returns true when row 1 should keep showing a value (instead of the LCD
  // level-meter bar) for this channel. Base behaviour: non-ProX units while
  // the channel's fader is touched, so the Volume/Pan value the fader controls
  // stays visible even with "meters on display" active. PanMode extends this
  // to also cover the brief VPOT-value display window.
  virtual bool suppressDisplayMeterForValue(int channel);

  virtual Options *getOptions() override { return Tracks::instance()->getOptions(); }
  virtual Options *get2ndOptions() override {
    return Tracks::instance()->get2ndOptions();
  }

  // MultiTrackMode (and subclasses PanMode, CommandMode) support
  // extended channels from extender units.
  virtual bool supportsExtendedChannels() const override { return true; }

  virtual Component **createEditorComponent() override;
  virtual void deleteEditorComponent() override;

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
