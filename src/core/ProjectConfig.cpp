/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ProjectConfig.h"
#include "csurf.h"
#include <cstring>

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

  // '<' and '>' can't be used in the project files, so escape them as |#{ / }#|
  // before writing, and convert back when reading. XmlElement::toString() emits
  // CRLF line endings, so each physical line ends in "\r\n".
  //
  // Escape + line-split are done in ONE pass over a writable byte buffer we
  // own. The previous version built a JUCE String and split it with
  // indexOfChar/substring in a loop; both walk UTF-8 from index 0 on every
  // call, so the split was O(n^2) and the profiler showed it dominating
  // saveExtensionConfig (called by REAPER per undo checkpoint, i.e. per track
  // add). Scanning raw UTF-8 bytes is safe here: '<', '>', '\r', '\n' are all
  // ASCII and never occur inside a multibyte sequence, so byte positions are
  // correct line boundaries and the emitted line bytes are preserved verbatim.
  String rawXml = createXmlDocString();
  const char *src = rawXml.toRawUTF8();
  const size_t len = std::strlen(src);

  juce::HeapBlock<char> buf(len * 3 + 1);
  char *w = buf;
  for (size_t i = 0; i < len; ++i) {
    const char c = src[i];
    if (c == '<') {
      *w++ = '|'; *w++ = '#'; *w++ = '{';
    } else if (c == '>') {
      *w++ = '}'; *w++ = '#'; *w++ = '|';
    } else {
      *w++ = c;
    }
  }
  *w = '\0';
  const size_t wlen = (size_t) (w - buf);

  ctx->AddLine(CONFIG_ID);
  // Emit each CRLF-delimited line (stripping the "\r\n"), chunked at 4000 to
  // dodge a ctx->AddLine buffer-overwrite with very long lines.
  char *lineStart = buf;
  for (size_t i = 0; i <= wlen; ++i) {
    if (i == wlen || buf[i] == '\n') {
      char *contentEnd = &buf[i];
      if (contentEnd > lineStart && contentEnd[-1] == '\r')
        --contentEnd;  // drop the '\r' of "\r\n"

      size_t n = (size_t) (contentEnd - lineStart);
      size_t off = 0;
      while (n - off > 4000) {
        char saved = lineStart[off + 4000];
        lineStart[off + 4000] = '\0';
        ctx->AddLine("%s", lineStart + off);
        lineStart[off + 4000] = saved;
        off += 4000;
      }
      char saved = *contentEnd;
      *contentEnd = '\0';
      ctx->AddLine("%s", lineStart + off);
      *contentEnd = saved;

      lineStart = &buf[i] + 1;  // next line begins after the '\n'
    }
  }
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
  String doc = root->toString (XmlElement::TextFormat().withoutHeader());
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
