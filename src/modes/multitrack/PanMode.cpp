/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PanMode.h"
#include "reaper_plugin.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "Display.h"
#include "HardwareUnit.h"
#include "Tracks.h"

PanMode::PanMode(CCSManager *pManager) : MultiTrackMode(pManager) {
  // CTRL+VPOT command set (modifier-command-scheme.md Layer 2). `control`
  // is the unshifted 0..7 VPOT index (hardware VPOT N = control N-1).
  // Target is the track on the pressed channel.
  m_ctrlCommands.add(VK_CONTROL, 0, // hardware VPOT 1: "Insert"
                     [this](int ch) { return ctrlInsertTrack(ch); });
  m_ctrlCommands.add(VK_CONTROL, 1, // hardware VPOT 2: "Duplic"
                     [this](int ch) { return ctrlDuplicateTrack(ch); });
  m_ctrlCommands.add(VK_CONTROL, 2, // hardware VPOT 3: "Clear"
                     [this](int ch) { return ctrlClearTrack(ch); });
  m_ctrlCommands.add(VK_CONTROL, 3, // hardware VPOT 4: "Remove"
                     [this](int ch) { return ctrlRemoveTrack(ch); });
}

PanMode::~PanMode(void) {}

void PanMode::ensureVpotValueState(int channelCount) {
  if ((int)m_vpotValueShownTill.size() < channelCount + 1)
    m_vpotValueShownTill.resize(channelCount + 1, 0);
}

bool PanMode::vpotMoved(int channel, int numSteps) {
  if (m_pCCSManager->getVPOT(channel)->isPressed()) {
    numSteps *= 5;
  }

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    if (s_flipmode) {
      CSurf_SetSurfaceVolume(
          tr, CSurf_OnVolumeChange(tr, numSteps * 11.0 / 31.0, true), NULL);
    } else {
      CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr, numSteps / 40.0, true),
                          NULL);
    }
    updateVPOTs();

    // Briefly show the value the VPOT just changed (Pan, or Volume when
    // flipped) on this channel's row 1. ProX already shows every value, so
    // only non-ProX units opt in.
    HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(channel);
    if (u && !u->isProX()) {
      ensureVpotValueState(m_pCCSManager->getMCU()->availableChannels());
      m_vpotValueShownTill[channel] =
          m_pCCSManager->getLastTime() + VPOT_VALUE_SHOW_MS;
    }

    return true;
  }

  return false;
}

bool PanMode::vpotPressed(int channel, bool pressed) {
  if (!pressed)
    return false;

  // CTRL+VPOT commands (modifier command scheme), unshifted VPOT range
  // only. A MATCHED command always consumes the event — when its
  // precondition fails (no track on this channel, except Insert) it is a
  // no-op instead of falling through.
  int localCh = (channel - 1) % 8;
  if (isModifierPressed(VK_CONTROL) && !isModifierPressed(VK_SHIFT)) {
    if (m_ctrlCommands.dispatch(VK_CONTROL, localCh, channel))
      return true;
    if (m_ctrlCommands.hasCommand(VK_CONTROL, localCh))
      return false; // command inactive on this channel: do nothing else
  }
  return false;
}

void PanMode::activate() {
  // call MultiTrackMode::activate() first to handle display init
  // (MultiDisplay::switchToAll for N>1, clear + switch for N=1)
  MultiTrackMode::activate();
  m_pCCSManager->getMCU()->enableMCUMeters(true);
}

void PanMode::updateDisplay() {
	m_pCCSManager->switchToDisplay(this, m_pDisplay);
	
  MultiTrackMode::updateDisplay();
  // widened from 8 to getNumberOfChannelStrips()
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  DWORD now = m_pCCSManager->getLastTime();
    for (int iTrack = 1; iTrack <= nStrips; iTrack++) {
      MediaTrack *tr = getMediaTrackForChannel(iTrack);
      if (tr) {
        // per-unit ProX check
        HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(iTrack);
        if (u && u->isProX()) {
          if (s_flipmode) {
            m_pDisplay->showPan(3, iTrack,
                                m_pCCSManager->getMCU()->GetSurfacePan(tr));
            m_pDisplay->showDB(1, iTrack,
                              m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
          }
          else {
            m_pDisplay->showDB(3, iTrack,
                              m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
            m_pDisplay->showPan(1, iTrack,
                                m_pCCSManager->getMCU()->GetSurfacePan(tr));
          }
        } else {
          // Non-ProX: row 1 normally shows the value the FADER controls
          // (Volume, or Pan when flipped). For ~1s after the VPOT is turned
          // it instead shows the value the VPOT controls (Pan, or Volume when
          // flipped), which is otherwise not visible on a single-panel unit.
          if (showingVpotValue(iTrack, now)) {
            if (s_flipmode)
              m_pDisplay->showDB(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
            else
              m_pDisplay->showPan(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfacePan(tr));
          } else {
            if (s_flipmode)
              m_pDisplay->showPan(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfacePan(tr));
            else
              m_pDisplay->showDB(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
          }
        }
      } else {
        m_pDisplay->changeField(1, iTrack, "");
      }
    }

  // CONTROL held: per-channel command legend on row 1 (same convention as
  // ChannelStripMode / PlugMode). Redrawn every frame, so press/release
  // needs no edge tracking here.
  if (isModifierPressed(VK_CONTROL))
    updateCtrlLegend();
}

// --- CTRL command handlers (modifier command scheme) ---
//
// Target is the track on the pressed channel (getMediaTrackForChannel).
// All commands need a track except Insert, which inserts at position 0
// when the channel has no track.
//
// REAPER has no "duplicate track" and no "delete envelope" API, so those
// two commands work on the RPPXML state chunk (GetSetObjectState):
//   • Duplic: read the whole track chunk (items, envelopes, FX, settings),
//     drop the track's own GUID line so the copy gets a fresh identity,
//     and apply it to a new track.
//   • Clear:  delete every item via API and strip every <ENV> section from
//     the track chunk (the standard "remove all envelopes" technique).

bool PanMode::ctrlInsertTrack(int channel) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  // Insert directly after the channel's track; position 0 when the
  // channel has no track.
  int idx = 0;
  if (tr) {
    for (int i = 0; i < CountTracks(NULL); i++)
      if (GetTrack(NULL, i) == tr) {
        idx = i + 1;
        break;
      }
  }
  Undo_BeginBlock();
  InsertTrackAtIndex(idx, false);
  Undo_EndBlock("Insert track", 0);
  updateEverything();
  return true;
}

bool PanMode::ctrlDuplicateTrack(int channel) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (!tr)
    return false; // no track on this channel: command inactive
  // Full copy (track + items + envelopes + FX) via the state chunk,
  // inserted right after the original.
  char *state = GetSetObjectState(tr, "");
  if (!state)
    return false;
  String chunk = withoutTopLevelGUID(String::fromUTF8(state));
  FreeHeapPtr(state);
  int idx = 0;
  for (int i = 0; i < CountTracks(NULL); i++)
    if (GetTrack(NULL, i) == tr) {
      idx = i + 1;
      break;
    }
  Undo_BeginBlock();
  MediaTrack *copy = InsertTrackAtIndex(idx, false);
  if (copy)
    GetSetObjectState(copy, chunk.toRawUTF8());
  Undo_EndBlock("Duplicate track", 0);
  updateEverything();
  return true;
}

bool PanMode::ctrlClearTrack(int channel) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (!tr)
    return false; // no track on this channel: command inactive
  Undo_BeginBlock();
  // Delete every item (index 0 always holds the next item after a delete).
  while (MediaItem *item = GetTrackMediaItem(tr, 0))
    DeleteTrackMediaItem(tr, item);
  // Delete every envelope by stripping the <ENV> sections from the track
  // chunk. The write-back is skipped unless the surgery succeeded, left
  // the chunk balanced, and really removed something.
  char *chunk = GetSetObjectState(tr, "");
  if (chunk) {
    String stripped;
    if (stripEnvelopeSections(String::fromUTF8(chunk), stripped) &&
        !stripped.isEmpty() && !stripped.contains("<ENV"))
      GetSetObjectState(tr, stripped.toRawUTF8());
    FreeHeapPtr(chunk);
  }
  Undo_EndBlock("Clear track", 0);
  updateEverything();
  return true;
}

bool PanMode::ctrlRemoveTrack(int channel) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (!tr)
    return false; // no track on this channel: command inactive
  Undo_BeginBlock();
  DeleteTrack(tr);
  Undo_EndBlock("Remove track", 0);
  updateEverything();
  return true;
}

// Remove every <ENV ...> section from a track state chunk. RPPXML sections
// are brace-nested: a section opener line starts with '<', the matching
// closer is a lone '>' at the opener's depth. We skip from a track-level
// <ENV opener to its closing '>'. Returns false (and leaves `out` empty)
// when the chunk is unbalanced or contains no envelope section — callers
// must then leave the track untouched.
bool PanMode::stripEnvelopeSections(const String &chunk, String &out) {
  StringArray lines;
  lines.addLines(chunk);
  StringArray kept;
  int depth = 0;
  int envDepth = -1; // >= 0: inside an <ENV> section, opened at this depth
  bool removedAny = false;
  for (int i = 0; i < lines.size(); i++) {
    String t = lines[i].trim();
    if (envDepth < 0 && t.startsWith("<ENV")) {
      envDepth = depth;
      removedAny = true;
    }
    if (envDepth >= 0) {
      if (t.startsWith("<"))
        depth++;
      else if (t == ">") {
        depth--;
        if (depth == envDepth)
          envDepth = -1; // <ENV> section closed: resume keeping lines
      }
      continue; // this line belongs to the section being stripped
    }
    if (t.startsWith("<"))
      depth++;
    else if (t == ">")
      depth--;
    kept.add(lines[i]);
  }
  if (envDepth != -1 || depth != 0 || !removedAny)
    return false;
  out = kept.joinIntoString("\n");
  return true;
}

// Drop the track's own GUID line (a top-level "GUID ..." line) from a
// track state chunk, so a new track built from this chunk gets a fresh
// GUID instead of inheriting the source track's identity.
String PanMode::withoutTopLevelGUID(const String &chunk) {
  StringArray lines;
  lines.addLines(chunk);
  StringArray kept;
  int depth = 0;
  for (int i = 0; i < lines.size(); i++) {
    String t = lines[i].trim();
    if (depth == 0 && t.startsWith("GUID"))
      continue; // drop the track GUID line
    if (t.startsWith("<"))
      depth++;
    else if (t == ">")
      depth--;
    kept.add(lines[i]);
  }
  return kept.joinIntoString("\n");
}

void PanMode::updateCtrlLegend() {
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iTrack = 1; iTrack <= nStrips; iTrack++) {
    int localCh = (iTrack - 1) % 8;
    MediaTrack *tr = getMediaTrackForChannel(iTrack);
    String label;
    if (!tr) {
      // Empty channel: only Insert is possible there.
      if (localCh == 0) // hardware VPOT 1
        label = "Insert";
    } else {
      switch (localCh) {
      case 0: // hardware VPOT 1
        label = "Insert";
        break;
      case 1: // hardware VPOT 2
        label = "Duplic";
        break;
      case 2: // hardware VPOT 3
        label = "Clear";
        break;
      case 3: // hardware VPOT 4
        label = "Remove";
        break;
      default:
        break;
      }
    }
    m_pDisplay->changeField(1, iTrack, label.toRawUTF8());
  }
}
