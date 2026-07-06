/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "Region.h"
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdio.h>
#define REAPER_PLUGIN_DECLARE_APIFUNCS

Region::Region() {
  m_start = 0;
  m_end = 0;
}

Region::Region(double start, double end) {
  m_start = start;
  m_end = end;
}

bool Region::FindRegion(int searchedIndex) {
  bool isrgn;
  int lIndex;

  int idx = 0;
  do {
    idx = EnumProjectMarkers(idx, &isrgn, &m_start, &m_end, 0, &lIndex);
    if (isrgn && lIndex == searchedIndex) {
      return true;
    }
  } while (idx > 0);
  return false;
}

double Region::MarkerPos(int markerID) {
  bool isrgn;
  int lIndex;
  double start, end;

  int idx = 0;
  do {
    idx = EnumProjectMarkers(idx, &isrgn, &start, &end, 0, &lIndex);
    if (!isrgn && lIndex == markerID) {
      return start;
    }
  } while (idx > 0);
  return -1;
}

void Region::DeleteRegion(int index) {
  /*
                double originalStart = m_start;
                double originalEnd = m_end;
                if (FindRegion(index)) {
    SetEditCurPos(m_start, false, false);
    SendMessage(g_hwnd, WM_COMMAND, 40615, 0); // Delete region
                }
                m_start = originalStart;
                m_end = originalEnd;
  */
  DeleteProjectMarker(NULL, index, true);
}

void Region::DeleteRegionStartWithPos(double pos) {
  double start, end;
  int lIndex;
  bool isrgn;

  int idx = 0;
  do {
    idx = EnumProjectMarkers(idx, &isrgn, &start, &end, 0, &lIndex);
    if (idx > 0 && isrgn && start == pos) {
      DeleteRegion(lIndex);
      DeleteRegionStartWithPos(pos);
    }
  } while (idx > 0);
}

bool Region::FindRegionBefore(double pos) {
  bool isrgn;
  int lIndex;
  int foundIndex = -1;

  int idx = 0;
  do {
    idx = EnumProjectMarkers(idx, &isrgn, &m_start, &m_end, 0, &lIndex);
    if (isrgn && m_end < pos) {
      foundIndex = lIndex;
    }
  } while (idx > 0);

  return FindRegion(foundIndex);
}

bool Region::FindRegionBehind(double pos) {
  bool isrgn;
  int lIndex;

  int idx = 0;
  do {
    idx = EnumProjectMarkers(idx, &isrgn, &m_start, &m_end, 0, &lIndex);
    if (isrgn && m_start > pos) {
      return true;
    }
  } while (idx > 0);
  return false;
}

void Region::SwapRegionForward(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  m_start = end;
  m_end = end + (end - start);
  Set(asLoop);
}

void Region::SwapRegionBackward(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  m_end = start;
  m_start = start - (end - start);
  Set(asLoop);
}

void Region::NextRegion(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  if (FindRegionBehind(start))
    Set(asLoop);
}

void Region::GetFromActualRegion(bool asLoop) {
  GetSet_LoopTimeRange(false, asLoop, &m_start, &m_end, false);
}

/*static*/ double Region::GetRegionStart(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  return start;
}

/*static*/ double Region::GetRegionEnd(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  return end;
}

/*static*/ bool Region::IsActive(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  return (start == end == 0.);
}

void Region::PreviousRegion(bool asLoop) {
  double start, end;
  GetSet_LoopTimeRange(false, asLoop, &start, &end, false);
  if (FindRegionBefore(end))
    Set(asLoop);
}

void Region::Set(bool asLoop) {
  if (m_start < 0)
    m_start = 0;
  if (m_end < 0)
    m_end = 0;

  if (m_start == m_end)
    return;

  if (m_end < m_start) {
    double swap = m_start;
    m_start = m_end;
    m_end = swap;
  }

  GetSet_LoopTimeRange(true, asLoop, &m_start, &m_end, false);

  //  if (asLoop) {
  //    SetEditCurPos(m_start, true, false);
  //  }
}

/*
// Code to edit regions
// Based on code from SWS marker list plugin, many thanks to him
BOOL CALLBACK FindWindow(HWND hwnd, LPARAM lParam)
{
char str[100];
HWND* dlg = (HWND*) lParam;

GetWindowText(hwnd, str, 100);
if (strcmp(str, "Edit Region") == 0)
{
*dlg = hwnd;
return false; // This stops the search, since we've found our child
}
*dlg = 0;
return true;
}


void MarkerTextThread(void* param)
{
int id = *((int*) param);
DWORD startTicks = GetTickCount();
HWND dlg;
do
{
// Don't stick around too long, must have been an error otherwise
if (GetTickCount()-startTicks > 1000)
return;
Sleep(1);
EnumWindows(FindWindow, (LPARAM)&dlg);
}
while (dlg == 0);
// Setting text in marker dlg
// We have a handle to the marker dialog.
char str[100];
sprintf(str, "%d", id);
SendMessage(GetDlgItem(dlg, 1009), WM_SETTEXT, 0, (LPARAM)str);
SendMessage(dlg, WM_COMMAND, IDOK, 0);
}
*/

void Region::Store(int id, bool bLoop) {
  Undo_BeginBlock();
  //  double originalCurPos = GetCursorPosition();

  DeleteRegion(id);
  //  DeleteRegionStartWithPos(m_start);
  AddProjectMarker(NULL, true, m_start, m_end, NULL, id);
  UpdateTimeline();

  Undo_EndBlock("Add Region (via Surface)", UNDO_STATE_MISCCFG);
  //  adjustZoom(prevZoom, 1, false, -1);
  //  prevZoom = -1.0;
}
