/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * SurfaceConfig dialog — REAPER control-surface configuration UI.
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

// Per-unit checkbox ids and their UNIT_FLAG_* bits.
struct UnitFlagCheck { int ctrlId; int flag; };
static const UnitFlagCheck s_unitFlagChecks[] = {
  { IDC_METERS_ON_DISPLAY, UNIT_FLAG_METERS_ON_DISPLAY },
  { IDC_SWITCH_ROWS,       UNIT_FLAG_SWITCH_ROWS },
};
static const int s_unitFlagCheckCount =
    sizeof(s_unitFlagChecks) / sizeof(s_unitFlagChecks[0]);

static int unitFlagsFromDialog(HWND hwndDlg) {
  int flags = 0;
  for (int i = 0; i < s_unitFlagCheckCount; i++)
    if (IsDlgButtonChecked(hwndDlg, s_unitFlagChecks[i].ctrlId))
      flags |= s_unitFlagChecks[i].flag;
  return flags;
}

static void unitFlagsIntoDialog(HWND hwndDlg, int unitFlags, bool enabled) {
  for (int i = 0; i < s_unitFlagCheckCount; i++) {
    CheckDlgButton(hwndDlg, s_unitFlagChecks[i].ctrlId,
                   (enabled && (unitFlags & s_unitFlagChecks[i].flag))
                       ? BST_CHECKED : BST_UNCHECKED);
    HWND c = GetDlgItem(hwndDlg, s_unitFlagChecks[i].ctrlId);
    if (c) EnableWindow(c, enabled);
  }
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
  cfg->units[i] =
      unitConfigFromType(typeIdx, inDev, outDev, unitFlagsFromDialog(hwndDlg));
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

  unitFlagsIntoDialog(hwndDlg, u.unitFlags, !disabled);

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

  // validate dense unit topology.
  // A hand-edited or stale KLINKE2 string that is non-dense is logged and
  // replaced with the safe default before constructing CSurf_MCU.
  if (!hasDenseUnitTopology(cfg)) {
    MCU_LOG("createFunc: non-dense unit topology, replacing with default config");
    cfg = makeDefaultSurfaceConfig();
  }

  return new CSurf_MCU(cfg, errStats);
}

// --- layout ---

enum LayoutAnchor {
  LAYOUT_FIXED = 0,
  LAYOUT_STRETCH_RIGHT = 1,
  LAYOUT_MOVE_RIGHT = 2,
  LAYOUT_MOVE_DOWN = 4
};

struct LayoutControl {
  int id;
  int anchors;
};

static const LayoutControl s_layoutControls[] = {
  { IDC_UNIT_LABEL,        LAYOUT_FIXED },
  { IDC_UNIT_SELECT,       LAYOUT_FIXED },
  { IDC_UNIT_TYPE_LABEL,   LAYOUT_FIXED },
  { IDC_UNIT_TYPE,         LAYOUT_STRETCH_RIGHT },
  { IDC_MIDI_INPUT_LABEL,  LAYOUT_FIXED },
  { IDC_COMBO2,            LAYOUT_STRETCH_RIGHT },
  { IDC_MIDI_OUTPUT_LABEL, LAYOUT_FIXED },
  { IDC_COMBO3,            LAYOUT_STRETCH_RIGHT },
  { IDC_UNIT_GROUP,        LAYOUT_STRETCH_RIGHT },
  { IDC_METERS_ON_DISPLAY, LAYOUT_FIXED },
  { IDC_SWITCH_ROWS,       LAYOUT_FIXED },
  { IDC_GLOBAL_GROUP,      LAYOUT_STRETCH_RIGHT },
  { IDC_KEYBOARD_MODIFIER, LAYOUT_STRETCH_RIGHT },
  { IDC_CHECK2,            LAYOUT_STRETCH_RIGHT },
  { BTN_OPEN_MANUAL,       LAYOUT_MOVE_RIGHT | LAYOUT_MOVE_DOWN },
  { BTN_DONATE,            LAYOUT_MOVE_RIGHT | LAYOUT_MOVE_DOWN }
};

static const int s_layoutControlCount =
    sizeof(s_layoutControls) / sizeof(s_layoutControls[0]);

struct DialogState {
  explicit DialogState(const char *configString)
      : config(parseSurfaceConfig(configString)), currentUnit(0),
        layoutCaptured(false), initialWidth(0), initialHeight(0) {}

  SurfaceConfig config;
  int currentUnit;
  bool layoutCaptured;
  int initialWidth;
  int initialHeight;
  RECT initialRects[s_layoutControlCount];
};

static DialogState *getDialogState(HWND hwndDlg) {
  return (DialogState *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
}

static RECT controlRectInDialog(HWND hwndDlg, HWND control) {
  RECT rect;
  GetWindowRect(control, &rect);
  ScreenToClient(hwndDlg, (POINT *)&rect);
  ScreenToClient(hwndDlg, ((POINT *)&rect) + 1);

  RECT normalized;
  normalized.left = rect.left < rect.right ? rect.left : rect.right;
  normalized.right = rect.left < rect.right ? rect.right : rect.left;
  normalized.top = rect.top < rect.bottom ? rect.top : rect.bottom;
  normalized.bottom = rect.top < rect.bottom ? rect.bottom : rect.top;
  return normalized;
}

static void captureDialogLayout(HWND hwndDlg, DialogState *state) {
  RECT client;
  GetClientRect(hwndDlg, &client);
  state->initialWidth = client.right - client.left;
  state->initialHeight = client.bottom - client.top;

  for (int i = 0; i < s_layoutControlCount; i++) {
    HWND control = GetDlgItem(hwndDlg, s_layoutControls[i].id);
    if (control)
      state->initialRects[i] = controlRectInDialog(hwndDlg, control);
    else {
      state->initialRects[i].left = 0;
      state->initialRects[i].top = 0;
      state->initialRects[i].right = 0;
      state->initialRects[i].bottom = 0;
    }
  }
  state->layoutCaptured = true;
}

static void layoutDlgControls(HWND hwndDlg, DialogState *state) {
  if (!state || !state->layoutCaptured)
    return;

  RECT client;
  GetClientRect(hwndDlg, &client);
  int dx = (client.right - client.left) - state->initialWidth;
  int dy = (client.bottom - client.top) - state->initialHeight;

  for (int i = 0; i < s_layoutControlCount; i++) {
    const RECT &initial = state->initialRects[i];
    if (initial.right <= initial.left || initial.bottom <= initial.top)
      continue;

    int x = initial.left;
    int y = initial.top;
    int width = initial.right - initial.left;
    int height = initial.bottom - initial.top;
    int anchors = s_layoutControls[i].anchors;

    if (anchors & LAYOUT_STRETCH_RIGHT)
      width = width + dx > 40 ? width + dx : 40;
    if (anchors & LAYOUT_MOVE_RIGHT && dx > 0)
      x += dx;
    if (anchors & LAYOUT_MOVE_DOWN && dy > 0)
      y += dy;

    HWND control = GetDlgItem(hwndDlg, s_layoutControls[i].id);
    if (control)
      SetWindowPos(control, NULL, x, y, width, height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
  }

  InvalidateRect(hwndDlg, NULL, TRUE);
}

// --- dialog procedure ---

static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    DialogState *state = new DialogState((const char *)lParam);
    SurfaceConfig *cfg = &state->config;
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);

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

    // display meters / switch rows are per-unit and were
    // already applied by loadUnitIntoDialog() above.
    if (cfg->flags & CONFIG_FLAG_SWAPZOOM)
      CheckDlgButton(hwndDlg, IDC_CHECK2, BST_CHECKED);
    if (cfg->flags & CONFIG_FLAG_KEYBOARD_MODIFIER)
      CheckDlgButton(hwndDlg, IDC_KEYBOARD_MODIFIER, BST_CHECKED);

    // Show the loaded build; the private KLINKE build appends a "k" so it
    // is distinguishable from the public release in this dialog.
    HWND vLabel = GetDlgItem(hwndDlg, IDC_VERSION_LABEL);
    if (vLabel) {
      char vbuf[128];
#ifdef KLINKE
      sprintf(vbuf, "Version: " MCU_VERSION_STRING "k");
#else
      sprintf(vbuf, "Version: " MCU_VERSION_STRING);
#endif
      SetWindowText(vLabel, vbuf);
    }

    captureDialogLayout(hwndDlg, state);
  } break;

  case WM_COMMAND: {
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_UNIT_SELECT) {
      DialogState *state = getDialogState(hwndDlg);
      if (state) {
        int nIn = GetNumMIDIInputs();
        int nOut = GetNumMIDIOutputs();
        saveUnitFromDialog(hwndDlg, &state->config, state->currentUnit);
        state->currentUnit = currentUnitIndex(hwndDlg);
        loadUnitIntoDialog(hwndDlg, &state->config, state->currentUnit, nIn, nOut);
      }
    }
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_UNIT_TYPE) {
      LRESULT r = SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETCURSEL, 0, 0);
      int typeIdx = (r != CB_ERR)
          ? (int)SendDlgItemMessage(hwndDlg, IDC_UNIT_TYPE, CB_GETITEMDATA, r, 0)
          : -1;
      bool disabled = (typeIdx == UNIT_TYPE_DISABLED);
      for (int i = 0; i < s_unitFlagCheckCount; i++) {
        HWND c = GetDlgItem(hwndDlg, s_unitFlagChecks[i].ctrlId);
        if (c) EnableWindow(c, !disabled);
        if (disabled)
          CheckDlgButton(hwndDlg, s_unitFlagChecks[i].ctrlId, BST_UNCHECKED);
      }
      ShowWindow(GetDlgItem(hwndDlg, IDC_COMBO2), disabled ? SW_HIDE : SW_SHOW);
      ShowWindow(GetDlgItem(hwndDlg, IDC_COMBO3), disabled ? SW_HIDE : SW_SHOW);
      if (!disabled) {
        DialogState *state = getDialogState(hwndDlg);
        if (state) {
          int nIn = GetNumMIDIInputs();
          int nOut = GetNumMIDIOutputs();
          const UnitConfig &u = state->config.units[state->currentUnit];
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
    layoutDlgControls(hwndDlg, getDialogState(hwndDlg));
    break;

  case WM_USER + 1024: {
    if (wParam > 1 && lParam) {
      DialogState *state = getDialogState(hwndDlg);
      if (!state) break;
      SurfaceConfig *cfg = &state->config;

      int cur = currentUnitIndex(hwndDlg);
      saveUnitFromDialog(hwndDlg, cfg, cur);

      cfg->flags = 0;
      if (IsDlgButtonChecked(hwndDlg, IDC_CHECK2))
        cfg->flags |= CONFIG_FLAG_SWAPZOOM;
      if (IsDlgButtonChecked(hwndDlg, IDC_KEYBOARD_MODIFIER))
        cfg->flags |= CONFIG_FLAG_KEYBOARD_MODIFIER;

      cfg->valid = true;

      // reject non-dense unit topology (gaps not allowed).
      if (!hasDenseUnitTopology(*cfg)) {
        MessageBox(hwndDlg,
                   "Unit positions must be contiguous from Unit 1. "
                   "Disable Unit N only if all units after it are also disabled.",
                   "Invalid configuration", MB_OK | MB_ICONWARNING);
        break;
      }

      std::string s = serializeSurfaceConfig(*cfg);
      lstrcpyn((char *)lParam, s.c_str(), (int)wParam);

      delete state;
      SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
    }
  } break;

  case WM_DESTROY: {
    DialogState *state = getDialogState(hwndDlg);
    delete state;
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
