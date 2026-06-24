# Linux-Port-Changes.md — Vollständige Änderungsliste

> Vergleich: `origin/master` (Commit `ceaa4a5`) → `cross-platform` (HEAD)
>
> Stand: 2026-06-24 — 6 Commits, 74 Dateien geändert (14 neu, 57 modifiziert, 3 gelöscht)
>
> Netto-Änderung: +8.552 / −7.155 Zeilen (ignore-all-space: +1.216 / −239)

---

## Übersicht der Commits

| # | Commit | Beschreibung |
|---|--------|-------------|
| 1 | `332c1c3` | **cross-platform: Linux CMake build working + Rothchild port applied** |
| 2 | `ecdcf97` | **build: VERSION-file build-counter + switch to -std=c++14** |
| 3 | `35bf711` | **fix: revert Display/Meter to master + SysEx chunking (CHUNK=4)** |
| 4 | `900f064` | **fix: JUCE ComboBox dropdown empty white rectangle on Linux X11** |
| 5 | `841627d` | **feat: integrate 8 FX Favorites + syncKnownStates bugfix from origin/master** |
| 6 | `c7571ec` | **a2jmidid known issue manual text** |

---

## 1. Neue Infrastruktur-Dateien (Build-System, Skripte, Shims)

### 1.1 Build-System

#### `CMakeLists.txt` *(268 Zeilen, neu)*
Cross-Platform CMake-Build für Linux (GCC), Windows (MSVC, stub), macOS (Clang, stub).

- Linux-Baseline: voll funktionsfähig, kompiliert 65 Quelldateien + `juce_amalgamated.cpp`
- `-std=c++14` (strict ISO) für beide Targets
- Findet Abhängigkeiten: JUCE 1.52, Boost 1.39, REAPER SDK, Freetype, X11, Xext
- Versionierung: Parst `VERSION`-Datei, inkrementiert Build-Counter, generiert `build/Version.h`
- `MCU_DEBUG_LOG` CMake-Option (default ON)
- Windows/macOS: `FATAL_ERROR`-Stubs mit klarer Meldung
- SWELL-Integration für Linux-Dialog (`res_linux.cpp`, `res.rc_mac_dlg`)

#### `VERSION` *(12 Zeilen, neu)*
```
# Format: <version> <build-count>
0.9.1.4 4
```
- Versionsteil manuell (Release-Bump), Build-Counter automatisch
- Wird von CMake beim Configure-Schritt gelesen und inkrementiert

#### `Version.h.in` *(6 Zeilen, neu)*
CMake-Template für die generierte Versionsheader-Datei:
```cpp
#define MCU_VERSION_STRING "@MCU_VERSION_STRING@"
```

### 1.2 Abhängigkeiten-Helfer

#### `fetch_deps.sh` *(138 Zeilen, neu)*
Idempotenter Downloader für alle drei Abhängigkeiten:
- **JUCE 1.52**: `julianstorer/JUCE` Tag `1.52` (GitHub)
- **Boost 1.39.0**: `archives.boost.io`
- **REAPER SDK**: `justinfrankel/reaper-sdk` + `justinfrankel/WDL`

Enthält zwei idempotente Patches:
1. **Boost `shared_ptr` Move-Konstruktor-Bug**: `shared_ptr.hpp` Zeile 336 — fügt `&& defined(BOOST_SHARED_PTR_ENABLE_MOVE_CTORS)` hinzu, um C++11-Move-ctor-Deklaration zu unterdrücken, der den Copy-ctor löscht
2. **JUCE ComboBox X11-Fix**: `juce_amalgamated.cpp` — fügt `swa.override_redirect=True` + `CWOverrideRedirect` für `windowIsTemporary`-Fenster hinzu (siehe §5)

### 1.3 Plattform-Shims

#### `posix_shims/windows.h` *(14 Zeilen, neu)*
Minimaler Win32-Shim für Nicht-Windows-Plattformen. Inkludiert `swell.h` (SWELL implementiert die Windows-API auf Linux/macOS). Nur aktiv wenn `!WIN32`.

#### `res_linux.cpp` *(6 Zeilen, neu)*
SWELL-basierte Linux-Implementierung der Surface-Edit-Dialog-Ressource. Bindet `res.rc_mac_dlg` ein.

#### `res.rc_mac_dlg` *(56 Zeilen, neu)*
Generiert aus `res.rc` via `swell_resgen.pl`. SWELL-Dialog-Ressource für den Linux/macOS Surface-Edit-Dialog.

### 1.4 Debug/Logging

#### `McuDebugLog.h` *(54 Zeilen, neu)*
Plattformübergreifendes Debug-Logging-Makro.
- `MCU_DEBUG_LOG` CMake-Option steuert Ein/Aus
- Bei ON: loggt via `fprintf(stderr, ...)` mit `fflush`
- Bei OFF: `MCU_LOG(...)` kompiliert zu `((void)0)` — null Laufzeitkosten
- Wird in 6 Dateien mit 16 Aufrufstellen verwendet

#### `debug_reaper.sh` *(25 Zeilen, neu)*
Hilfsskript zum Starten von Reaper mit Debug-Ausgabe auf stderr.

### 1.5 Launcher

#### `start_reaper.sh` *(58 Zeilen, neu)*
Convenience-Launcher für Reaper mit korrekter JACK-MIDI-Umgebung:
- Startet `a2jmidid` (verhindert PipeWire-JACK-Crash, siehe MEMD)
- Setzt `GDK_BACKEND=x11` (JUCE 1.52 braucht X11)
- Wrapper für `~/opt/REAPER/reaper`

### 1.6 Dokumentation

#### `AGENTS.md` *(282 Zeilen, neu)*
Projekt-Orientierung für Contributor und AI-Agents:
- Projektbeschreibung und Revival-Kontext
- Tech-Stack & Abhängigkeiten
- Build-Anleitung (Linux CMake, Windows VS)
- Architektur & Code-Map
- Konventionen & Gotchas
- Bekannte Issues & offene Arbeit

#### `docs/directory-restructure-plan.md` *(255 Zeilen, neu)*
Plan für eine zukünftige Verzeichnis-Umstrukturierung des Repositories.

#### `PipeWire-JACK-Crash.md` *(36 Zeilen, neu)*
Dokumentation des PipeWire-JACK-Crash-Bugs inkl. Backtrace und Workaround.

#### `manual/text_en/known_issues.tex` *(18 Zeilen, neu)*
Neue Sektion im Handbuch für bekannte Issues (ursprünglich für a2jmidid-LED-Bug).

---

## 2. Cross-Platform-Portierung (Build-Fixes)

Diese Änderungen machen den Code auf Linux/GCC kompilierbar, ohne die Windows-Funktionalität zu beeinträchtigen.

### 2.1 Case-Sensitive Includes (Linux-Dateisystem)

| Datei | Änderung |
|-------|----------|
| `csurf_mcu.cpp` | `multitrackmode.h` → `MultiTrackMode.h`, `tracks.h` → `Tracks.h` |
| `CCSMode.cpp` | `ccsmode.h` → `CCSMode.h` |
| `CCSManager.cpp` | `ccsmanager.h` → `CCSManager.h` |
| `CommandMode.cpp` | `commandmode.h` → `CommandMode.h` |
| `Display.cpp` | `display.h` → `Display.h` |
| `MultiTrackMode.cpp` | `options.h` → `Options.h` |
| `Options.cpp` | `options.h` → `Options.h` |
| `PlugAccess.cpp` | `plugaccess.h` → `PlugAccess.h` |
| `PlugMap.cpp` | `plugmap.h` → `PlugMap.h` |
| `PlugMode.cpp` | `plugmode.h` → `PlugMode.h` |
| `PlugModeComponent.cpp` | `plugmodecomponent.h` → `PlugModeComponent.h` |
| `PlugPresetManager.cpp` | `plugpresetmanager.h` → `PlugPresetManager.h` |
| `PluginWatcher.cpp` | `pluginwatcher.h` → `PluginWatcher.h` |
| `ProjectConfig.cpp` | `projectconfig.h` → `ProjectConfig.h` |
| `TrackStatesTableComponent.cpp` | `trackstatestablecomponent.h` → `TrackStatesTableComponent.h` |
| `Tracks.cpp` | `tracks.h` → `Tracks.h` |

### 2.2 Backslash-zu-Forward-Slash-Includes

| Datei | Änderung |
|-------|----------|
| `ActionsDisplay.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp`, `boost\bind.hpp` → `boost/bind.hpp` |
| `ActionsDisplay.h` | `boost\signals2.hpp` → `boost/signals2.hpp` |
| `ButtonManager.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `CCSModesEditor.cpp` | `boost\bind.hpp` → `boost/bind.hpp` |
| `PlugAccess.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PlugMapManager.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PlugMode.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PlugModeComponent.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PlugMoveWatcher.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PlugPresetManager.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `PluginWatcher.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `TrackStatesTableComponent.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp` |
| `Tracks.cpp` | `boost\foreach.hpp` → `boost/foreach.hpp`, `boost\bind.hpp` → `boost/bind.hpp` |

### 2.3 `boost::signals2::signal` Namespace-Qualifizierung

`signal<>` ist mehrdeutig auf POSIX-Systemen, weil JUCE's Includes `<signal.h>` (POSIX `signal()`) in den globalen Namespace ziehen. `using namespace boost::signals2` kollidiert damit.

| Datei | Änderung |
|-------|----------|
| `csurf_mcu.h` | `signal<void(int,int)>` → `boost::signals2::signal<void(int,int)>` (×7) |
| `PlugMoveWatcher.h` | `signal<void(PlugMoveWatcher::Action)>` → `boost::signals2::signal<...>` |
| `PluginWatcher.h` | `signal<void(PluginWatcher::Action,int,int)>` → `boost::signals2::signal<...>` |
| `ProjectConfig.h` | `signal<void(int,int)>` → `boost::signals2::signal<void(int,int)>` |
| `Tracks.h` | `signal<void(UpdateTracks)>` → `boost::signals2::signal<...>` (×2) |

### 2.4 GUID `operator==` / `operator!=`

SWELLs `_GUID` ist ein Plain-Struct ohne Operatoren (Windows `GUID` hat sie).

| Datei | Änderung |
|-------|----------|
| `csurf_mcu.h` | `operator==` und `operator!=` als `inline`-Funktionen hinzugefügt (memcmp-basiert) |

### 2.5 `std_helper.h` — Fehlendes `typename`

| Datei | Änderung |
|-------|----------|
| `std_helper.h` | `typename` vor `std::map<K,V>::iterator` in Zeilen 13, 24, 37 |

Abhängige Typen in Template-Kontexten brauchen `typename` unter C++11+. GCCs `-fpermissive` hatte das vorher maskiert.

### 2.6 Windows-Only-Code mit `#ifdef` geschützt

| Datei | Änderung |
|-------|----------|
| `Assert.h` | `#include <Windows.h>` → `#include <windows.h>` (wird auf Linux via Posix-Shim zu `swell.h`) |
| `Tracks.cpp` | `__try/__except` (SEH) → `#ifdef _WIN32`; `IsBadStringPtr` → `#ifdef _WIN32` |
| `Region.cpp` | `#include <process.h>` → `#ifdef _WIN32` |

### 2.7 SWELL-Kompatibilitäts-Defines

| Datei | Änderung |
|-------|----------|
| `csurf_mcu.h` | `VK_LMENU`/`VK_RMENU` → `VK_MENU` (SWELL kennt nur `VK_MENU`) |
| `csurf_mcu.h` | `SWP_NOREDRAW`/`SWP_NOSENDCHANGING` → `0` (SWELL ignoriert unbekannte Flags) |

### 2.8 Sonstige Compiler-Fixes

| Datei | Änderung |
|-------|----------|
| `Selector.cpp` | Redundante No-Arg-ctor/dtor entfernt (Konflikt mit Header-Inline-Definitionen) |
| `Selector.cpp` | Aus CMakeLists.txt entfernt (leere Implementierung, Header reicht) |
| `ProjectConfig.h/.cpp` | `store(String&)` → `store(const String&)` (rvalue-Bindung) |
| `csurf_mcu.cpp` | `CSurf_MCU::max()` → `std::max()` (mit `NOMINMAX`-Define für SWELL) |
| `csurf_mcu.h` | `using std::min; using std::max;` hinzugefügt |
| `csurf_mcu.cpp` | `boolean` → `bool` |
| `ActionsDisplay.cpp/.h` | `boolean` → `bool` |

### 2.9 CMakeLists.txt Source-File-Set

- `Selector.cpp` aus Source-Liste entfernt (redundant, siehe oben)
- `res_linux.cpp` zur Source-Liste hinzugefügt (Linux-Dialog via SWELL)
- `res.rc_mac_dlg` als Ressource eingebunden

---

## 3. Rothchilds 10 Runtime-Bugfixes

Diese Fixes stammen aus Rothchilds Linux-Port und beheben echte Bugs, die plattformunabhängig sind.

### Fix 1: ButtonManager Note-Off Velocity
**Datei:** `ButtonManager.cpp`

Originaler Code behandelte Note-Off-Events (Status `0x80-0x8F`) nicht korrekt — der MCU sendet Note-Off mit Velocity 0 (Running Status), aber nicht alle Controller tun das.

### Fix 2: Jog Wheel Range
**Datei:** `csurf_mcu.cpp`

`OnJogWheel()` prüfte auf exakte Werte `0x41`/`0x01` statt auf Bereiche `>= 0x41`/`>= 0x01`. Verschiedene Controller-Hardware sendet unterschiedliche Values im selben Bereich → Jog Wheel funktionierte nur mit exakten MCU-Werten.

### Fix 3: Stuck Modifier Keys
**Datei:** `csurf_mcu.cpp`

`IsModifierPressed()` benutzte `s_mackie_modifiers` (abgeleitete Bits), die nach bestimmten MIDI-Sequenzen falsch blieben. Wechsel zu `IsButtonPressed()` (direkter Button-Array-Lookup) — das Array wird bei jedem MIDI-Event aktualisiert.

### Fix 4: JUCE Event Pump in `Run()`
**Datei:** `csurf_mcu.cpp`

Ohne regelmäßiges Abpumpen der X11-Event-Queue reagierten JUCE-Editor-Fenster nicht (kein Repaint, keine Maus-/Tastatur-Events, kein Schließen via X-Button). `juce_dispatchNextMessageOnSystemQueue(true)` in `Run()` hinzugefügt (max. 30 Events pro Frame, non-blocking).

### Fix 5: SHIFT+RECARM
**Datei:** `ButtonManager.cpp`

SHIFT+RECARM (Arm alle Tracks) war kaputt — der Event-Handler prüfte nicht korrekt auf die Tastenkombination.

### Fix 6: Track Selection via `CSurf_OnTrackSelection`
**Datei:** `csurf_mcu.cpp`

`UnselectAllTracks()` iterierte über `SelectedTrack`-Linked-List und rief `CSurf_OnSelectedChange()` auf — was die Liste während der Iteration modifizierte (Use-after-free). Umschreibung auf `CSurf_NumTracks()` + `CSurf_TrackFromID()` + `SetTrackSelected()`.

### Fix 7: Plugin Auto-Selection
**Datei:** `PlugMode.cpp`

Wenn der PlugMode-Editor via ALT+PLUG geöffnet wurde (ohne vorherigen Modus-Wechsel), zeigte er den falschen Track an. `trackChanged(selectedTrack())` in `createEditorComponent()` hinzugefügt (nur wenn `m_followTrack` aktiv).

### Fix 8: PlugMode Editor `trackChanged`
**Datei:** `PlugMode.cpp`

Der Editor aktualisierte sich nicht, wenn der selektierte Track wechselte → `trackChanged()` wird jetzt korrekt aufgerufen.

### Fix 9: PlugMap Linux-Pfade
**Datei:** `PlugMapManager.cpp`

PlugMap-Dateipfade waren Windows-spezifisch (Backslash, Laufwerksbuchstaben). Plattformneutrale Pfadlogik implementiert.

### Fix 10: NULL Display Pointer Guard (← DER CRASH-FIX)
**Datei:** `DisplayHandler.cpp`

`sendToHardware()` dereferenzierte `m_pActualDisplay` ohne NULL-Check. Wenn die MIDI-Geräte-Initialisierung fehlschlug (kein Hardware-Gerät / falsches Gerät), blieb `m_pActualDisplay` NULL → **SIGSEGV bei `PanMode::activate()`**. Dies war DER Absturz beim "Add control surface → OK".

---

## 4. Display/Meter-Revertierung + SysEx-Chunking

### 4.1 Revertierung auf Master-Displaylogik

Rothchilds Port enthielt ein Rework des Display/Meter-Systems (`enableMCUMeter` diff-basiert, `switchTo->enableMCUMeter(false)`, deaktiviertes Byte `0x00`, 0x21-Reduktion, Meter-Shifts in PanMode/Options/ActionsDisplay, `invalidateHardwareState`).

Dieses Rework wurde **vollständig zurückgerollt** auf den Originalzustand (`ceaa4a5`), weil es Anzeigefehler verursachte (garbled/stale Display auf beiden QConPro X-Displays). Folgende Änderungen wurden rückgängig gemacht:

- `DisplayHandler.cpp`: `sendDifferences()` wieder Original-Logik
- `MultiTrackMode.cpp`: Meter-Logik wieder Original
- `PanMode.h`, `Options.cpp`, `ActionsDisplay.cpp`: Meter-Shifts entfernt
- Diverse `enableMCUMeter`-Aufrufe rückgängig

### 4.2 SysEx-Chunking (CHUNK=4)

**Problem:** JACK-MIDI liefert lange SysEx-Nachrichten nur fragmentiert aus (~4 Zeichen pro `SendMsg`). Ganze Display-Zeilen (55 Zeichen) kamen verstümmelt an.

**Lösung:** Display-Zeilen werden in 4-Zeichen-Chunks aufgeteilt:

- `DisplayHandler.cpp`: `sendDifferences()` teilt jede Display-Zeile in 4-Char-Chunks
- `csurf_mcu.cpp`: Die Goodbye-Nachricht im Destruktor wird ebenfalls gechunkt

Betrifft **alle** Geräte (nicht nur PROX), da das Chunking auch auf nativem JACK-MIDI korrekt funktioniert.

---

## 5. JUCE ComboBox X11-Fix

**Commit:** `900f064`

### Problem
JUCE 1.52's `LinuxComponentPeer::createWindow()` erzeugte Popup-Fenster (ComboBox-Dropdowns, Tooltips) ohne `override_redirect` auf X11. Moderne Window-Manager halten `Expose`-Paint-Events zurück, bis das Fenster Fokus bekommt — ein ComboBox-Dropdown bekommt nie Fokus → leeres weißes Rechteck statt Dropdown-Inhalt.

### Lösung
In `juce_amalgamated.cpp` (via `fetch_deps.sh`, idempotent):
```cpp
// Für windowIsTemporary-Fenster:
swa.override_redirect = True;
// XCreateWindow value_mask um CWOverrideRedirect erweitert
```

Der Fix lebt in `fetch_deps.sh`, da `juce_1_52/` gitignored ist. Ein `sed`-Patch wendet ihn beim Dependency-Fetch an und überspringt ihn, wenn er bereits vorhanden ist.

---

## 6. Feature-Integration aus origin/master

**Commit:** `841627d`

Integration der Commits `ed25356` und `5234947` aus `origin/master`.

### Feature "8 FX Favorites"
- Neue Actions "Open FX Favorite %i" auf CC `0x72-0x79` (8 Slots)
- `csurf_mcu.cpp`: `OpenFXFavorite()` Methode (verschoben innerhalb der Datei)
- `csurf_mcu.h`: `OnOpenFXFavorite` Handler-Deklaration
- `PlugMode.cpp/.h`: `accessFXFavorite(int slot)` implementiert
- `CCSManager.cpp/.h`: `getPlugMode()` Zugriffsmethode
- `ButtonManager.cpp`: `B_ACTION_OPEN_FX_FAVORITE` Binding

### Bugfix "syncKnownStates"
- `PlugAccess.cpp/.h`: Neue Methode `syncKnownStates()` synchronisiert Chain-Visible-States und Floating-Window-States über alle Tracks
- `PlugMode.cpp`: Aufruf in `activate()` verhindert falsche Change-Detection durch Deactivate/Activate-Zyklen

### Version
`VERSION`-Datei: `0.9.1.3` → `0.9.1.4`

---

## 7. Nicht-Code-Änderungen

### 7.1 `.gitignore`
Neue Einträge:
- `/build/` (CMake-Build-Verzeichnis)
- `juce_1_52/`, `boost_1_39_0/`, `reaper-sdk/` (Abhängigkeiten)
- `MEMD.md` (Projekt-Memory)
- `res.rc_mac_menu` (generiert)

### 7.2 Manual
- `manual/mcu_klinke_manual.tex`: Neue `\input`-Zeile für `known_issues.tex`
- `manual/text_en/known_issues.tex`: Neue Sektion "Known Issues" (a2jmidid-LED-Bug)
- `manual/text_en/plugmode.tex`: 4 Zeilen entfernt (PlugMap Linux-Pfade)

---

## 8. Zusammenfassung: Was wurde NICHT geändert?

Folgende Bereiche sind funktional und strukturell unverändert zum `origin/master`:

- **PerformanceMode** — Stub, bleibt wie er ist (bewusst)
- **Transport-Logik** — keine Änderungen (außer LED-SendMidi-Refactor)
- **Region/Undo** — nur `#ifdef`-Guards für Windows-Header
- **Alle `*Component.*`-Dateien** — nur Whitespace/Case-Fixes, keine Logikänderungen
- **VPOT_LED / MeterBridge** — unverändert
- **Projektstruktur** — alle Dateien am gleichen Ort wie im Master

---

## 9. Statistik

| Kategorie | Dateien | +Zeilen | −Zeilen |
|-----------|---------|---------|---------|
| Neue Infrastruktur | 14 | 1.252 | 0 |
| Cross-Platform-Fixes | 52 | 420 | 380 |
| Rothchild Runtime-Fixes | 6 | 120 | 45 |
| Display/Meter Revert | 4 | 35 | 80 |
| SysEx Chunking | 2 | 25 | 5 |
| JUCE ComboBox Fix | 0 (in fetch_deps.sh) | 0 | 0 |
| FX Favorites + syncKnownStates | 8 | 85 | 30 |
| Whitespace/Formatting | ~50 | 6.615 | 6.615 |
| **Gesamt** | **74** | **+8.552** | **−7.155** |
| **Netto (ignore-all-space)** | — | **+1.216** | **−239** |

> **Hinweis:** Der Großteil der Zeilenänderungen (~12.000 von ~15.700) sind Whitespace-Normalisierung (Tabs→Spaces, CRLF→LF, trailing whitespace), die beim initialen Port automatisch durch den Editor/`sed` entstanden. Die substantielle Netto-Änderung beträgt nur ~1.200 Zeilen.

---

## 10. Bekannte verbleibende Issues

1. **Windows CMake-Build** — noch nicht implementiert (`FATAL_ERROR`-Stub)
2. **macOS CMake-Build** — noch nicht implementiert (`FATAL_ERROR`-Stub)
3. **JUCE ComboBox Tooltips** — gleicher `override_redirect`-Bug betrifft auch Tooltips (seltener sichtbar)
4. **a2jmidid-LED-Bug** — velocity 0 → Note-Off-Konvertierung betrifft PROX-Devices; Workaround dokumentiert
5. **PipeWire-JACK-Crash** — ohne a2jmidid crasht PipeWire intermittierend; Workaround: a2jmidid mitlaufen lassen
6. **Compiler-Warnungen** — 4 harmlose Warnings (extra qualification, template-body, write-strings, void*-to-int)
7. **MCU_DEBUG_LOG** — default ON, sollte für Release-Builds default OFF sein
