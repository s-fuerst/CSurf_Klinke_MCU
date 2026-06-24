# Directory Restructure Plan — Kritische Bewertung von Plan v1

> **Status:** Kritik / Review des ursprünglichen `directory-restructure-plan.md` (v1).
> **Datum:** 2026-06-24
> **Methode:** Plan v1 gegen den tatsächlichen Codebaum (137 Dateien, 366 Includes,
> aktuelle CMakeLists.txt, MEMD-Stand) geprüft — nicht gegen den veralteten
> Snapshot, auf dem v1 basiert.

---

## TL;DR

Plan v1 hat die richtige Grundidee (flat → nested), aber:

- **(a)** rechnet mit falschen Zahlen / veraltetem Baum,
- **(b)** wählt die teuerste mögliche Include-Strategie (366 manuelle Edits statt 0),
- **(c)** kommt zum schlechtestmöglichen Zeitpunkt (aktive Portierung + Upstream-Merges),
- **(d)** brickt potenziell den einzigen funktionierenden Windows-Build-Pfad (`.vcxproj`),
- **(e)** kategorisiert teils willkürlich und kohäsionsbrechend.

---

## 1. Tatsachenfehler / veralteter Baum

| Behauptung in v1 | Realität (Stand 2026-06-24) |
|---|---|
| „67 `.cpp`/`.h` files in repo root" | **137** Dateien (66 `.cpp` + 71 `.h`). Die „67" ist exakt die `.cpp`-Zahl — `.cpp`-Count wurde mit Gesamtcount verwechselt. |
| `windows.h` künftig bei `src/windows.h` | Falsch. Am 2026-06-22 nach `posix_shims/windows.h` verschoben (MEMD). Plan ist **stale**. |
| „~200 includes need relative paths" | Tatsächlich **366** `#include "..."`-Direktiven auf **81** distinct Projekt-Header. Um ~80 % unterschätzt. |
| `res.rc_mac_dlg` / `res.rc_mac_menu` | Kommen im Plan **gar nicht vor**, obwohl beide committed sind und von `csurf_main.cpp` / `res_linux.cpp` benutzt werden. |
| Mapping-Tabelle „Total 132" | Eigener Baum listet 132; es sind 137 Dateien. Die Lücke (SDK-Header im Root) wird nirgends reconciled. |

→ **v1 wurde nicht gegen den aktuellen Baum validiert**, sondern an einem veralteten Snapshot geschrieben.

---

## 2. Der größte technische Punkt: teuerste Include-Strategie gewählt

v1 schlägt vor, **alle** Includes umzuschreiben:
```cpp
#include "csurf_mcu.h"   →   #include "core/csurf_mcu.h"
```
`csurf_mcu.h` allein wird **43×** per Flat-Name eingebunden. Über **81** Header sind das **366 manuelle, fehleranfällige Edits** — für **null funktionalen Gewinn**.

### Sauberer CMake-Weg: Subdirs auf den Include-Pfad nehmen

```cmake
file(GLOB_RECURSE SRC_DIRS LIST_DIRECTORIES true src/*/)
target_include_directories(reaper_csurf_mcu_klinke PRIVATE ${SRC_DIRS})
```
Dann funktioniert `#include "csurf_mcu.h"` **unverändert** weiter. Die
Verzeichnisstruktur existiert weiterhin für die menschliche Navigation, aber
das Build zwingt niemanden zu Pfad-Rewrites.

**Vorteile:**
- Späteres Verschieben einer Datei bricht keine Includes mehr.
- Bei pfadbasierten Includes müsste man bei jedem `git mv` wieder alle Includer anfassen.
- Aufwand: 1 CMakeLists-Zeile + reines `git mv` statt 366 Edits.

→ **Das ist *der* Verbesserungspunkt**, der den Aufwand von „riesige Mechanical-Change-Orgie"
auf „CMakeLists-Einzeiler + `git mv`" reduziert.

---

## 3. Risiko / Timing: der schlechtestmögliche Zeitpunkt

- Plan v1 datiert auf **2026-06-22** — exakt der Tag mit dem massivsten
  Porting-Aufwand (Linux-Baseline gerade eben lauffähig, 10 Rothchild-Bugfixes
  frisch integriert, JUCE-ComboBox-Fix, a2jmidid-LED-Fixes).
- Aktive **Upstream-Integration**: `origin/master` commits ed25356 + 5234947
  am 2026-06-23 gemerged (MEMD). Ein 137-Dateien-`git mv` erzeugt
  **katastrophale Merge-Konflikte** mit jedem künftigen Upstream-Pull.
- AGENTS.md §4: der **Windows-CMake-Zweig ist noch ein `FATAL_ERROR`-Stub**;
  die `.vcxproj` ist dort „source of truth". v1 listet „`.vcxproj` aktualisieren"
  als routinehafte Checkbox — tatsächlich brickt die Aktion den **einzigen**
  funktionierenden Windows-Build-Pfad, solange der Windows-CMake-Zweig fehlt.
- Aktuell steht ein **uncommitted Change** an (`ButtonManager.cpp modified`).
  Restructure auf einem nicht-cleanen Tree = Chaos.

→ **Restructure erst laufen lassen, wenn CMake auf allen 3 Plattformen
source-of-truth ist** (insb. Windows). Vorher ist es pure zusätzliche
Bruchfläche quer durch die noch unfertigen Plattformen.

---

## 4. Kategorisierung teils willkürlich / kohäsionsbrechend

- **`state/`** = Grab-Bag: `Tracks`, `Transport`, `Region`, `Options`,
  `ProjectConfig`, `UndoEnd`, `VPOT_LED`. „Region" (Loop/Time-Selection-Speicher)
  und „Transport" sind keine „State" im engeren Sinn; `UndoEnd` ist ein
  Winz-Helper. Der Bucketname erzwingt keine Erkenntnis.
- **`display/`** mischt generisches `Display`/`DisplayHandler` mit
  `ActionsDisplay`/`ActionsDialogComponent` — letztere gehören **logisch zu
  CommandMode**. Das splittet ein kohäsives Feature über `modes/commands/`
  und `display/`.
- **`meter/`** reißt die `MeterBridge`-Subklassen von ihren Moden ab — aber
  `PlugModeMeterBridge.cpp` includiert `PlugMode.h` + `PlugAccess.h`, ist also
  fest an den PlugMode gekoppelt. Separater Ordner erzeugt nur
  Cross-Dir-Abhängigkeit ohne Entkopplung.
- **Asymmetrie bei `editor/`**: PlugMode kriegt einen `editor/`-Subordner für
  seine `*Component`-Dateien, **CommandMode nicht** — obwohl beide einen
  parallelen Satz Editor-Components haben (`CommandModeMainComponent`,
  `CommandModePageComponent`, `CommandModeVPOTComponent`). Inkonsistent.

→ Entweder **jeder** Modus mit Editor-Components kriegt `editor/`, oder keiner.
Und feature-zusammenhängende Dateien **zusammen** lassen (`ActionsDisplay` →
CommandMode; MeterBridge-Subklassen → jeweiliger Modus).

---

## 5. `src/`-Root ist ein Mix aus Unzusammenhängendem

`csurf_main.cpp`, `res_linux.cpp`, `McuDebugLog.h` (fälschlich `windows.h`)
auf einer Ebene — das sind Entry-Point, Plattform-Ressource und ein
projektweit (5×) eingebundenes Logging-Makro. `McuDebugLog.h` wäre in
`core/` oder `util/` besser aufgehoben als neben `csurf_main.cpp`.

Zudem: `Assert.h`, `std_helper.h`, `mcu_button_defines.h` bleiben laut v1 im
**Repo-Root** (nicht in `src/`), werden aber von `src/`-Dateien per Flat-Name
eingebunden. Damit bleiben **zwei parallele Include-Roots** bestehen
(Repo-Root + `src/*`) — funktionsfähig, aber v1 dokumentiert das nirgends
und verschweigt, dass der Repo-Root weiterhin auf dem Include-Pfad bleiben muss.

---

## Verbesserungsvorschläge (konkret, ausführbar)

1. **Include-Strategie umstellen:** `target_include_directories` mit allen
   `src/`-Subdirs statt Pfad-Rewrites → **366 Edits auf 0**.
2. **Timing:** verschieben, bis Windows-CMake-Zweig steht und der Tree
   clean + upstream-merged ist. Sonst Brick-Risiko + Merge-Hölle.
3. **Tatsachen korrigieren:** 137 Dateien, `windows.h` in `posix_shims/`,
   366 Includes, `res.rc_mac_dlg/_menu` einplanen.
4. **Kategorien konsistent machen:** `editor/` für alle Modi ODER keinen;
   feature-zusammenhängende Dateien beisammen lassen (`ActionsDisplay` →
   CommandMode, MeterBridge-Subklassen → Modus).
5. **`state/` auflösen** oder ehrlicher benennen (`transport/`, `config/`,
   `core/`) — kein Grab-Bag.
6. **Pro-Verzeichnis-CMake:** statt einer 65-Zeilen-Quellliste lieber
   `add_subdirectory(src/...)` mit lokalen `CMakeLists.txt`-Fragmenten +
   `target_sources(... PRIVATE ...)`. Skaliert besser und macht
   Verschiebungen lokal.
7. **Als einzelnen, wohlabgesicherten mechanischen Commit** ausführen
   (`git mv` + glob-basierte CMakeLists + grüner Build), nicht mitten im Port.
