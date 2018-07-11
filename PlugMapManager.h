/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <src/juce_WithoutMacros.h>
#include <list>
#include <map>
#include "ProjectConfig.h"


class PlugMap;
class PlugMode;
class MediaTrack;

class PlugMapManager
{
 protected:
  typedef std::list<File> tFiles;

 public:
  enum eMapType {
    USER_MAP = 0,
    INSTALLED_MAP,
    LOCAL_MAP,
    MAP_NOT_EXIST
  };

  PlugMapManager(PlugMode* pMode);
  ~PlugMapManager(void);

  PlugMap* getActiveMap(){return m_pMap;}
  eMapType getMapType() {return m_mapType;}
  const String getMapName(){return m_mapName;}

  bool getAutoSave(){return m_autoSave;}
  void setAutoSave(bool autoSave){m_autoSave = autoSave;}

  void setLocalMap(bool localMap);

  void initMap();

  bool writeMapFile(String mapFileName);
  void rewriteMapFile();

  bool loadMapForPlug(MediaTrack* pMediaTrack, int iSlot);

  const File getUserMapsLocation();

  void deselectMap();

  const String getUserMapsAsString();
  bool isValidAdditionalName(String newName); 

  const String getInstalledMapsAsString();

  void rescanDirs();

  String nameIsInvalidBecauseOf();

  void projectChanged(XmlElement* pXmlElement, ProjectConfig::EAction action);
  void plugMoved(MediaTrack* pOldTrack, int oldSlot, MediaTrack* pNewTrack, int newSlot); // pNewTrack == NULL => plug is removed
  void plugMoveCheckFinished();
 private:
  bool findMapInList(String& plugName, tFiles& mapList);
  const File getInstalledMapsLocation();

  bool readMapFile(File mapFile);

  static void scanDir(const File& dirToScan,  tFiles& addFoundMaps);

  void writeLocalMapsToProjectConfig(XmlElement* pNode);
  void readLocalMapsFromProjectConfig(XmlElement* pNode);


  eMapType m_mapType;

  tFiles m_userFiles; 
  tFiles m_installedFiles;

  PlugMap* m_pMap; // the active (used) map
  String m_mapName;

  bool m_autoSave; // save changes in the mapping automatically to the xml file after each change

  String m_newNameInvalidBecauseOf;

  PlugMode* m_pMode;

  typedef boost::tuple<String, int> tPlugID; // String is GUID of track
  tPlugID m_plugID;
  typedef std::map<tPlugID, PlugMap*> tPlugMap;
  tPlugMap m_localMaps;
  void movePlugMap(tPlugMap& plugMaps, tPlugID oldPlugID, tPlugID newPlugID);
  tPlugMap m_oldMaps; // plugMove need to store a second set of maps for swaping them

  MediaTrack* m_pPlugTrack;
  int m_iSlot;

  int m_projectChangedConnectionId;
  int m_plugMovedConnectionId;
  int m_plugMovedFinishedConnectionId;
};
