/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * TrackFX_* wrapper for Channel Strip Mode.
 *
 * Responsibilities (Step B4):
 *   • enumerate installed FX (EnumInstalledFX) — for the editor combo box
 *   • resolve a binding's fxIdent/fxGUID to a live 0-based FX slot on a track
 *   • read/write a bound parameter (normalized 0..1) and toggle 0/1
 *   • add a missing plugin at the binding's insert position (the "+" flow)
 *
 * Parameter values use REAPER's NORMALIZED 0..1 APIs so a generic VPOT can
 * drive any parameter type without per-plugin range knowledge.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripMap.h"
#include <vector>

class ChannelStripMode;
class MediaTrack;

class ChannelStripAccess {
public:
  struct InstalledFX {
    String name;  // display name, e.g. "ReaEQ (Cockos)"
    String ident; // AddByName key, e.g. "VST3:ReaEQ (Cockos)"
  };

  ChannelStripAccess(ChannelStripMode *pMode);
  ~ChannelStripAccess();

  // Called when the selected track changes.
  void trackChanged(MediaTrack *pTrack);
  MediaTrack *getTrack() const { return m_pTrack; }

  // --- installed FX enumeration ---
  static void getInstalledFX(std::vector<InstalledFX> &out);

  // --- resolution (all return a 0-based slot index, or -1 if not found) ---
  static int findSlotByGUID(MediaTrack *tr, const String &guid);
  static int findSlotByIdent(MediaTrack *tr, const String &fxIdent);
  // Resolve a binding against the track: if its fxGUID matches a live slot,
  // keep it; otherwise try fxIdent by name. Updates b.fxGUID. Returns the slot
  // index or -1 if the plugin is not on the track (dangling / needs "+").
  static int resolveBinding(MediaTrack *tr, ChannelStripMap &strip);

  // --- parameter I/O (normalized 0..1) ---
  static double getParamValue(MediaTrack *tr, int slot, int param);
  static void setParamValue(MediaTrack *tr, int slot, int param, double norm);
  // VPOT turn: step the normalized value by numSteps; returns the new value.
  static double nudgeParam(MediaTrack *tr, int slot, int param, int numSteps);
  // Toggle: set to 1.0 if the current value != 1.0, else 0.0 (per notes.org).
  static void toggleParam(MediaTrack *tr, int slot, int param);

  // --- parameter metadata ---
  static int getNumParams(MediaTrack *tr, int slot);
  static String getParamName(MediaTrack *tr, int slot, int param);

  // --- add plugin ("+" flow) ---
  // Adds the binding's plugin at its insert position on the current track and
  // fills b.fxGUID. Returns the new 0-based slot index, or -1 on failure.
  int addPlugin(ChannelStripMap &strip);
  // TrackFX_AddByName instantiate argument for (insertPos, current chain len).
  static int instantiateArgFor(ChannelStripMap::InsertPos pos,
                               int chainLen);

private:
  // Strip a "TYPE:" prefix and surrounding whitespace from a name or ident so
  // "VST3: ReaEQ (Cockos)" / "VST3:ReaEQ (Cockos)" compare equal.
  static String normalizeName(const String &nameOrIdent);

  ChannelStripMode *m_pMode;
  MediaTrack *m_pTrack;
};
