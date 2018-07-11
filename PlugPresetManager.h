/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once

#include<vector>
#include<boost/tuple/tuple.hpp>
#include<boost/tuple/tuple_comparison.hpp>
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "ProjectConfig.h"
#include "PlugAccess.h"

class MediaTrack;

class PlugPresetManager
{
 public:
  PlugPresetManager(CSurf_MCU*  pMCU);
  ~PlugPresetManager(void);

  typedef boost::tuple<int, double> tPlugValue;
  typedef std::list<tPlugValue> tPreset;
  void storePreset(MediaTrack* pTrack, int slot, int presetNr, PlugAccess* pAccess);
  bool presetMatchState(MediaTrack* pTrack, int slot, int presetNr);

  bool hasPreset(String& fxGUID, int presetNr);
  bool recallPreset(MediaTrack* pTrack, int slot, int presetNr);
  bool deletePreset(String& fxGUID, int presetNr);
  void deleteAllPresets(String& fxGUID);

  void projectChanged(XmlElement* pXmlElement, ProjectConfig::EAction action);

 private:
  void addParam2Preset( PlugAccess* pAccess, PlugAccess::ElementDesc ed, MediaTrack* pTrack, int slot, tPreset &newPreset );

  void writePresetsToProjectConfig(XmlElement* pNode);
  void readPresetsFromProjectConfig(XmlElement* pNode);

  void deletePresetsWherePluginsAreRemoved();

  typedef boost::tuple<String, int, tPreset> tPresetWithPos; // FX GUID, presetNr, time
  typedef std::list<tPresetWithPos> tPresetStorage;
  tPresetStorage m_presetStorage;

  int m_projectChangedConnectionId;

  CSurf_MCU* m_pMCU;
};
