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
  int x = iChannel - 1; // 0-based strip index
  // Check mute/solo state of the track. Muted and missing tracks render as
  // an empty software meter so a stale bar cannot remain on the display.
  if (ts && ts->getVUactive() && pMT) {
    v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
    double pp = 0.0;
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
  // Software-meter bars are only drawn for modes that opt in via
  // alsoOnDisplay() (MultiTrack/Pan/Send). Plug Mode never draws them —
  // they would overwrite the parameter name/value text on the LCD.
  // Inside showMeterOnDisplay the per-unit metersOnDisplay() option is the
  // final gate, so even opted-in modes only paint on units with the option.
  if (alsoOnDisplay())
    showMeterOnDisplay(pMCU, iChannel, v);
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
                                     short meter) {
  HardwareUnit *unit = pMCU->unitForChannel(channel);
  if (!unit)
    return;

  // The software meter bars are only drawn on units that opted into
  // "Emulate level meters on the display". Without this guard the bars
  // overwrite the regular row-1 text on every unit (e.g. the FADER
  // name/value in Plug Mode), so extenders appear to have no text and
  // units that never enabled the option still show meters.
  if (!unit->metersOnDisplay())
    return;

  Display *display = unit->displayHandler()->getDisplay();
  if (!display)
    return;

  // The LCD field is six characters wide. Meter values 1..12 become one to
  // six bars; silence clears the field. This does not depend on Mackie's
  // optional LCD-meter SysEx support.
  int bars = std::min(6, std::max(0, (int)((meter + 1) / 2)));
  char text[7];
  memset(text, '|', bars);
  text[bars] = 0;
  display->changeField(1, CSurf_MCU::localOf(channel), text);
}
