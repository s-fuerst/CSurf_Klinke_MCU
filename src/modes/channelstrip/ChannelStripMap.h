/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * One Channel Strip — a single plugin plus the mapping of its parameters onto
 * the 8 VPOTs (16 with Shift). One of 16 GLOBAL strips, shared across all
 * Reaper projects and tracks (see ChannelStripMode). Each unit on each track
 * is assigned one strip index; that unit's VPOTs then drive this strip.
 *
 *   fxIdent    — EnumInstalledFX ident (e.g. "VST3:ReaEQ (Cockos)"), used to
 *                find/add the plugin on a track.
 *   shortName  — up to 5 chars shown on the MCU display.
 *   insertPos  — where TrackFX_AddByName inserts the plugin ("+" flow).
 *   vpotParam  — paramIndex (of this plugin) for each VPOT position 1..8 normal
 *                and 9..16 with Shift. -1 = unbound.
 *   discrete   — per VPOT: number of discrete values detected when the
 *                parameter was bound
 *                (ChannelStripAccess::detectDiscreteCount). 0 = not discrete
 *                / unknown (plain 0.01 nudges); >0 = discrete, the VPOT
 *                walks values in 0.01 steps until the value name changes
 *                (see ChannelStripAccess::nudgeDiscreteParam).
 *
 * NOTE: the live FX slot (fxGUID) is instance-specific and therefore does
 * NOT live in this global map. It is resolved at runtime per (track, strip)
 * and cached in ChannelStripAccess.
 *
 * XML:
 *   <STRIP nr="1" fxident="VST3:ReaEQ (Cockos)" name="EQ" inspos="last">
 *     <VPOT nr="1" param="3" discrete="4" manual="1"/>
 *     ...
 *   </STRIP>
 *
 *   discrete absent/0 = plain (0.01 nudges); >0 = value count.
 *   manual=1 = the count was entered by hand in the mapping editor.
 */
#pragma once
#include "JuceHeader.h"

// shared XML tokens + insert-position helpers
#define CSB_ATT_NR String("nr")
#define CSB_ATT_FXIDENT String("fxident")
#define CSB_ATT_NAME String("name")
#define CSB_ATT_INSPOS String("inspos")
#define CSB_ATT_PARAM String("param")
#define CSB_ATT_DISCRETE String("discrete")
#define CSB_ATT_MANUAL String("manual")
#define CSB_INS_FIRST String("first")
#define CSB_INS_LAST String("last")

class ChannelStripMap {
public:
  enum InsertPos {
    FIRST = 0, // top of the chain
    POS2, POS3, POS4, POS5, POS6, POS7, POS8,
    LAST, // append at the end (resolved at add time)
    INSERT_COUNT
  };

  static const int kNumVPOTs = 16; // VPOTs 1..8 normal, 9..16 with Shift

  ChannelStripMap();
  ~ChannelStripMap() {}

  void initEmpty();

  bool isAssigned() const { return m_fxIdent.isNotEmpty(); }

  // --- the plugin ---
  const String &getFxIdent() const { return m_fxIdent; }
  void setFxIdent(const String &ident) { m_fxIdent = ident; }
  const String &getShortName() const { return m_shortName; }
  void setShortName(const String &name) { m_shortName = name; }
  InsertPos getInsertPos() const { return m_insertPos; }
  void setInsertPos(InsertPos pos) { m_insertPos = pos; }

  // --- VPOT → parameter mapping (position 0..15, i.e. VPOT 1..8 + 9..16 shift)
  int getParamForVPOT(int position) const;        // -1 = unbound
  void setParamForVPOT(int position, int paramIdx);
  const String &getVPOTName(int position) const;
  void setVPOTName(int position, const String &name);
  // Discrete value count detected at bind time: 0 = not discrete (or
  // unknown), >0 = discrete with that many values.
  int getDiscreteCountForVPOT(int position) const;
  void setDiscreteCountForVPOT(int position, int count);
  bool isDiscreteForVPOT(int position) const;     // count > 0
  // True when the count was entered by hand in the mapping editor instead
  // of being auto-detected: the runtime then steps that grid unconditionally
  // (no live step grid, no name walk).
  bool isStepsManualForVPOT(int position) const;
  void setStepsManualForVPOT(int position, bool manual);
  int numBoundVPOTs() const;

  // insert-pos helpers (shared with ChannelStripAccess)
  int fixedChainPosition() const; // 1..8 or -1 for LAST
  static InsertPos insertPosFromToken(const String &token);
  static String tokenForInsertPos(InsertPos pos);

  // --- XML ---
  // Everything for one strip goes into a single <STRIP> element (header attrs
  // + <VPOT> children) inside the one global channelstrips.xml file.
  // nr = 1-based slot number (1..16) so the file can route each element back
  // to its slot.
  void writeToXml(XmlElement *pParent, int nr) const; // adds a <STRIP> child
  bool readFromXml(const XmlElement *pStrip);         // reads a <STRIP> element

private:
  String m_fxIdent;
  String m_shortName;
  InsertPos m_insertPos;
  int m_vpotParam[kNumVPOTs]; // -1 = unbound
  String m_vpotName[kNumVPOTs];
  int m_vpotDiscreteCount[kNumVPOTs]; // 0 = not discrete, >0 = value count
  bool m_vpotStepsManual[kNumVPOTs];  // count entered by hand, not detected
};
