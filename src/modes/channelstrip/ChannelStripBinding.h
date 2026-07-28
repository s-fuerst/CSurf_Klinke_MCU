/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * One VPOT-slot binding of a Channel Strip Map.
 *
 * A binding selects a single parameter of a plugin:
 *   fxIdent   — EnumInstalledFX ident (e.g. "VST3:ReaEQ (Cockos)"), stable
 *               across tracks; used to find/add the plugin on a track.
 *   fxGUID    — stringified TrackFX_GetFXGUID of the live instance; used to
 *               address it at runtime and survives reordering. May be empty
 *               until the plugin has been resolved/added on the track.
 *   paramIndex— parameter index within that FX (-1 = unassigned).
 *   shortName — up to 5 chars shown on the MCU display.
 *   insertPos — where TrackFX_AddByName inserts the plugin if it is not yet
 *               on the track (the "+" flow).
 *
 * The same plugin may occupy several slots (different parameters); reusing an
 * already-present instance does NOT add a second one.
 */
#pragma once
#include "JuceHeader.h"

// XML attribute / value tokens for a <SLOT .../> element (shared with
// ChannelStripMap, which owns the slot numbering).
#define CSB_ATT_NR String("nr")
#define CSB_ATT_FXIDENT String("fxident")
#define CSB_ATT_FXGUID String("fxguid")
#define CSB_ATT_PARAM String("param")
#define CSB_ATT_NAME String("name")
#define CSB_ATT_INSPOS String("inspos")
#define CSB_INS_FIRST String("first")
#define CSB_INS_LAST String("last")

class ChannelStripBinding {
public:
  enum InsertPos {
    FIRST = 0, // top of the chain
    POS2, POS3, POS4, POS5, POS6, POS7, POS8,
    LAST, // append at the end (resolved at add time)
    INSERT_COUNT
  };

  ChannelStripBinding();
  ~ChannelStripBinding() {}

  bool isAssigned() const { return m_fxIdent.isNotEmpty(); }
  bool isResolved() const { return m_fxGUID.isNotEmpty(); }

  // XML read/write on a <SLOT .../> element (this binding fills the element)
  void writeToXml(XmlElement *pSlotElement) const;
  bool readFromXml(const XmlElement *pSlotElement);

  // --- accessors ---
  const String &getFxIdent() const { return m_fxIdent; }
  void setFxIdent(const String &ident) { m_fxIdent = ident; }

  const String &getFxGUID() const { return m_fxGUID; }
  void setFxGUID(const String &guid) { m_fxGUID = guid; }

  int getParamIndex() const { return m_paramIndex; }
  void setParamIndex(int idx) { m_paramIndex = idx; }

  const String &getShortName() const { return m_shortName; }
  void setShortName(const String &name) { m_shortName = name; }

  InsertPos getInsertPos() const { return m_insertPos; }
  void setInsertPos(InsertPos pos) { m_insertPos = pos; }

  // The 1-based chain position for POS2..POS8 / FIRST; returns -1 for LAST
  // (LAST has no fixed position — it is resolved against the current chain
  // length when the plugin is added). Used by ChannelStripAccess.
  int fixedChainPosition() const;

  // insert-pos token <-> enum (for the JUCE combo box and persistence)
  static InsertPos insertPosFromToken(const String &token);
  static String tokenForInsertPos(InsertPos pos);

private:
  String m_fxIdent;
  String m_fxGUID;
  int m_paramIndex;
  String m_shortName;
  InsertPos m_insertPos;
};
