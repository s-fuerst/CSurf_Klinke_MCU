/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "boost/signals2.hpp"
#include "reaper_plugin.h"
#include "JuceHeader.h"
#include <map>

using boost::signals2::connection;

class ProjectConfig {
public:
  static ProjectConfig *instance();

  ~ProjectConfig(void);

  project_config_extension_t *getRegisterInfo();

  bool processExtensionLine(const char *line, ProjectStateContext *ctx,
                            bool isUndo,
                            struct project_config_extension_t
                                *reg) /* returns BOOL if line (and optionally
                                         subsequent lines) processed */
      ;
  void saveExtensionConfig(ProjectStateContext *ctx, bool isUndo,
                           struct project_config_extension_t *reg);
  void beginLoadProjectState(bool isUndo,
                             struct project_config_extension_t *reg);

  void checkReaProjectChange();

  enum EAction {
    READ = 0,
    WRITE,
    FREE
  }; //  only modes then can be set directly are enumerated here
  typedef boost::signals2::signal<void(XmlElement *, EAction)>
      tProjectChangedSignal; // XmlElement is NULL in BEGIN_LOAD case
  typedef tProjectChangedSignal::slot_type tProjectChangedSignalSlot;
  int connect2ProjectChangeSignal(
      const tProjectChangedSignalSlot
          &slot); // the returned int is the id that must be used for disconnect
  void disconnectProjectChangeSignal(int connectionId);

private:
  ProjectConfig(void);
  String createXmlDocString();
  void store(MediaTrack *pMT, String &xmlString);

  static ProjectConfig *s_instance;

  typedef std::map<MediaTrack *, XmlDocument *> tXMLStorage;
  tXMLStorage m_xmlStorage;

  tProjectChangedSignal m_signalProjectChanged;
  std::map<int, connection> m_activeProjectChangedConnections;
  int m_nextConnectionId;

  MediaTrack *m_pLastMaster;

  // slot fuer moechte bei aenderungen XMLDocument, je einer fuer lesen und
  // schreiben
};
