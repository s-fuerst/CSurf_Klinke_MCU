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
#include "McuDebugLog.h"
#include <cstring>
#include <unordered_set>

PanMode::PanMode(CCSManager *pManager) : MultiTrackMode(pManager) {
  // CTRL+VPOT command set (modifier-command-scheme.md Layer 2). `control`
  // is the unshifted 0..7 VPOT index (hardware VPOT N = control N-1).
  // The target is the single selected normal track.
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
  // precondition fails (not exactly one selected normal track) it is a
  // no-op instead of falling through.
  int localCh = (channel - 1) % 8;
  if (isModifierPressed(VK_CONTROL) && !isModifierPressed(VK_SHIFT)) {
    MCU_LOG("PanMode CTRL VPOT ch=%d local=%d selectedTrack=%p", channel,
            localCh, (void *)selectedCommandTrack());
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

  bool showSelectionHint = isModifierPressed(VK_CONTROL) &&
                           !selectedCommandTrack();
  if (m_ctrlSelectionHintVisible && !showSelectionHint)
    m_pDisplay->clearLine(1);
  m_ctrlSelectionHintVisible = showSelectionHint;

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
          // Row 3 (second panel) is written by MultiTrackMode::updateDisplay();
          // here row 1 shows the value the VPOT controls.
          if (s_flipmode)
            m_pDisplay->showDB(1, iTrack,
                               m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
          else
            m_pDisplay->showPan(1, iTrack,
                                m_pCCSManager->getMCU()->GetSurfacePan(tr));
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

  // CONTROL held: show either the command legend or a centered selection
  // hint on row 1 of every unit.
  if (isModifierPressed(VK_CONTROL)) {
    if (showSelectionHint)
      m_pDisplay->changeTextFullLine(1, "You must select a single track.",
                                     true);
    else
      updateCtrlLegend();
  }
}

// --- CTRL command handlers (modifier command scheme) ---
//
// All commands target exactly one selected normal track. Duplicate uses
// REAPER's native action so that it handles identities and folder contents.
// Clear deletes items via API and removes track envelope sections from the
// state chunk.

MediaTrack *PanMode::selectedCommandTrack() const {
  MediaTrack *tr = Tracks::instance()->getSelectedSingleTrack(true);
  return tr && tr != GetMasterTrack(NULL) ? tr : NULL;
}

bool PanMode::ctrlInsertTrack(int channel) {
  MediaTrack *tr = selectedCommandTrack();
  if (!tr)
    return false;
  // Insert directly after the selected track.
  int idx = -1;
  for (int i = 0; i < CountTracks(NULL); i++)
    if (GetTrack(NULL, i) == tr) {
      idx = i + 1;
      break;
    }
  if (idx < 0)
    return false;
  Undo_BeginBlock();
  InsertTrackAtIndex(idx, false);
  Undo_EndBlock("Insert track", 0);
  updateEverything();
  return true;
}

bool PanMode::ctrlDuplicateTrack(int channel) {
  MediaTrack *tr = selectedCommandTrack();
  if (!tr)
    return false;

  // REAPER duplicates folders according to its own rules. Identify the first
  // newly inserted track so only the copied parent is selected afterwards.
  std::unordered_set<MediaTrack *> existingTracks;
  for (int i = 0; i < CountTracks(NULL); ++i)
    existingTracks.insert(GetTrack(NULL, i));
#ifdef MCU_DEBUG_LOG
  DWORD started = timeGetTime();
#endif
  const int duplicateTracksAction = 40062; // Track: Duplicate tracks
  SendMessage(g_hwnd, WM_COMMAND, duplicateTracksAction, 0);
#ifdef MCU_DEBUG_LOG
  DWORD duplicated = timeGetTime();
#endif
  MediaTrack *copy = NULL;
  for (int i = 0; i < CountTracks(NULL); ++i) {
    MediaTrack *candidate = GetTrack(NULL, i);
    if (existingTracks.find(candidate) == existingTracks.end()) {
      copy = candidate;
      break;
    }
  }
  if (!copy) {
    MCU_LOG("CTRL Duplic ch=%d: REAPER did not create a track", channel);
    return false;
  }
  for (int i = 0; i < CountTracks(NULL); ++i)
    SetTrackSelected(GetTrack(NULL, i), false);
  SetTrackSelected(copy, true);
  CSurf_OnTrackSelection(copy);
  TrackList_AdjustWindows(false);
  updateEverything();
  MCU_LOG("CTRL Duplic ch=%d: copy=%p actionMs=%lu totalMs=%lu", channel,
          (void *)copy, (unsigned long)(duplicated - started),
          (unsigned long)(timeGetTime() - started));
  return true;
}

bool PanMode::ctrlClearTrack(int channel) {
  MediaTrack *tr = selectedCommandTrack();
  if (!tr) {
    MCU_LOG("CTRL Clear ch=%d: no single selected normal track", channel);
    return false;
  }
  Undo_BeginBlock();
  // Delete every item (index 0 always holds the next item after a delete).
  int deletedItems = 0;
  while (MediaItem *item = GetTrackMediaItem(tr, 0)) {
    if (!DeleteTrackMediaItem(tr, item)) {
      MCU_LOG("CTRL Clear ch=%d: item deletion failed", channel);
      break;
    }
    deletedItems++;
    if (deletedItems > 10000) {
      MCU_LOG("CTRL Clear ch=%d: item delete loop aborted (safety cap)",
              channel);
      break;
    }
  }
  MCU_LOG("CTRL Clear ch=%d: deleted %d items", channel, deletedItems);
  // Delete every envelope by stripping the <ENV> sections from the track
  // chunk. The write-back is skipped unless the surgery succeeded, left
  // the chunk balanced, and really removed something.
  char *chunk = GetSetObjectState(tr, "");
  if (chunk) {
    MCU_LOG("CTRL Clear ch=%d: chunk len=%u head=[%.*s]",
            channel, (unsigned)strlen(chunk), 200, chunk);
    String stripped;
    bool stripOk = stripEnvelopeSections(String::fromUTF8(chunk), stripped);
    bool writeBack = stripOk && !stripped.isEmpty() &&
                     !stripped.contains("<ENV");
    char *written = NULL;
    if (writeBack)
      written = GetSetObjectState(tr, stripped.toRawUTF8());
    MCU_LOG(
        "CTRL Clear ch=%d: stripOk=%d strippedLen=%d writeBack=%d "
        "writeResult=%p",
        channel, (int)stripOk, stripped.length(), (int)writeBack,
        (void *)written);
    if (written)
      FreeHeapPtr(written);
    FreeHeapPtr(chunk);
  } else {
    MCU_LOG("CTRL Clear ch=%d: GetSetObjectState(tr) returned NULL",
            channel);
  }
  Undo_EndBlock("Clear track", 0);
  UpdateArrange();
  updateEverything();
  return true;
}

bool PanMode::ctrlRemoveTrack(int channel) {
  MediaTrack *tr = selectedCommandTrack();
  if (!tr)
    return false;
  Undo_BeginBlock();
  DeleteTrack(tr);
  Undo_EndBlock("Remove track", 0);
  updateEverything();
  return true;
}

// Remove every track-envelope section from a state chunk. REAPER names these
// sections VOLENV, PANENV, MUTEENV, AUXVOLENV, AUXPANENV and PARMENV; they
// are direct children of the outer <TRACK> section (depth 1). Sections are
// brace-nested, so the matching closer is a lone '>' at the opener's depth.
bool PanMode::stripEnvelopeSections(const String &chunk, String &out) {
  StringArray lines;
  lines.addLines(chunk);
  StringArray kept;
  int depth = 0;
  int envelopeDepth = -1;
  bool removedAny = false;
  for (int i = 0; i < lines.size(); i++) {
    String t = lines[i].trim();
    if (t.startsWith("<") && t.contains("ENV"))
      MCU_LOG("CTRL Clear: envelope section depth=%d header=[%s]", depth,
              t.toRawUTF8());
    bool isEnvelope =
        depth == 1 &&
        (t.startsWith("<VOLENV") || t.startsWith("<PANENV") ||
         t.startsWith("<MUTEENV") || t.startsWith("<AUXVOLENV") ||
         t.startsWith("<AUXPANENV") || t.startsWith("<PARMENV"));
    if (envelopeDepth < 0 && isEnvelope) {
      envelopeDepth = depth;
      removedAny = true;
    }
    if (envelopeDepth >= 0) {
      if (t.startsWith("<"))
        depth++;
      else if (t == ">") {
        depth--;
        if (depth == envelopeDepth)
          envelopeDepth = -1;
      }
      continue;
    }
    if (t.startsWith("<"))
      depth++;
    else if (t == ">")
      depth--;
    kept.add(lines[i]);
  }
  if (envelopeDepth != -1 || depth != 0 || !removedAny)
    return false;
  out = kept.joinIntoString("\n");
  return true;
}

void PanMode::updateCtrlLegend() {
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  bool available = selectedCommandTrack() != NULL;
  for (int iTrack = 1; iTrack <= nStrips; iTrack++) {
    int localCh = (iTrack - 1) % 8;
    String label;
    if (available) {
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
