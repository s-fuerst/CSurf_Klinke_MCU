/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include "PlugPresetManager.h"
#include "PlugMoveWatcher.h"

PlugPresetManager::PlugPresetManager(CSurf_MCU *pMCU) : m_pMCU(pMCU) {
  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&PlugPresetManager::projectChanged, this, _1, _2));
}

PlugPresetManager::~PlugPresetManager(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);
}

void PlugPresetManager::storePreset(MediaTrack *pTrack, int slot, int presetNr,
                                    PlugAccess *pAccess) {
  String fxGUID = GUID2String(TrackFX_GetFXGUID(pTrack, slot));

  deletePreset(fxGUID, presetNr);

  tPreset newPreset;
  PlugAccess::ElementDesc ed(0, 0, PlugAccess::ElementDesc::FADER, 0);
  for (int iBank = 0; iBank < 8; iBank++) {
    ed.m_bank = iBank;
    for (int iPage = 0; iPage < 8; iPage++) {
      ed.m_page = iPage;
      for (int iChannel = 0; iChannel < 8; iChannel++) {
        ed.m_channel = iChannel;
        // FADER
        ed.m_type = PlugAccess::ElementDesc::FADER;
        addParam2Preset(pAccess, ed, pTrack, slot, newPreset);
        // VPOT
        ed.m_type = PlugAccess::ElementDesc::VPOT;
        addParam2Preset(pAccess, ed, pTrack, slot, newPreset);
      }
    }
  }

  ed.m_type = PlugAccess::ElementDesc::DRYWET;
  addParam2Preset(pAccess, ed, pTrack, slot, newPreset);

  ed.m_type = PlugAccess::ElementDesc::BYPASS;
  addParam2Preset(pAccess, ed, pTrack, slot, newPreset);

  ed.m_type = PlugAccess::ElementDesc::DELTA;
  addParam2Preset(pAccess, ed, pTrack, slot, newPreset);

  m_presetStorage.push_back(tPresetWithPos(fxGUID, presetNr, newPreset));
}

bool PlugPresetManager::presetMatchState(MediaTrack *pTrack, int slot,
                                         int presetNr) {
  String fxGUID = GUID2String(TrackFX_GetFXGUID(pTrack, slot));

  if (!hasPreset(fxGUID, presetNr))
    return false;

  tPreset compareTo;

  BOOST_FOREACH (tPresetWithPos preset, m_presetStorage) {
    if (preset.get<0>() == fxGUID && preset.get<1>() == presetNr) {
      compareTo = preset.get<2>();
      break;
    }
  }

  BOOST_FOREACH (tPlugValue &v, compareTo) {
    double min, max;
    double val = TrackFX_GetParam(pTrack, slot, v.get<0>(), &min, &max);
    if (abs(val - v.get<1>()) > 0.0001)
      return false;
  }

  return true;
}

void PlugPresetManager::addParam2Preset(PlugAccess *pAccess,
                                        PlugAccess::ElementDesc ed,
                                        MediaTrack *pTrack, int slot,
                                        tPreset &newPreset) {
  int paramId = pAccess->getParamID(&ed);
  if (paramId != NOT_ASSIGNED && paramId >= 0 &&
      paramId < pAccess->getNumParams(true) /*incl dry, wet & delta*/) {
    double min, max;
    double val = TrackFX_GetParam(pTrack, slot, paramId, &min, &max);
    newPreset.push_back(tPlugValue(paramId, val));
  }
}

bool PlugPresetManager::hasPreset(String &fxGUID, int presetNr) {
  BOOST_FOREACH (tPresetWithPos preset, m_presetStorage) {
    if (preset.get<0>() == fxGUID && preset.get<1>() == presetNr)
      return true;
  }

  return false;
}

bool PlugPresetManager::recallPreset(MediaTrack *pTrack, int slot,
                                     int presetNr) {
  String fxGUID = GUID2String(TrackFX_GetFXGUID(pTrack, slot));
  BOOST_FOREACH (tPresetWithPos preset, m_presetStorage) {
    if (preset.get<0>() == fxGUID && preset.get<1>() == presetNr) {
      tPreset p = preset.get<2>();
      BOOST_FOREACH (tPlugValue &v, p) {
        TrackFX_SetParam(pTrack, slot, v.get<0>(), v.get<1>());
      }
      return true;
    }
  }

  return false;
}

void PlugPresetManager::deleteAllPresets(String &fxGUID) {
  std::vector<tPresetWithPos> toDelete;
  for (tPresetStorage::iterator iterPreset = m_presetStorage.begin();
       iterPreset != m_presetStorage.end(); ++iterPreset) {
    if ((*iterPreset).get<0>() == fxGUID) {
      toDelete.push_back(*iterPreset);
    }
  }

  for (std::vector<tPresetWithPos>::iterator iterPreset = toDelete.begin();
       iterPreset != toDelete.end(); ++iterPreset) {
    m_presetStorage.remove(*iterPreset);
  }
}

bool PlugPresetManager::deletePreset(String &fxGUID, int presetNr) {
  for (tPresetStorage::iterator iterPreset = m_presetStorage.begin();
       iterPreset != m_presetStorage.end(); ++iterPreset) {
    if ((*iterPreset).get<0>() == fxGUID &&
        (*iterPreset).get<1>() == presetNr) {
      m_presetStorage.erase(iterPreset);
      return true;
    }
  }

  return false;
}

#define PLUGPRESETMANAGER_NODE_ROOT String("PLUGPRESETMANAGER")

void PlugPresetManager::projectChanged(XmlElement *pXmlElement,
                                       ProjectConfig::EAction action) {
  XmlElement *pPlugModeNode;

  switch (action) {
  case ProjectConfig::WRITE:
    pPlugModeNode = new XmlElement(PLUGPRESETMANAGER_NODE_ROOT);
    pXmlElement->addChildElement(pPlugModeNode);
    writePresetsToProjectConfig(pPlugModeNode);
    break;
  case ProjectConfig::FREE:
    m_presetStorage.clear();
    break;
  case ProjectConfig::READ:
    pPlugModeNode = pXmlElement->getChildByName(PLUGPRESETMANAGER_NODE_ROOT);
    if (pPlugModeNode)
      readPresetsFromProjectConfig(pPlugModeNode);
    break;
  }
}

#define PLUGPRESETMANAGER_NODE_PRESET String("PLUG-PRESET")
#define PLUGPRESETMANAGER_ATT_FX String("fxGUID")
#define PLUGPRESETMANAGER_ATT_NR String("nr")

#define PLUGPRESETMANAGER_NODE_PRESETDATA String("DA")
#define PLUGPRESETMANAGER_ATT_PRESETDATA String("ta")

void PlugPresetManager::writePresetsToProjectConfig(XmlElement *pNode) {
  BOOST_FOREACH (tPresetWithPos &presetWP, m_presetStorage) {
    XmlElement *pMapNode = new XmlElement(PLUGPRESETMANAGER_NODE_PRESET);

    pMapNode->setAttribute(PLUGPRESETMANAGER_ATT_FX, presetWP.get<0>());
    pMapNode->setAttribute(PLUGPRESETMANAGER_ATT_NR, presetWP.get<1>());

    MemoryOutputStream out;

    BOOST_FOREACH (tPlugValue &pv, presetWP.get<2>()) {
      int id = pv.get<0>();
      double val = pv.get<1>();
      out.write(&id, sizeof(int));
      out.write(&val, sizeof(double));

      if (out.getDataSize() >= 512) {
        XmlElement *pDataNode =
            new XmlElement(PLUGPRESETMANAGER_NODE_PRESETDATA);
        MemoryBlock presetData(out.getData(), out.getDataSize());
        pDataNode->setAttribute(PLUGPRESETMANAGER_ATT_PRESETDATA,
                                presetData.toBase64Encoding());
        pMapNode->addChildElement(pDataNode);
        out.reset();
      }
    }

    // write the remaining data
    if (out.getDataSize() > 0) {
      XmlElement *pDataNode = new XmlElement(PLUGPRESETMANAGER_NODE_PRESETDATA);
      MemoryBlock presetData(out.getData(), out.getDataSize());
      pDataNode->setAttribute(PLUGPRESETMANAGER_ATT_PRESETDATA,
                              presetData.toBase64Encoding());
      pMapNode->addChildElement(pDataNode);
    }

    pNode->addChildElement(pMapNode);
  }
}

void PlugPresetManager::readPresetsFromProjectConfig(XmlElement *pNode) {
  forEachXmlChildElement(*pNode, pChild) {
    if (pChild->getTagName() == PLUGPRESETMANAGER_NODE_PRESET) {
      String fxGUID = pChild->getStringAttribute(PLUGPRESETMANAGER_ATT_FX);
      int nr = pChild->getIntAttribute(PLUGPRESETMANAGER_ATT_NR);

      tPreset preset;
      forEachXmlChildElement(*pChild, pData) {
        assert(pData->getTagName() == PLUGPRESETMANAGER_NODE_PRESETDATA);
        if (pData->getTagName() == PLUGPRESETMANAGER_NODE_PRESETDATA) {
          String dataString =
              pData->getStringAttribute(PLUGPRESETMANAGER_ATT_PRESETDATA);
          MemoryBlock presetData;
          presetData.fromBase64Encoding(dataString);
          MemoryInputStream in(presetData.getData(), presetData.getSize(),
                               false);
          do {
            int id;
            double val;
            in.read(&id, sizeof(int));
            in.read(&val, sizeof(double));
            preset.push_back(tPlugValue(id, val));
          } while (!in.isExhausted());
        }
      }
      m_presetStorage.push_back(tPresetWithPos(fxGUID, nr, preset));
    }
  }

  deletePresetsWherePluginsAreRemoved();
}

void PlugPresetManager::deletePresetsWherePluginsAreRemoved() {
  // first create a list with all plugin GUIDs ...
  std::list<String> allFXGUIDs;
  for (TrackIterator ti; !ti.end(); ++ti) {
    int iNumFX = TrackFX_GetCount(*ti);
    for (int iFX = 0; iFX < iNumFX; iFX++) {
      allFXGUIDs.push_back(GUID2String(TrackFX_GetFXGUID(*ti, iFX)));
    }
  }

  // ... they search the presets that must be remove ...
  std::list<tPresetWithPos> presetToDelete;
  BOOST_FOREACH (tPresetWithPos &presetWP, m_presetStorage) {
    XmlElement *pMapNode = new XmlElement(PLUGPRESETMANAGER_NODE_PRESET);

    if (std::find(allFXGUIDs.begin(), allFXGUIDs.end(), presetWP.get<0>()) ==
        allFXGUIDs.end()) {
      presetToDelete.push_back(presetWP);
    }
  }

  // ... and at least remove them from the storage
  BOOST_FOREACH (tPresetWithPos &presetWP, presetToDelete) {
    deletePreset(presetWP.get<0>(), presetWP.get<1>());
  }
}
