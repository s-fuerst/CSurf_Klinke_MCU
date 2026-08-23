/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

//#include "SnM/stdafx.h"
#include "boost/foreach.hpp"
#include "PlugAccess.h"
#include "PlugMode.h"
#include "PlugMap.h"
#include "PluginWatcher.h"
#include "PlugModeComponent.h"
#include "csurf_mcu.h"
#include "boost/bind.hpp"
#include "PlugMapManager.h"
#include "PlugWindowManager.h"
#include "std_helper.h"
#include "Tracks.h"
#include "McuDebugLog.h"
#include <algorithm>
#include <memory>
//#include "SnM/SnM_Actions.h"

//#define TRACK_CHANGE_TRACK_UNKNOWN -1
//#define TRACK_CHANGE_TRACK_NOT_CHANGED -2

PlugAccess::PlugAccess(PlugMode *pMode)
    : m_pMode(pMode), m_pPlugTrack(NULL), m_iSlot(-1),
      m_pMapManager(NULL), m_plugName(String()),
      m_GUIDplugTrack(GUID_NOT_ACTIVE) {
  // init per-unit state to bank 0 / page 0 for all units
  m_selectedBankPerUnit.fill(0);
  for (int u = 0; u < MAX_SURFACE_UNITS; u++)
    m_selectedPagePerUnit[u].fill(0);
  m_unitEmpty.fill(false);

  m_pMapManager = new PlugMapManager(pMode);
  m_pWindowManager = new PlugWindowManager(pMode);

  Tracks::instance()->connect2TrackRemovedSignal(
      boost::bind(&PlugAccess::trackRemoved, this, _1));

  m_pPlugWatcher = new PluginWatcher(pMode->getCCSManager()->getMCU());
  m_nameChangedConnectionId = m_pPlugWatcher->connect2NameChanged(
      boost::bind(&PlugAccess::watchedNameParameterChanged, this, _1, _2, _3));

  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&PlugAccess::projectChanged, this, _1, _2));
}

PlugAccess::~PlugAccess(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);

  m_pPlugWatcher->disconnectNameChange(m_nameChangedConnectionId);
  safe_delete(m_pPlugWatcher);
  safe_delete(m_pMapManager);
  safe_delete(m_pWindowManager);
}

void PlugAccess::trackChanged(MediaTrack *pMediaTrack) {
  if (pMediaTrack == m_pPlugTrack)
    return;

  m_track2Slot.erase(reinterpret_cast<std::uintptr_t>(m_pPlugTrack));
  m_track2Slot.insert(
      std::pair<std::uintptr_t, int>(reinterpret_cast<std::uintptr_t>(m_pPlugTrack), m_iSlot));

  if (!pMediaTrack) {
    accessPlugin(NULL, -1);
    return;
  }

  tTrack2Plug::iterator iterT2P = m_track2Slot.find(reinterpret_cast<std::uintptr_t>(pMediaTrack));
  if (iterT2P != m_track2Slot.end()) {
    accessPlugin(pMediaTrack, (*iterT2P).second);
  } else {
    // No prior mapping: prefer whichever slot has a floating window open,
    // otherwise default to slot 0.  This makes "open a plugin then switch
    // to PlugMode" Just Work without requiring the user to manually select
    // a slot via the MCU SELECT buttons.
    int numFX = TrackFX_GetCount(pMediaTrack);
    if (numFX > 0) {
      int slotToUse = 0;
      for (int i = 0; i < numFX; i++) {
        if (TrackFX_GetFloatingWindow(pMediaTrack, i)) {
          slotToUse = i;
          break;
        }
      }
      accessPlugin(pMediaTrack, slotToUse);
    } else {
      accessPlugin(NULL, -1);
    }
  }
}

void PlugAccess::accessPlugin(MediaTrack *pMediaTrack, int iSlot,
                              bool changeTriggeredFromGUI,
                              bool changeTriggeredFromProjectChange) {
  if (changeTriggeredFromGUI) {
    if (pMediaTrack == m_pPlugTrack && iSlot == m_iSlot)
      return;
  }

  storeActualSlotState();

  m_pPlugTrack = pMediaTrack;
  if (pMediaTrack != NULL) {
    m_GUIDplugTrack = *(CSurf_MCU::GUIDfromTrack(m_pPlugTrack));
  } else {
    m_GUIDplugTrack = GUID_NOT_ACTIVE;
  }
  m_iSlot = iSlot;

  // reset ALL units to bank 0 / page 0
  m_selectedBankPerUnit.fill(0);
  for (int u = 0; u < MAX_SURFACE_UNITS; u++)
    m_selectedPagePerUnit[u].fill(0);
  m_unitEmpty.fill(false);

  if (changeTriggeredFromProjectChange || !plugExist()) {
    m_pMapManager->deselectMap();
    m_pMode->updateEverything();
    m_pPlugWatcher->setPlugin(NULL, -1);
    m_iSlot = -1;
    return;
  }

  m_plugName = getPlugNameLong();
  m_pMapManager->loadMapForPlug(m_pPlugTrack, m_iSlot);

  // restore stored per-unit state (or default page-spread for N>1)
  tSlotStatesMap::iterator iterStoredStates = m_knownSlotStates.find(
      tSlotLocation(GUID2String(CSurf_MCU::GUIDfromTrack(pMediaTrack)), iSlot));
  bool restoredFromStored = false;
  if (iterStoredStates != m_knownSlotStates.end()) {
    tSlotState storedState = (*iterStoredStates).second;
    String storedPlugName = storedState.get<0>();
    if (storedPlugName.equalsIgnoreCase(getPlugNameLong())) {
      // restore per-unit arrays
      m_selectedBankPerUnit = storedState.get<1>();
      m_selectedPagePerUnit = storedState.get<2>();
      m_unitEmpty = storedState.get<3>();
      restoredFromStored = true;
    }
  }

  if (!restoredFromStored) {
    // Default page spread across used pages. Unit u gets the page at
    // used-page offset u; units beyond the number of used pages stay empty
    // (no page) so no two units ever display the same page. All units on
    // bank 0. The used-page sequence lives on the RESOLVED bank (bank 0
    // may `refer` to another bank), mirroring the transport/cascade math.
    int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
    int resolved0 = resolveBankReference(0);
    int count = usedPageCount(resolved0);
    for (int u = 0; u < nUnits; u++) {
      m_selectedBankPerUnit[u] = 0;
      if (u < count)
        m_selectedPagePerUnit[u][0] = pageAtUsedOffset(resolved0, u);
      else
        m_unitEmpty[u] = true;
    }
  } else {
    // Legacy/foreign states may contain duplicate (bank, page) assignments;
    // normalize them so every page is shown at most once.
    enforceUniquePages(-1);
  }

  m_pPlugWatcher->setPlugin(pMediaTrack, iSlot);
  m_pMode->plugChanged();

  if (!changeTriggeredFromGUI)
    m_pWindowManager->switchedTo(pMediaTrack, iSlot);
  else if (m_pMode->isFollowTrack()) {
    m_pMode->getCCSManager()->getMCU()->UnselectAllTracks();
    CSurf_OnSelectedChange(pMediaTrack, 1);
  }

  m_pMode->updateEverything();
}

String PlugAccess::getPlugName(bool shortName, MediaTrack *pMediaTrack,
                               int slot) {
  if (!plugExist()) {
    return String();
  }

  char paramName[80] = {};
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  if (shortName) {
    return m_pMode->shortPlugName(paramName);
  }

  return m_pMode->longPlugName(paramName);
}

String PlugAccess::getFullPlugName(MediaTrack *pMediaTrack, int slot) {
  if (!plugExist()) {
    return String();
  }

  char paramName[80] = {};
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  return String(paramName);
}

String PlugAccess::formattedValueName(MediaTrack *pTrack, int slot,
                                     int paramId, double normalizedValue) {
  if (pTrack == NULL || TrackFX_FormatParamValueNormalized == NULL)
    return String();
  char valueString[80] = {};
  if (TrackFX_FormatParamValueNormalized(pTrack, slot, paramId,
                                         normalizedValue, valueString, 79) &&
      valueString[0] != 0)
    return String(valueString);
  return String();
}

int PlugAccess::discreteStepSegments(MediaTrack *pTrack, int slot,
                                     int paramId, int maxSegments) {
  if (pTrack == NULL || TrackFX_GetParameterStepSizes == NULL)
    return 0;

  double step = 0.0, smallStep = 0.0, largeStep = 0.0;
  bool isToggle = false;
  if (!TrackFX_GetParameterStepSizes(pTrack, slot, paramId, &step, &smallStep,
                                     &largeStep, &isToggle))
    return 0;

  int segments = 0;
  if (step > 0.0) {
    // REAPER does not document whether step is normalized or in the raw
    // range of the parameter. Use the reading that divides the respective
    // range into an integral number of segments.
    double minVal = 0.0, maxVal = 1.0;
    TrackFX_GetParam(pTrack, slot, paramId, &minVal, &maxVal);

    const double segmentsNormalized = 1.0 / step;
    const double devNormalized =
        fabs(segmentsNormalized - floor(segmentsNormalized + 0.5));
    double segments = segmentsNormalized;
    if (maxVal > minVal) {
      const double segmentsRaw = (maxVal - minVal) / step;
      const double devRaw = fabs(segmentsRaw - floor(segmentsRaw + 0.5));
      if (devRaw + 0.000001 < devNormalized)
        segments = segmentsRaw;
    }
    segments = (int)floor(segments + 0.5);
  }

  // Toggles are not handled here: a toggle always has exactly two values
  // and no usable step grid. fillStepsFromStepGrid treats the toggle flag
  // separately, before it calls this function.
  if (segments < 2 || segments > maxSegments)
    return 0;
  return segments;
}

int PlugAccess::fillStepsFromStepGrid(MediaTrack *pTrack, int slot,
                                      int paramId, PMVPot::tSteps *pSteps) {
  if (pTrack == NULL || pSteps == NULL ||
      TrackFX_GetParameterStepSizes == NULL)
    return 0;

  // Query the step information directly (discreteStepSegments uses the
  // same call, but the toggle flag must be known here as well): a toggle
  // has exactly two real values, the endpoints of the parameter range.
  // Its reported step size must never be read as a grid, see below.
  double step = 0.0, smallStep = 0.0, largeStep = 0.0;
  bool isToggle = false;
  if (!TrackFX_GetParameterStepSizes(pTrack, slot, paramId, &step, &smallStep,
                                     &largeStep, &isToggle))
    return 0;

  double minVal = 0.0, maxVal = 1.0;
  TrackFX_GetParam(pTrack, slot, paramId, &minVal, &maxVal);
  const double range = maxVal - minVal;
  if (range <= 0.0)
    return 0;

  MCU_LOG("PM stepgrid: param %d step=%g small=%g large=%g toggle=%d "
          "min=%g max=%g",
          paramId, step, smallStep, largeStep, isToggle ? 1 : 0, minVal,
          maxVal);

  // The step map is keyed by raw parameter values (see PlugMode vpotMoved
  // and getParamValueShort), so every grid position is converted first.
  int added = 0;

  if (isToggle) {
    // A toggle has exactly two values: the endpoints of the range.
    // REAPER also reports step sizes for many toggles (e.g. 0.5
    // normalized) or none at all; reading such a step as a grid would
    // divide the range into segments and produce intermediate positions
    // (0.0/0.5/1.0) that all display one of the two real names
    // ("0.0: Off, 0.5: Off, 1.0: On"). Only the endpoints are used.
    for (int i = 0; i <= 1; i++) {
      const String name =
          formattedValueName(pTrack, slot, paramId, (double)i);
      if (name.isEmpty())
        continue; // no real value name -> no entry
      (*pSteps)[(i == 0) ? minVal : maxVal] = PMVPot::tStepsValue(
          shortNameFromCString(name.toRawUTF8()),
          longNameFromCString(name.toRawUTF8()));
      added++;
    }
    return added;
  }

  const int segments = discreteStepSegments(pTrack, slot, paramId);
  if (segments < 2)
    return 0;
  MCU_LOG("PM stepgrid: param %d uses step grid with %d segments", paramId,
          segments);

  for (int i = 0; i <= segments; i++) {
    const double norm = (double)i / (double)segments;
    const String name = formattedValueName(pTrack, slot, paramId, norm);
    if (name.isEmpty())
      continue; // no real value name -> no entry
    (*pSteps)[minVal + norm * range] = PMVPot::tStepsValue(
        shortNameFromCString(name.toRawUTF8()),
        longNameFromCString(name.toRawUTF8()));
    added++;
  }
  return added;
}

int PlugAccess::fillStepsByValueNameScan(MediaTrack *pTrack, int slot,
                                         int paramId, PMVPot::tSteps *pSteps) {
  if (pTrack == NULL || pSteps == NULL)
    return 0;

  // without value names the heuristic cannot work
  const String nameAtZero = formattedValueName(pTrack, slot, paramId, 0.0);
  if (nameAtZero.isEmpty())
    return 0;

  // if the names of 0.00 and 0.01 differ, the display changes with every
  // 1% step and the parameter is most likely continuous
  const String nameAtOnePercent =
      formattedValueName(pTrack, slot, paramId, 0.01);
  if (!nameAtOnePercent.isEmpty() && nameAtOnePercent != nameAtZero)
    return 0;

  // collect the distinct display names in the order they appear on a scan
  // from 0.00 to 1.00 in 0.01 steps
  std::vector<String> names;
  String lastName;
  for (int i = 0; i <= 100; i++) {
    const String name = formattedValueName(pTrack, slot, paramId, i / 100.0);
    if (name.isEmpty() || name == lastName)
      continue;
    lastName = name;
    names.push_back(name);
  }

  const int numValues = (int)names.size();
  if (numValues < 2 || numValues > 100)
    return 0;

  MCU_LOG("PM scan: param %d distinct value names=%d", paramId, numValues);

  // The scan finds the names at 0.01-quantized positions that do not
  // necessarily match the real values of the parameter: a binary parameter
  // for example reports its second name first at 0.51, not at 1.00.
  // Distribute the collected values evenly over the parameter range
  // instead, with the first value at the minimum and the last at the
  // maximum (2 values -> 0.0 and 1.0, 3 values -> 0.0/0.5/1.0, ...).
  double minVal = 0.0, maxVal = 1.0;
  TrackFX_GetParam(pTrack, slot, paramId, &minVal, &maxVal);
  if (maxVal <= minVal)
    return 0;
  const double range = maxVal - minVal;

  // Verify that the parameter really displays the collected names at the
  // evenly distributed positions. If it does not, the values are not evenly
  // distributed and the parameter is not treated as discrete at all.
  for (int i = 0; i < numValues; i++) {
    const double norm = (double)i / (double)(numValues - 1);
    if (formattedValueName(pTrack, slot, paramId, norm) != names[i])
      return 0;
  }

  for (int i = 0; i < numValues; i++) {
    const double norm = (double)i / (double)(numValues - 1);
    // The step map is keyed by raw parameter values (see PlugMode vpotMoved
    // and getParamValueShort), so convert the position first.
    (*pSteps)[minVal + norm * range] = PMVPot::tStepsValue(
        shortNameFromCString(names[i].toRawUTF8()),
        longNameFromCString(names[i].toRawUTF8()));
  }
  return numValues;
}

// A table with exactly three entries whose middle entry carries the
// same display name as the first or the last entry describes a parameter
// with only two real values: the generated grid contains an intermediate
// position (typically 0.5 for an On/Off parameter) that the FX displays
// with the name of one of its real values ("0.0: Off, 0.5: Off, 1.0: On").
// Such a middle entry carries no information and would make the V-Pot
// step through a state that does not exist, so it is dropped again.
static void dropRedundantMiddleStep(int paramId, PMVPot::tSteps *pSteps) {
  if (pSteps == NULL || pSteps->size() != 3)
    return;

  PMVPot::tSteps::iterator iterFirst = pSteps->begin();
  PMVPot::tSteps::iterator iterMiddle = iterFirst;
  ++iterMiddle;
  PMVPot::tSteps::iterator iterLast = iterMiddle;
  ++iterLast;

  const double rawMiddle = iterMiddle->first;
  const String nameFirst = iterFirst->second.get<1>();
  const String nameMiddle = iterMiddle->second.get<1>();
  const String nameLast = iterLast->second.get<1>();
  if (nameMiddle != nameFirst && nameMiddle != nameLast)
    return; // the middle entry shows a name of its own -> a real value

  MCU_LOG("PM steps: param %d dropped redundant middle entry raw=%g "
          "name='%s' (first='%s', last='%s')",
          paramId, rawMiddle, nameMiddle.toRawUTF8(), nameFirst.toRawUTF8(),
          nameLast.toRawUTF8());
  pSteps->erase(iterMiddle);
}

int PlugAccess::fillDiscreteSteps(MediaTrack *pTrack, int slot, int paramId,
                                  PMVPot::tSteps *pSteps) {
  if (pTrack == NULL || pSteps == NULL)
    return 0;

  // (1) the FX reports a step size for the parameter: use the exact grid
  int added = fillStepsFromStepGrid(pTrack, slot, paramId, pSteps);

  // (2) heuristic over the displayed value names, with verification
  if (added == 0)
    added = fillStepsByValueNameScan(pTrack, slot, paramId, pSteps);

  if (added > 0) {
    // A two-value parameter must not keep a redundant middle entry that
    // displays the same name as one of its neighbours.
    dropRedundantMiddleStep(paramId, pSteps);
    disambiguateStepShortNames(pSteps);
  }
  MCU_LOG("PM fillDiscreteSteps: param %d entries=%d", paramId,
          (int)pSteps->size());
  return (int)pSteps->size();
}

// Short names hold up to 6 characters. The QCon Pro X displays fader short
// names with only 5 of those characters, so when a QCon Pro X unit is part
// of the surface, the fader short names are reduced to 5 characters and
// must be distinguishable within those 5. V-Pot names always use 6.
static const int kShortNameWidth = 6;
static const int kFaderDisplayChars = 5;

void PlugAccess::disambiguateStepShortNames(PMVPot::tSteps *pSteps) {
  if (pSteps == NULL || pSteps->empty())
    return;

  std::vector<String> longNames;
  for (PMVPot::tSteps::iterator iterStep = pSteps->begin();
       iterStep != pSteps->end(); ++iterStep)
    longNames.push_back(iterStep->second.get<1>());

  const std::map<String, String> shorts =
      disambiguateShortNames(longNames, kShortNameWidth);
  for (PMVPot::tSteps::iterator iterStep = pSteps->begin();
       iterStep != pSteps->end(); ++iterStep) {
    std::map<String, String>::const_iterator iterShort =
        shorts.find(iterStep->second.get<1>());
    if (iterShort != shorts.end())
      iterStep->second.get<0>() = iterShort->second;
  }
}

static String shortDisplayKey(const String &s, int width) {
  return s.substring(0, width).trimEnd().toUpperCase();
}

static bool isVowel(juce_wchar c) {
  return c == 'a' || c == 'A' || c == 'e' || c == 'E' || c == 'i' ||
         c == 'I' || c == 'o' || c == 'O' || c == 'u' || c == 'U';
}

// Vowel-reduced skeleton of a word; the first character is always kept
// ("Drive" -> "Drv", "Level" -> "Lvl", "Amount" -> "Amnt").
static String wordSkeleton(const String &word) {
  String skeleton;
  for (int i = 0; i < word.length(); i++) {
    if (i > 0 && isVowel(word[i]))
      continue;
    skeleton += word[i];
  }
  return skeleton;
}

static void splitWords(const String &name, std::vector<String> &words) {
  int start = 0;
  for (int i = 0; i <= name.length(); i++) {
    if (i == name.length() || name[i] == ' ') {
      if (i > start)
        words.push_back(name.substring(start, i));
      start = i + 1;
    }
  }
}

// Splits a trailing number off the name ("Band Transient 12" -> the words
// of "Band Transient" and the number "12"). A name that ends with a number
// must keep that number complete in its abbreviation ("BndT12", never
// "BndT1"), otherwise numbered parameters would collide on the display.
static String splitTrailingNumber(const String &name,
                                  std::vector<String> &words) {
  int end = name.length();
  while (end > 0 && name[end - 1] >= '0' && name[end - 1] <= '9')
    end--;
  const String number = name.substring(end);
  splitWords(name.substring(0, end).trimEnd(), words);
  return number;
}

// Distributes a character budget over the words of a name (each word gets
// at least one character). Enumerates front-loaded allocations first.
static void appendAllocations(const std::vector<String> &words, int idx,
                              int remaining, std::vector<int> &current,
                              std::vector<std::vector<int>> &out) {
  if (idx == (int)words.size()) {
    out.push_back(current);
    return;
  }
  const int maxChars = words[idx].length();
  const int wordsAfter = (int)words.size() - idx - 1;
  for (int t = maxChars; t >= 1; t--) {
    if (t > remaining - wordsAfter)
      continue; // leave at least one character for every remaining word
    current[idx] = t;
    appendAllocations(words, idx + 1, remaining - t, current, out);
  }
}

// Preferred allocations: use the whole budget, keep the words balanced,
// then prefer more characters for the earlier words.
static bool allocationOrder(const std::vector<int> &a,
                            const std::vector<int> &b) {
  int sumA = 0, sumB = 0, minA = a[0], minB = b[0];
  for (unsigned int i = 0; i < a.size(); i++) {
    sumA += a[i];
    sumB += b[i];
    if (a[i] < minA)
      minA = a[i];
    if (b[i] < minB)
      minB = b[i];
  }
  if (sumA != sumB)
    return sumA > sumB;
  if (minA != minB)
    return minA > minB;
  for (unsigned int i = 0; i < a.size(); i++)
    if (a[i] != b[i])
      return a[i] > b[i];
  return false;
}

// Candidate abbreviations for one long name, most readable first.
// A name that ends with a number keeps that number complete at the end of
// every candidate; the words before it share the remaining characters.
// Multi-word names are contracted word by word with the vowels removed
// ("Drive Level" -> "DrvLvl"), followed by plain word truncation
// ("DriLev") as second choice and the naive truncation as last resort.
static void buildAbbreviationCandidates(const String &longName, int width,
                                        std::vector<String> &candidates) {
  std::vector<String> words;
  const String number = splitTrailingNumber(longName, words);
  if (words.size() > (unsigned)width)
    words.resize(width);

  const String naive = longName.substring(0, width);

  // If the trailing number does not fit or leaves no character for every
  // word, it cannot be kept; the naive truncation is the only option.
  if (number.length() >= width || (int)words.size() > width - number.length()) {
    candidates.push_back(naive);
    return;
  }
  const int budget = width - number.length();

  if (words.size() <= 1) {
    if (words.size() == 1) {
      // keep the plain prefix intact first ("Cutoff 2" -> "Cuto2")
      candidates.push_back(words[0].substring(0, budget) + number);
      candidates.push_back(
          wordSkeleton(words[0]).substring(0, budget) + number);
    }
    candidates.push_back(naive);
    return;
  }

  std::vector<std::vector<int>> allocations;
  std::vector<int> current(words.size(), 1);
  appendAllocations(words, 0, budget, current, allocations);
  std::sort(allocations.begin(), allocations.end(), allocationOrder);
  if (allocations.size() > 16)
    allocations.resize(16);

  for (int skeleton = 1; skeleton >= 0; skeleton--) {
    for (unsigned int a = 0; a < allocations.size(); a++) {
      String abbr;
      for (unsigned int w = 0; w < words.size(); w++) {
        const String base = skeleton ? wordSkeleton(words[w]) : words[w];
        abbr += base.substring(0, allocations[a][w]);
      }
      abbr = (abbr + number).substring(0, width);
      if (abbr.isNotEmpty() &&
          std::find(candidates.begin(), candidates.end(), abbr) ==
              candidates.end())
        candidates.push_back(abbr);
    }
  }
  candidates.push_back(naive);
}

std::map<String, String>
PlugAccess::disambiguateShortNames(const std::vector<String> &longNames,
                                   int width) {
  // Names are compared like the display shows them: only the first `width`
  // characters count, case-insensitively and ignoring trailing spaces.

  std::map<String, String> result;                 // long name -> short name
  std::map<String, std::vector<String>> collisionGroups; // display key of
                                                    // the truncated short
                                                    // -> long names
  for (unsigned int i = 0; i < longNames.size(); i++) {
    const String &longName = longNames[i];
    if (longName.isEmpty())
      continue;
    result[longName] = longName.substring(0, width);
    collisionGroups[shortDisplayKey(longName.substring(0, width), width)]
        .push_back(longName);
  }

  for (std::map<String, std::vector<String>>::iterator iterGroup =
           collisionGroups.begin();
       iterGroup != collisionGroups.end(); ++iterGroup) {
    std::vector<String> &group = iterGroup->second;
    if (group.size() < 2)
      continue;

    // The longest names are the most constrained, so they choose first.
    std::sort(group.begin(), group.end(), [](const String &a, const String &b) {
      if (a.length() != b.length())
        return a.length() > b.length();
      return a < b;
    });

    std::set<String> usedKeys; // display keys already taken in this group
    for (unsigned int i = 0; i < group.size(); i++) {
      std::vector<String> candidates;
      buildAbbreviationCandidates(group[i], width, candidates);

      bool assigned = false;
      for (unsigned int c = 0; c < candidates.size(); c++) {
        const String key = shortDisplayKey(candidates[c], width);
        if (key.isEmpty() || usedKeys.find(key) != usedKeys.end())
          continue;
        result[group[i]] = candidates[c];
        usedKeys.insert(key);
        assigned = true;
        break;
      }
      if (!assigned)
        usedKeys.insert(shortDisplayKey(result[group[i]], width));
    }
  }

  // The abbreviations of one group could collide with short names from
  // other groups; revert those to the naive truncation, which is never
  // worse than the previous behaviour.
  std::map<String, int> shortUseCount; // display key -> number of uses
  for (std::map<String, String>::iterator iter = result.begin();
       iter != result.end(); ++iter)
    shortUseCount[shortDisplayKey(iter->second, width)]++;
  for (std::map<String, String>::iterator iter = result.begin();
       iter != result.end(); ++iter) {
    if (shortUseCount[shortDisplayKey(iter->second, width)] > 1) {
      const String naive = iter->first.substring(0, width);
      if (iter->second != naive)
        iter->second = naive;
    }
  }

  return result;
}

// Applies the disambiguated short names of one element group (faders or
// V-Pots) to the parameters. Manually edited or already abbreviated short
// names are not rewritten, but their names are reserved against the new
// abbreviations. Returns true if a short name was changed.
bool PlugAccess::applyDisambiguatedShortNames(std::vector<PMParam *> &params,
                                              std::vector<String> &longNames,
                                              int width) {
  if (params.empty())
    return false;

  const std::map<String, String> shorts =
      PlugAccess::disambiguateShortNames(longNames, width);

  std::set<String> used; // display keys of short names that are taken
  std::vector<bool> frozen(params.size(), false);
  for (unsigned int i = 0; i < params.size(); i++) {
    // A short name that equals the plain truncation (in either of the two
    // widths) has not been edited manually and may be rewritten.
    if (params[i]->getNameShort() != longNames[i].substring(0, width) &&
        params[i]->getNameShort() !=
            longNames[i].substring(0, kShortNameWidth)) {
      frozen[i] = true;
      used.insert(shortDisplayKey(params[i]->getNameShort(), width));
    }
  }

  bool changed = false;
  for (unsigned int i = 0; i < params.size(); i++) {
    if (frozen[i])
      continue;

    std::map<String, String>::const_iterator iterShort =
        shorts.find(longNames[i]);
    String candidate =
        (iterShort != shorts.end()) ? iterShort->second
                                    : longNames[i].substring(0, width);
    candidate = candidate.trimEnd();
    if (used.find(shortDisplayKey(candidate, width)) != used.end()) {
      const String naive = longNames[i].substring(0, width);
      if (used.find(shortDisplayKey(naive, width)) != used.end())
        continue; // collision cannot be avoided, keep the current name
      candidate = naive;
    }
    if (candidate != params[i]->getNameShort()) {
      params[i]->setNameShort(candidate);
      changed = true;
    }
    used.insert(shortDisplayKey(candidate, width));
  }
  return changed;
}

bool PlugAccess::disambiguateShortNamesInMap() {
  // The QCon Pro X shows fader short names with five characters only. If
  // any unit of this surface is a QCon Pro X, the fader names are therefore
  // reduced to five characters; otherwise they use the full six. V-Pot
  // names always use six characters.
  int faderWidth = kShortNameWidth;
  if (m_pMode && m_pMode->getCCSManager() &&
      m_pMode->getCCSManager()->getMCU()) {
    CSurf_MCU *pMCU = m_pMode->getCCSManager()->getMCU();
    for (int u = 0; u < pMCU->numUnits(); u++) {
      HardwareUnit *pUnit = pMCU->unitForChannel(u * 8 + 1);
      if (pUnit && pUnit->isProX()) {
        faderWidth = kFaderDisplayChars;
        break;
      }
    }
  }

  // Faders and V-Pots never share a display spot, so they are
  // disambiguated separately with their respective display width.
  std::vector<PMParam *> faderParams;
  std::vector<String> faderLongNames;
  std::vector<PMParam *> vpotParams;
  std::vector<String> vpotLongNames;
  for (int bank = 0; bank < 8; bank++) {
    for (int page = 0; page < 8; page++) {
      for (int channel = 0; channel < 8; channel++) {
        PMParam *pFader =
            getMap()->getBank(bank)->getPage(page)->getFader(channel);
        PMParam *pVPot =
            getMap()->getBank(bank)->getPage(page)->getVPot(channel);
        if (pFader->getParamID() != NOT_ASSIGNED &&
            !pFader->getNameLong().isEmpty()) {
          faderParams.push_back(pFader);
          faderLongNames.push_back(pFader->getNameLong());
        }
        if (pVPot->getParamID() != NOT_ASSIGNED &&
            !pVPot->getNameLong().isEmpty()) {
          vpotParams.push_back(pVPot);
          vpotLongNames.push_back(pVPot->getNameLong());
        }
      }
    }
  }

  const bool faderChanged =
      applyDisambiguatedShortNames(faderParams, faderLongNames, faderWidth);
  const bool vpotChanged = applyDisambiguatedShortNames(
      vpotParams, vpotLongNames, kShortNameWidth);
  return faderChanged || vpotChanged;
}

void PlugAccess::createDefaultMap() {
  const int numParamsExist = getNumParams();
  int numParamsMapped = 0;

  char paramName[80] = {};

  for (int bank = 0; bank < 8 && numParamsMapped < numParamsExist; bank++) {
    getMap()
        ->getBank(bank)
        ->setNameLong(String::formatted(String("Bank %d"), bank + 1));
    getMap()
        ->getBank(bank)
        ->setNameShort(String::formatted(String("Bank %d"), bank + 1));
    for (int page = 0; page < 8 && numParamsMapped < numParamsExist; page++) {
      getMap()
          ->getBank(bank)
          ->getPage(page)
          ->setNameLong(String::formatted(String("Page %d"), page + 1));
      getMap()
          ->getBank(bank)
          ->getPage(page)
          ->setNameShort(String::formatted(String("Page %d"), page + 1));
      for (int channel = 0; channel < 8 && numParamsMapped < numParamsExist;
           channel++) {
        // Every channel strip offers two slots, one continuous parameter on
        // the fader and one discrete parameter on the V-Pot. Parameters that
        // belong together are assumed to be adjacent in the parameter list
        // of the FX (e.g. LFO waveform followed by LFO rate), so up to two
        // adjacent parameters share the same channel strip.
        bool faderUsed = false;
        bool vpotUsed = false;
        for (int slotIdx = 0; slotIdx < 2 && numParamsMapped < numParamsExist;
             slotIdx++) {
          // Discrete parameters go to the V-Pot, but only if their value
          // table could be filled: a V-Pot with an empty step map cannot
          // be controlled at all (PlugMode::vpotMoved does nothing in that
          // case), so such a parameter stays on the fader.
          ElementDesc::eType type = ElementDesc::FADER;
          PMVPot::tSteps steps;
          if (fillDiscreteSteps(m_pPlugTrack, m_iSlot, numParamsMapped,
                                &steps) > 0)
            type = ElementDesc::VPOT;

          if ((type == ElementDesc::VPOT && vpotUsed) ||
              (type == ElementDesc::FADER && faderUsed))
            break; // this slot of the strip is taken -> next channel strip

          ElementDesc desc(bank, page, type, channel);
          PMParam *pParam = getPMParam(&desc);
          if (!pParam)
            break;

          pParam->setParamID(numParamsMapped);
          bool valid = TrackFX_GetParamName(m_pPlugTrack, m_iSlot,
                                            numParamsMapped, paramName, 79);
          if (valid) {
            pParam->setNameShort(shortNameFromCString(paramName));
            pParam->setNameLong(longNameFromCString(paramName));
          }

          if (type == ElementDesc::VPOT) {
            *(dynamic_cast<PMVPot *>(pParam)->getStepsMap()) = steps;
            vpotUsed = true;
          } else {
            faderUsed = true;
          }
          numParamsMapped++;
        }
      }
    }
  }

  // Finally make the parameter short names unique across the whole map:
  // parameters that share a name prefix ("Drive Attack", "Drive Amount",
  // ...) would otherwise all be truncated to the same 6 characters.
  disambiguateShortNamesInMap();
}

String PlugAccess::shortNameFromCString(const char *pName) {
  String name = pName;
  return name.substring(0, 6);
}

String PlugAccess::longNameFromCString(const char *pName) {
  String name = pName;
  return name.substring(0, 17);
}

bool PlugAccess::plugExist() {
  return (m_pPlugTrack && TrackFX_GetCount(m_pPlugTrack) > m_iSlot);
}

/**
 * Return the number of params this plugin has.
 *
 * @param includeReaper If true then the returned value will include the params for bypass, dry/wet and delta solo. If
 *                      false then the returned value will only include the params implemented for the plugin itself.
 * @return int
 */
int PlugAccess::getNumParams(bool includeReaper) {
  if (!plugExist()) {
    return 0;
  }

  int numParams = TrackFX_GetNumParams(m_pPlugTrack, m_iSlot);
  if (includeReaper)
      return numParams;

  return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":bypass");
}

PMParam *PlugAccess::getPMParam(ElementDesc *pElement) {
  if (!pElement || !pElement->isValid() || !plugExist()) {
    return NULL;
  }

  // A page of -1 marks a unit that shows no page (empty unit); there is no
  // valid PMPage for it, so every param lookup must fail cleanly.
  if (pElement->m_page < 0) {
    return NULL;
  }

  if (!resolveIndirection(pElement)) {
    return NULL;
  }

  ASSERT(pElement->isValid());

  switch (pElement->m_type) {
  case ElementDesc::FADER:
    return getMap()
        ->getBank(pElement->m_bank)
        ->getPage(pElement->m_page)
        ->getFader(pElement->m_channel);
  case ElementDesc::VPOT:
    return getMap()
        ->getBank(pElement->m_bank)
        ->getPage(pElement->m_page)
        ->getVPot(pElement->m_channel);
  case ElementDesc::DRYWET:
  case ElementDesc::BYPASS:
  case ElementDesc::UNKNOWN:
  case ElementDesc::DELTA:
    break;
  }
  ASSERT_M(false, "type is unknown");
  return NULL;
}

bool PlugAccess::resolveIndirection(ElementDesc *pDesc) {
  if (!pDesc || !pDesc->isValid())
    return false;
  pDesc->m_offset = 0;

  PMBank *pBank = getMap()->getBank(pDesc->m_bank);
  if (pBank->doesRefer()) {
    pDesc->m_bank = pBank->referTo();
    pDesc->m_offset = pBank->getParamIDOffset();
  }

  PMPage *pPage = getMap()->getBank(pDesc->m_bank)->getPage(pDesc->m_page);
  if (pPage->doesRefer()) {
    pDesc->m_page = pPage->referTo();
    pDesc->m_offset += pPage->getParamIDOffset();
  }

  PMParam *pParam;
  switch (pDesc->m_type) {
  case ElementDesc::FADER:
    pParam = getMap()
                 ->getBank(pDesc->m_bank)
                 ->getPage(pDesc->m_page)
                 ->getFader(pDesc->m_channel);
    break;
  case ElementDesc::VPOT:
    pParam = getMap()
                 ->getBank(pDesc->m_bank)
                 ->getPage(pDesc->m_page)
                 ->getVPot(pDesc->m_channel);
    break;
  default:
    ASSERT_M(false, "type is unknown");
    return false;
  }

  if (pParam->getParamID() == NOT_ASSIGNED) {
    pDesc->m_offset = 0;
  }

  return true;
}

// ---- Explicit bank/page parameter overloads ----

String PlugAccess::getParamNameShort(int bank, int page,
                                     ElementDesc::eType type, int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getPMParam(&desc) ? getPMParam(&desc)->getNameShort() : String();
}

String PlugAccess::getParamNameLong(int bank, int page,
                                    ElementDesc::eType type, int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getPMParam(&desc) ? getPMParam(&desc)->getNameLong() : String();
}

int PlugAccess::getParamID(ElementDesc *pElement) {
  if (!pElement || !pElement->isValid() || !plugExist())
    return NOT_ASSIGNED;
  if (pElement->m_type == ElementDesc::DRYWET) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":wet");
  } else if (pElement->m_type == ElementDesc::BYPASS) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":bypass");
  } else if (pElement->m_type == ElementDesc::DELTA) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":delta");
  }

  PMParam *pParam = getPMParam(pElement);
  if (pParam == NULL)
    return NOT_ASSIGNED;

  return pParam->getParamID() + pElement->m_offset;
}

int PlugAccess::getParamID(int bank, int page, ElementDesc::eType type,
                           int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getParamID(&desc);
}

void PlugAccess::setParamValueInt(int bank, int page,
                                  ElementDesc::eType type, int channel,
                                  int value) {
  if (!plugExist())
    return;

  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id,
                     convertMCU2R(id, std::min(value, MAX_FADER_VALUE_INT)));
    if (type == ElementDesc::FADER || type == ElementDesc::VPOT) {
      // This write originates from the extension (MCU fader/VPOT/button),
      // not from the user in the FX GUI. Mirror the value the host actually
      // stored into the follow-change cache so followChanges() does not
      // treat the write as an external change and yank the follow unit's
      // bank/page.
      double min, max;
      m_pMode->onParamValueWrittenFromMCU(
          bank, page, type, channel,
          TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max));
    }
  }
}

void PlugAccess::setParamValueDouble(int bank, int page,
                                     ElementDesc::eType type, int channel,
                                     double value) {
  if (!plugExist())
    return;

  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id, value);
    if (type == ElementDesc::FADER || type == ElementDesc::VPOT) {
      // See setParamValueInt: keep the follow-change cache in sync with
      // extension-originated writes so followChanges() only reacts to
      // external (mouse) changes.
      double min, max;
      m_pMode->onParamValueWrittenFromMCU(
          bank, page, type, channel,
          TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max));
    }
  }
}

int PlugAccess::getParamValueInt(int bank, int page, ElementDesc::eType type,
                                 int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return convertR2MCU(
        id, TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max));
  }

  return 0;
}

double PlugAccess::getParamValueDouble(int bank, int page,
                                       ElementDesc::eType type, int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}

double PlugAccess::getParamValueDouble(ElementDesc *desc) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(desc);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}

PMVPot::tSteps *PlugAccess::getParamSteps(int bank, int page, int vpot) {
  if (!plugExist())
    return NULL;

  ElementDesc desc(bank, page, ElementDesc::VPOT, vpot);
  PMVPot *pVPot = static_cast<PMVPot *>(getPMParam(&desc));
  if (!pVPot)
    return NULL;

  return pVPot->getStepsMap();
}

String PlugAccess::getParamValueShort(int bank, int page,
                                      ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(bank, page, channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<0>();
      } else
        return String();
    }

    char valueString[80] = {};
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return shortNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(bank, page, type, channel));
    }
  }

  return String();
}

String PlugAccess::getParamValueLong(int bank, int page,
                                     ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(bank, page, channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<1>();
      }
    }

    char valueString[80] = {};
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return longNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(bank, page, type, channel));
    }
  }

  return String();
}

double PlugAccess::convertMCU2R(int id, int value) {
  ASSERT(plugExist());

  double min, max;
  TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  double valueAsD = ((double)value) / MAX_FADER_VALUE;
  return min + (max - min) * valueAsD;
}

int PlugAccess::convertR2MCU(int id, double value) {
  ASSERT(plugExist());

  double min, max;
  TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  double normed = (value - min) / (max - min);
  return (int)(normed * MAX_FADER_VALUE);
}

// ---- per-unit setters ----

void PlugAccess::setSelectedBank(int bank, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS || bank < 0 || bank >= 8)
    return;
  m_selectedBankPerUnit[unit] = bank;
  // Always update LEDs/faders — affects all units (N>1)
  m_pMode->updateSoloLEDs();
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
  m_pMode->updateSelectLEDs();
  m_pMode->updateRecLEDs();
}

void PlugAccess::setSelectedPage(int bank, int page, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS || bank < 0 || bank >= 8 ||
      page < 0 || page >= 8)
    return;
  m_selectedPagePerUnit[unit][bank] = page;
  // Selecting a page makes the unit non-empty.
  m_unitEmpty[unit] = false;
  // Always update LEDs/faders — affects all units (N>1)
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
  m_pMode->updateSelectLEDs();
  m_pMode->updateRecLEDs();
}

void PlugAccess::setSelectedPageInSelectedBank(int page, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS)
    return;
  setSelectedPage(m_selectedBankPerUnit[unit], page, unit);
}

void PlugAccess::trackRemoved(MediaTrack *pMT) {
  Tracks::instance()->selectionChanged();

  if (pMT == m_pPlugTrack) {
    accessPlugin(NULL, -1);
  }
}

// resolveBankReference uses the active unit's bank
int PlugAccess::resolveBankReference() {
  int activeUnit = m_pMode->getActiveUnit();
  if (activeUnit < 0 || activeUnit >= MAX_SURFACE_UNITS)
    return 0;
  int activeBank = m_selectedBankPerUnit[activeUnit];
  return resolveBankReference(activeBank);
}

int PlugAccess::resolveBankReference(int bank) {
  if (bank < 0 || bank >= 8)
    return 0;
  return getMap()->getBank(bank)->doesRefer()
             ? getMap()->getBank(bank)->referTo()
             : bank;
}

bool PlugAccess::isPageUsedInSelectedBank(int page) {
  return getMap()->getBank(resolveBankReference())->getPage(page)->isUsed();
}

PlugMap *PlugAccess::getMap() { return m_pMapManager->getActiveMap(); }

// ---- Used-page sequence helpers ----

std::vector<int> PlugAccess::usedPages(int bank) {
  std::vector<int> result;
  PlugMap *map = getMap();
  if (!map)
    return result;
  for (int p = 0; p < 8; p++) {
    if (map->getBank(bank)->getPage(p)->isUsed())
      result.push_back(p);
  }
  return result;
}

int PlugAccess::usedPageCount(int bank) {
  return (int)usedPages(bank).size();
}

int PlugAccess::pageAtUsedOffset(int bank, int offset) {
  std::vector<int> pages = usedPages(bank);
  if (pages.empty())
    return 0;
  if (offset < 0)
    return pages.front();
  if (offset >= (int)pages.size())
    return pages.back();
  return pages[offset];
}

int PlugAccess::pageUsedOffsetForPage(int bank, int page) {
  std::vector<int> pages = usedPages(bank);
  for (int i = 0; i < (int)pages.size(); i++) {
    if (pages[i] == page)
      return i;
  }
  return -1;
}

// ---- Page uniqueness ----

int PlugAccess::firstFreePageForUnit(int bank, int unit) {
  std::vector<int> pages = usedPages(bank);
  int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
  for (size_t i = 0; i < pages.size(); i++) {
    int p = pages[i];
    bool taken = false;
    for (int v = 0; v < nUnits; v++) {
      if (v == unit || isUnitEmpty(v))
        continue;
      if (selectedBankForUnit(v) == bank && selectedPageForUnit(v) == p) {
        taken = true;
        break;
      }
    }
    if (!taken)
      return p;
  }
  return -1;
}

void PlugAccess::enforceUniquePages(int changedUnit) {
  int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
  // Iterate from the highest unit index so that, when no changedUnit is
  // given (-1), the lowest-index unit of a duplicate group keeps its page.
  for (int v = nUnits - 1; v >= 0; v--) {
    if (v == changedUnit || isUnitEmpty(v))
      continue;
    int bankV = selectedBankForUnit(v);
    int pageV = selectedPageForUnit(v);
    if (pageV < 0)
      continue;
    for (int w = 0; w < nUnits; w++) {
      if (w == v || isUnitEmpty(w))
        continue;
      if (selectedBankForUnit(w) == bankV && selectedPageForUnit(w) == pageV) {
        clearUnitPage(v);
        break;
      }
    }
  }
}

// ---- Persistence ----

#define PLUGACCESS_NODE_ROOT String("PLUGACCESS")
#define PLUGACCESS_NODE_SLOTSTATE String("SLOTSTATES")
#define PLUGACCESS_ATT_SLOTSTATE_TRACK String("track")
#define PLUGACCESS_ATT_SLOTSTATE_SLOT String("slot")
#define PLUGACCESS_ATT_SLOTSTATE_PLUGNAME String("plugname")
#define PLUGACCESS_ATT_SLOTSTATE_BANK String("bank")
#define PLUGACCESS_NODE_SLOTSTATE_PAGE String("PAGE")
#define PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX String("nr")

// versioned UNIT_STATES block
#define PLUGACCESS_NODE_UNIT_STATES String("UNIT_STATES")
#define PLUGACCESS_ATT_VERSION String("version")
#define PLUGACCESS_NODE_UNIT String("UNIT")
#define PLUGACCESS_ATT_UNIT_INDEX String("nr")
#define PLUGACCESS_ATT_UNIT_BANK String("bank")
#define PLUGACCESS_ATT_UNIT_EMPTY String("empty")

#define PLUGACCESS_NODE_SELECTED_PLUG String("SELECTED_PLUG")
#define PLUGACCESS_ATT_SELECTED_PLUG_TRACK String("track")
#define PLUGACCESS_ATT_SELECTED_PLUG_SLOT String("slot")

void PlugAccess::projectChanged(XmlElement *pXmlElement,
                                ProjectConfig::EAction action) {
  XmlElement *pPlugAccessNode;

  switch (action) {
  case ProjectConfig::WRITE:
    storeActualSlotState();
    pPlugAccessNode = new XmlElement(PLUGACCESS_NODE_ROOT);
    writeSlotStatesToProjectConfig(pPlugAccessNode);
    writeSelectedPlugToProjectConfig(pPlugAccessNode);
    pXmlElement->addChildElement(pPlugAccessNode);
    break;

  case ProjectConfig::FREE:
    m_knownSlotStates.clear();
    break;

  case ProjectConfig::READ:
    pPlugAccessNode = pXmlElement->getChildByName(PLUGACCESS_NODE_ROOT);
    if (pPlugAccessNode) {
      readSlotStatesFromProjectConfig(pPlugAccessNode);
      readSelectedPlugFromProjectConfig(pPlugAccessNode, true);
    }
    break;
  }
}

void PlugAccess::writeSlotStatesToProjectConfig(XmlElement *pNode) {
  BOOST_FOREACH (tSlotStatePair &entry, m_knownSlotStates) {
    XmlElement *pSlotState = new XmlElement(PLUGACCESS_NODE_SLOTSTATE);
    tSlotLocation loc = entry.first;
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_TRACK, loc.get<0>());
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_SLOT, loc.get<1>());

    tSlotState state = entry.second;
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_PLUGNAME, state.get<0>());

    // write versioned UNIT_STATES block (v2 adds the per-unit empty flag)
    XmlElement *pUnitStates =
        new XmlElement(PLUGACCESS_NODE_UNIT_STATES);
    pUnitStates->setAttribute(PLUGACCESS_ATT_VERSION, 2);

    boost::array<int, MAX_SURFACE_UNITS> banksPerUnit = state.get<1>();
    boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> pagesPerUnit =
        state.get<2>();
    boost::array<bool, MAX_SURFACE_UNITS> emptyPerUnit = state.get<3>();

    for (int u = 0; u < MAX_SURFACE_UNITS; u++) {
      XmlElement *pUnit = new XmlElement(PLUGACCESS_NODE_UNIT);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_INDEX, u);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_BANK, banksPerUnit[u]);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_EMPTY, emptyPerUnit[u]);

      for (int p = 0; p < 8; p++) {
        XmlElement *pPage = new XmlElement(PLUGACCESS_NODE_SLOTSTATE_PAGE);
        pPage->setAttribute(PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX,
                            pagesPerUnit[u][p]);
        pUnit->addChildElement(pPage);
      }
      pUnitStates->addChildElement(pUnit);
    }
    pSlotState->addChildElement(pUnitStates);
    pNode->addChildElement(pSlotState);
  }
}

void PlugAccess::readSlotStatesFromProjectConfig(XmlElement *pNode) {
  for (auto* pChild : pNode->getChildIterator()) {
    if (pChild->getTagName() == PLUGACCESS_NODE_SLOTSTATE) {
      tSlotLocation loc(
          pChild->getStringAttribute(PLUGACCESS_ATT_SLOTSTATE_TRACK),
          pChild->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_SLOT));

      String plugName =
          pChild->getStringAttribute(PLUGACCESS_ATT_SLOTSTATE_PLUGNAME);

      boost::array<int, MAX_SURFACE_UNITS> banksPerUnit;
      boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> pagesPerUnit;
      boost::array<bool, MAX_SURFACE_UNITS> emptyPerUnit;
      banksPerUnit.fill(0);
      for (int u = 0; u < MAX_SURFACE_UNITS; u++)
        pagesPerUnit[u].fill(0);
      emptyPerUnit.fill(false);

      // try versioned UNIT_STATES block first
      XmlElement *pUnitStates =
          pChild->getChildByName(PLUGACCESS_NODE_UNIT_STATES);
      if (pUnitStates && pUnitStates->getIntAttribute(PLUGACCESS_ATT_VERSION) >= 1) {
        int unitCount = 0;
        for (auto* pUnit : pUnitStates->getChildIterator()) {
          if (pUnit->getTagName() == PLUGACCESS_NODE_UNIT) {
            int u = pUnit->getIntAttribute(PLUGACCESS_ATT_UNIT_INDEX);
            if (u >= 0 && u < MAX_SURFACE_UNITS) {
              int bank = pUnit->getIntAttribute(PLUGACCESS_ATT_UNIT_BANK);
              banksPerUnit[u] = bank >= 0 && bank < 8 ? bank : 0;
              // v1 states have no empty attribute; the flag defaults to
              // false (unit shows its stored page). Duplicate pages in
              // legacy states are normalized by enforceUniquePages on
              // restore.
              emptyPerUnit[u] =
                  pUnit->getBoolAttribute(PLUGACCESS_ATT_UNIT_EMPTY, false);
              int page = 0;
              for (auto* pPage : pUnit->getChildIterator()) {
                if (page < 8) {
                  int pageIndex = pPage->getIntAttribute(
                      PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX);
                  pagesPerUnit[u][page++] =
                      pageIndex >= 0 && pageIndex < 8 ? pageIndex : 0;
                }
              }
              unitCount++;
            }
          }
        }
      } else {
        // Legacy format: single bank + 8 pages → map to unit 0
        int bank = pChild->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_BANK);
        banksPerUnit[0] = bank >= 0 && bank < 8 ? bank : 0;
        int page = 0;
        for (auto* pPage : pChild->getChildIterator()) {
          if (pPage->getTagName() == PLUGACCESS_NODE_SLOTSTATE_PAGE &&
              page < 8) {
            int pageIndex = pPage->getIntAttribute(
                PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX);
            pagesPerUnit[0][page++] =
                pageIndex >= 0 && pageIndex < 8 ? pageIndex : 0;
          }
        }
        // Units 1..N−1: defaults from default page-spread logic
        // (handled by accessPlugin when restoredFromStored is false for those)
      }

      tSlotState state(plugName, banksPerUnit, pagesPerUnit, emptyPerUnit);
      m_knownSlotStates[loc] = state;
    }
  }
}

void PlugAccess::storeActualSlotState() {
  // store per-unit arrays
  if (m_iSlot >= 0 && m_pPlugTrack != NULL) {
    tSlotLocation loc(GUID2String(&m_GUIDplugTrack), m_iSlot);
    tSlotState state(m_plugName, m_selectedBankPerUnit, m_selectedPagePerUnit,
                     m_unitEmpty);
    m_knownSlotStates.erase(loc);
    m_knownSlotStates.insert(std::pair<tSlotLocation, tSlotState>(loc, state));
  }
}

void PlugAccess::writeSelectedPlugToProjectConfig(XmlElement *pPlugAccessNode) {
  XmlElement *pSelectedPlug = new XmlElement(PLUGACCESS_NODE_SELECTED_PLUG);
  pSelectedPlug->setAttribute(PLUGACCESS_ATT_SELECTED_PLUG_TRACK,
                              GUID2String(&m_GUIDplugTrack));
  pSelectedPlug->setAttribute(PLUGACCESS_ATT_SELECTED_PLUG_SLOT, m_iSlot);
  pPlugAccessNode->addChildElement(pSelectedPlug);
}

void PlugAccess::readSelectedPlugFromProjectConfig(
    XmlElement *pPlugAccessNode,
    bool changeTriggeredFromProjectChange) {
  for (auto* pChild : pPlugAccessNode->getChildIterator()) {
    if (pChild->getTagName() == PLUGACCESS_NODE_SELECTED_PLUG) {
      String guidString =
          pChild->getStringAttribute(PLUGACCESS_ATT_SELECTED_PLUG_TRACK);
      GUID guid;
      String2GUID(guidString, &guid);
      MediaTrack *pMediaTrack = CSurf_MCU::TrackFromGUID(guid);
      int iSlot = pChild->getIntAttribute(PLUGACCESS_ATT_SELECTED_PLUG_SLOT);

      if (!pMediaTrack || iSlot < 0 || iSlot >= TrackFX_GetCount(pMediaTrack)) {
        accessPlugin(NULL, -1, false, changeTriggeredFromProjectChange);
        return;
      }

      accessPlugin(pMediaTrack, iSlot, false, changeTriggeredFromProjectChange);
      return;
    }
  }
}

// ---- unchanged below this line ----

void PlugAccess::watchedNameParameterChanged(MediaTrack *pMediaTrack, int iSlot,
                                             String newPlugName) {
  if (pMediaTrack == m_pPlugTrack && iSlot == m_iSlot) {
    accessPlugin(pMediaTrack, iSlot);
  }
}

void PlugAccess::checkFloatWindows() {
  std::unique_ptr<FloatingWindowInfo> pFWI;

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_SAME_TRACK) &&
      m_pMode->isFollowTrack()) {
    pFWI.reset(checkAppearingFloats(m_pPlugTrack));
  }

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      std::unique_ptr<FloatingWindowInfo> pTrackFWI(
          checkAppearingFloats(*ti));
      if (pTrackFWI) {
        pFWI = std::move(pTrackFWI);
      }
    }
    // trigger update when user opens floating plugin on master track
    if (!pFWI)
      pFWI.reset(checkAppearingFloats(GetMasterTrack(NULL)));
  }

  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_CHAIN)) {
    if (pFWI) {
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 2); // close float
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 1); // open chain
      tWindowStates::iterator iterWS = m_knownWndStates.find(
          boost::tuple<MediaTrack *, int>(pFWI->pMediaTrack, pFWI->iSlot));
      if (iterWS != m_knownWndStates.end())
        (*iterWS).second = NULL;
    } else if (!isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
      // we havn't searched for every track
      for (TrackIterator ti; !ti.end(); ++ti) {
        std::unique_ptr<FloatingWindowInfo> trackFWI(
            checkAppearingFloats(*ti, false));
        if (trackFWI) {
          TrackFX_Show(trackFWI->pMediaTrack, trackFWI->iSlot, 2); // close float
          TrackFX_Show(trackFWI->pMediaTrack, trackFWI->iSlot, 1); // open chain
          tWindowStates::iterator iterWS = m_knownWndStates.find(
              boost::tuple<MediaTrack *, int>(trackFWI->pMediaTrack,
                                               trackFWI->iSlot));
          if (iterWS != m_knownWndStates.end())
            (*iterWS).second = NULL;
        }
      }
    }
  }

  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_GLOBAL)) {
    m_pWindowManager->allowOnlySelectedFloat(m_pPlugTrack, m_iSlot);
  }
}

void PlugAccess::syncKnownStates() {
  // Sync chain visible states for all tracks so the next frame update
  // doesn't detect false changes from deactivate/activate cycles.
  for (TrackIterator ti; !ti.end(); ++ti) {
    int chainVisible = TrackFX_GetChainVisible(*ti);
    if (chainVisible >= 0) {
      m_knownChainStates[*ti] = chainVisible;
    } else {
      m_knownChainStates.erase(*ti);
    }
  }
  // Also check master track
  MediaTrack *master = GetMasterTrack(NULL);
  int masterChain = TrackFX_GetChainVisible(master);
  if (masterChain >= 0) {
    m_knownChainStates[master] = masterChain;
  } else {
    m_knownChainStates.erase(master);
  }

  // Sync known floating window states
  for (TrackIterator ti; !ti.end(); ++ti) {
    int numFX = TrackFX_GetCount(*ti);
    for (int i = 0; i < numFX; i++) {
      m_knownWndStates[boost::tuple<MediaTrack *, int>(*ti, i)] =
          TrackFX_GetFloatingWindow(*ti, i);
    }
  }
}

void PlugAccess::checkChainChanges() {
  static MediaTrack *lastMediaTrack = NULL;
  static int lastSlot = -1;

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_SAME_TRACK) &&
      m_pMode->isFollowTrack()) {
    checkChain(m_pPlugTrack);
  }

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      checkChain(*ti);
    }
    // trigger update when user changes selected plugin on master track
    checkChain(GetMasterTrack(NULL));
  }
}

void PlugAccess::checkChain(MediaTrack *pTrack) {
  int chainVisible = TrackFX_GetChainVisible(pTrack);
  if (chainVisible < 0)
    return;

  tChainSlot::iterator iterCS = m_knownChainStates.find(pTrack);
  if (iterCS != m_knownChainStates.end()) {
    if ((*iterCS).second != chainVisible) {
      accessPlugin(pTrack, chainVisible, true);
    }
    (*iterCS).second = chainVisible;
  } else {
    m_knownChainStates.insert(tChainSlot::value_type(pTrack, chainVisible));
    accessPlugin(pTrack, chainVisible, true);
  }
}

FloatingWindowInfo *PlugAccess::checkAppearingFloats(MediaTrack *pTrack,
                                                     bool accessAppearing) {
  int numFX = TrackFX_GetCount(pTrack);
  for (int i = 0; i < numFX; i++) {
    tWindowStates::iterator iterWS =
        m_knownWndStates.find(boost::tuple<MediaTrack *, int>(pTrack, i));
    HWND actualHWND = TrackFX_GetFloatingWindow(pTrack, i);
    HWND knownHWND = NULL;
    if (iterWS != m_knownWndStates.end()) {
      knownHWND = (*iterWS).second;
      (*iterWS).second = actualHWND;
    } else {
      m_knownWndStates.insert(tWindowStates::value_type(
          boost::tuple<MediaTrack *, int>(pTrack, i), actualHWND));
    }
    if (knownHWND != actualHWND && actualHWND != NULL) {
      if (accessAppearing && (i != m_iSlot || pTrack != m_pPlugTrack))
        accessPlugin(pTrack, i, true);

      return new FloatingWindowInfo(actualHWND, pTrack, i);
    }
  }
  return NULL;
}

void PlugAccess::openFX() {
  if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN) ||
      isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN_CLOSE_FLOAT)) {
    TrackFX_Show(m_pPlugTrack, m_iSlot, 1);
  } else if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_FLOATING)) {
    TrackFX_Show(m_pPlugTrack, m_iSlot, 3);
  }
}
