/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include <boost\smart_ptr\scoped_ptr.hpp>
#include "ccsmode.h"
#include "PlugModeSelectors.h"
#include "PlugModeOptions.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include <boost\tuple\tuple.hpp>
#include <vector>
#include "Options.h"
#include "ProjectConfig.h"

#pragma once

class PlugAccess;
class PlugModeComponent;
class PlugPresetManager;

class PlugMode :
public CCSMode
{
 public:
  PlugMode(CCSManager* pManager);
 public:
  ~PlugMode(void);

  void activate();
  void deactivate();


  // from MCU via CCSManager
  bool buttonFaderBanks(int button, bool pressed);
  bool buttonFlip(bool pressed);
  bool buttonGView(bool pressed);
  bool buttonNameValue(bool pressed);

  // channels are 1-based, channel 0 exist for fader, it is the Master-Fader
  bool buttonRec(int channel, bool pressed);// select Preset
  bool buttonRecDC(int channel, bool pressed); // all presets in the channel

  bool buttonMute(int channel, bool pressed); // select Page
  bool buttonSolo(int channel, bool pressed); // select Bank
  bool buttonSelect(int channel, bool pressed);

  bool fader(int channel, int value);
  bool singleFaderTouched(int channel); // channels are 1-based, channel 0 is called, when no fader or more then one fader is touched
  bool singleVPotTouched(int channel); // channels are 1-based, channel 0 is called, when no vpot or more then one vpot is touched
  //      virtual bool faderTouched(int channel, bool touched){return false;}
  //      virtual bool someFadersTouched(bool touched){return false;} // is called when the first fader is touched or the last fader touch is released (incl. a short delay)

  bool vpotMoved(int channel, int numSteps); // numSteps are negativ for left rotation
  bool vpotPressed(int channel, bool pressed);

  // default update implementation turn LEDs off
  void updateRecLEDs();
  void updateSelectLEDs();
  void updateSoloLEDs();
  void updateMuteLEDs();
  void updateFlipLED();  
  void updateGlobalViewLED(){m_pCCSManager->setGlobalViewLED(this, m_followTrack ? LED_OFF : LED_ON);} 
  void updateAssignmentDisplay();
  void updateDisplay();
  void updateEverything();

  // default update implementation does nothing
  void updateFaders();
  void updateVPOTs();

  void trackListChange();
  void trackVolume(MediaTrack *trackid, double volume){updateFaders();updateVPOTs();updateAssignmentDisplay();} // wegen master for volume
  void trackPan(MediaTrack *trackid, double pan){}
  void trackMute(MediaTrack *trackid, bool mute){}
  void trackSelected(MediaTrack *trackid, bool selected);
  void trackSolo(MediaTrack *trackid, bool solo){}
  void trackRecArm(MediaTrack *trackid, bool recarm){}
  void trackName(MediaTrack* trackid, const char* pName){}

  void frameUpdate();
  PlugModeSelector* getSelector(){return m_pPlugSelector;}
  Options* getOptions(){return m_pPlugModeOptions;}
  Options* get2ndOptions(){return m_pPlugMode2ndOptions;}


  Component** createEditorComponent();
  void deleteEditorComponent();
  void removeEditor();

  void projectChanged(XmlElement* pXmlElement, ProjectConfig::EAction action);
  void plugMoved(MediaTrack* pOldTrack, int oldSlot, MediaTrack* pNewTrack, int newSlot); // pNewTrack == NULL => plug is removed

  // PlugMode specific methods
  PlugAccess* getPlugAccess(){return m_pAccess;}
  String getPlugNameShort(int iSlot);
  int getNumPlugsInSelectedTrack();
  bool isFollowTrack(){return m_followTrack;}
  void setLastTimePlugWasSelected(DWORD time){m_lastTimePlugWasSelected = time;}

  typedef boost::tuple<GUID, int, unsigned int> tFav; // GUID, slot, frameTimeOfChange
  tFav getFavorite(unsigned i);

  // Helper
  String shortPlugName(const char* pName);
  String longPlugName(const char* pName);

  //      MediaTrack* selectedTrack(); // returns the MasterTrack if zero or more then one track is selected

 private:        
  void switchDisplay();
  void updateParamsDisplay();
  void updateValueDisplay();
  void updateTouchedDisplay();
  int randomPreset();

  bool isSingleFaderTouched() {return (m_iSingleFaderTouched > 0 && m_pCCSManager->getNumVPotTouched() == 0);}
  bool isSingleVPotTouched() {return (m_pCCSManager->getNumFadersTouched() == 0 && m_iSingleVPotTouched > 0);}
  void writeFavsToProjectConfig(XmlElement* pNode);
  void readFavsFromProjectConfig(XmlElement* pNode);
  void writeLastCalledPresetsToProjectConfig(XmlElement* pNode);
  void readLastCalledPresetsFromProjectConfig(XmlElement* pNode);
  void handlePresetChange( int presetNr, int slot, int randomPresetNr );
  bool m_followTrack;

  Display* m_pParamsDisplay;
  Display* m_pValueDisplay;
  Display* m_pTouchedDisplay;
  Display* m_pSingleTrackMessage;
  Display* m_pNoPlugMessage;
  Display* m_pNoPlugSelectedMessage;

  int m_iSingleFaderTouched; // the channel of the touched fader, 0 means no fader or more then 1 is touched
  int m_iSingleVPotTouched; // the channel of the touched vpot, 0 means no vpot or more then 1 is touched
  bool m_buttonNameValuePressed;

  std::vector<tFav> m_favPlugins;

  PlugAccess* m_pAccess;

  PlugPresetManager* m_pPresetManager;

  PlugModeSelector* m_pPlugSelector;
  BankPagePlugSelector* m_pBankPagePlugSelector;

  Options* m_pPlugModeOptions;
  Options* m_pPlugMode2ndOptions;

  PlugModeComponent* m_pPlugEditor;

  DWORD m_lastTimePlugWasSelected;
  //      int m_lastCalledPreset; // -1 if no preset was called or lastCalled has Changed

  int m_projectChangedConnectionId;
  int m_plugMovedConnectionId;

  typedef std::map<String, int> tLCPs; // fxGUID
  tLCPs m_lastCalledPreset;
};
