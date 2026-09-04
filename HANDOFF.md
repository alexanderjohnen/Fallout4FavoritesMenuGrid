# Favorites Menu Grid (Fallout 4) — Arbeitsstand

Stand: 2026-09-04. Portierung von
[Favorites Menu Grid für Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid).
**Abschnitt 0 ist der Einstieg** — dort steht, was gilt und was offen ist.

---

## 0. Aktueller Stand

**Meilenstein 0 steht: ein Plugin, das nichts verändert.** Es liest die zwölf
Favoritenplätze, protokolliert jedes Öffnen und Schließen eines Menüs und
versucht ein Rechteck in die Bühne von `HUDMenu` zu zeichnen. Mehr nicht.
Der Zweck ist, drei Fragen zu beantworten, die in keinem Header stehen —
siehe Abschnitt 2.

**Erster Lauf im Spiel: 2026-09-04, 21:19 Uhr.** Zwei der drei Fragen aus
Abschnitt 2 sind beantwortet:

- **Die Bühne nimmt einen eigenen Sprite an.** `sprite added to the stage of
  HUDMenu`, danach `rectangle drawn` — und das Rechteck war im Spiel zu
  sehen. Der Port braucht **keine ersetzte Interface-Datei**. Der Sprite
  überlebt auch weitere Menüereignisse (`the sprite from a previous probe is
  still there`).
- **Fallout 4 hat ein eigenes `FavoritesMenu`.** Das war die Korrektur des
  Tages: Die Annahme, das Kreuz stecke in `HUDMenu`, stammte daher, dass in
  `Data\Interface` keine `FavoritesMenu.swf` liegt — sie steckt in einem BA2.
  Im Log öffnet und schließt `FavoritesMenu` sauber als eigenes Menü. Das
  Grid gehört dorthin, nicht in den HUD.
- **Offen bleibt Frage 1.** Der Singleton löst stabil auf
  (`0x7ff7ab5fc3e0`, über den ganzen Lauf derselbe), aber alle zwölf Plätze
  waren leer und `weaponLoadedAmmo` durchgehend `-1`. Das ist mit einem
  Spielstand ohne gesetzte Favoriten verträglich, beweist das Layout aber
  nicht. **Nächster Test: einen Favoriten setzen und nachsehen, ob der Platz
  im Log auftaucht.**

**Zweiter Lauf, 23:43 Uhr — die Oberflaechenfrage ist ganz beantwortet.**
`FavoritesMenu` nimmt den Sprite ebenfalls an, seine Buehne ist 1280 x 720,
und `menuObj` hat **`ProcessUserEvent`** (`HUDMenu` hat es nicht). Damit steht
auch der Auswahlweg des Starfield-Grids in Fallout 4 offen: Auswahl durch den
eigenen Pfad des Menuees schicken, statt selbst auszuruesten. Die Probe zielt
seitdem nur noch auf `FavoritesMenu` — in `HUDMenu` blieb das Rechteck sonst
die ganze Sitzung stehen.

Zwei Kleinigkeiten aus demselben Lauf: `stageWidth`/`stageHeight` kamen als
`-1` zurück, weil Scaleform sie als Int statt als Number liefert — dafür gibt
es jetzt `ReadNumber`, wie im Starfield-Projekt. Und die Probe zielt seitdem
auf **beide** Menüs.

**Quellcode ist öffentlich:**
`https://github.com/alexanderjohnen/Fallout4FavoritesMenuGrid` (GPL-3.0-or-later).

### Was offen ist

1. **Alle drei Fragen aus Abschnitt 2.** Erst danach steht fest, ob die
   Oberfläche wie in Starfield gebaut werden kann.
2. **Der Kern.** `favorites_core.cpp` aus dem Starfield-Projekt wird nicht
   portiert, sondern neu geschrieben. Der größere Teil davon entfällt
   allerdings — siehe Abschnitt 3.
3. **Spielversionen.** Nur 1.10.163 (OG). 1.10.980 und später brauchen eine
   zweite Adress-Datenbank; F4SE sieht das ausdrücklich vor, aber prüfen kann
   es nur jemand mit der Version.
4. **Serialisierung.** F4SE hat eine Co-Save-Schnittstelle, SFSE nicht. Der
   Zustand gehört dorthin und nicht in eine Datei daneben. Damit entfällt der
   ganze Abgleich, der in Starfield nötig ist.

---

## 1. Bauen

Vorausgesetzt: VS Build Tools 2022, vcpkg unter `C:\Dev\vcpkg`
(`VCPKG_ROOT`), Python für `deploy.py`.

```
cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
py -3 deploy.py
```

`cmake.exe` liegt hier nicht im PATH, sondern unter
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.

CommonLibF4 ist ein Submodul (`extern/CommonLibF4`, Branch `community`,
Commit `da3e995`) und wird als Unterverzeichnis mitgebaut; vcpkg liefert nur
`boost-stl-interfaces`, `fmt`, `spdlog` und `rsm-mmio`.

**CommonLibF4 braucht einen Ein-Zeilen-Patch**, sonst kompiliert es mit dem
aktuellen MSVC nicht: In `include/RE/msvc/memory.h` steht zweimal
`std::is_lvalue_reference<deleter_type> && ...` statt
`std::is_lvalue_reference_v<...>`. Ohne `_v` ist das ein Typ, und `&&`
dahinter liest MSVC als Rvalue-Referenz — Syntaxfehler (C2059). Der Patch
liegt unter `patches/0001-commonlibf4-is_lvalue_reference_v.patch` und muss
nach einem frischen `git submodule update` neu angewandt werden:

```
git -C extern/CommonLibF4 apply ../../patches/0001-commonlibf4-is_lvalue_reference_v.patch
```

Sauberer wäre ein eigener Fork von CommonLibF4 mit dieser Korrektur als
Submodul. **Noch nicht entschieden.**

Das Spiel liegt unter
`G:\Program Files (x86)\Steam\steamapps\common\Fallout 4`, Version
1.10.163.0, F4SE und Address Library sind installiert (`version-1-10-163-0.bin`
liegt in `Data\F4SE\Plugins`).

**Vor jedem Deploy committen.** Gleiche Regel wie im Starfield-Projekt, und
aus demselben Grund: Ohne Historie wird die Ursache eines Absturzes unter den
eigenen Änderungen gesucht, auch wenn sie woanders liegt.

---

## 2. Was Meilenstein 0 beantworten soll

Das Log liegt in
`Documents\My Games\Fallout4\F4SE\FavoritesMenuGrid.log`.

1. **Stimmt `FavoritesManager` auf 1.10.163?** `REL::ID(198281)` ist die
   Singleton-Adresse aus CommonLibF4. Erwartet wird eine Zeile
   `favorites: singleton at ...` und danach zwölf Plätze, deren Namen zu dem
   passen, was im Spiel auf den Zifferntasten liegt. Passt es nicht, stimmt
   entweder die ID oder das Speicherlayout nicht — und dann taugt auch
   `weaponLoadedAmmo` nichts, das als zweite Probe daneben ausgegeben wird.
2. **Welches Menü trägt das Favoritenkreuz?** Fallout 4 hat kein eigenes
   `FavoritesMenu.swf`; die Vermutung ist `HUDMenu`. Das Log schreibt jeden
   Menüwechsel mit, also steht danach fest, welche Menüs es überhaupt gibt.
3. **Nimmt die Bühne einen eigenen Sprite an?** Genau das macht das
   Starfield-Grid (`favorites_grid.cpp`, `BuildOverlay`). Klappt es hier
   auch, braucht der Port **keine** ersetzte Interface-Datei — der wichtigste
   Punkt überhaupt, weil in Alexanders Ladeordnung FallUI und DEF_UI die
   `HUDMenu.swf` bereits besetzen. Meldet das Log stattdessen
   „exposes no stage", ist das Movie kein AS3, und die Oberfläche muss anders
   gebaut werden.

Die Probe zeichnet bei **jedem** Menüereignis. Das ist für Meilenstein 0
Absicht: Ein Rechteck, das nach dem Schließen des Pip-Boys noch da ist, sagt
etwas über die Lebensdauer der Bühne aus.

---

## 3. Warum der Port kleiner ist als das Original

In Starfield zeigt ein Favorit auf eine **Inventarzeile**. Daher stammen
`uniqueIDs`, `rowOrdinal`, `UniqueIdentity`, `sessionInstanceData` und
`ResolveInventoryRow` — zusammen 36 Stellen im Kern, und die Quelle der
meisten Fehler in jenem Projekt (wandernde Favoriten, halber Abgleich beim
Laden).

In Fallout 4 ist ein Favorit ein **Basisobjekt**:

```cpp
TESBoundObject* storedFavTypes[12];  // 0x090
```

Es gibt nichts aufzulösen und nichts abzugleichen. Dazu kommt: Lesen und
Schreiben sind Datenzugriffe, keine Engine-Aufrufe — genau die Reihenfolge,
die sich in Starfield bewährt hat.

Offen bleibt, ob ein direkter Schreibzugriff auf `storedFavTypes` die
Oberfläche aktualisiert oder ob es eine Benachrichtigung braucht.
`InventoryInterface::FavoriteChangedEvent` und
`FavoriteMgr_Events::ComponentFavoriteEvent` sind die Kandidaten; beide sind
in CommonLibF4 abgebildet.

---

## 4. Was aus dem Starfield-Projekt übernommen wird

- **`favorites_grid.cpp`** (rund 2.400 Zeilen) — Raster, Icons, Edit-Modus,
  Hover, Kopfzeile. Kein einziger Adressnachschlag, vier Engine-Typen. Die
  Scaleform-Schnittstelle sieht in CommonLibF4 fast gleich aus; der
  sichtbarste Unterschied ist, dass es kein `CreateString` gibt — ein
  `GFx::Value` nimmt einen `const char*` direkt.
- **Der Entwurf des Zustands**, aber nicht seine Speicherung: Das gehört ins
  Co-Save.
- **Die Farbgebung**, allerdings umgestellt. Neun Konstanten an einer Stelle
  in `favorites_grid.cpp`; in Fallout 4 kommen die Werte aus
  `Fallout4Prefs.ini` (`iHUDColorR/G/B`, dazu der `[Pipboy]`-Block), mit einer
  MCM-Überschreibung darüber. Dieselbe Technik wie das Lesen der ControlMap.

Nicht übernommen wird alles, was mit dem Rad und seinen ActionScript-Haken zu
tun hat. Wenn das Grid die Oberfläche ist, braucht es die neun Haken nicht:
Es wechselt die Seite selbst, bevor es die Auswahl auslöst.

---

## 5. Das Umfeld: was andere Mods am Favoritensystem tun

Aus Alexanders Ladeordnung, an den Dateien selbst abgelesen — nicht aus
Beschreibungen.

**FavoritesMenuEx** ändert die Oberfläche des Menüs. Wird für den ersten Test
deinstalliert.

**Nested Hotkeys** fasst das Vanilla-Menü nicht an, hängt aber am selben
System. Die Zeichenketten in `NestedHotkeys.dll` zeigen:

- `BSTEventSink<InventoryInterface::FavoriteChangedEvent>` und eine Klasse
  `FavoritesChangedHooks` — es lauscht auf Favoritenänderungen und hakt sich
  ein.
- `nestedHotkeysHud`, `registerHudMenu`, `root.requestCloseMenu` — es
  registriert ein **eigenes Menü** mit eigener SWF, statt in `HUDMenu` zu
  zeichnen.
- `Quickkey1` bis `Quickkey7` — es löst die Auswahl über dieselben
  Benutzerereignisse aus, die das Starfield-Grid benutzt.

Drei Schlüsse daraus:

1. **Die Quickkey-Ereignisse heißen in Fallout 4 genauso.** Der Weg, eine
   Auswahl durch den eigenen Pfad des Menüs zu schicken statt selbst
   auszurüsten, steht damit offen.
2. **Ein eigenes Menü ist der Notausgang**, falls die Bühne von `HUDMenu`
   unseren Sprite nicht annimmt. Das kostet eine eigene SWF — aber eine
   **neue** Datei, keine ersetzte, also ohne den Konflikt mit FallUI und
   DEF_UI.
3. **Verträglichkeit im Auge behalten.** Nested Hotkeys tauscht Favoriten zur
   Laufzeit aus, das Grid tut dasselbe beim Seitenwechsel. Zwei Schreiber auf
   denselben zwölf Plätzen können sich in die Quere kommen; das
   `FavoriteChangedEvent` ist die Stelle, an der man das mitbekommt.
