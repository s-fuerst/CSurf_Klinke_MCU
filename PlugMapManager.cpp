/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include <boost/bind.hpp>
#include <boost\foreach.hpp>
#include "PlugMapManager.h"
#include "PlugMap.h"
#include "PlugMode.h"
#include "PlugModeComponent.h"
#include "juce_amalgamated.h"
#include "PlugMoveWatcher.h"

#define PMM_ATT_VERSION JUCE_T("version")

// comparison, not case sensitive.
bool compare_file(File first, File second) {
  return first.getFileName().toLowerCase() < second.getFileName().toLowerCase();
}

PlugMapManager::PlugMapManager(PlugMode *pMode)
    : m_pMap(NULL), m_pMode(pMode), m_mapType(MAP_NOT_EXIST),
      m_pPlugTrack(NULL), m_iSlot(-1), m_mapName(String::empty),
      m_autoSave(false) {
  m_pMap = new PlugMap();

  rescanDirs();

  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&PlugMapManager::projectChanged, this, _1, _2));
  m_plugMovedConnectionId = PlugMoveWatcher::instance()->connectPlugMoveSignal(
      boost::bind(&PlugMapManager::plugMoved, this, _1, _2, _3, _4));
  m_plugMovedFinishedConnectionId =
      PlugMoveWatcher::instance()->connectPlugMoveFinishedSignal(
          boost::bind(&PlugMapManager::plugMoveCheckFinished, this));
}

PlugMapManager::~PlugMapManager(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);
  PlugMoveWatcher::instance()->disconnectPlugMoveSignal(
      m_plugMovedConnectionId);
  PlugMoveWatcher::instance()->disconnectPlugMoveFinishedSignal(
      m_plugMovedFinishedConnectionId);

  if (m_autoSave && m_mapType == USER_MAP)
    rewriteMapFile();

  safe_delete(m_pMap);
}

const File PlugMapManager::getUserMapsLocation() {
  File userMapDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      JUCE_T("\\Reaper\\MCU\\PlugMaps\\");

  if (!userMapDir.exists()) {
    userMapDir.createDirectory();
  }

  return userMapDir;
}

const File PlugMapManager::getInstalledMapsLocation() {
  return File(String(GetResourcePath()) + JUCE_T("\\UserPlugins\\MCU\\PlugMaps\\"));
}

void PlugMapManager::initMap() {
  m_pMap->initValues();

  m_pMode->updateEverything();
}

bool PlugMapManager::readMapFile(File mapFile) {
  m_pMap->initValues();

  XmlDocument *pXmlFile = new XmlDocument(mapFile);
  if (!pXmlFile)
    return false;

  XmlElement *pRootElement = pXmlFile->getDocumentElement();
  if (!pRootElement)
    return false;

  bool succeded = m_pMap->readFromXml(pRootElement);

  safe_delete(pRootElement);
  safe_delete(pXmlFile);
  return succeded;
}

bool PlugMapManager::writeMapFile(String mapFileName) {
  assert(isValidAdditionalName(mapFileName));

  BOOST_FOREACH (File &f, m_userFiles) {
    if (mapFileName.equalsIgnoreCase(f.getFileNameWithoutExtension())) {
      if (!AlertWindow::showOkCancelBox(
              AlertWindow::QuestionIcon, JUCE_T("Overwrite file"),
              JUCE_T("Are you sure that you want to overwrite an already "
                     "existing map?")))
        return false;
    }
  }

  File writeFile = File(getUserMapsLocation().getFullPathName() + JUCE_T("\\") +
                        mapFileName + JUCE_T(".xml"));
  if (!writeFile.create()) {
    AlertWindow::showMessageBox(
        AlertWindow::WarningIcon, JUCE_T("Couldn't write file"),
        JUCE_T("For whatever reason the file couldn't be saved (maybe the name "
               "contains characters that are invalid for filenames)"));
    return false;
  }

  m_mapName = mapFileName;
  m_mapType = USER_MAP;

  rewriteMapFile();

  return true;
}

void PlugMapManager::rewriteMapFile() {
  ASSERT(m_mapName.isNotEmpty());

  XmlElement *pRootElement = new XmlElement("PLUGMAP");
  pRootElement->setAttribute(PMM_ATT_VERSION, 1);

  m_pMap->writeToXml(pRootElement);

  File writeFile = File(getUserMapsLocation().getFullPathName() + JUCE_T("\\") +
                        m_mapName + JUCE_T(".xml"));

  pRootElement->writeToFile(writeFile, "", JUCE_T("UTF-8"));

  safe_delete(pRootElement);

  scanDir(getUserMapsLocation(), m_userFiles);
}

void PlugMapManager::scanDir(const File &dirToScan, tFiles &addFoundMaps) {
  addFoundMaps.clear();

  Array<File> results;

  FileSearchPath search;

  search.add(dirToScan);

  search.findChildFiles(results, File::findFiles, false, JUCE_T("*.xml"));

  for (int i = 0; i < results.size(); i++)
    addFoundMaps.push_front(results[i]);
}

bool PlugMapManager::loadMapForPlug(MediaTrack *pMediaTrack, int iSlot) {
  deselectMap();

  m_pPlugTrack = pMediaTrack;
  m_iSlot = iSlot;
  m_plugID = tPlugID(GUID2String(CSurf_MCU::GUIDfromTrack(pMediaTrack)), iSlot);

  String plugName = GetPlugName(m_pPlugTrack, m_iSlot);

  // first look in the local maps
  if (m_localMaps.find(m_plugID) != m_localMaps.end()) {
    delete (m_pMap);
    m_pMap = m_localMaps[m_plugID];
    m_mapType = LOCAL_MAP;
    return true;
  }

  // then the user-files
  bool validFile = findMapInList(plugName, m_userFiles);
  if (validFile) {
    m_mapType = USER_MAP;
    return true;
  }

  // and at least the installed files
  validFile = findMapInList(plugName, m_installedFiles);
  if (validFile) {
    m_mapType = INSTALLED_MAP;
    return true;
  }

  m_pMap->initValues();
  m_pMode->getPlugAccess()->createDefaultMap();
  m_mapName = String::empty;
  m_mapType = MAP_NOT_EXIST;
  return false;
}

bool PlugMapManager::findMapInList(String &plugName, tFiles &mapList) {
  for (tFiles::iterator iterFiles = mapList.begin(); iterFiles != mapList.end();
       ++iterFiles) {
    String mapName = (*iterFiles).getFileNameWithoutExtension().toLowerCase();
    if (plugName.toLowerCase().indexOf(mapName) > -1) {
      m_mapName = (*iterFiles).getFileNameWithoutExtension();
      return readMapFile(*iterFiles);
    }
  }

  return false;
}

const String PlugMapManager::getUserMapsAsString() {
  String ret;
  m_userFiles.sort(compare_file);
  BOOST_FOREACH (File &f, m_userFiles) {
    ret += f.getFileNameWithoutExtension() + "\n";
  }
  return ret;
}

const String PlugMapManager::getInstalledMapsAsString() {
  String ret;
  m_installedFiles.sort(compare_file);
  BOOST_FOREACH (File &f, m_installedFiles) {
    ret += f.getFileNameWithoutExtension() + "\n";
  }
  return ret;
}

// check that no file of the userFiles contains the newName and vis a vis
bool PlugMapManager::isValidAdditionalName(String newName) {
  BOOST_FOREACH (File &f, m_userFiles) {
    if (newName.toLowerCase().contains(
            f.getFileNameWithoutExtension().toLowerCase()) ||
        f.getFileNameWithoutExtension().toLowerCase().contains(
            newName.toLowerCase())) {
      m_newNameInvalidBecauseOf = f.getFileNameWithoutExtension();
      return newName.equalsIgnoreCase(f.getFileNameWithoutExtension());
    }
  }

  return true;
}

void PlugMapManager::deselectMap() {
  if (m_autoSave && m_mapType == PlugMapManager::USER_MAP)
    rewriteMapFile();

  if (m_mapType == PlugMapManager::LOCAL_MAP) {
    m_pMap = new PlugMap();
  }

  m_mapType = MAP_NOT_EXIST;
  m_mapName = String::empty;
}

void PlugMapManager::rescanDirs() {
  scanDir(getUserMapsLocation(), m_userFiles);
  scanDir(getInstalledMapsLocation(), m_installedFiles);
}

String PlugMapManager::nameIsInvalidBecauseOf() {
  return m_newNameInvalidBecauseOf;
}

void PlugMapManager::setLocalMap(bool localMap) {
  if (localMap) {
    m_mapType = LOCAL_MAP;
    m_localMaps[m_plugID] = m_pMap;
  } else {
    ASSERT(m_localMaps.find(m_plugID) != m_localMaps.end());
    m_localMaps.erase(m_plugID);

    m_mapType = MAP_NOT_EXIST;
    loadMapForPlug(m_pPlugTrack, m_iSlot);
  }
}

#define PLUGMAPMANAGER_NODE_ROOT JUCE_T("PLUGMAPMANAGER")
#define PLUGMAPMANAGER_NODE_MAP JUCE_T("LOCALMAP")
#define PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_TRACK JUCE_T("track")
#define PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_SLOT JUCE_T("slot")

void PlugMapManager::projectChanged(XmlElement *pXmlElement,
                                    ProjectConfig::EAction action) {
  XmlElement *pPlugModeNode;

  switch (action) {
  case ProjectConfig::WRITE:
    pPlugModeNode = new XmlElement(PLUGMAPMANAGER_NODE_ROOT);
    pXmlElement->addChildElement(pPlugModeNode);
    writeLocalMapsToProjectConfig(pPlugModeNode);
    break;
  case ProjectConfig::FREE:
    for (tPlugMap::iterator iterMaps = m_localMaps.begin();
         iterMaps != m_localMaps.end(); ++iterMaps) {
      delete ((*iterMaps).second);
    }
    m_localMaps.clear();
    break;
  case ProjectConfig::READ:
    pPlugModeNode = pXmlElement->getChildByName(PLUGMAPMANAGER_NODE_ROOT);
    if (pPlugModeNode)
      readLocalMapsFromProjectConfig(pPlugModeNode);
    break;
  }
}

void PlugMapManager::writeLocalMapsToProjectConfig(XmlElement *pNode) {
  for (tPlugMap::iterator iterMaps = m_localMaps.begin();
       iterMaps != m_localMaps.end(); ++iterMaps) {
    XmlElement *pMapNode = new XmlElement(PLUGMAPMANAGER_NODE_MAP);

    tPlugID plugID = (*iterMaps).first;
    pMapNode->setAttribute(PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_TRACK,
                           plugID.get<0>());
    pMapNode->setAttribute(PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_SLOT,
                           plugID.get<1>());
    (*iterMaps).second->writeToXml(pMapNode);
    pNode->addChildElement(pMapNode);
  }
}

void PlugMapManager::readLocalMapsFromProjectConfig(XmlElement *pNode) {
  forEachXmlChildElement(*pNode, pChild) {
    if (pChild->getTagName() == PLUGMAPMANAGER_NODE_MAP) {
      String guidString =
          pChild->getStringAttribute(PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_TRACK);
      int slot = pChild->getIntAttribute(PLUGMAPMANAGER_NODE_ROOT_ATT_FAV_SLOT);
      PlugMap *pPlugMap = new PlugMap();
      pPlugMap->readFromXml(pChild);
      m_localMaps[tPlugID(guidString, slot)] = pPlugMap;
    }
  }
}

void PlugMapManager::plugMoved(MediaTrack *pOldTrack, int oldSlot,
                               MediaTrack *pNewTrack, int newSlot) {
  tPlugID oldPlugID = tPlugID(GUID2String(CSurf_MCU::GUIDfromTrack(pOldTrack)), oldSlot);
  tPlugID newPlugID;
  if (pNewTrack) {
    newPlugID = tPlugID(GUID2String(CSurf_MCU::GUIDfromTrack(pNewTrack)), newSlot);
  } // so if we don't have a new track, newPlugID.get<0>() will return an empty
    // string in movePlugMap

  if (m_oldMaps.find(oldPlugID) != m_oldMaps.end() &&
      m_oldMaps[oldPlugID] == NULL) {
    return; // a plugin was already moved to oldPlugID, but the plugin that was
            // on this place, didn't had a local map, so nothing is to do
  }

  if (oldPlugID == m_plugID && m_mapType == LOCAL_MAP) {
    deselectMap(); // the actual selected plugin was moved, first deselect it,
                   // so the local maps gets stored
  }

  if (m_localMaps.find(oldPlugID) == m_localMaps.end()) {
    return; // we have no local map for the moved plugin, so nothing is to do
  }

  // in the case that a localMap would be overwritten, store it to the local
  // maps
  if (pNewTrack && m_localMaps.find(newPlugID) != m_localMaps.end()) {
    m_oldMaps[newPlugID] = m_localMaps[newPlugID];
  } else {
    m_oldMaps[newPlugID] =
        NULL; // NULL is used as a marker, that the slot was empty before a
    // plugin with a local map was moved to this position
  }

  if (m_oldMaps.find(oldPlugID) !=
      m_oldMaps.end()) // something was already moved to the plug-in slot was
                       // already stored to oldMaps
    movePlugMap(m_oldMaps, oldPlugID, newPlugID);
  else
    movePlugMap(m_localMaps, oldPlugID, newPlugID);
}

void PlugMapManager::movePlugMap(tPlugMap &plugMaps, tPlugID oldPlugID,
                                 tPlugID newPlugID) {
  if (newPlugID.get<0>() != String::empty) {
    m_localMaps[newPlugID] = plugMaps[oldPlugID];
  } else {
    delete (plugMaps[oldPlugID]);
  }
  plugMaps.erase(oldPlugID);
}

void PlugMapManager::plugMoveCheckFinished() { m_oldMaps.clear(); }
