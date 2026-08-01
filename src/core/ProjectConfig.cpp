/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ProjectConfig.h"
#include "csurf.h"

#define CONFIG_ID "<MCU_KLINKE"
#define CONFIG_ID_JUCE String("<MCU_KLINKE")

bool ProcessExtensionLine(
			  const char *line, ProjectStateContext *ctx, bool isUndo,
			  struct project_config_extension_t *
			  reg) // returns BOOL if line (and optionally subsequent lines) processed
{
  return ProjectConfig::instance()->processExtensionLine(line, ctx, isUndo,
                                                         reg);
}

void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo,
                         struct project_config_extension_t *reg) {
  ProjectConfig::instance()->saveExtensionConfig(ctx, isUndo, reg);
}

void BeginLoadProjectState(bool isUndo,
                           struct project_config_extension_t *reg) {
  ProjectConfig::instance()->beginLoadProjectState(isUndo, reg);
}

project_config_extension_t csurf_mcu_pcreg = {
  ProcessExtensionLine,
  SaveExtensionConfig,
  BeginLoadProjectState,
  NULL,
};

ProjectConfig *ProjectConfig::s_instance = NULL;

ProjectConfig *ProjectConfig::instance() {
  if (s_instance == NULL) {
    s_instance = new ProjectConfig();
  }
  return s_instance;
}

ProjectConfig::ProjectConfig(void)
  : m_pLastMaster(NULL), m_nextConnectionId(0) {}

ProjectConfig::~ProjectConfig(void) {
  for (tXMLStorage::iterator iterStorage = m_xmlStorage.begin();
       iterStorage != m_xmlStorage.end(); ++iterStorage) {
    delete ((*iterStorage).second);
  }
  m_xmlStorage.clear();

  s_instance = NULL;
}

project_config_extension_t *ProjectConfig::getRegisterInfo() {
  return &csurf_mcu_pcreg;
}

bool ProjectConfig::processExtensionLine(
					 const char *line, ProjectStateContext *ctx, bool isUndo,
					 struct project_config_extension_t *
					 reg) // returns BOOL if line (and optionally subsequent lines) processed
{
  bool commentign = false;
  String buildString;

  char linebuf[4096];

  if (String(line).contains(CONFIG_ID_JUCE)) {
    for (;;) {
      if (ctx->GetLine(linebuf, sizeof(linebuf)))
        break;

      if (String(linebuf).trimStart().startsWithChar('>'))
        break;

      buildString += String(linebuf);
    }

    // '<' and '>' can't be used in the project files, so i replace them with
    // |#{ and }#| before writing into the file and convert this back here
    XmlDocument *pTmpDoc =
      new XmlDocument(buildString.replace(String("|#{"), String("<"))
		      .replace(String("}#|"), String(">")));
    auto docRoot = pTmpDoc->getDocumentElement();
    XmlElement *pElement = docRoot.get();
    if (pElement) {
      m_signalProjectChanged(pElement, READ);
    }
    delete (pTmpDoc);
    return true;
  }

  return false;
}

void ProjectConfig::saveExtensionConfig(
					ProjectStateContext *ctx, bool isUndo,
					struct project_config_extension_t *reg) {
  bool commentign = false;

  // '<' and '>' can't be used in the project files, so i replace them with |#{
  // and }#| before writing into the file and convert this back after reading
  // from it
  String xmlDocString = createXmlDocString()
    .replace(String("<"), String("|#{"))
    .replace(String(">"), String("}#|"));

  ctx->AddLine(CONFIG_ID);
  // the fucking buffer overwrite in ctx->AddLine took me two days of debugging
  // to avoid it, the xmlDocString must be diveded in single parts
  int from = 0;
  int to = 0;
  do {
    int to = xmlDocString.indexOfChar(from, '\n');

    if (to != -1) {
      if (to - from > 4000) {
        to = from + 4000;
        ctx->AddLine("%s", xmlDocString.substring(from, to).toRawUTF8());
      } else {
        ctx->AddLine("%s", xmlDocString.substring(from, to - 1).toRawUTF8());
        from = to + 1;
      }
    } else {
      ctx->AddLine("%s", xmlDocString.substring(from).toRawUTF8());
      break;
    }
  } while (to != -1); // to < found

  ctx->AddLine(">");
}

void ProjectConfig::beginLoadProjectState(
					  bool isUndo, struct project_config_extension_t *reg) {
  checkReaProjectChange();
  m_pLastMaster = GetMasterTrack(NULL);
  bool commentign = false;
  m_signalProjectChanged(NULL, FREE);
}

String ProjectConfig::createXmlDocString() {
  XmlElement *root = new XmlElement(String("PROJECT_CONFIG"));
  m_signalProjectChanged(root, WRITE);
  String doc = root->createDocument("", false, false);
  delete (root);
  return doc;
}

void ProjectConfig::checkReaProjectChange() {
  MediaTrack *pActualMasterTrack = GetMasterTrack(NULL);
  if (m_pLastMaster != pActualMasterTrack) {
    if (m_pLastMaster != NULL) {
      { String s = createXmlDocString(); store(m_pLastMaster, s); }
    }
    if (m_xmlStorage.find(pActualMasterTrack) != m_xmlStorage.end()) {
      m_signalProjectChanged(NULL, FREE);
      auto docRoot = m_xmlStorage[pActualMasterTrack]->getDocumentElement();
      XmlElement *pDocElement = docRoot.get();
      if (pDocElement)
        m_signalProjectChanged(pDocElement, READ);
    }
    m_pLastMaster = pActualMasterTrack;
  }
}

void ProjectConfig::store(MediaTrack *pMT, String &xmlString) {
  if (m_xmlStorage.find(pMT) != m_xmlStorage.end()) {
    delete (m_xmlStorage[pMT]);
  }
  m_xmlStorage[pMT] = new XmlDocument(xmlString);
}

int ProjectConfig::connect2ProjectChangeSignal(
					       const tProjectChangedSignalSlot &slot) {
  m_activeProjectChangedConnections[++m_nextConnectionId] =
    m_signalProjectChanged.connect(slot);
  return m_nextConnectionId;
}

void ProjectConfig::disconnectProjectChangeSignal(int connectionId) {
  m_activeProjectChangedConnections[connectionId].disconnect();
  m_activeProjectChangedConnections.erase(
					  m_activeProjectChangedConnections.find(connectionId));
}
