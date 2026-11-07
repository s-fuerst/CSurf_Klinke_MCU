/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include <boost/smart_ptr/scoped_ptr.hpp>
#include "CCSMode.h"
#include "ModifierCommands.h" // CTRL+VPOT command table (Layer 2)
#include "PlugModeSelectors.h"
#include "PlugModeOptions.h"
#include "PlugMode2ndOptions.h"
#include "JuceHeader.h"
#include <boost/tuple/tuple.hpp>
#include <vector>
#include "Options.h"
#include "ProjectConfig.h"

#pragma once

class PlugAccess;
class PlugMap;
class PlugModeComponent;
class PlugPresetManager;

class PlugMode : public CCSMode {
public:
  PlugMode(CCSManager *pManager);

  // Access to the owning surface (used e.g. by PlugAccess to query the
  // configured hardware units for the short-name width heuristic).
  CCSManager *getCCSManager() const { return m_pCCSManager; }

  // opt into extender (channel > 8) events
  bool supportsExtendedChannels() const override { return true; }

public:
  ~PlugMode(void);

  void activate() override;
  void deactivate() override;

  // from MCU via CCSManager
  bool buttonFaderBanks(int button, bool pressed) override;
  bool buttonFlip(bool pressed) override;
  bool buttonGView(bool pressed) override;
  bool buttonNameValue(bool pressed) override;

  // channels are 1-based, channel 0 exist for fader, it is the Master-Fader
  bool buttonRec(int channel, bool pressed) override;   // select Preset
  bool buttonRecDC(int channel, bool pressed) override; // all presets in the channel

  bool buttonMute(int channel, bool pressed) override; // select Page
  bool buttonSolo(int channel, bool pressed) override; // select Bank
  bool buttonSelect(int channel, bool pressed) override;

  bool fader(int channel, int value) override;
  bool singleFaderTouched(
      int channel) override; // channels are 1-based, channel 0 is called, when no fader
                    // or more then one fader is touched
  bool singleVPotTouched(
      int channel) override; // channels are 1-based, channel 0 is called, when no vpot
                    // or more then one vpot is touched
  //      virtual bool faderTouched(int channel, bool touched){return false;}
  //      virtual bool someFadersTouched(bool touched){return false;} // is
  //      called when the first fader is touched or the last fader touch is
  //      released (incl. a short delay)

  bool vpotMoved(int channel,
                 int numSteps) override; // numSteps are negativ for left rotation
  bool vpotPressed(int channel, bool pressed) override;

  // default update implementation turn LEDs off
  void updateRecLEDs() override;
  void updateSelectLEDs() override;
  void updateSoloLEDs() override;
  void updateMuteLEDs() override;
  void updateFlipLED() override;
  void updateGlobalViewLED() override {
    m_pCCSManager->setGlobalViewLED(this, m_followTrack ? LED_OFF : LED_ON);
  }
  void updateAssignmentDisplay() override;
  void updateDisplay() override;
  void updateEverything() override;

  // default update implementation does nothing
  void updateFaders() override;
  void updateVPOTs() override;

  void trackListChange() override;
  void trackVolume(MediaTrack *trackid, double volume) {
    updateFaders();
    updateVPOTs();
    updateAssignmentDisplay();
  } // wegen master for volume
  void trackPan(MediaTrack *trackid, double pan) {}
  void trackMute(MediaTrack *trackid, bool mute) {}
  void trackSelected(MediaTrack *trackid, bool selected);
  void trackSolo(MediaTrack *trackid, bool solo) {}
  void trackRecArm(MediaTrack *trackid, bool recarm) {}
  void trackName(MediaTrack *trackid, const char *pName) override {}

  void frameUpdate() override;

  // Hardware CANCEL button: aborts the plugin insert flow (the only way to
  // leave it without inserting). Returns true while the mode consumed the
  // event, so the global "stop and delete media" action is skipped.
  bool buttonCancel(bool pressed) override;

  // ---- plugin category insert flow (CTRL+VPOT4) ----
  // Three stages: CTRL+VPOT4 opens the CATEGORY list of every unit, a VPOT
  // press picks one category (SHIFT = rows 8..15), the next VPOT press
  // inserts that plugin directly after the currently accessed FX (or at the
  // end of the chain when there is none) and accesses the new instance.
  // See PlugCategoryConfig for the data and the on-screen editor.
  enum tInsertState {
    INSERT_NONE = 0,
    INSERT_CATEGORY, // choose one of the unit's 16 categories
    INSERT_PLUGIN    // choose one of that category's 16 plugins
  };
  bool isInsertFlowActive() const { return m_insertState != INSERT_NONE; }
  // On-screen editor for the per-unit categories (CTRL+ALT+VPOT4). The
  // editor WINDOW owns the component (CCSModesEditor::setComponentWindow) and
  // NULLs m_pCategoryEditor when it closes - never free it here.
  Component **createCategoryEditorComponent();
  void openCategoryEditor();

  PlugModeSelector *getSelector() override { return m_pPlugSelector; }
  Options *getOptions() override { return m_pPlugModeOptions; }
  Options *get2ndOptions() override { return m_pPlugMode2ndOptions; }

  Component **createEditorComponent() override;
  void deleteEditorComponent() override;
  void removeEditor();

  // ---- active unit management ----
  int  getActiveUnit() const { return m_activeUnit; }
  void setActiveUnit(int unit);

  void projectChanged(XmlElement *pXmlElement, ProjectConfig::EAction action);
  void plugMoved(MediaTrack *pOldTrack, int oldSlot, MediaTrack *pNewTrack,
                 int newSlot); // pNewTrack == NULL => plug is removed

  // PlugMode specific methods
  PlugAccess *getPlugAccess() { return m_pAccess; }
  // Routes a host CSURF_EXT_SETFXPARAM notification to the parameter watcher
  // (Part C event-driven Learn feed). Owns the deep PlugAccess->PluginWatcher
  // chain so CSurf_MCU::Extended() does not need those includes.
  void onHostParamChanged(MediaTrack *pTrack, int fxidx, int paramidx);
  String getPlugNameShort(int iSlot);
  int getNumPlugsInSelectedTrack();
  bool isFollowTrack() { return m_followTrack; }
  void setLastTimePlugWasSelected(DWORD time) {
    m_lastTimePlugWasSelected = time;
  }

  typedef boost::tuple<GUID, int, unsigned int>
      tFav; // GUID, slot, frameTimeOfChange
  tFav getFavorite(unsigned i);
	bool accessFXFavorite(int slot);

  // Helper
  String shortPlugName(const char *pName);
  String longPlugName(const char *pName);

  // ---- Display helpers ----
  Display *mainChildOrNull(Display *d);
  int      anchorUnit();
  void     clearNonAnchorChildren(Display *d);

  // Per-unit selector display/handler for the VPOT-ASSIGN PLUG selector
  // (multi-unit plugin list). Returns NULL for unconfigured units.
  Display *selectorDisplayForUnit(int u);
  DisplayHandler *selectorHandlerForUnit(int u);

  // ---- transport lock-step + cascade ----
  // Cascade bank/page selection to units `unit..N-1`, spreading each unit's
  // page along the used-page sequence from `baseOffset`. Units 0..unit-1 are
  // left unchanged. Used by Control+SOLO (bank selection from any unit).
  void     cascadeFromUnit(int unit, int bank, int baseOffset);

  MediaTrack *selectedTrack(); 
	void followChanges();
	void plugChanged();

	// the unit whose attribute is selected for
	// PMO2_FOLLOW_CHANGE, or -1 if OFF / out of range (>= numUnits()).
	int followChangeUnit();

	// Called by PlugAccess after the extension itself wrote a plugin
	// parameter value (fader/VPOT/button on the MCU). Synchronizes the
	// follow-change value cache so the write is not misread as an external
	// (mouse) change that would yank the follow-change unit's bank/page
	// (see followChanges()). `type` is a PlugAccess::ElementDesc::eType
	// value; only FADER and VPOT touch the cache.
	void onParamValueWrittenFromMCU(int bank, int page, int type, int channel,
	                                double value);

	
private:
  void switchDisplay();
  // True while any unit's BankPagePlugSelector is in PLUG state (i.e. a
  // Select button is held somewhere). Drives the all-units PLUG overlay.
  bool anyPlugSelectorActive() const;

  // CTRL+VPOT commands (modifier command scheme, see
  // ai-docs/modifier-command-scheme.md). Target is the MODE-LEVEL accessed
  // plugin (one at a time) regardless of which unit's VPOT was pressed.
  // VPOT-3 (the mode-switch slot) is deliberately NOT registered here.
  // `m_ctrlCommands` is populated in the constructor.
  bool ctrlResolveAccessed(MediaTrack *&tr, int &fxSlot);
  // CTRL+VPOT4: open/keep the category list of the pressed unit's column.
  bool ctrlInsertPlugin(int channel);
  bool ctrlToggleFloating();
  bool ctrlToggleChain();
  bool ctrlRemoveAccessed();
  bool ctrlMoveAccessed(int dir);
  // Per-unit command legend on row 1 while CONTROL is held.
  void updateCtrlLegend();

  // --- plugin category insert flow (see tInsertState) ---
  void startInsertFlow(int unit);
  void cancelInsertFlow();
  void insertFlowPressed(int unit, int localCh);
  // Row (0..15) of the VPOT field selected by `localCh` for the CURRENT
  // SHIFT state: the mode-wide convention, unshifted = 0..7, SHIFT = 8..15.
  int insertRowForLocalCh(int localCh);
  // Track the new plugin goes to: the accessed plugin's track, else the
  // selected one (NULL = no target, the command is inactive).
  MediaTrack *insertTargetTrack();
  // Slot the new plugin is inserted after; -1 = append at the chain end.
  int insertAfterSlot();
  // Renders the category / plugin labels on the insert display.
  void updateInsertDisplay();

  void updateParamsDisplay();
  void updateValueDisplay();
  void updateTouchedDisplay();
  int randomPreset();

  bool isSingleFaderTouched() {
    return (m_iSingleFaderTouched > 0 &&
            m_pCCSManager->getNumVPotTouched() == 0);
  }
  bool isSingleVPotTouched() {
    return (m_pCCSManager->getNumFadersTouched() == 0 &&
            m_iSingleVPotTouched > 0);
  }
  void writeFavsToProjectConfig(XmlElement *pNode);
  void readFavsFromProjectConfig(XmlElement *pNode);
  void writeLastCalledPresetsToProjectConfig(XmlElement *pNode);
  void readLastCalledPresetsFromProjectConfig(XmlElement *pNode);
  void handlePresetChange(int presetNr, int slot, int randomPresetNr);

	bool isSlotBypassed(MediaTrack *pPlugTrack, int iSlot);
	
  bool m_followTrack;
  bool m_plugBroadcastActive; // true while the PLUG overlay is broadcast to all units

  Display *m_pParamsDisplay;
  Display *m_pValueDisplay;
  Display *m_pTouchedDisplay;
  Display *m_pSingleTrackMessage;
  Display *m_pNoPlugMessage;
  Display *m_pNoPlugSelectedMessage;

  int m_iSingleFaderTouched; // the channel of the touched fader, 0 means no
                             // fader or more then 1 is touched
  int m_iSingleVPotTouched;  // the channel of the touched vpot, 0 means no vpot
                             // or more then 1 is touched
  bool m_buttonNameValuePressed;

  std::vector<tFav> m_favPlugins;

  PlugAccess *m_pAccess;

  PlugPresetManager *m_pPresetManager;

  PlugModeSelector *m_pPlugSelector;
  // per-unit BankPagePlugSelector instances
  BankPagePlugSelector *m_pBankPagePlugSelectorPerUnit[MAX_SURFACE_UNITS];

  // CTRL+VPOT command table (Layer 2); see updateCtrlLegend(). No
  // modifier-state member needed: PlugMode redraws its display every
  // frame, so the legend simply follows the live modifier state.
  ModifierCommands m_ctrlCommands;

  Options *m_pPlugModeOptions;
  PlugMode2ndOptions *m_pPlugMode2ndOptions;

  PlugModeComponent *m_pPlugEditor;
  PlugMap *m_editorMap; // the map object the editor UI tree was built from

  // True while the NEXT activate() is a fresh entry into the mode: entering
  // PlugMode on a track without any FX then opens the category list
  // automatically. Cleared by activate(); re-set by deactivate(), because
  // CCSManager re-activates the CURRENT mode (without deactivate()) when a
  // selector overlay ends — that must not restart the flow.
  bool m_autoInsertOnEnter;

  // plugin category insert flow (see tInsertState) + its editor
  tInsertState m_insertState;
  int m_insertUnit;     // unit whose category list is open
  int m_insertCategory; // 0..15 within that unit
  bool m_cancelConsumed; // CANCEL press consumed the following release
  Display *m_pInsertDisplay;
  // Window-owned (see createCategoryEditorComponent): NULL while the editor
  // window is closed.
  Component *m_pCategoryEditor;

  DWORD m_lastTimePlugWasSelected;
  //      int m_lastCalledPreset; // -1 if no preset was called or lastCalled
  //      has Changed

  int m_projectChangedConnectionId;
  int m_plugMovedConnectionId;

  typedef std::map<String, int> tLCPs; // fxGUID
  tLCPs m_lastCalledPreset;

	// param-change cache. Flat [8][8][8] vector
	// (bank/page/channel), refilled on first scan and after every
	// plugin/map change so the old map's values don't register as changes.
	std::vector<double> lastFaderValues;
	std::vector<double> lastVPotValues;
	bool m_paramCacheValid;
	void invalidateParamCache();
	void refillParamCache();
	static int paramCacheIndex(int bank, int page, int channel) {
		return (bank * 8 + page) * 8 + channel;
	}

	// per-unit state
	int m_activeUnit; // pinned before every unit-specific callback
};
