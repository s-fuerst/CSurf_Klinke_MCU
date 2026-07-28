/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripAccess.h"
#include "ChannelStripMode.h"
#include "csurf.h" // TrackFX_* + EnumInstalledFX/TrackFX_AddByName (vendored)
#include "csurf_mcu.h" // GUID2String
#include "Tracks.h"

// VPOT step size in normalized space (one detent = 1% of the parameter range)
#define CSA_VPOT_STEP (1.0 / 100.0)

namespace {
// Clamp a normalized 0..1 value.
inline double clampN(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
} // namespace

ChannelStripAccess::ChannelStripAccess(ChannelStripMode *pMode)
    : m_pMode(pMode), m_pTrack(NULL) {}

ChannelStripAccess::~ChannelStripAccess() {}

void ChannelStripAccess::trackChanged(MediaTrack *pTrack) { m_pTrack = pTrack; }

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
  String want = normalizeName(fxIdent);
  if (want.isEmpty())
    return -1;
  int n = TrackFX_GetCount(tr);
  char buf[256];
  for (int slot = 0; slot < n; slot++) {
    if (TrackFX_GetFXName(tr, slot, buf, 255)) {
      if (normalizeName(String(buf)) == want)
        return slot;
    }
  }
  return -1;
}

int ChannelStripAccess::resolveBinding(MediaTrack *tr,
                                        ChannelStripMap &strip) {
  if (!tr || !strip.isAssigned())
    return -1;
  // Prefer the instance GUID (survives reordering, disambiguates duplicates).
  if (strip.isResolved()) {
    int slot = findSlotByGUID(tr, strip.getFxGUID());
    if (slot >= 0)
      return slot;
    // GUID stale (plugin removed/moved): fall through to name match.
  }
  int slot = findSlotByIdent(tr, strip.getFxIdent());
  if (slot >= 0) {
    GUID *g = TrackFX_GetFXGUID(tr, slot);
    if (g)
      strip.setFxGUID(GUID2String(g));
    return slot;
  }
  // Not on the track: clear any stale GUID so the strip reads as "+"/dangling.
  strip.setFxGUID(String());
  return -1;
}

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

int ChannelStripAccess::instantiateArgFor(ChannelStripMap::InsertPos pos,
                                          int chainLen) {
  using IP = ChannelStripMap::InsertPos;
  // TrackFX_AddByName: instantiate <= -1000 -> -(1000) is first, -(1001) second
  if (pos == IP::LAST)
    return -(1000 + (chainLen < 0 ? 0 : chainLen)); // append after current end
  int chainPos = (pos == IP::FIRST) ? 1 : (static_cast<int>(pos) + 1);
  return -(999 + chainPos);
}

int ChannelStripAccess::addPlugin(ChannelStripMap &strip) {
  if (!m_pTrack || !strip.isAssigned() || !TrackFX_AddByName)
    return -1;
  // Reuse an existing instance of the same plugin if one is present (notes.org:
  // reusing the same plugin does NOT add a second instance).
  int existing = findSlotByIdent(m_pTrack, strip.getFxIdent());
  if (existing >= 0) {
    GUID *g = TrackFX_GetFXGUID(m_pTrack, existing);
    if (g)
      strip.setFxGUID(GUID2String(g));
    return existing;
  }
  int chainLen = TrackFX_GetCount(m_pTrack);
  int instArg = instantiateArgFor(strip.getInsertPos(), chainLen);
  int slot =
      TrackFX_AddByName(m_pTrack, strip.getFxIdent().toRawUTF8(), false, instArg);
  if (slot < 0)
    return -1;
  GUID *g = TrackFX_GetFXGUID(m_pTrack, slot);
  if (g)
    strip.setFxGUID(GUID2String(g));
  return slot;
}
