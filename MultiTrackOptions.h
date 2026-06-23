/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "Options.h"

#define MTO_SHOW JUCE_T("Show")
#define MTOA_SHOW_ALL JUCE_T("all")
#define MTOA_SHOW_MCP JUCE_T("Mixer")
#define MTOA_SHOW_TCP JUCE_T("TCP")
#define MTOA_SHOW_SET JUCE_T("Mackie Set")
#define MTOA_SHOW_SENDS JUCE_T("sends")
#define MTOA_SHOW_RECEIVES JUCE_T("receives")

#define MTO_REFLECT_FOLDER JUCE_T("Folder Mode")
#define MTOA_REFLECT_NO JUCE_T("flat")
#define MTOA_REFLECT_YES JUCE_T("only children")
#define MTOA_REFLECT_PLUS JUCE_T("incl. parent")

#define MTO_DISABLE_ANCHORS JUCE_T("Use Anchors")
#define MTOA_ANCHORS_NO JUCE_T("no")
#define MTOA_ANCHORS_YES JUCE_T("yes")

class MultiTrackOptions : public Options {
public:
  MultiTrackOptions(DisplayHandler *pDH);

public:
  virtual ~MultiTrackOptions(void);

protected:
  String getConfigFileName();
  void checkAndModifyOptions(); // when the reflect_folder option change, the
                                // track graph must be rebuild

  int m_iReflect;
};
