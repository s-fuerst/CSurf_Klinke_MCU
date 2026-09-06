/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * TrackFX_* wrapper for Channel Strip Mode.
 *
 * Responsibilities:
 *   • enumerate installed FX (EnumInstalledFX) — for the editor combo box
 *   • resolve a strip's fxIdent to a live 0-based FX slot on a track, with a
 *     per-(trackGUID, stripIndex) slot cache that survives reorders and is
 *     invalidated by PlugMoveWatcher
 *   • read/write a bound parameter (normalized 0..1) and toggle 0/1
 *   • add a missing plugin at the strip's insert position (the "+" flow)
 *
 * Parameter values use REAPER's NORMALIZED 0..1 APIs so a generic VPOT can
 * drive any parameter type without per-plugin range knowledge.
 *
 * The strip's live fxGUID is NOT stored in the global ChannelStripMap (which
 * is shared across all tracks); it lives only in this per-(track, strip)
 * cache.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripMap.h"
#include "FxSlotCommands.h" // slot helpers (delegated, Layer 1)
#include <vector>
#include <map>

class ChannelStripMode;
class MediaTrack;

class ChannelStripAccess {
public:
  struct InstalledFX {
    String name;  // display name, e.g. "ReaEQ (Cockos)"
    // AddByName key: "JS:reafx/..." for JS, but a FILE PATH for
    // VST/VST3/CLAP (e.g. "/path/reaeq.vst.so").
    String ident;
  };

  ChannelStripAccess(ChannelStripMode *pMode);
  ~ChannelStripAccess();

  // --- installed FX enumeration ---
  static void getInstalledFX(std::vector<InstalledFX> &out);

  // --- resolution (all return a 0-based slot index, or -1 if not found) ---
  // Thin delegation: the canonical implementation lives in
  // FxSlotCommands (shared with the CTRL+VPOT command bodies).
  static int findSlotByGUID(MediaTrack *tr, const String &guid) {
    return FxSlotCommands::findSlotByGUID(tr, guid);
  }
  static int findSlotByIdent(MediaTrack *tr, const String &fxIdent);
  // Display name of the installed FX whose EnumInstalledFX ident equals the
  // given ident ("" if not found). Used to match file-path idents (VST/VST3)
  // against on-track FX display names.
  static String installedNameForIdent(const String &ident);

  // Resolve a strip against the given track using the slot cache. Returns the
  // 0-based slot or -1 if the plugin is not on the track (dangling / "+").
  // The cache is keyed by (trackGUID, stripIndex) and stores the slot plus the
  // instance GUID at cache time; a stale slot (GUID mismatch after reorder)
  // triggers a re-resolve by fxIdent.
  int resolveSlot(MediaTrack *tr, int stripIndex, const ChannelStripMap &strip);

  // Invalidate all cache entries for one track (called on plugMoved / delete).
  void invalidateTrack(MediaTrack *tr);
  void invalidateAll();

  // --- parameter I/O (normalized 0..1) ---
  static double getParamValue(MediaTrack *tr, int slot, int param);
  static void setParamValue(MediaTrack *tr, int slot, int param, double norm);
  // VPOT turn: step the normalized value by numSteps; returns the new value.
  static double nudgeParam(MediaTrack *tr, int slot, int param, int numSteps);
  // VPOT turn for a DISCRETE parameter (detected at bind time, see
  // ChannelStripMap::getDiscreteCountForVPOT). One turn event moves to the
  // next/previous discrete value only, no matter the CC delta. Resolution
  // order:
  //   (0) the count was entered by hand in the mapping editor (manual),
  //       or the parameter has exactly two values (valueCount == 2)
  //       -> step that grid unconditionally. A two-value parameter flips
  //       between its range endpoints; the name walk would stop at the
  //       display boundary (~0.5) and leave the value mid-range.
  //   (1) the FX reports a step size:
  //       - the stored valueCount DIFFERS from the live grid's value count
  //         -> a stale detection: step the STORED grid.
  //       - it matches (the normal detected case) -> step the exact live
  //         quantization grid. This takes precedence over the name walk
  //         because value formatters may show unquantized values, which
  //         would make the name walk stop between grid points (the FX
  //         snaps the value back).
  //   (2) no live step grid, but value names: walk the normalized value in
  //       0.01 increments (direction +1 = turned right, -1 = turned left)
  //       until the formatted value NAME changes or the value reaches 1 or
  //       0 (covers parameters with non-even value distributions).
  //   (3) no step grid and no value names: step the stored valueCount grid
  //       (detected at bind time or entered by hand).
  // Returns the new value (the old one if nothing changed).
  static double nudgeDiscreteParam(MediaTrack *tr, int slot, int param,
                                   int direction, int valueCount,
                                   bool manual);
  // Press on a DISCRETE parameter: advance to the NEXT value and wrap from
  // the highest back to the lowest (cycle). The steps are the even grid of
  // the stored valueCount — the same positions the VPOT LED ring quantizes
  // to. valueCount == 2 therefore behaves like a plain 0/1 toggle.
  static void cycleDiscreteParam(MediaTrack *tr, int slot, int param,
                                 int valueCount);
  // Discrete detection at bind time (channel-strip specific, looser than
  // PlugMode's fillDiscreteSteps, which is kept untouched): returns the
  // number of discrete values, or 0 = not discrete / continuous.
  //   (1) the FX reports a step grid — the quantization is known exactly
  //       (spurious toggle flags on stepped parameters are filtered out,
  //       genuine toggles count as two values)
  //   (2) PlugAccess::fillDiscreteSteps (verified, evenly distributed
  //       value-name scan)
  //   (3) a value-name scan WITHOUT the even-distribution verification
  //       (PlugMode needs it for its exact step positions; the channel
  //       strip's name-change walk does not): 2..100 distinct value names
  //       across the range count as discrete
  static int detectDiscreteCount(MediaTrack *tr, int slot, int param);
  // Toggle: set to 1.0 if the current value != 1.0, else 0.0 (per notes.org).
  static void toggleParam(MediaTrack *tr, int slot, int param);

  // --- parameter metadata ---
  static int getNumParams(MediaTrack *tr, int slot);
  static String getParamName(MediaTrack *tr, int slot, int param);
  // Formatted parameter value (e.g. "1.0k", "-3.2 dB"). Uses the optional
  // TrackFX_FormatParamValue API; returns "" if unavailable.
  static String getFormattedParamValue(MediaTrack *tr, int slot, int param);
  // Formatted value name at a NORMALIZED 0..1 position (e.g. "On"/"Off",
  // "SINE"). Uses TrackFX_FormatParamValueNormalized; returns "" if the FX
  // reports none. Public: used by the discrete detection and the VPOT
  // name-change walk.
  static String formattedValueName(MediaTrack *tr, int slot, int param,
                                   double normalized);

  // --- add plugin ("+" flow) ---
  // Adds the strip's plugin at its insert position on the given track and
  // updates the slot cache for (track, stripIndex). Returns the new 0-based
  // slot index, or -1 on failure.
  int addPlugin(MediaTrack *tr, int stripIndex, const ChannelStripMap &strip);
  // User-visible UI slot (1-based, includes empty slots) of an FX index
  // (REAPER 7.75+ named config parm chain_index_to_slot), or -1 if unknown
  // (REAPER < 7.75). NOTE: the API returns 0-based slot numbers (verified
  // 2026-08-29: first FX reports slot 0) — the "UI slot" shown in REAPER's
  // TCP/MCP is this value + 1.
  // Delegated to FxSlotCommands (canonical implementation there).
  static int uiSlotForIndex(MediaTrack *tr, int fxIndex) {
    return FxSlotCommands::uiSlotForIndex(tr, fxIndex);
  }
  // Try to place the FX at fxIdx into the given SLOT (0-based, as reported
  // by chain_index_to_slot). REAPER 7.75+ only. Returns: 1 = FX is now at
  // targetSlot; 0 = no effect; -1 = the FX moved but NOT to targetSlot.
  static int tryMoveToUiSlot(MediaTrack *tr, int fxIdx, int targetSlot) {
    return FxSlotCommands::tryMoveToUiSlot(tr, fxIdx, targetSlot);
  }
  // TrackFX_AddByName instantiate argument for (insertPos, current chain len).
  static int instantiateArgFor(ChannelStripMap::InsertPos pos, int chainLen);

  // --- PlugMoveWatcher hook ---
  void plugMoved(MediaTrack *pOldTrack, int oldSlot,
                 MediaTrack *pNewTrack, int newSlot);

private:
  // Strip a "TYPE:" prefix and surrounding whitespace from a name or ident so
  // "VST3: ReaEQ (Cockos)" / "VST3:ReaEQ (Cockos)" compare equal.
  static String normalizeName(const String &nameOrIdent);

  ChannelStripMode *m_pMode;

  // cache key: (trackGUID, stripIndex) -> (slot, instance GUID at cache time)
  struct CacheEntry {
    int slot;
    String fxGUID;
  };
  std::map<std::pair<String, int>, CacheEntry> m_slotCache;

  int m_plugMoveConnectionId;
};
