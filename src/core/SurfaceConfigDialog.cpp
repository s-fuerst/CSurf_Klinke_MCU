/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-B: SurfaceConfig dialog — REAPER control-surface configuration UI.
 * Unit selector + per-unit type/MIDI combos, all 8 units configurable
 * through a single compact dialog.
 */

#include "csurf_mcu.h"
#include "SurfaceConfig.h"
#include "McuDebugLog.h"

// --- dialog helpers ---

static const char *s_deviceTypeNames[] = {
    "Mackie Main",          // 0 = UNIT_TYPE_MACKIE_MAIN
    "Mackie Extender",      // 1 = UNIT_TYPE_MACKIE_EXT
    "QCon ProX",            // 2 = UNIT_TYPE_PROX_MAIN
    "QCon ProX Extender",   // 3 = UNIT_TYPE_PROX_EXT
    "Disabled",             // 4 = UNIT_TYPE_DISABLED
};

// All five types (incl. Disabled) are valid for EVERY unit position.
// Unit position (channel-strip order) and main/extender role are
// orthogonal — a main unit may sit at any position, not just unit 1.
// Configs with zero main units are allowed (no validation).
static const int s_validTypesAll[] = {0, 1, 2, 3, 4, -1};

// Unit 0 (channels 1-8) may NOT be disabled — the first 8 channel slots
// must always exist. The 4 hardware types (Main/Extender × Mackie/ProX)
// are still unrestricted.
static const int s_validTypesUnit0[] = {0, 1, 2, 3, -1};

static void populateMidiCombo(HWND hwnd, int nDevices,
    bool (*getNameFunc)(int, char*, int), int targetDevId) {
  LRESULT idx = SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)"None");
  SendMessage(hwnd, CB_SETITEMDATA, idx, -1);
  for (int i = 0; i < nDevices; i++) {
    char buf[512];
    if (getNameFunc(i, buf, sizeof(buf))) {
      LRESULT a = SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)buf);
      SendMessage(hwnd, CB_SETITEMDATA, a, i);
      if (i == targetDevId)
        SendMessage(hwnd, CB_SETCURSEL, a, 0);
    }
  }
  if (targetDevId == -1)
    SendMessage(hwnd, CB_SETCURSEL, 0, 0);
}

static void populateTypeCombo(HWND hwnd, int selectedType, const int *validTypes) {
  for (int i = 0; validTypes[i] >= 0; i++) {
    int ti = validTypes[i];
    LRESULT a = SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)s_deviceTypeNames[ti]);
    SendMessage(hwnd, CB_SETITEMDATA, a, ti);
    if (ti == selectedType)
      SendMessage(hwnd, CB_SETCURSEL, a, 0);
  }
}

static int currentUnitIndex(HWND hwndDlg) {
  LRESULT u = SendDlgItemMessage(hwndDlg, IDC_UNIT_SELECT, CB_GETCURSEL, 0, 0);
  return (u == CB_ERR) ? 0 : (int)u;
}

static void saveUnitFromDialog(HWND hwndDlg, SurfaceConfig *cfg, int i) {
  LRESULT r = SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETCURSEL, 0, 0);
  int typeIdx = UNIT_TYPE_DISABLED;
  if (r != CB_ERR) {
    typeIdx = (int)SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETITEMDATA, r, 0);
    if (typeIdx < 0 || typeIdx > 4) typeIdx = UNIT_TYPE_DISABLED;
  }
  int inDev = -1;
  r = SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_GETCURSEL, 0, 0);
  if (r != CB_ERR)
    inDev = (int)SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_GETITEMDATA, r, 0);
  int outDev = -1;
  r = SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_GETCURSEL, 0, 0);
  if (r != CB_ERR)
    outDev = (int)SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_GETITEMDATA, r, 0);
  cfg->units[i] = unitConfigFromType(typeIdx, inDev, outDev);
}

static void loadUnitIntoDialog(HWND hwndDlg, const SurfaceConfig *cfg, int i,
                               int nIn, int nOut) {
  const UnitConfig &u = cfg->units[i];

  bool disabled = (i == 0) ? false : (u.midiInDev == -1 && u.midiOutDev == -1 && !u.isMain);

  HWND typeCb = GetDlgItem(hwndDlg, IDC_UNIT_TYPE);
  SendMessage(typeCb, CB_RESETCONTENT, 0, 0);
  int typeIdx;
  if (disabled) {
    typeIdx = UNIT_TYPE_DISABLED;
  } else {
    typeIdx = (u.model == QConProX)
        ? (u.isMain ? UNIT_TYPE_PROX_MAIN : UNIT_TYPE_PROX_EXT)
        : (u.isMain ? UNIT_TYPE_MACKIE_MAIN : UNIT_TYPE_MACKIE_EXT);
  }
  populateTypeCombo(typeCb, typeIdx, i == 0 ? s_validTypesUnit0 : s_validTypesAll);

  HWND inCb  = GetDlgItem(hwndDlg, IDC_COMBO2);
  HWND outCb = GetDlgItem(hwndDlg, IDC_COMBO3);
  int show = disabled ? SW_HIDE : SW_SHOW;
  ShowWindow(inCb, show);
  ShowWindow(outCb, show);

  if (!disabled) {
    SendMessage(inCb, CB_RESETCONTENT, 0, 0);
    populateMidiCombo(inCb, nIn, GetMIDIInputName, u.midiInDev);
    SendMessage(outCb, CB_RESETCONTENT, 0, 0);
    populateMidiCombo(outCb, nOut, GetMIDIOutputName, u.midiOutDev);
  }
}

// --- createFunc ---

static IReaperControlSurface *
createFunc(const char *type_string, const char *configString, int *errStats) {
  (void)type_string;
  SurfaceConfig cfg = parseSurfaceConfig(configString);

  // WP-EF-0a: validate dense unit topology.
  // A hand-edited or stale KLINKE2 string that is non-dense is logged and
  // replaced with the safe default before constructing CSurf_MCU.
  if (!hasDenseUnitTopology(cfg)) {
    MCU_LOG("createFunc: non-dense unit topology, replacing with default config");
    cfg = makeDefaultSurfaceConfig();
  }

  return new CSurf_MCU(cfg, errStats);
}

// --- layout ---

static void layoutDlgControls(HWND hwndDlg) {
  RECT cr;
  GetClientRect(hwndDlg, &cr);
  int W = cr.right, H = cr.bottom;
  if (W < 40 || H < 40) return;

  auto move = [&](int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwndDlg, id);
    if (c) SetWindowPos(c, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
  };

  auto nthStatic = [&](int n) -> HWND {
    int count = 0;
    HWND ch = GetWindow(hwndDlg, GW_CHILD);
    while (ch) {
      if (GetWindowLong(ch, GWL_ID) == 0 && count++ == n) return ch;
      ch = GetWindow(ch, GW_HWNDNEXT);
    }
    return NULL;
  };

  const int mx = 8, gap = 4;
  const int rowH = 22, comboH = 20, lblH = 18;
  int y = mx;

  // Row 0: Unit selector + Type combo. Keep explicit columns; SWELL can make
  // expanding controls overlap adjacent labels in this compact dialog.
  const int unitLabelX = mx;
  const int unitLabelW = 30;
  const int unitComboX = 42;
  const int unitComboW = 64;
  const int typeLabelX = 118;
  const int typeLabelW = 32;
  const int typeComboX = 156;
  const int typeComboW = 200;
  HWND lblUnit = nthStatic(0);
  if (lblUnit) SetWindowPos(lblUnit, NULL, unitLabelX, y + 3, unitLabelW, lblH, SWP_NOZORDER | SWP_NOACTIVATE);
  move(IDC_UNIT_SELECT, unitComboX, y + 2, unitComboW, comboH);
  HWND lblType = nthStatic(1);
  if (lblType) SetWindowPos(lblType, NULL, typeLabelX, y + 3, typeLabelW, lblH, SWP_NOZORDER | SWP_NOACTIVATE);
  move(IDC_UNIT_TYPE, typeComboX, y + 2, typeComboW, comboH);
  y += rowH + gap;

  // Row 1+2: MIDI input / output
  const int midiLabelX = mx;
  const int midiLabelW = 86;
  const int midiComboX = 100;
  const int midiComboW = 400;
  HWND lblIn = nthStatic(2);
  if (lblIn) SetWindowPos(lblIn, NULL, midiLabelX, y + 3, midiLabelW, lblH, SWP_NOZORDER | SWP_NOACTIVATE);
  move(IDC_COMBO2, midiComboX, y + 2, midiComboW, comboH);
  y += rowH + gap;

  HWND lblOut = nthStatic(3);
  if (lblOut) SetWindowPos(lblOut, NULL, midiLabelX, y + 3, midiLabelW, lblH, SWP_NOZORDER | SWP_NOACTIVATE);
  move(IDC_COMBO3, midiComboX, y + 2, midiComboW, comboH);
  y += rowH + gap + gap;

  // Checkboxes
  int chkW = W - 2 * mx;
  int checkH = 16;
  move(IDC_EMULATE_BLINKING,  mx, y, chkW, checkH); y += checkH;
  move(IDC_KEYBOARD_MODIFIER, mx, y, chkW, checkH); y += checkH;
  move(IDC_FAKE_TOUCH,        mx, y, chkW, checkH); y += checkH;
  move(IDC_CHECK2,            mx, y, W / 2, checkH);

  // Buttons at bottom
  int btnW = 90, btnH = 20;
  move(BTN_OPEN_MANUAL, W - 2 * (btnW + gap), H - btnH - mx, btnW, btnH);
  move(BTN_DONATE,      W - (btnW + mx),       H - btnH - mx, btnW, btnH);

  InvalidateRect(hwndDlg, NULL, TRUE);
}

// --- dialog procedure ---

static int s_dlgCurrentUnit = 0;

static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    SurfaceConfig *cfg = new SurfaceConfig(parseSurfaceConfig((const char *)lParam));
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)cfg);
    s_dlgCurrentUnit = 0;

    int nIn = GetNumMIDIInputs();
    int nOut = GetNumMIDIOutputs();

    HWND unitSel = GetDlgItem(hwndDlg, IDC_UNIT_SELECT);
    for (int i = 0; i < 8; i++) {
      char buf[16];
      sprintf(buf, "Unit %d", i + 1);
      SendMessage(unitSel, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendMessage(unitSel, CB_SETCURSEL, 0, 0);

    loadUnitIntoDialog(hwndDlg, cfg, 0, nIn, nOut);

    HWND proxChk = GetDlgItem(hwndDlg, IDC_PROX);
    if (proxChk) ShowWindow(proxChk, SW_HIDE);

    if (cfg->flags & CONFIG_FLAG_FADER_TOUCH_FAKE)
      CheckDlgButton(hwndDlg, IDC_FAKE_TOUCH, BST_CHECKED);
    if (cfg->flags & CONFIG_FLAG_SWAPZOOM)
      CheckDlgButton(hwndDlg, IDC_CHECK2, BST_CHECKED);
    if (cfg->flags & CONFIG_FLAG_EMULATING_BLINKING)
      CheckDlgButton(hwndDlg, IDC_EMULATE_BLINKING, BST_CHECKED);
    if (cfg->flags & CONFIG_FLAG_KEYBOARD_MODIFIER)
      CheckDlgButton(hwndDlg, IDC_KEYBOARD_MODIFIER, BST_CHECKED);

    RECT rc;
    GetClientRect(hwndDlg, &rc);
    if (rc.right < 268 || rc.bottom < 114) {
      SetWindowPos(hwndDlg, NULL, 0, 0, 268, 130, SWP_NOMOVE | SWP_NOZORDER);
    }

    layoutDlgControls(hwndDlg);
  } break;

  case WM_COMMAND: {
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_UNIT_SELECT) {
      SurfaceConfig *cfg = (SurfaceConfig *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
      if (cfg) {
        int nIn = GetNumMIDIInputs();
        int nOut = GetNumMIDIOutputs();
        saveUnitFromDialog(hwndDlg, cfg, s_dlgCurrentUnit);
        s_dlgCurrentUnit = currentUnitIndex(hwndDlg);
        loadUnitIntoDialog(hwndDlg, cfg, s_dlgCurrentUnit, nIn, nOut);
      }
    }
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_UNIT_TYPE) {
      LRESULT r = SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETCURSEL, 0, 0);
      int typeIdx = (r != CB_ERR)
          ? (int)SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETITEMDATA, r, 0)
          : -1;
      bool disabled = (typeIdx == UNIT_TYPE_DISABLED);
      ShowWindow(GetDlgItem(hwndDlg, IDC_COMBO2), disabled ? SW_HIDE : SW_SHOW);
      ShowWindow(GetDlgItem(hwndDlg, IDC_COMBO3), disabled ? SW_HIDE : SW_SHOW);
      if (!disabled) {
        SurfaceConfig *cfg = (SurfaceConfig *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
        if (cfg) {
          int nIn = GetNumMIDIInputs();
          int nOut = GetNumMIDIOutputs();
          const UnitConfig &u = cfg->units[s_dlgCurrentUnit];
          HWND inCb = GetDlgItem(hwndDlg, IDC_COMBO2);
          SendMessage(inCb, CB_RESETCONTENT, 0, 0);
          populateMidiCombo(inCb, nIn, GetMIDIInputName, u.midiInDev);
          HWND outCb = GetDlgItem(hwndDlg, IDC_COMBO3);
          SendMessage(outCb, CB_RESETCONTENT, 0, 0);
          populateMidiCombo(outCb, nOut, GetMIDIOutputName, u.midiOutDev);
        }
      }
    }
    switch (LOWORD(wParam)) {
    case BTN_DONATE:
      ShellExecute(NULL, "open",
                   "https://www.paypal.com/cgi-bin/"
                   "webscr?cmd=_s-xclick&hosted_button_id=LR54GZHGL6VHA",
                   NULL, NULL, SW_SHOWDEFAULT);
      break;
    case BTN_OPEN_MANUAL:
      ShellExecute(NULL, "open",
                   "https://bitbucket.org/"
                   "Klinkenstecker/csurf_klinke_mcu/downloads/mcu_klinke_manual.pdf",
                   NULL, NULL, SW_SHOWDEFAULT);
      break;
    }
  } break;

  case WM_SIZE:
    layoutDlgControls(hwndDlg);
    break;

  case WM_USER + 1024: {
    if (wParam > 1 && lParam) {
      SurfaceConfig *cfg = (SurfaceConfig *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
      if (!cfg) break;

      int cur = currentUnitIndex(hwndDlg);
      saveUnitFromDialog(hwndDlg, cfg, cur);

      cfg->flags = 0;
      if (IsDlgButtonChecked(hwndDlg, IDC_FAKE_TOUCH))
        cfg->flags |= CONFIG_FLAG_FADER_TOUCH_FAKE;
      if (IsDlgButtonChecked(hwndDlg, IDC_CHECK2))
        cfg->flags |= CONFIG_FLAG_SWAPZOOM;
      if (IsDlgButtonChecked(hwndDlg, IDC_EMULATE_BLINKING))
        cfg->flags |= CONFIG_FLAG_EMULATING_BLINKING;
      if (IsDlgButtonChecked(hwndDlg, IDC_KEYBOARD_MODIFIER))
        cfg->flags |= CONFIG_FLAG_KEYBOARD_MODIFIER;

      if (cfg->units[0].model == QConProX)
        cfg->flags |= CONFIG_FLAG_PROX;
      cfg->valid = true;

      // WP-EF-0a: reject non-dense unit topology (gaps not allowed).
      if (!hasDenseUnitTopology(*cfg)) {
        MessageBox(hwndDlg,
                   "Unit positions must be contiguous from Unit 1. "
                   "Disable Unit N only if all units after it are also disabled.",
                   "Invalid configuration", MB_OK | MB_ICONWARNING);
        break;
      }

      std::string s = serializeSurfaceConfig(*cfg);
      lstrcpyn((char *)lParam, s.c_str(), (int)wParam);

      delete cfg;
      SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
    }
  } break;

  case WM_DESTROY: {
    SurfaceConfig *cfg = (SurfaceConfig *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    delete cfg;
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
  } break;
  }
  return 0;
}

// --- configFunc + Reaper registration ---

static HWND configFunc(const char *type_string, HWND parent,
                       const char *initConfigString) {
  (void)type_string;
  HWND ret = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_MCUMAIN),
                               parent, dlgProc, (LPARAM)initConfigString);
  if (ret) ShowWindow(ret, SW_SHOW);
  return ret;
}

reaper_csurf_reg_t csurf_mcu_modified_reg = {
	MAIN_ID,
	"Mackie Control Protocol (Klinke)",
	createFunc,
	configFunc,
};
