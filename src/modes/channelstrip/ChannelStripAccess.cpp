/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripAccess.h"
#include "ChannelStripMode.h"
#include "csurf.h" // TrackFX_* + EnumInstalledFX/TrackFX_AddByName (vendored)
#include "csurf_mcu.h" // GUID2String
#include "Tracks.h"
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

String ChannelStripAccess::normalizeName(const String &nameOrIdent) {
  String s = nameOrIdent.trim();
  // Strip a leading "TYPE:" prefix (e.g. "VST3:", "JS:", "AU:").
  int colon = s.indexOfChar(':');
  if (colon >= 0)
    s = s.substring(colon + 1);
  return s.trim();
}

int ChannelStripAccess::findSlotByGUID(MediaTrack *tr, const String &guid) {
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
  if (want.isEmpty())
    return -1;
  int n = TrackFX_GetCount(tr);
  char buf[256];
  for (int slot = 0; slot < n; slot++) {
    if (TrackFX_GetFXName(tr, slot, buf, 255)) {
      String norm = normalizeName(String(buf));
      MCU_LOG("CSA  pass2 slot=%d fxname=[%s] norm=[%s] want=[%s]", slot, buf,
              norm.toRawUTF8(), want.toRawUTF8());
      if (norm == want)
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

void ChannelStripAccess::toggleParam(MediaTrack *tr, int slot, int param) {
  if (!tr || slot < 0 || param < 0)
    return;
  // notes.org: set to 1 if current != 1, else 0. Compare in normalized space.
  double v = getParamValue(tr, slot, param);
  setParamValue(tr, slot, param, (v < 1.0) ? 1.0 : 0.0);
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
  int instArg = instantiateArgFor(strip.getInsertPos(), chainLen);
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
