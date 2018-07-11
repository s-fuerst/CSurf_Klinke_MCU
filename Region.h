/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_REGION
#define MCU_REGION

#include "csurf.h"

class Region {
 private:
  double m_start;
  double m_end;

 public:
  static const bool LOOP = true;
  static const bool TIME = false;

  Region();
  Region(double start, double end);
  bool FindRegion( int searchedIndex );
  void DeleteRegion( int index );
  void DeleteRegionStartWithPos( double pos );
  bool FindRegionBefore( double pos );
  bool FindRegionBehind( double pos );
  void SwapRegionForward( bool asLoop );
  void SwapRegionBackward( bool asLoop );
  void NextRegion ( bool asLoop );
  void GetFromActualRegion( bool asLoop );
  void PreviousRegion ( bool asLoop );
  void SetStart(double start) { m_start = start; }
  void SetEnd(double end) { m_end = end; }
  void Set( bool asLoop );
  double GetStart() { return m_start; }
  double GetEnd() { return m_end; }
  void Store(int id, bool bNew);
  double MarkerPos(int markerID );
  static double GetRegionStart( bool asLoop );
  static double GetRegionEnd( bool asLoop );
  static bool IsActive( bool asLoop );
};

#endif
