/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "Options.h"

#define MTO_SHOW String("Show")
#define MTOA_SHOW_ALL String("all")
#define MTOA_SHOW_MCP String("Mixer")
#define MTOA_SHOW_TCP String("TCP")
#define MTOA_SHOW_SET String("Mackie Set")
#define MTOA_SHOW_SENDS String("sends")
#define MTOA_SHOW_RECEIVES String("receives")

#define MTO_REFLECT_FOLDER String("Folder Mode")
#define MTOA_REFLECT_NO String("flat")
#define MTOA_REFLECT_YES String("only children")
#define MTOA_REFLECT_PLUS String("incl. parent")

#define MTO_DISABLE_ANCHORS String("Use Anchors")
#define MTOA_ANCHORS_NO String("no")
#define MTOA_ANCHORS_YES String("yes")

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
