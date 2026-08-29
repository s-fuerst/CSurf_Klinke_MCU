/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "MeterBridge.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "Tracks.h"
#include "HardwareUnit.h"
#include "DisplayHandler.h"
#include "Display.h"

#include <algorithm>
#include <cstring>

MeterBridge::MeterBridge() {
  m_stripMeterPos.assign(8, -100000.0);
  m_masterMeterPos[0] = -100000.0;
  m_masterMeterPos[1] = -100000.0;
  m_mcu_meter_lastrun = 0;
}

void MeterBridge::ensureStripMeterState(int channelCount) {
  if ((int)m_stripMeterPos.size() != channelCount)
    m_stripMeterPos.assign(channelCount, -100000.0);
}

void MeterBridge::updateMeter(int iChannel, MediaTrack *pMT, CSurf_MCU *pMCU,
                              double decay, int pin) {
  auto ts = Tracks::instance()->getTrackStateForMediaTrack(pMT);
  int v = 0x0;
  double pp = -1000000.0; // peak in dB, kept for the display-meter gate below
  int x = iChannel - 1; // 0-based strip index
  // Check mute/solo state of the track. Muted and missing tracks render as
  // an empty software meter so a stale bar cannot remain on the display.
  if (ts && ts->getVUactive() && pMT) {
    v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
    // get peak
    if (pin < 0)
      pp = VAL2DB((Track_GetPeakInfo(pMT, 0) + Track_GetPeakInfo(pMT, 1)) * 0.5);
    else
      pp = VAL2DB((Track_GetPeakInfo(pMT, pin)));

    if (x < (int)m_stripMeterPos.size() && m_stripMeterPos[x] > -VU_BOTTOM * 2)
      m_stripMeterPos[x] -= decay * 2;

    if (x < (int)m_stripMeterPos.size() && pp > m_stripMeterPos[x]) {
      m_stripMeterPos[x] = pp;
    } else if (x < (int)m_stripMeterPos.size()) {
      pp = m_stripMeterPos[x];
    }

    if (pp < 0.0) {
      if (pp <= -VU_SIGNAL_LED)
        v = 0x0;
      else if (pp <= -VU_BOTTOM)
        v = 0x1;
      else
        v = (int)((pp + VU_BOTTOM) * 11.0 / VU_BOTTOM) + 1;
    }
    sendToHardware(pMCU, x, v);
  }
  // The software meter is only drawn for modes that opt in via
  // alsoOnDisplay() (MultiTrack/Pan). Inside showMeterOnDisplay the per-unit
  // metersOnDisplay() option is the final gate, so even opted-in modes only
  // paint on units with the option.
  // The hardware LED meters keep their original -70 dB signal threshold,
  // but the emulated LCD segments only appear for signals above -60 dB so
  // that low-level noise no longer paints a one-segment flicker on the
  // display.
  if (alsoOnDisplay())
    showMeterOnDisplay(pMCU, iChannel, pp > -60.0 ? v : 0, pp >= 0.0);
}

void MeterBridge::updateMasterLEDs(CSurf_MCU *pMCU, double decay) {
  // send master meters to every ProX unit.
  // Compute values once, then broadcast.
  short leftVal = 0, rightVal = 0;
  for (int x = 0; x < 2; x++) {
    int v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
    double pp = VAL2DB(Track_GetPeakInfo(GetMasterTrack(NULL), x));

    if (m_masterMeterPos[x] > -VU_BOTTOM * 2)
      m_masterMeterPos[x] -= decay * 2;

    if (pp > m_masterMeterPos[x]) {
      m_masterMeterPos[x] = pp;
    } else {
      pp = m_masterMeterPos[x];
    }

    if (pp < 0.0) {
      if (pp <= -VU_SIGNAL_LED)
        v = 0x0;
      else if (pp <= -VU_BOTTOM)
        v = 0x1;
      else
        v = (int)((pp + VU_BOTTOM) * 11.0 / VU_BOTTOM) + 1;
    }
    if (x == 0) leftVal = v; else rightVal = v;
  }
  pMCU->sendMasterMetersToProXUnits(leftVal, rightVal);
}

void MeterBridge::sendToHardware(CSurf_MCU *pMCU, int pos, short meter) {
  ASSERT(pos >= 0);

  if (meter > 12) meter = 12;
  if (meter < 0)  meter = 0;

  // strip meters route via sendStripMeter (owning unit, 0xD0).
  // Master meters route via sendMasterMetersToProXUnits (0xD1).
  // pos is 0-based strip index.
  pMCU->sendStripMeter(pos + 1, meter);
}

void MeterBridge::showMeterOnDisplay(CSurf_MCU *pMCU, int channel,
                                     short meter, bool clip) {
  HardwareUnit *unit = pMCU->unitForChannel(channel);
  if (!unit)
    return;

  // The meter segments are only drawn on units that opted into
  // "Emulate level meters on the display". They live in the separator
  // columns between the field areas (and the last column for local channel
  // 8), so they do not touch the mode text in the fields.
  if (!unit->metersOnDisplay())
    return;

  Display *display = unit->displayHandler()->getDisplay();
  if (!display)
    return;

  // The meter lives in a single column per channel: the separator column
  // right after the channel's six-character field. Row-0/1 fields sit at
  // (local-1)*7, six chars wide, so local channels 1..8 use 1-based row
  // positions 7, 14, ..., 56. Position 56 is the column beyond the
  // original 55-char Mackie LCD (the unused byte at the end of each
  // hardware row) — on iCON/Behringer 56-char displays it is visible, on
  // the original Mackie Control it is not (that model uses the native
  // hardware meters instead).
  int local = CSurf_MCU::localOf(channel);
  int col = local * 7 - 1; // 0-based

  // The meter column has two rows, giving four fill states. Meter values
  // 1..12 map to the four states in thirds; meter <= 0 clears the column
  // (spaces) so a stale segment cannot remain. This does not depend on
  // Mackie's optional LCD-meter SysEx support.
  //   1..3   : '.' on row 1
  //   4..6   : ':' on row 1
  //   7..9   : '.' on row 0 plus ':' on row 1
  //   10..12 : ':' on both rows
  // While the held peak is above 0 dB (clip), the top character is replaced
  // by '*' so an overdriven channel is recognizable at a glance. The clip
  // mark follows the meter peak decay like the bar itself.
  char top = ' ', bottom = ' ';
  if (meter >= 10)
    top = bottom = ':';
  else if (meter >= 7)
    top = '.', bottom = ':';
  else if (meter >= 4)
    bottom = ':';
  else if (meter >= 1)
    bottom = '.';
  if (clip)
    top = '*';

  display->changeText(0, col, &top, 1);
  display->changeText(1, col, &bottom, 1);
}
