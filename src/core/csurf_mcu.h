/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

#ifndef MCU_CSURF_MCU
#define MCU_CSURF_MCU

#define EXT_ID  "unused"
#define MAIN_ID "MCUM5"
// list of CCs that are send to reaper (for midi learn):
// 
// for global view buttons: Channel 1-16, 0x3e - 0x45 (62 - 69). Till v0.4 also 0x0e - 0x15, 0x1e - 0x25, 0x2e - 0x35   
//
// command mode: Channel 1-8, 0x06-0x0b, 0x16-0x1b, 0x26-0x2b ... 0x76-0x7b 
// (0xab : a is channel, b has six different state, vpot pressed, released, left while pressed, right while pressed, left while not pressed, right while not pressed)
#include "csurf.h"
#include "ptrlist.h"
#include "mcu_button_defines.h"
#include "Region.h"
#include "CCSManager.h"
#include "HardwareUnit.h"
#include "boost/signals2.hpp"
#include "Version.h"   // generated: MCU_VERSION_STRING (v<version> build <count>)
#include <vector>

using boost::signals2::connection;

static const GUID GUID_NOT_ACTIVE = {
    0x00000000,
    0x0000,
    0x0000,
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static GUID GUID_MASTER = {
	  0x12345678,
    0x8765,
    0x4321,
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

#ifndef _WIN32
inline bool operator==(const GUID &a, const GUID &b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID &a, const GUID &b) { return memcmp(&a, &b, sizeof(GUID)) != 0; }
#endif

#define safe_call(p, f)                                                        \
  if (p != NULL) {                                                             \
    p->f;                                                                      \
  }

#define safe_delete(x)                                                         \
  if (x != NULL) {                                                             \
    delete (x);                                                                \
    x = NULL;                                                                  \
  }
#define safe_delete_array(x)                                                   \
  if (x != NULL) {                                                             \
    delete[](x);                                                               \
    x = NULL;                                                                  \
  }

#define VK_OPTION 2
#define VK_ALT VK_MENU

#define NUM_DLG_PARAMS 5

class CSurf_MCU;
class Transport;
class DisplayHandler;
class ButtonManager;
class ActionsDisplay;
class ActionsDialogComponent;

static WDL_PtrList<CSurf_MCU> g_mcu_list;

typedef void (CSurf_MCU::*ScheduleFunc)();

struct ScheduledAction {
  ScheduledAction(DWORD time, ScheduleFunc func) {
    this->next = NULL;
    this->time = time;
    this->func = func;
  }

  ScheduledAction *next;
  DWORD time;
  ScheduleFunc func;
};

#define CONFIG_FLAG_FADER_TOUCH_FAKE 1
#define CONFIG_FLAG_SWAPZOOM 2
#define CONFIG_FLAG_EMULATING_BLINKING 4
#define CONFIG_FLAG_KEYBOARD_MODIFIER 8
#define CONFIG_FLAG_PROX 16 
#define CONFIG_FLAG_STARTGLOBALVIEW 32
// 64 was already used, continue with 128

#define DOUBLE_CLICK_INTERVAL 200 /* ms */

#define ARROW_STATE_SCRUB 0x80
#define ARROW_STATE_ZOOM 0x40

// todo: move this to MultiTrackMode.h
#define FIXID(id)                                                              \
  int id = CSurf_TrackToID(trackid, MultiTrackMode::getMCPMode());             \
  int oid = id;                                                                \
  if (id > 0) {                                                                \
    id -= m_offset + Tracks::instance()->getGlobalOffset() + 1;                \
    if (id == 8)                                                               \
      id = -1;                                                                 \
  } else if (id == 0)                                                          \
    id = 8;

static double charToVol(unsigned char val) {
  double pos = ((double)val * 1000.0) / 127.0;
  pos = SLIDER2DB(pos);
  return DB2VAL(pos);
}

static int msbLsbToInt(unsigned char msb, unsigned char lsb) {
  return (lsb | (msb << 7));
}

static double int14ToVol(int val) {
  double pos = ((double)val * 1000.0) / 16383.0;
  pos = SLIDER2DB(pos);
  return DB2VAL(pos);
}

static double int14ToVol(unsigned char msb, unsigned char lsb) {
  return int14ToVol(msbLsbToInt(msb, lsb));
}

static double int14ToPan(int val) { return 1.0 - (val / (16383.0 * 0.5)); }

static double int14ToPan(unsigned char msb, unsigned char lsb) {
  return int14ToPan(msbLsbToInt(msb, lsb));
}

static int volToInt14(double vol) {
  double d = (DB2SLIDER(VAL2DB(vol)) * 16383.0 / 1000.0);
  if (d < 0.0)
    d = 0.0;
  else if (d > 16383.0)
    d = 16383.0;

  return (int)(d + 0.5);
}

static int panToInt14(double pan) {
  double d = ((1.0 - pan) * 16383.0 * 0.5);
  if (d < 0.0)
    d = 0.0;
  else if (d > 16383.0)
    d = 16383.0;

  return (int)(d + 0.5);
}

static unsigned char volToChar(double vol) {
  double d = (DB2SLIDER(VAL2DB(vol)) * 127.0 / 1000.0);
  if (d < 0.0)
    d = 0.0;
  else if (d > 127.0)
    d = 127.0;

  return (unsigned char)(d + 0.5);
}

static double charToPan(unsigned char val) {
  double pos = ((double)val * 1000.0 + 0.5) / 127.0;

  pos = (pos - 500.0) / 500.0;
  if (fabs(pos) < 0.08)
    pos = 0.0;

  return pos;
}

static unsigned char panToChar(double pan) {
  pan = (pan + 1.0) * 63.5;

  if (pan < 0.0)
    pan = 0.0;
  else if (pan > 127.0)
    pan = 127.0;

  return (unsigned char)(pan + 0.5);
}

static void GUID2String(GUID *guid, String &str) {
  char guidcstr[64];
  guidToString(guid, guidcstr);
  str = guidcstr;
}

static String GUID2String(GUID *guid) {
  char guidcstr[64];
  guidToString(guid, guidcstr);
  return String(guidcstr);
}

static void String2GUID(String &str, GUID *guid) {
  stringToGuid(str.toRawUTF8(), guid);
}

static String GetPlugName(MediaTrack *pMediaTrack, int slot) {
  char paramName[80];
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  return String(paramName);
}

class TrackIterator {
  int m_index;
  int m_len;

public:
  TrackIterator() {
    m_index = 1;
    m_len = CSurf_NumTracks(false);
  }
  MediaTrack *operator*() { return CSurf_TrackFromID(m_index, false); }
  TrackIterator &operator++() {
    if (m_index <= m_len)
      ++m_index;
    return *this;
  }
  bool end() { return m_index > m_len; }
};

class DropState {
#define DROP_NORMAL 0
#define DROP_TIME 1
#define DROP_ITEM 2
#define DROP_NUM_STATES 3

  int m_current_state;
  bool m_led_on;

public:
  DropState() {
    m_current_state = DROP_NORMAL;
    m_led_on = false;
  }

  void toggleStateAndUpdate(bool is_mcuex, midi_Output *p_midiout) {
    toggleState();
    updateReaper();
    updateMCU(is_mcuex, p_midiout);
  }

  void toggleState() {
    m_current_state++;
    if (m_current_state == DROP_NUM_STATES)
      m_current_state = 0;
  }

  void updateReaper() {
    switch (m_current_state) {
    case DROP_NORMAL:
      SendMessage(g_hwnd, WM_COMMAND, ID_RECORD_MODE_NORMAL, 0);
      break;
    case DROP_TIME:
      SendMessage(g_hwnd, WM_COMMAND, ID_RECORD_MODE_TIME, 0);
      break;
    case DROP_ITEM:
      SendMessage(g_hwnd, WM_COMMAND, ID_RECORD_MODE_ITEM, 0);
      break;
    }
  }

  void updateMCU(bool is_mcuex, midi_Output *p_midiout) {
    if (!is_mcuex && p_midiout) {
      switch (m_current_state) {
      case DROP_NORMAL:
        p_midiout->Send(0x90, B_DROP, LED_OFF, -1);
        break;
      case DROP_TIME:
        p_midiout->Send(0x90, B_DROP, LED_ON, -1);
        break;
      case DROP_ITEM:
        p_midiout->Send(0x90, B_DROP, LED_BLINK, -1);
        break;
      }
    }
  }
};

class SelectedTrack;

class CSurf_MCU : public IReaperControlSurface {
private:
  Transport *m_pTransport;
  Display *m_pSplashDisplay;
  CCSManager *m_pCCSManager;
  DropState m_dropstate;
  int m_metronom_offset;
  bool m_repeatState;
  Region m_region;
  bool m_is_mcuex;
  int m_midi_in_dev, m_midi_out_dev;
  int m_offset, m_size;
  std::vector<HardwareUnit *> m_units; // WP-A: N physical units (N=1 in WP-A)
  // m_midiout/m_midiin are NON-OWNING cached pointers into m_units[0]'s
  // ports. Ownership moved to HardwareUnit (WP-A Step 2). Kept so the many
  // legacy call sites (MCUReset/Run/SetPlayState/...) stay unchanged; they
  // will be removed once all sites route through the unit.
  midi_Output *m_midiout;
  midi_Input *m_midiin;

  char m_mackie_lasttime[10];
  int m_mackie_lasttime_mode;
  static int
      s_mackie_modifiers; // don't use this direcly, only via IsModifierPressed
                          // etc.. (todo: create a Modifiers class)
  static int s_cfg_flags; // CONFIG_FLAG_FADER_TOUCH_MODE etc

  std::map<MediaTrack *, bool> m_fader_touchstate;
  std::map<MediaTrack *, unsigned int> m_pan_lasttouch;

  WDL_String m_descspace;
  char m_configtmp[1024];

  DWORD m_mcu_timedisp_lastforce;
  int m_button_states; // 1=rew, 2=ffwd
  int m_mackie_arrow_states;
  unsigned int m_buttonstate_lastrun;
  unsigned int m_frameupd_lastrun;
  ScheduledAction *m_schedule;

  std::map<MediaTrack *, double> m_surface_volume;
  std::map<MediaTrack *, double> m_surface_pan;

  SelectedTrack *m_selected_tracks;

  ButtonManager *m_pButtonManager;

  ActionsDisplay *m_pActionDisplay;
  Component *m_pActionsDialogComponent;

public:
  typedef boost::signals2::signal<void(DWORD)> tFrameSignal;
  typedef tFrameSignal::slot_type tFrameSignalSlot;

  static int s_iNumInstances;

private:
  tFrameSignal signalFrame;

public:
  connection connect2FrameSignal(const tFrameSignalSlot &slot) {
    return signalFrame.connect(slot);
  }

public:
  CSurf_MCU(bool ismcuex, int offset, int size, int indev, int outdev,
            int cfgflags, int *errStats);
  virtual ~CSurf_MCU();

  void ScheduleAction(DWORD time, ScheduleFunc func);
  void MCUReset();
  void UpdateMackieDisplay(int pos, const char *text, int pad);
  bool OnMCUReset(MIDI_event_t *evt);
  bool OnFaderMove(MIDI_event_t *evt);
  bool OnRotaryEncoder(MIDI_event_t *evt);
  bool OnJogWheel(MIDI_event_t *evt);
  bool OnAutoMode(MIDI_event_t *evt);
  bool OnBankChannel(MIDI_event_t *evt);
  bool OnSMPTEBeats(MIDI_event_t *evt);
  bool OnRotaryEncoderPush(MIDI_event_t *evt);
  bool OnVPOTAssign(MIDI_event_t *evt);
  bool OnRecArm(MIDI_event_t *evt);
  bool OnRecArmDC(MIDI_event_t *evt);
  bool OnMute(MIDI_event_t *evt);
  bool OnSolo(MIDI_event_t *evt);
  bool OnSoloDC(MIDI_event_t *evt);
  bool OnChannelSelect(MIDI_event_t *evt);
  bool OnChannelSelectDC(MIDI_event_t *evt);
  bool OnChannelSelectLong(int channel);
  bool OnTransport(MIDI_event_t *evt);
  bool OnTransportDC(MIDI_event_t *evt);
  bool OnMarker(MIDI_event_t *evt);
  bool OnNudge(MIDI_event_t *evt);
  bool OnCycle(MIDI_event_t *evt);
  bool OnClick(MIDI_event_t *evt);
  void ClearSaveLed();
  bool OnSave(MIDI_event_t *evt);
  void ClearUndoLed();
  bool OnUndo(MIDI_event_t *evt);
  bool OnCancel(MIDI_event_t *evt);
  bool OnZoom(MIDI_event_t *evt);
  bool OnScrub(MIDI_event_t *evt);
  bool OnFlip(MIDI_event_t *evt);
  bool OnNameValue(MIDI_event_t *evt);
  bool OnNameValueDC(MIDI_event_t *evt);
  bool OnGlobal(MIDI_event_t *evt);
  bool OnKeyModifier(MIDI_event_t *evt);
  bool OnScroll(MIDI_event_t *evt);
  bool OnTouch(MIDI_event_t *evt);
  bool OnFunctionKey(MIDI_event_t *evt);
	bool ResetAllFaderTouch(MIDI_event_t *evt);
	bool OpenFXFavorite(MIDI_event_t *evt);

  void HandleFunctionKeyForRegionsOrLoops(int fkey, bool loop);
  bool OnGlobalViewKeys(MIDI_event_t *evt);
  bool OnGlobalSoloButton(MIDI_event_t *evt);
  bool OnDropButton(MIDI_event_t *evt);
  bool OnButtonPress(MIDI_event_t *evt);
  bool OnPedalMove(MIDI_event_t *evt);
  void OnMIDIEvent(MIDI_event_t *evt);
  void CloseNoReset();
  void Run();
  void SetLED(int button_nr, int led_state);
	void EmulateBlinkingLEDs(DWORD now);

  void UpdateAutoModes();
  void UpdateGlobalSoloLED();
  void OnTrackSelection(MediaTrack *trackid);
  bool IsModifierPressed(int key);
  bool IsNoModifierPressed() {
    return !IsModifierPressed(VK_SHIFT) && !IsModifierPressed(VK_OPTION) &&
           !IsModifierPressed(VK_CONTROL) && !IsModifierPressed(VK_ALT);
  }
  void CallTransportForward();
  void CallTransportRewind();

  Region *GetRegion() { return &m_region; }
  bool GetRepeatState() { return m_repeatState; }
  bool IsExtender() { return m_is_mcuex; }
  bool IsButtonPressed(int i);
  int IsLastButton(int button);

  int GetOffset() { return m_offset; }
  int GetSize() { return m_size; }
  static int GetNumMCUs() { return g_mcu_list.GetSize(); }
  SelectedTrack *GetSelectedTracks() { return m_selected_tracks; }
  void SetSelectedTracks(SelectedTrack *pTracks) {
    m_selected_tracks = pTracks;
  }
  void UnselectAllTracks();
  midi_Output *GetMidiOutput() { return m_midiout; }
  DisplayHandler *getDisplayHandler() {
    return m_units.empty() ? NULL : m_units[0]->displayHandler();
  }

  void sendStripFader(int channel, int value); // routes to owning unit (N=1: unit 0)
  int  getFaderPos(int channel);              // reads unit's cached position

  // WP-A translation helpers (N-generic plumbing, N=1 for now)
  int  numUnits() const { return (int)m_units.size(); }
  HardwareUnit *unitForChannel(int g) { return m_units[(g - 1) / 8]; } // g is 1-based
  static int localOf(int g) { return (g - 1) % 8 + 1; }                 // 1-based→local 1..8
  int  availableChannels() const { return numUnits() * 8; }
  void broadcastMasterFader(int value); // all units setMasterFader(value)

  // these will be called by the host when states change etc
  void SetTrackListChange();
  void SetSurfaceVolume(MediaTrack *trackid,
                        double volume); // will be stored in m_surface_volume
  void SetSurfacePan(MediaTrack *trackid, double pan);
  void SetSurfaceMute(MediaTrack *trackid, bool mute);
  void SetSurfaceSelected(MediaTrack *trackid, bool selected);
  void SetSurfaceSolo(MediaTrack *trackid, bool solo);
  void SetSurfaceRecArm(MediaTrack *trackid, bool recarm);
  void SetPlayState(bool play, bool pause, bool rec);
  void SetRepeatState(bool rep);
  void SetTrackTitle(MediaTrack *trackid, const char *title);
  bool GetTouchState(MediaTrack *trackid, int isPan = 0);
  void SetAutoMode(int mode); // automation mode for current track
  void SendMidi(unsigned char status, unsigned char d1, unsigned char d2,
                int frame_offset);
  void SendMsg(MIDI_event_t *message, int frame_offset);

  double GetSurfaceVolume(MediaTrack *pMT);
  double GetSurfaceVolume(int channel);
  double GetSurfacePan(MediaTrack *pMT);
  double GetSurfacePan(int channel);

  static bool IsFlagSet(int flag) { return (flag & s_cfg_flags) != 0; }

  const char *GetTypeString() {
    return m_is_mcuex ? EXT_ID : MAIN_ID;
  }

  const char *GetDescString() {
    m_descspace.Set("Mackie Control Protocol (Klinke " MCU_VERSION_STRING ")");
    char tmp[512];
    sprintf(tmp, " (dev %d,%d)", m_midi_in_dev, m_midi_out_dev);
    m_descspace.Append(tmp);
    return m_descspace.Get();
  }
  const char *GetConfigString() // string of configuration data
  {
    sprintf(m_configtmp, "%d %d %d %d %d", m_offset, m_size, m_midi_in_dev,
            m_midi_out_dev, s_cfg_flags);
    return m_configtmp;
  }

  // returns pos in sec
  static double QuantizeTimeToBeat(double pos) {
    double qn = TimeMap_timeToQN(pos);
    return TimeMap_QNToTime(floor(qn + 0.5));
  }

  // returns pos in sec
  static double QuantizeTimeToBar(double pos) {
    double bpm, bpi;
    GetProjectTimeSignature(&bpm, &bpi);
    double qn = TimeMap_timeToQN(pos) / bpi;
    return TimeMap_QNToTime(floor(qn + 0.5) * bpi);
  }

  // return pos in sec
  static double MoveInBeats(double pos, double moveInBeats) {
    double qn = TimeMap_timeToQN(pos);
    double newTime = TimeMap_QNToTime(qn + moveInBeats);
    if (newTime > 0)
      return newTime;
    return 0;
  }

  static double MoveInBars(double pos, double moveInBars) {
    double bpm, bpi;
    GetProjectTimeSignature(&bpm, &bpi);
    double qn = TimeMap_timeToQN(pos);
    double newTime = TimeMap_QNToTime(qn + moveInBars * bpi);
    if (newTime > 0)
      return newTime;
    return 0;
  }

  void UpdateRecArmLEDs();
  double CalcMovement(double oldPos, int dir);
  int FindTrackNr(MediaTrack *tr);
  static MediaTrack *TrackFromGUID(const GUID &guid);
	static GUID *GUIDfromTrack(MediaTrack *tr);
  bool SomethingSoloed();
  const char *GetTrackName(MediaTrack *tr);
  char trackName[4];
  bool IsKeyboardPressed(int key);
  void UpdateMetronomLED();
  unsigned int GetActualFrameTime() { return m_frameupd_lastrun; }

  ButtonManager *GetButtonManager() { return m_pButtonManager; }

private:
	int runCounter = 0;
};

class SelectedTrack {
public:
  GUID guid;
  SelectedTrack *next;

  SelectedTrack(MediaTrack *tr) {
    next = NULL;
    guid = *GetTrackGUID(tr);
  }

  MediaTrack *track() { return CSurf_MCU::TrackFromGUID(guid); }

  static void FreeTrackList(SelectedTrack *tracks) {
    while (tracks != NULL) {
      SelectedTrack *del = tracks;
      tracks = tracks->next;
      delete (del);
    }
  }
};

#endif
