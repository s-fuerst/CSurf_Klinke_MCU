/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * FxSlotCommands — shared, mode-agnostic FX-slot command bodies.
 * See FxSlotCommands.h for the layer description.
 *
 * The moveFx() slot machinery was moved here verbatim from
 * ChannelStripMode::moveFx (2026-09, modifier command scheme Phase 1);
 * uiSlotForIndex()/tryMoveToUiSlot()/findSlotByGUID() were moved from
 * ChannelStripAccess (they are now the canonical location,
 * ChannelStripAccess delegates to them).
 */
#include "FxSlotCommands.h"

#include "csurf.h" // TrackFX_*, TrackList_*, CSurf_OnFXChange
#include "csurf_mcu.h" // GUID2String
#include "McuDebugLog.h"
#include <cstdlib>
#include <cstring>

void FxSlotCommands::toggleFloatingWindow(MediaTrack *tr, int fxSlot) {
  // Deliberate: the PlugMode window settings are IGNORED here. TOGGLE:
  // if the floating window for this FX is already open it is closed,
  // else it is opened. showflag 3/2 = show/close floating window.
  HWND floatHwnd = TrackFX_GetFloatingWindow(tr, fxSlot);
  int action = (floatHwnd == NULL) ? 3 : 2;
  MCU_LOG("FXC toggleFloatingWindow slot=%d floatHwnd=%p action=%d", fxSlot,
          (void *)floatHwnd, action);
  TrackFX_Show(tr, fxSlot, action);
}

void FxSlotCommands::toggleFxChain(MediaTrack *tr, int fxSlot) {
  // TOGGLE: if the FX chain for this track is already open it is closed,
  // else it is opened. showflag 1/0 = show/close chain.
  int chainVis = TrackFX_GetChainVisible(tr);
  int action = (chainVis == -1) ? 1 : 0;
  MCU_LOG("FXC toggleFxChain slot=%d chainVis=%d action=%d", fxSlot, chainVis,
          action);
  TrackFX_Show(tr, fxSlot, action);
}

bool FxSlotCommands::moveFx(MediaTrack *tr, int fxSlot, int dir) {
  // REAPER 7.75+ allows EMPTY FX SLOTS: the user-visible "slot" (as
  // reported by chain_index_to_slot, 0-BASED — verified) can differ from
  // the dense FX "index" (0-based, real FX only). Moving by index +/- 1
  // would jump OVER empty slots instead of stepping into them, so on 7.75+
  // we move by SLOT. The actual move is done by tryMoveToUiSlot, which
  // tries the possible API variants (slot_hint) and verifies each; only
  // if nothing had any effect we fall back to the classic dense index
  // move (no-op at real chain edges).
  if (!tr || fxSlot < 0)
    return false;
  int n = TrackFX_GetCount(tr);
  if (fxSlot >= n)
    return false;

  if (TrackFX_GetNamedConfigParm) {
    int uiSlot = uiSlotForIndex(tr, fxSlot);
    if (uiSlot >= 0) {
      int targetSlot = uiSlot + dir;
      char probe[64] = {0};
      if (targetSlot >= 0 &&
          TrackFX_GetNamedConfigParm(tr, targetSlot, "chain_slot_to_index",
                                     probe, sizeof(probe))) {
        MCU_LOG("FXC moveFx slot-aware idx=%d uiSlot=%d targetSlot=%d"
                " raw=[%s]",
                fxSlot, uiSlot, targetSlot, probe);
        // An occupied target must use the original dense move. With two
        // adjacent occupied slots, TrackFX_CopyToTrack(is_move=true) swaps
        // their order; the slot_hint path would insert and shift later FX.
        // chain_slot_to_index returns a dense FX index for an occupied slot
        // and "empty:x" for an empty slot.
        if (strncmp(probe, "empty:", 6) != 0) {
          int targetIdx = atoi(probe);
          if (targetIdx < 0 || targetIdx == fxSlot)
            return false;
          MCU_LOG("FXC moveFx occupied swap idx=%d targetIdx=%d", fxSlot,
                  targetIdx);
          TrackFX_CopyToTrack(tr, fxSlot, tr, targetIdx, true);
          TrackList_AdjustWindows(false);
          CSurf_OnFXChange(tr, 1);
          TrackList_UpdateAllExternalSurfaces();
          return true;
        }
        int r = tryMoveToUiSlot(tr, fxSlot, targetSlot);
        if (r == 1)
          return true;
        if (r == -1)
          return false; // moved somewhere unexpected: stop here
        // r == 0: no slot candidate had any effect -> dense fallback below
      } else {
        MCU_LOG("FXC moveFx slot-aware targetSlot=%d unknown (chain edge)",
                targetSlot);
      }
    }
  }

  // Fallback (REAPER < 7.75, or slot machinery without effect): dense
  // index +/- 1.
  int newSlot = fxSlot + dir;
  if (newSlot < 0 || newSlot >= n)
    return false; // would move out of the chain
  MCU_LOG("FXC moveFx index-based slot=%d dir=%d new=%d chain=%d", fxSlot,
          dir, newSlot, n);
  // (No explicit surface notifications here — identical to the original
  // ChannelStripMode fallback, which relied on the caller's refresh.)
  TrackFX_CopyToTrack(tr, fxSlot, tr, newSlot, true);
  return true;
}

bool FxSlotCommands::removeFx(MediaTrack *tr, int fxSlot) {
  if (!tr || fxSlot < 0 || fxSlot >= TrackFX_GetCount(tr))
    return false;
  MCU_LOG("FXC removeFx slot=%d", fxSlot);
  if (!TrackFX_Delete(tr, fxSlot))
    return false;
  TrackList_AdjustWindows(false);
  CSurf_OnFXChange(tr, 1);
  TrackList_UpdateAllExternalSurfaces();
  return true;
}

// --- slot helpers (moved from ChannelStripAccess) ---

int FxSlotCommands::findSlotByGUID(MediaTrack *tr, const String &guid) {
  if (!tr || guid.isEmpty())
    return -1;
  int n = TrackFX_GetCount(tr);
  for (int slot = 0; slot < n; slot++) {
    GUID *g = TrackFX_GetFXGUID(tr, slot);
    if (g && GUID2String(g) == guid)
      return slot;
  }
  return -1;
}

int FxSlotCommands::uiSlotForIndex(MediaTrack *tr, int fxIndex) {
  if (!TrackFX_GetNamedConfigParm || !tr || fxIndex < 0)
    return -1;
  char buf[32] = {0};
  if (TrackFX_GetNamedConfigParm(tr, fxIndex, "chain_index_to_slot", buf,
                                 sizeof(buf)))
    return atoi(buf);
  return -1;
}

int FxSlotCommands::tryMoveToUiSlot(MediaTrack *tr, int fxIdx,
                                    int targetSlot) {
  // REAPER 7.75+ only. Verified 2026-08-29: TrackFX_SetNamedConfigParm
  // with "slot_hint" = <0-based slot number> is the ONLY mechanism that
  // actually moves an FX into a (possibly empty) UI slot; the
  // TrackFX_CopyToTrack 0x800000 flag had no effect at all. The value is
  // 0-based (slot_hint = "2" placed the FX at 0-based slot 2).
  if (!TrackFX_SetNamedConfigParm || !tr || fxIdx < 0)
    return 0;
  GUID *g0 = TrackFX_GetFXGUID(tr, fxIdx);
  if (!g0)
    return 0;
  String guid = GUID2String(g0);
  int oldIdx = fxIdx;
  int oldSlot = uiSlotForIndex(tr, oldIdx);
  if (oldSlot < 0 || oldSlot == targetSlot)
    return 0;

  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%d", targetSlot);
  bool ok = TrackFX_SetNamedConfigParm(tr, oldIdx, "slot_hint", tmp);
  // Verify where the FX actually ended up.
  int moved = findSlotByGUID(tr, guid);
  int newSlot = (moved >= 0) ? uiSlotForIndex(tr, moved) : -1;
  MCU_LOG("FXC moveToUiSlot slot_hint=%d ok=%d old=(idx %d slot %d)"
          " -> new=(idx %d slot %d) target=%d",
          targetSlot, (int)ok, oldIdx, oldSlot, moved, newSlot, targetSlot);
  if (newSlot == targetSlot) {
    // slot_hint is a "preferred slot" hint — REAPER does NOT redraw the
    // TCP/MCP FX list on its own after this call. Adjust the track panels
    // first, notify the FX change, then republish the surface state.
    TrackList_AdjustWindows(false);
    CSurf_OnFXChange(tr, 1);
    TrackList_UpdateAllExternalSurfaces();
    return 1;
  }
  if (moved < 0 || moved != oldIdx || newSlot != oldSlot)
    return -1; // moved, but to the wrong place: stop touching it
  return 0;    // no effect at all
}
