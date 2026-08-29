/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * FxSlotCommands — shared, mode-agnostic FX-slot command bodies.
 *
 * Layer 1 of the modifier command scheme
 * (ai-docs/modifier-command-scheme.md). Pure REAPER TrackFX_* API, no
 * mode/csurf dependencies. Used by ChannelStripMode (the strip FX of the
 * selected track) and PlugMode (the mode-level accessed plugin) for the
 * CTRL+VPOT commands.
 *
 * Post-processing that is MODE-specific (slot-cache invalidation,
 * updateEverything, re-accessing the moved plugin) stays in the CALLING
 * MODE — the helpers do the FX operation (incl. the generic surface
 * notifications) and return success.
 */
#pragma once
#include "JuceHeader.h"

class MediaTrack;

class FxSlotCommands {
public:
  // Toggle the FLOATING FX window of the slot: open it if closed, close
  // it if already open. Deliberately ignores the PlugMode window options
  // (PMO_LIMIT_FLOATING & co) — those are PlugMode's own window
  // management; in PlugMode the per-frame checkFloatWindows() may still
  // apply them afterwards.
  static void toggleFloatingWindow(MediaTrack *tr, int fxSlot);
  // Toggle the track's FX CHAIN window: open it if closed, close it if
  // already open. fxSlot must be a valid slot of tr (TrackFX_Show needs
  // one even for chain show/hide).
  static void toggleFxChain(MediaTrack *tr, int fxSlot);
  // Move the FX by dir (+1 down / -1 up) in the chain. Slot-aware on
  // REAPER >= 7.75 (steps into EMPTY slots); dense-index fallback below.
  // Returns false when refused (chain edge / moved unexpectedly). The
  // caller does the mode-specific post-processing.
  static bool moveFx(MediaTrack *tr, int fxSlot, int dir);
  // Remove the FX slot and notify the surfaces (track-panel adjust,
  // FX-change broadcast, external surface refresh). Returns false when
  // refused.
  static bool removeFx(MediaTrack *tr, int fxSlot);

  // --- slot helpers (shared by the commands and ChannelStripAccess) ---

  // Dense FX index of the FX with the given GUID, or -1 if not on the
  // track.
  static int findSlotByGUID(MediaTrack *tr, const String &guid);
  // 0-based UI slot ("chain_index_to_slot") of a dense FX index, or -1
  // if unavailable (REAPER < 7.75).
  static int uiSlotForIndex(MediaTrack *tr, int fxIndex);
  // Move fxIdx into the 0-based UI targetSlot via the "slot_hint"
  // mechanism (REAPER 7.75+ only). Returns 1 = moved to the target
  // (verified + surfaces notified), -1 = moved somewhere unexpected,
  // 0 = no effect.
  static int tryMoveToUiSlot(MediaTrack *tr, int fxIdx, int targetSlot);
};
