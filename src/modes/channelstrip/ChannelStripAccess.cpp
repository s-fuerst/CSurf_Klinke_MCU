/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripAccess.h"
#include "ChannelStripMode.h"
#include "csurf.h" // TrackFX_* + EnumInstalledFX/TrackFX_AddByName (vendored)
#include "csurf_mcu.h" // GUID2String
#include "Tracks.h"
#include "PlugAccess.h" // fillDiscreteSteps (verified name scan, detectDiscreteCount path 2)
#include "PlugMoveWatcher.h"
#include "McuDebugLog.h"
#include <boost/bind.hpp>

using boost::placeholders::_1;
using boost::placeholders::_2;
using boost::placeholders::_3;
using boost::placeholders::_4;

// VPOT step size in normalized space (one detent = 1% of the parameter range)
#define CSA_VPOT_STEP (1.0 / 100.0)

namespace {
// Clamp a normalized 0..1 value.
inline double clampN(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// Number of quantization grid segments the FX reports for the parameter
// via TrackFX_GetParameterStepSizes (0 = continuous / no usable step,
// 1 = toggle, i.e. the two range endpoints). *isToggleOut (optional) is
// set when the FX marks the parameter as a two-state toggle. The
// dual-range evaluation mirrors PlugAccess::discreteStepSegments (which is
// private there): use the reading that divides the respective range into
// an integral number of segments.
//
// Some FXs set the toggle flag on parameters that also report a fine step
// grid (e.g. SPL BiG marks its 41-step BiGness as toggle). Such a flag is
// spurious: a genuine toggle reports step=1 or none. Disambiguation via
// the value names: a real 2-state parameter displays one of its two
// endpoint names at the middle of the range ("Off"/"Off"/"On"), a stepped
// parameter displays a distinct middle name. Without names the fine grid
// is trusted.
int reportedStepSegments(MediaTrack *tr, int slot, int param,
                          bool *isToggleOut = NULL) {
  if (isToggleOut)
    *isToggleOut = false;
  if (!tr || slot < 0 || param < 0 || !TrackFX_GetParameterStepSizes)
    return 0;
  double step = 0.0, smallStep = 0.0, largeStep = 0.0;
  bool isToggle = false;
  if (!TrackFX_GetParameterStepSizes(tr, slot, param, &step, &smallStep,
                                     &largeStep, &isToggle))
    return 0;
  if (isToggleOut)
    *isToggleOut = true;
  if (step <= 0.0)
    return isToggle ? 1 : 0;
  double minVal = 0.0, maxVal = 1.0;
  TrackFX_GetParam(tr, slot, param, &minVal, &maxVal);
  double segments = 1.0 / step;
  const double devNormalized = fabs(segments - floor(segments + 0.5));
  if (maxVal > minVal) {
    const double segmentsRaw = (maxVal - minVal) / step;
    const double devRaw = fabs(segmentsRaw - floor(segmentsRaw + 0.5));
    if (devRaw + 0.000001 < devNormalized)
      segments = segmentsRaw;
  }
  const int segs = (int)floor(segments + 0.5);
  if (segs < 2 || segs > 100)
    return isToggle ? 1 : 0; // no fine grid: toggle = endpoints only
  if (isToggle) {
    const String n0 = ChannelStripAccess::formattedValueName(tr, slot, param, 0.0);
    const String nMid = ChannelStripAccess::formattedValueName(tr, slot, param, 0.5);
    const String n1 = ChannelStripAccess::formattedValueName(tr, slot, param, 1.0);
    if (nMid.isNotEmpty() && (nMid == n0 || nMid == n1)) {
      MCU_LOG("CSA stepSegs param=%d toggle flag + fine grid (segs=%d), middle name redundant -> real toggle",
              param, segs);
      return 1; // genuine toggle: only the two endpoints are real states
    }
    MCU_LOG("CSA stepSegs param=%d toggle flag + fine grid (segs=%d), distinct middle name -> spurious flag, use grid",
            param, segs);
    return segs; // spurious flag: the fine grid is the real quantization
  }
  return segs;
}

// Position (normalized) of the grid neighbour of v in the given direction
// on an even segments-point grid, or -1 if the value is already at the
// 0/1 end in that direction.
//
// A two-value grid (segments == 1) is special: its only two positions are
// the range endpoints, so any mid-range value belongs to neither. A turn
// always moves to the endpoint in the turn direction instead of deriving
// a "current index" (rounding would map the whole upper half of the range
// to the top step and make right turns from there dead).
double stepGridPosition(double v, int segments, int direction) {
  if (segments < 1)
    return -1.0;
  v = clampN(v);
  if (segments == 1) {
    const double target = (direction < 0) ? 0.0 : 1.0;
    if (fabs(v - target) < 1e-9)
      return -1.0; // already at that endpoint
    return target;
  }
  const int idx0 = (int)floor(v * segments + 0.5);
  int idx = idx0 + (direction < 0 ? -1 : 1);
  if (idx < 0)
    idx = 0;
  if (idx > segments)
    idx = segments;
  if (idx == idx0)
    return -1.0;
  return (double)idx / (double)segments;
}

// The EnumInstalledFX list is static within a REAPER session; cache it so
// per-channel findSlotByIdent() calls (display updates) do not re-enumerate
// the whole plugin list every time.
const std::vector<ChannelStripAccess::InstalledFX> &installedFXCache() {
  static std::vector<ChannelStripAccess::InstalledFX> list;
  static bool loaded = false;
  if (!loaded) {
    ChannelStripAccess::getInstalledFX(list);
    loaded = true;
  }
  return list;
}
} // namespace

ChannelStripAccess::ChannelStripAccess(ChannelStripMode *pMode)
    : m_pMode(pMode), m_plugMoveConnectionId(-1) {
  m_plugMoveConnectionId = PlugMoveWatcher::instance()->connectPlugMoveSignal(
      boost::bind(&ChannelStripAccess::plugMoved, this, _1, _2, _3, _4));
}

ChannelStripAccess::~ChannelStripAccess() {
  if (m_plugMoveConnectionId >= 0)
    PlugMoveWatcher::instance()->disconnectPlugMoveSignal(
        m_plugMoveConnectionId);
}

void ChannelStripAccess::getInstalledFX(std::vector<InstalledFX> &out) {
  out.clear();
  if (!EnumInstalledFX)
    return;
  // index = -1 re-reads JSFX info (REAPER 7.42+); ignore its boolean return.
  EnumInstalledFX(-1, NULL, NULL);
  for (int i = 0;; i++) {
    const char *name = NULL;
    const char *ident = NULL;
    if (!EnumInstalledFX(i, &name, &ident) || !ident)
      break;
    InstalledFX fx;
    fx.name = name ? String(name) : String();
    fx.ident = String(ident);
    out.push_back(fx);
  }
}

String ChannelStripAccess::installedNameForIdent(const String &ident) {
  if (ident.isEmpty())
    return String();
  for (const auto &fx : installedFXCache())
    if (fx.ident == ident)
      return fx.name;
  return String();
}

String ChannelStripAccess::normalizeName(const String &nameOrIdent) {
  String s = nameOrIdent.trim();
  // Strip a leading "TYPE:" prefix (e.g. "VST3:", "JS:", "AU:").
  int colon = s.indexOfChar(':');
  if (colon >= 0)
    s = s.substring(colon + 1);
  return s.trim();
}

int ChannelStripAccess::findSlotByIdent(MediaTrack *tr,
                                         const String &fxIdent) {
  if (!tr || fxIdent.isEmpty())
    return -1;
  MCU_LOG("CSA findSlotByIdent want=[%s] GetNamedConfigParm=%p count=%d",
          fxIdent.toRawUTF8(), (void *)TrackFX_GetNamedConfigParm,
          TrackFX_GetCount(tr));
  // First pass: exact match via TrackFX_GetNamedConfigParm (available in
  // REAPER 6.x+). This correctly distinguishes VST2 vs VST3 instances.
  if (TrackFX_GetNamedConfigParm) {
    int n = TrackFX_GetCount(tr);
    char buf[512];
    for (int slot = 0; slot < n; slot++) {
      if (TrackFX_GetNamedConfigParm(tr, slot, "fx_ident", buf, 511)) {
        if (fxIdent == String(buf)) {
          MCU_LOG("CSA  pass1 match slot=%d", slot);
          return slot;
        }
      }
    }
  }
  // Fallback: match by name suffix (strips the TYPE: prefix). This cannot
  // distinguish e.g. VST2:ReaEQ from VST3:ReaEQ — returns the first match.
  String want = normalizeName(fxIdent);
  // For VST/VST3/CLAP the stored ident is a FILE PATH (EnumInstalledFX
  // convention), which can never equal an on-track FX name. Also accept the
  // installed display name that belongs to that path (e.g. path
  // "/.../reaeq.vst.so" -> "ReaEQ (Cockos)").
  String wantInstalled = normalizeName(installedNameForIdent(fxIdent));
  if (want.isEmpty() && wantInstalled.isEmpty())
    return -1;
  int n = TrackFX_GetCount(tr);
  char buf[256];
  for (int slot = 0; slot < n; slot++) {
    if (TrackFX_GetFXName(tr, slot, buf, 255)) {
      String norm = normalizeName(String(buf));
      MCU_LOG("CSA  pass2 slot=%d fxname=[%s] norm=[%s] want=[%s] wantInstalled=[%s]",
              slot, buf, norm.toRawUTF8(), want.toRawUTF8(),
              wantInstalled.toRawUTF8());
      if (norm == want || (!wantInstalled.isEmpty() && norm == wantInstalled))
        return slot;
    }
  }
  MCU_LOG("CSA  findSlotByIdent NO MATCH");
  return -1;
}

int ChannelStripAccess::resolveSlot(MediaTrack *tr, int stripIndex,
                                    const ChannelStripMap &strip) {
  if (!tr || !strip.isAssigned())
    return -1;
  GUID *g = GetTrackGUID(tr);
  if (!g)
    return -1;
  String guid = GUID2String(g);
  std::pair<String, int> key(guid, stripIndex);

  auto it = m_slotCache.find(key);
  if (it != m_slotCache.end()) {
    int slot = findSlotByGUID(tr, it->second.fxGUID);
    MCU_LOG("CSA resolveSlot cache key=(%s,%d) cachedGUID=%s -> byGUID=%d",
            guid.toRawUTF8(), stripIndex, it->second.fxGUID.toRawUTF8(), slot);
    if (slot >= 0) {
      it->second.slot = slot;
      return slot;
    }
  }
  int slot = findSlotByIdent(tr, strip.getFxIdent());
  MCU_LOG("CSA resolveSlot byIdent=%d ident=[%s]", slot,
          strip.getFxIdent().toRawUTF8());
  if (slot >= 0) {
    GUID *ig = TrackFX_GetFXGUID(tr, slot);
    CacheEntry e;
    e.slot = slot;
    e.fxGUID = ig ? GUID2String(ig) : String();
    m_slotCache[key] = e;
  } else {
    m_slotCache.erase(key);
  }
  return slot;
}

void ChannelStripAccess::invalidateTrack(MediaTrack *tr) {
  if (!tr)
    return;
  GUID *g = GetTrackGUID(tr);
  if (!g)
    return;
  String guid = GUID2String(g);
  for (auto it = m_slotCache.begin(); it != m_slotCache.end();) {
    if (it->first.first == guid)
      it = m_slotCache.erase(it);
    else
      ++it;
  }
}

void ChannelStripAccess::invalidateAll() { m_slotCache.clear(); }

double ChannelStripAccess::getParamValue(MediaTrack *tr, int slot, int param) {
  if (!tr || slot < 0 || param < 0)
    return 0.0;
  double minv = 0.0, maxv = 1.0;
  double v = TrackFX_GetParam(tr, slot, param, &minv, &maxv);
  double span = maxv - minv;
  if (span <= 0.0)
    return 0.0;
  return clampN((v - minv) / span);
}

void ChannelStripAccess::setParamValue(MediaTrack *tr, int slot, int param,
                                        double norm) {
  if (!tr || slot < 0 || param < 0)
    return;
  norm = clampN(norm);
  double minv = 0.0, maxv = 1.0;
  TrackFX_GetParam(tr, slot, param, &minv, &maxv);
  TrackFX_SetParam(tr, slot, param, minv + norm * (maxv - minv));
}

double ChannelStripAccess::nudgeParam(MediaTrack *tr, int slot, int param,
                                       int numSteps) {
  if (!tr || slot < 0 || param < 0)
    return 0.0;
  double v = clampN(getParamValue(tr, slot, param) + numSteps * CSA_VPOT_STEP);
  setParamValue(tr, slot, param, v);
  return v;
}

String ChannelStripAccess::formattedValueName(MediaTrack *tr, int slot,
                                               int param,
                                               double normalized) {
  if (!tr || slot < 0 || param < 0 || !TrackFX_FormatParamValueNormalized)
    return String();
  char buf[80] = {};
  if (TrackFX_FormatParamValueNormalized(tr, slot, param,
                                         clampN(normalized), buf, 79) &&
      buf[0] != 0)
    return String(buf);
  return String();
}

double ChannelStripAccess::nudgeDiscreteParam(MediaTrack *tr, int slot,
                                               int param, int direction,
                                               int valueCount, bool manual) {
  if (!tr || slot < 0 || param < 0)
    return 0.0;
  if (direction == 0)
    direction = 1;
  const double v0 = getParamValue(tr, slot, param);

  // (0) A count entered by hand in the mapping editor, or a genuine
  // two-value parameter: step that grid unconditionally, no matter what
  // the FX reports. A two-value parameter must flip between its range
  // endpoints — the name walk would stop at the display boundary (~0.5)
  // and leave the value mid-range (the ring then shows a middle position
  // although the parameter is a simple two-state switch).
  if ((manual && valueCount >= 2) || valueCount == 2) {
    const double target = stepGridPosition(v0, valueCount - 1, direction);
    if (target >= 0.0) {
      MCU_LOG("CSA nudgeDiscrete param=%d v0=%.4f -> MANUAL grid %.4f (count=%d)",
              param, v0, target, valueCount);
      setParamValue(tr, slot, param, target);
      return target;
    }
    return v0; // already at the 0/1 end in the turn direction
  }

  // (1) The FX reports a step size. If the stored value count (detected at
  // bind time or entered by hand in the mapping editor) DIFFERS from the
  // live grid's count, it is a manual override: step the stored grid. If
  // it matches (the normal detected case), step the exact live
  // quantization grid — which must take precedence over the name-change
  // walk because the FX value formatter may display unquantized values
  // (e.g. SPL BiG BiGness shows "3.0"/"3.2"/... although the real grid has
  // 41 positions), so a name walk would stop between grid points and the
  // FX would snap the value back, leaving it unchanged.
  const int liveSegments = reportedStepSegments(tr, slot, param);
  if (liveSegments >= 1) {
    const int liveCount = (liveSegments == 1) ? 2 : liveSegments + 1;
    const int useSegments =
        (valueCount > 0 && valueCount != liveCount) ? valueCount - 1
                                                     : liveSegments;
    if (useSegments < 1)
      return v0;
    const double target = stepGridPosition(v0, useSegments, direction);
    if (target >= 0.0) {
      MCU_LOG("CSA nudgeDiscrete param=%d v0=%.4f -> grid %.4f (stored=%d live=%d, segments=%d%s)",
              param, v0, target, valueCount, liveCount, useSegments,
              (valueCount > 0 && valueCount != liveCount) ? " OVERRIDE" : "");
      setParamValue(tr, slot, param, target);
      return target;
    }
    return v0; // already at the 0/1 end in the turn direction
  }

  // (2) No step grid: walk the normalized value in 0.01 increments
  // (direction +1 = turned right, -1 = turned left) until the formatted
  // value name changes or the value reaches 1 or 0. One turn event moves
  // to the next/previous discrete value only, no matter the CC delta.
  const String nameAt0 = formattedValueName(tr, slot, param, v0);
  if (nameAt0.isNotEmpty()) {
    for (int i = 1; i <= 100; i++) {
      const double cand = clampN(v0 + direction * i * CSA_VPOT_STEP);
      if (cand == 0.0 || cand == 1.0) {
        MCU_LOG("CSA nudgeDiscrete param=%d v0=%.4f -> endpoint %.0f",
                param, v0, cand);
        setParamValue(tr, slot, param, cand);
        return cand;
      }
      const String name = formattedValueName(tr, slot, param, cand);
      if (!name.isEmpty() && name != nameAt0) {
        MCU_LOG("CSA nudgeDiscrete param=%d v0=%.4f -> %.4f (name '%s' -> '%s')",
                param, v0, cand, nameAt0.toRawUTF8(), name.toRawUTF8());
        setParamValue(tr, slot, param, cand);
        return cand;
      }
    }
    return v0; // unreachable: i=100 always hits an endpoint
  }

  // (3) No step grid and no usable value names: fall back to the stored
  // value count detected at bind time (even grid).
  const double target = stepGridPosition(v0, valueCount - 1, direction);
  if (target < 0.0)
    return v0;
  MCU_LOG("CSA nudgeDiscrete param=%d v0=%.4f -> stored-count grid %.4f (count=%d)",
          param, v0, target, valueCount);
  setParamValue(tr, slot, param, target);
  return target;
}

int ChannelStripAccess::detectDiscreteCount(MediaTrack *tr, int slot,
                                             int param) {
  if (!tr || slot < 0 || param < 0)
    return 0;

  // (1) The FX reports a step grid (with spurious-toggle disambiguation):
  // the quantization is known exactly. A genuine toggle (segments == 1)
  // counts as exactly two values.
  bool isToggle = false;
  const int segs = reportedStepSegments(tr, slot, param, &isToggle);
  if (segs >= 1) {
    const int count = (segs == 1) ? 2 : segs + 1;
    MCU_LOG("CSA detectDiscrete param=%d via step grid count=%d (segments=%d, toggle=%d)",
            param, count, segs, isToggle ? 1 : 0);
    return count;
  }

  // (2) No step grid: PlugMode's verified, evenly distributed value-name
  // scan (its step-grid part cannot fire without a step size).
  PMVPot::tSteps steps;
  int n = PlugAccess::fillDiscreteSteps(tr, slot, param, &steps);
  if (n > 0) {
    MCU_LOG("CSA detectDiscrete param=%d via PlugMode detection count=%d",
            param, n);
    return n;
  }

  // (3) Value-name scan WITHOUT the even-distribution verification: some
  // FXs quantize normalized positions by truncation, which fails
  // PlugMode's exact i/(N-1) position check although the parameter is
  // clearly discrete. For the channel strip's name-change walk any
  // 2..100 distinct value names across the range are enough.
  if (!TrackFX_FormatParamValueNormalized)
    return 0;
  const String nameAtZero = formattedValueName(tr, slot, param, 0.0);
  if (nameAtZero.isEmpty())
    return 0; // without value names the heuristic cannot work
  // If the name of 0.00 and 0.01 differ, the display changes with every
  // 1% step and the parameter is effectively continuous.
  const String nameAtOnePercent = formattedValueName(tr, slot, param, 0.01);
  if (!nameAtOnePercent.isEmpty() && nameAtOnePercent != nameAtZero)
    return 0;
  int distinct = 1;
  String lastName = nameAtZero;
  for (int i = 1; i <= 100; i++) {
    const String name = formattedValueName(tr, slot, param, i / 100.0);
    if (name.isEmpty() || name == lastName)
      continue;
    lastName = name;
    if (++distinct > 100) {
      MCU_LOG("CSA detectDiscrete param=%d scan: >100 distinct names -> continuous",
              param);
      return 0;
    }
  }
  if (distinct < 2)
    return 0;
  MCU_LOG("CSA detectDiscrete param=%d via name scan (unverified) count=%d",
          param, distinct);
  return distinct;
}

void ChannelStripAccess::toggleParam(MediaTrack *tr, int slot, int param) {
  if (!tr || slot < 0 || param < 0)
    return;
  // notes.org: set to 1 if current != 1, else 0. Compare in normalized space.
  double v = getParamValue(tr, slot, param);
  setParamValue(tr, slot, param, (v < 1.0) ? 1.0 : 0.0);
}

void ChannelStripAccess::cycleDiscreteParam(MediaTrack *tr, int slot,
                                             int param, int valueCount) {
  if (!tr || slot < 0 || param < 0 || valueCount < 2)
    return;
  const double v0 = getParamValue(tr, slot, param);
  const int segments = valueCount - 1;
  // Current step index exactly as the VPOT LED ring derives it
  // (ChannelStripMode::updateVPOTs: round-half-up over the count grid).
  int idx = (int)floor(clampN(v0) * segments + 0.5);
  if (idx < 0)
    idx = 0;
  if (idx > segments)
    idx = segments;
  // Advance one value; after the highest wrap back to the lowest.
  const int next = (idx + 1) % valueCount;
  const double target = (double)next / (double)segments;
  MCU_LOG("CSA cycleDiscrete param=%d v0=%.4f -> %.4f (step %d -> %d of %d%s)",
          param, v0, target, idx, next, valueCount,
          next == 0 ? ", wrap" : "");
  setParamValue(tr, slot, param, target);
}

int ChannelStripAccess::getNumParams(MediaTrack *tr, int slot) {
  if (!tr || slot < 0)
    return 0;
  return TrackFX_GetNumParams(tr, slot);
}

String ChannelStripAccess::getParamName(MediaTrack *tr, int slot, int param) {
  if (!tr || slot < 0 || param < 0)
    return String();
  char buf[256];
  if (TrackFX_GetParamName(tr, slot, param, buf, 255))
    return String(buf);
  return String();
}

String ChannelStripAccess::getFormattedParamValue(MediaTrack *tr, int slot,
                                                   int param) {
  if (!tr || slot < 0 || param < 0)
    return String();
  if (!TrackFX_FormatParamValue)
    return String();
  double minv = 0.0, maxv = 1.0;
  double val = TrackFX_GetParam(tr, slot, param, &minv, &maxv);
  char buf[256];
  if (TrackFX_FormatParamValue(tr, slot, param, val, buf, 255))
    return String(buf);
  return String();
}

int ChannelStripAccess::instantiateArgFor(ChannelStripMap::InsertPos pos,
                                          int chainLen) {
  using IP = ChannelStripMap::InsertPos;
  // TrackFX_AddByName: instantiate <= -1000 encodes a fixed position, where
  // position = -(instantiate). So -1000 -> position 0 (first),
  // -1001 -> position 1 (second), ... For LAST we append after the current
  // end, i.e. position = chainLen, so instantiate = -(1000 + chainLen).
  if (pos == IP::LAST)
    return -(1000 + (chainLen < 0 ? 0 : chainLen));
  int chainPos = (pos == IP::FIRST) ? 0 : (static_cast<int>(pos));
  return -(1000 + chainPos);
}

int ChannelStripAccess::addPlugin(MediaTrack *tr, int stripIndex,
                                  const ChannelStripMap &strip) {
  if (!tr || !strip.isAssigned() || !TrackFX_AddByName)
    return -1;
  // Reuse an existing instance of the same plugin if one is present (notes.org:
  // reusing the same plugin does NOT add a second instance).
  int existing = findSlotByIdent(tr, strip.getFxIdent());
  if (existing >= 0) {
    MCU_LOG("CSA addPlugin REUSE existing=%d", existing);
    GUID *g = TrackFX_GetFXGUID(tr, existing);
    GUID *tg = GetTrackGUID(tr);
    if (tg && g) {
      m_slotCache[std::make_pair(GUID2String(tg), stripIndex)] =
          CacheEntry{existing, GUID2String(g)};
    }
    return existing;
  }
  int chainLen = TrackFX_GetCount(tr);
  using IP = ChannelStripMap::InsertPos;
  IP pos = strip.getInsertPos();

  // REAPER 7.75+ (empty FX slots): a SPECIFIC insert position (POS2..POS8)
  // is honored as a user-visible SLOT — the FX is added at the end and then
  // moved into the target slot, so the earlier slots are left EMPTY. The
  // classic "instantiate <= -1000" dense position cannot do that: on a
  // chain shorter than the target position REAPER clamps it. The actual
  // move is done by tryMoveToUiSlot (candidate-based, verified, logged).
  // If the target slot is not honored, the add is undone (TrackFX_Delete)
  // and the classic dense insertion below is used. FIRST/LAST are dense
  // positions by definition and always use the classic path.
  if (pos >= IP::POS2 && pos <= IP::POS8 && TrackFX_GetNamedConfigParm) {
    int targetSlot = static_cast<int>(pos); // POS2(=1) -> slot 1 (0-based)
    int slot = TrackFX_AddByName(tr, strip.getFxIdent().toRawUTF8(), false,
                                 -(1000 + (chainLen < 0 ? 0 : chainLen)));
    if (slot >= 0) {
      GUID *g = TrackFX_GetFXGUID(tr, slot);
      if (g) {
        String guid = GUID2String(g);
        int r = tryMoveToUiSlot(tr, slot, targetSlot);
        if (r == 1) {
          int moved = findSlotByGUID(tr, guid);
          if (moved >= 0) {
            GUID *tg = GetTrackGUID(tr);
            if (tg) {
              m_slotCache[std::make_pair(GUID2String(tg), stripIndex)] =
                  CacheEntry{moved, guid};
            }
            return moved;
          }
        }
        // Target slot not honored: undo the add, fall back below.
        int stillThere = findSlotByGUID(tr, guid);
        MCU_LOG("CSA addPlugin SLOT-TARGET FAILED r=%d stillThere=%d", r,
                stillThere);
        if (stillThere >= 0)
          TrackFX_Delete(tr, stillThere);
      }
    }
  }

  // Classic dense insertion position (also the fallback for REAPER < 7.75).
  int instArg = instantiateArgFor(pos, chainLen);
  int slot =
      TrackFX_AddByName(tr, strip.getFxIdent().toRawUTF8(), false, instArg);
  MCU_LOG("CSA addPlugin ADD ident=[%s] instArg=%d -> slot=%d",
          strip.getFxIdent().toRawUTF8(), instArg, slot);
  if (slot < 0)
    return -1;
  GUID *g = TrackFX_GetFXGUID(tr, slot);
  GUID *tg = GetTrackGUID(tr);
  if (tg && g) {
    m_slotCache[std::make_pair(GUID2String(tg), stripIndex)] =
        CacheEntry{slot, GUID2String(g)};
  }
  return slot;
}

void ChannelStripAccess::plugMoved(MediaTrack *pOldTrack, int oldSlot,
                                   MediaTrack *pNewTrack, int newSlot) {
  // A reorder invalidates the cached slot positions for the affected track(s).
  // The next resolveSlot() will re-resolve by GUID (cheap) or by ident.
  invalidateTrack(pOldTrack);
  if (pNewTrack && pNewTrack != pOldTrack)
    invalidateTrack(pNewTrack);
}
