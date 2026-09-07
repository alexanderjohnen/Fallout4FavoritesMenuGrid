# Favorites Menu Grid (Fallout 4) — Arbeitsstand

Stand: 2026-09-06. Portierung von
[Favorites Menu Grid für Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid).
**Abschnitt 0 ist der Einstieg.** Er sagt, was gilt; die nummerierten
Abschnitte danach sagen, wie es dazu kam, und sind Fundgrube, nicht Pflicht.

---

## 0. Aktueller Stand

Die Mod läuft und wird gespielt. **Sie ist das Grid** — ein einziger Weg,
kein zweiter durch das Vanilla-Kreuz (Abschnitt 36).

### Was funktioniert, im Spiel bestätigt

- **Mehrere Seiten zu zwölf Tasten**, alle gleichzeitig gezeichnet, eine Reihe
  je Seite, auf einem **eigenen Menü** mit eigener Bühne und Mauszeiger. Die
  Seiten liegen im Mitspeicher von F4SE, also je Charakter.
- **Zeiger und Tasten** erreichen jede Zelle jeder Seite. `w`/`s` zwischen den
  Reihen, `a`/`d` innerhalb einer, gehaltene Taste läuft weiter.
- **Benutzen** über `FavoritesManager::UseQuickkeyItem` (ID 303130). Liegt die
  Zelle auf einer anderen Seite, wird vorher dorthin gewechselt.
- **Ab- statt Anlegen** beim zweiten Druck. Nicht über den Equip-Manager —
  der lehnt ab —, sondern über ein Boolean in einem Aufruf innerhalb von
  `UseQuickkeyItem`, umgebogen per Trampolin (Abschnitt 35).
- **Symbole** aus der Sorter-Konfiguration des Spielers: Tag aus dem Namen,
  sonst FIS-eigenes Auto-Tagging, sonst ein Auffangsymbol nach Gegenstandsart.
  Mehrere Bibliotheken, nur die geladen, die eine Seite braucht
  (Abschnitte 31 und 32).
- **Zwei Zeilen über dem Gitter**: Name, darunter Schaden und Munition
  beziehungsweise Widerstände. Instanzdaten vor Basisdaten.
- **Tastenleiste** unter dem Panel, aus den tatsächlichen Bindungen gebaut.
- **Das Crosshair** verschwindet, solange das Menü offen ist — jedes Bild neu,
  weil auf einem stark gemoddeten Spiel mehrere Mods daran ziehen.

### Was gebaut, aber noch nicht im Spiel geprüft ist

- `INSERT` hebt eine Zelle auf, der nächste Druck tauscht sie mit der
  markierten — **auch über Seiten hinweg**.
- `DELETE` gibt die Taste frei (der Gegenstand bleibt Favorit ohne Ziffer).
- `DefaultPage` legt beim Schließen eine feste Seite zurück in die Engine.
- `GridWrap=0` als Wand statt Tür an den Rändern.
- **Das Gamepad** (Abschnitt 38): Steuerkreuz und linker Stick laufen durch
  die Zellen, `A` benutzt, `X` hebt auf, `Y` gibt frei. `B` und die Taste, die
  das Menü öffnet, werden nie genommen — die zweite wird aus den Bindungen
  des Spielers gelesen. Die Tastenleiste nennt Controllertasten, sobald eine
  gedrückt wurde.

### Wie es gebaut ist

| Datei | Aufgabe |
| --- | --- |
| `menu.cpp` | Das eigene Menü: registrieren, leere SWF laden, Flags. **Kein `kCustomRendering`** (Abschnitt 21) |
| `grid.cpp` | Zeichnen, Treffertest, Marke, Beschriftung. Alles aus `GridCellSize` abgeleitet |
| `input.cpp` | Eigener `BSInputEventUser` **vorn** in `MenuControls::handlers` (Abschnitt 22), Tastatur, Maus und Gamepad (Abschnitt 38) |
| `use.cpp` | `UseQuickkeyItem` auflösen und den Equip-Aufruf darin umleiten |
| `tags.cpp` | Sorter-Konfigurationen lesen: Tag → Symbol → Bibliothek, plus Auto-Tagging |
| `icons.cpp` | Bibliotheken zur Laufzeit in die eigene Anwendungsdomäne laden (Abschnitt 29) |
| `detail.cpp` | Was ein Gegenstand ist und tut, für die zweite Zeile |
| `main.cpp` | Seiten, Einstellungen, Zustand, Verdrahtung |
| `peek.cpp` | Werkzeug: Engine-Code aus dem laufenden Spiel neben das Log kopieren |

### Was offen ist

1. **Symbole in der Wertezeile** — Munitionstyp, Widerstände, Schadensart mit
   eigenem Zeichen, wie die Karte im Pip-Boy. Braucht eine Zeile aus
   Abschnitten statt einem Textfeld: jeder mit optionalem Symbol, jeder über
   `textWidth` gemessen, zusammen mittig gesetzt. Das Munitionssymbol ist das
   leichte — Munition ist ein Gegenstand mit Namen und läuft durch dieselbe
   Tag-Kette. Widerstände und Schadensarten haben in FIS **keine** Zeichen.
2. **Die Zeile „Powerful | Quick | Instigating"** — legendäre Wirkung und
   Modnamen, aus den Instanznamensregeln.
3. **Das Gamepad im Spiel prüfen.** Gebaut ist es (Abschnitt 38), gespielt
   noch nicht. Die eine Frage, die nur ein Lauf beantwortet: schließt sich
   das Menü noch mit dem Controller?
4. **Der Pip-Boy.** Das Zuweisen-Kreuz zeigt noch nichts von uns. Immerhin
   sagt die Eckmeldung des Spiels inzwischen, auf welche Seite man legt.
5. **Spätere Spielversionen.** 1.10.980 und aufwärts brauchen eigene IDs.

### Was nicht noch einmal herausgefunden werden muss

| | |
| --- | --- |
| `FavoritesManager::UseQuickkeyItem` | ID 303130, `bool(manager, index)` |
| Der Equip-Aufruf darin | `+0x1b3`, sein erstes Boolean heißt „und wieder ab" |
| Eingabe | `MenuControls::handlers`, vorn einfügen; ein Menü bekommt Tasten **nicht**, weil es ein Menü ist |
| Zeiger | `MenuCursor`, Bildschirmpixel, über `minCursorX`…`maxCursorX` umrechnen |
| Anwendungsdomäne | nicht über `GetVariable`, sondern `root.loaderInfo.applicationDomain` |
| Zellfarbe | `ff ff ff 33` aus der Vanilla-SWF; Weiß, **weil die Engine tönt** — wir tönen selbst |
| Symbole färben | multiplizieren, nicht ersetzen, sonst wird die Zeichnung zur Silhouette |
| Sorter-Variationen | nur lesen, wenn MCM sie gewählt hat |
| Fallout4.exe | auf der Platte gepackt; Code nur aus dem laufenden Spiel lesbar (`peek`) |
| Andere Mods | was roh die Tastatur liest, sieht unsere Ansprüche nicht — keine Buchstaben als Vorgabe |
| Gamepad-Codes | `idCode` ist die nackte XInput-Maske (A = `0x1000`), nicht `BS_BUTTON_CODE`; Trigger sind `0x9` und `0xA` |
| Wie das Kreuz schließt | `FavoritesMenu.as`: `ProcessUserEvent` schließt auf `Cancel` oder `Quickkeys`, beim **Loslassen** |
| Wie das Kreuz am Pad läuft | über Scaleform-Tastenereignisse (`Keyboard.UP`…`ENTER`) im Fokus, nicht über eigene Benutzerereignisse |

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
2. **Welches Menü trägt das Favoritenkreuz?** ~~Vermutung: `HUDMenu`.~~
   **Beantwortet:** Es ist ein eigenes `FavoritesMenu`. Die `.swf` dazu liegt
   in einem BA2, nicht lose in `Data\Interface` — daher die falsche Annahme.
   Das Log schreibt jeden Menüwechsel mit, also steht auch fest, welche Menüs
   es sonst noch gibt.
3. **Nimmt die Bühne einen eigenen Sprite an?** Genau das macht das
   Starfield-Grid (`favorites_grid.cpp`, `BuildOverlay`). Klappt es hier
   auch, braucht der Port **keine** ersetzte Interface-Datei — der wichtigste
   Punkt überhaupt, weil in Alexanders Ladeordnung FallUI und DEF_UI die
   `HUDMenu.swf` bereits besetzen. Meldet das Log stattdessen
   „exposes no stage", ist das Movie kein AS3, und die Oberfläche muss anders
   gebaut werden.

Die Probe läuft bei **jedem** Menüereignis, zeichnet aber nur noch in
`FavoritesMenu`. In `HUDMenu` blieb das Rechteck sonst die ganze Sitzung
stehen — was für sich genommen auch eine Antwort war: Die Bühne behält den
Sprite, bis das Menü selbst verschwindet.

---

## 2a. Der Schreibtest (Meilenstein 1a)

Gebaut am 2026-09-04, **im Spiel noch nicht geprüft.**

Nur solange `FavoritesMenu` offen ist und das Spiel im Vordergrund:

- **F9** kehrt die zwölf Einträge um und tut sonst nichts.
- **F10** kehrt sie um und schickt danach `kInventoryUpdate` an
  `FavoritesMenu`.

Umkehren ist seine eigene Umkehrung: Zweimal drücken stellt die
ursprüngliche Reihenfolge exakt wieder her, und selbst ein Abbruch dazwischen
hinterlässt die eigenen Favoriten des Spielers statt etwas Erfundenem. Es
wird kein Zeiger erzeugt oder freigegeben, also kann nichts baumeln.

Die Auswertung:

| Beobachtung | Schluss für den Seitenwechsel |
| --- | --- |
| F9 bewegt die Symbole | Ein Seitenwechsel ist ein schlichter Schreibzugriff. |
| Nur F10 bewegt sie | Die Nachricht gehört in den Seitenwechsel. |
| Keins von beiden, obwohl das Log das Feld umgekehrt zeigt | Das Menü hält eine eigene Kopie und bezieht sie woanders her — dann ist das die nächste Spur. |

Nicht mitgedreht wird `bufferedFavGeometries[12]`, das neben den Plätzen
liegt und vorgeladene Modelle hält. Falls die 3D-Vorschau im Menü danach
nicht zum Symbol passt, ist das der Grund und kein Fehler im Schreibweg.

Die Tastatur wird auf einem eigenen Thread abgefragt, der nur `GetMenuOpen`
anfasst; alles, was das Spiel berührt, läuft als UI-Task auf dem Thread, den
die Engine erwartet. Siehe die Regel aus dem Starfield-Projekt: kein
Scaleform von einem fremden Thread.

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

---

## 6. `storedFavTypes` ist nicht die Wahrheit, sondern ein Abbild

**Der Befund vom 2026-09-05, im Spiel gemessen.** Ein Schreibzugriff auf
`FavoritesManager::storedFavTypes` ändert **nichts**: nicht die Anzeige im
Kreuz, nicht mit `kInventoryUpdate` nachgeschoben, und auch nicht das
Verhalten — die Zifferntaste rüstet weiterhin das Item aus, das vorher dort
favorisiert war. Das Array lässt sich beschreiben, es liest nur niemand
zurück.

**Wo die Bindung stattdessen steht**, beides in CommonLibF4 abgebildet:

```cpp
class ExtraFavorite : public BSExtraData {
    std::int8_t quickkeyIndex;   // 0x18
};

// BGSInventoryItem::Stack-Seite, über ApplyChangesFunctor gesetzt:
std::int8_t favoriteIndex;       // 0x2B
```

Ein Favorit ist also eine Eigenschaft **des Inventargegenstands**, nicht ein
Eintrag in einer Liste des Managers. Dazu passt, was neben `storedFavTypes`
liegt: `bufferedFavGeometries[12]` mit vorgeladenen Modellen. Die zwölf
Zeiger sind der Vorlade- und Anzeige-Cache des Menüs, mehr nicht.

Die Engine hat dafür einen eigenen Funktor: RTTI `SetFavoriteFunctor`
(`REL::ID(222655)`), VTABLE `REL::ID(1064496)`. In CommonLibF4 ist er nicht
als Klasse modelliert, aber `ApplyChangesFunctor` trägt `favoriteIndex` an
0x2B und ist der dokumentierte Weg, eine Stack-Eigenschaft zu schreiben.

**Was das für den Entwurf heißt — Korrektur zu Abschnitt 3.** Die Annahme,
ein Favorit zeige in Fallout 4 auf das Basisobjekt und die
Auflösungs-Maschinerie aus Starfield entfalle, stammte daher, dass
`storedFavTypes` ein `TESBoundObject*`-Array ist. Das war der Cache. Die
echte Bindung hängt an einem Inventar-Stack, also näher an Starfield als
gedacht. Der Port ist damit nicht mehr klar kleiner als das Original; wie
viel größer, entscheidet sich daran, wie stabil ein Stack über Auf- und
Abgeben hinweg zu identifizieren ist.

**Die nächsten Schritte:**

1. Den Inventar-Weg lesen: über die Inventarliste des Spielers laufen und
   für jeden Gegenstand `favoriteIndex` bzw. `ExtraFavorite::quickkeyIndex`
   protokollieren. Wenn die zwölf Plätze daraus zu rekonstruieren sind,
   ist die Quelle bestaetigt.
2. Danach denselben Schreibtest gegen dieses Feld — erst dann steht fest,
   wie ein Seitenwechsel aussieht.

---

## 7. Bestätigt: Die Favoriten stehen im Inventar

**Gemessen am 2026-09-05, 00:24 Uhr.** Die Inventar-Sonde (F6) liefert zehn
Treffer:

```
inventory: quickkey  0 (key 1) count   2 form 000459C5 "Addictol"
inventory: quickkey  2 (key 3) count  30 form 00023736 "Stimpak"
inventory: quickkey 10 (key ?) count   1 form FE0A7F12 "Hunting Shotgun"
inventory: 704 stacks, 10 of them favorited
```

Drei Befunde:

1. **`ExtraFavorite::quickkeyIndex` ist die Quelle.** Vor dem Schreibtest
   stimmt das Array des Managers Platz für Platz mit den Inventar-Indizes
   überein.
2. **Das Array wird nicht nachgeführt.** Nach dem Umkehren blieb es umgekehrt,
   auch als das Menü danach geöffnet wurde — das Menü baut seine Einträge
   also nicht daraus auf. Es ist ein Cache, den das Spiel beim Favorisieren
   füllt und für die Anzeige nicht zurückliest.
3. **Fallout 4 hat zwölf Plätze und zwölf Tasten.** ~~`quickkey 10` und `11`
   sind über die Tastatur gar nicht erreichbar.~~ **Falsch** — die Ausgabe der
   Menüstruktur vom 2026-09-05 zeigt `Entry_10.Quickkey_tf` mit dem Text `-`
   und `Entry_11` mit `=`. Die beiden liegen also auf den Tasten neben der
   Null.

Nebenbei: 704 beziehungsweise 706 Stapel im Inventar, und die Zahl ändert
sich im Spielverlauf. Ein voller Durchlauf ist für einen Tastendruck billig,
für jeden Bildaufbau wäre er es nicht — das Grid muss seine Liste zwischen
Änderungen behalten.

### Was daraus für den Kern folgt

Ein Seitenwechsel schreibt `quickkeyIndex` auf den Inventar-Stapeln. Der Weg
dafür ist `BGSInventoryList::FindAndWriteStackDataForItem` zusammen mit
`ApplyChangesFunctor` (`favoriteIndex` an 0x2B); die Engine hat zusätzlich
einen `SetFavoriteFunctor` (RTTI `REL::ID(222655)`).

**Und damit steht die eigentliche Frage des Ports fest**, dieselbe wie in
Starfield: Ein Favorit hängt an einem **Stapel**, und ein `std::int8_t` mit
zwölf gültigen Werten hat keinen Platz für „gehört zu Seite 2, Platz 3". Der
Zustand der anderen Seiten muss also bei uns liegen und beim Zurückschalten
den richtigen Stapel wiederfinden. Wie stabil ein Stapel über Aufnehmen,
Ablegen und Modifizieren hinweg zu identifizieren ist, ist die nächste
Messung — und sie entscheidet den Aufwand.

---

## 8. Wie stabil ein Stapel ist (gemessen 2026-09-05, 00:33 bis 00:36)

Fünf Durchläufe von F6, dazwischen Ablegen und Aufnehmen, Verbrauchen und
allgemeine Inventarbewegung (705 → 706 → 694 Stapel).

**Der favorisierte Stapel hat alles überstanden.** Über alle fünf Läufe:
dieselbe Adresse, dieselbe Ordnungszahl 0 in der Kette, der Favorit blieb
daran hängen. Auch als der Stimpak-Stapel von 30 auf 29 schrumpfte.

**Ein aufgenommener Gegenstand verschmilzt nicht.** Nach Ablegen und
Wiederaufnehmen eines Stimpaks:

```
"Stimpak" stack 0 at 0x2b39103e3e0 count 29 quickkey 3 extras [ Favorite ]
"Stimpak" stack 1 at 0x2b3c1a4b290 count  1 quickkey -  extras [ #14 #72 ]
```

Ein **zweiter** Stapel derselben Sorte, ohne Favorit. Er trägt
`kStartingPosition` (14) und `kStartingWorldOrCell` (72) — die Spuren davon,
in der Welt gelegen zu haben. Das Spiel führt die beiden nicht zusammen.

**Die Extra-Typen aus den Läufen, entschlüsselt** (`BSExtraData.h`, die
Aufzählung ist lückenlos ab `kNone` = 0):

| Nr. | Typ | Wo gesehen |
| --- | --- | --- |
| 14 | `kStartingPosition` | aufgehobener Stimpak, Silver Shroud Hat |
| 53 | `kObjectInstance` | T60, Silver Shroud Hat (Modifikationen) |
| 64 | `kLastFinishedSequence` | Silver Shroud Hat |
| 72 | `kStartingWorldOrCell` | aufgehobener Stimpak |
| 136 | `kAliasInstanceArray` | Silver Shroud Hat (Quest-Gegenstand) |
| 140 | `kPromotedRef` | Silver Shroud Hat |
| 153 | `kTextDisplayData` | T60 (eigener Name) |
| 186 | `kInstanceData` | T60, Silver Shroud Hat |

### Was das für den Kern bedeutet

**Solange ein Gegenstand favorisiert ist, ist er sein eigener Merker** — das
`ExtraFavorite` hängt am richtigen Stapel und bleibt dort. Für die aktive
Seite braucht es also gar keine eigene Identität.

Die Frage stellt sich nur für **geparkte** Favoriten: Ein `int8` mit zwölf
gültigen Werten hat keinen Platz für Seite 2. Wer auf einer anderen Seite
liegt, verliert sein `ExtraFavorite` und damit sein Kennzeichen.

Der Entwurf, der sich daraus anbietet: einen geparkten Favoriten über
**Basisobjekt plus eine Signatur des Stapels** wiederfinden — die Liste
seiner Extra-Typen, bei modifizierten Gegenständen dazu die Instanzdaten.
Findet sich beim Zurückschalten kein passender Stapel, nimmt man den ersten
derselben Sorte; bei Stimpaks und Munition ist das ohnehin gleichwertig, und
bei einzigartigen Waffen unterscheiden die Instanzdaten sie.

**Noch nicht gemessen:** was passiert, wenn ein favorisierter Stapel ganz
aufgebraucht wird, und ob eine Modifikation an der Werkbank die Bindung
überlebt. Der Lauf vom 2026-09-05 zeigt am T60 keine Veränderung der
Extra-Liste, aber es ist nicht sicher, ob dabei tatsächlich modifiziert
wurde.

---

## 9. Der Cache ist das Gedächtnis der Engine (2026-09-05, 00:43)

**Korrektur zu Abschnitt 6.** Dort steht, `storedFavTypes` werde von niemandem
zurückgelesen. Das gilt für die Anzeige und für das Benutzen, aber nicht
allgemein:

1. Der favorisierte Stimpak-Stapel wurde vollständig aufgebraucht. Kein
   Inventarstapel trug mehr ein `ExtraFavorite` — `storedFavTypes[3]` stand
   aber weiter auf Stimpak.
2. Ein einzelner Stimpak wurde abgelegt und wieder aufgehoben. Er bekam
   `quickkey 3` **von selbst**.

Der Cache ist also die Merkliste „welche Sorte gehört auf welche Taste", und
die Engine hängt einem passenden Neuzugang den Favoriten wieder an. Für den
Seitenwechsel heißt das: Beim Wegschalten einer Seite muss der Cache
mitgeräumt werden, sonst zieht die Engine geparkte Favoriten beim nächsten
Aufheben eigenmächtig zurueck.

Nebenbei: Ein verwaister Platz ist möglich. Ein Favorit kann verschwinden,
ohne dass ihn jemand entfernt hat — der Stapel wird einfach aufgebraucht.

---

## 10. Warum Fallout 4 favorisierte Stapel spaltet

Alexanders alte Beschwerde, unterwegs mitgemessen. **Nicht das Favorisieren
spaltet, sondern das Ausrüsten.**

Zwei Stapel derselben Sorte verschmelzen nur bei gleichen Extra-Daten. Drei
Messungen an denselben Granaten:

```
00:47  stack 0 count 5 flags 0x0001 quickkey 3 extras [ Favorite ]
       -> abgelegtes Stück verschmilzt wieder hinein
00:49  stack 0 count 3 flags 0x0001 quickkey 3 extras [ Favorite #186 ]
       stack 1 count 1 flags 0x0000 quickkey -  extras [ none ]
```

`#186` ist `kInstanceData` und kam mit dem Ausrüsten dazu. Ab da
unterscheidet sich der Stapel von jedem frisch aufgesammelten Stück.

**Das `ExtraFavorite` ist ausdrücklich kein Hindernis:** Um 00:47 ist ein
blanker Stapel in einen mit `[ Favorite ]` hineingeflossen.

Der Stimpak-Fall aus Abschnitt 8 ist dieselbe Regel aus der anderen Richtung:
Dort brachte das *aufgehobene* Stück `kStartingPosition` und
`kStartingWorldOrCell` mit, die dem Stapel fehlten.

**Offen:** ob `#186` beim Ablegen der Ausrüstung wieder verschwindet. Der
Test dafür ist, die Waffe zu wechseln und danach erneut abzulegen und
aufzuheben.

Für die Mod ist das eine Warnung: Ein ausgerüsteter Stapel trägt etwas, das
ein anderer nicht hat. Eine Signatur, mit der wir geparkte Favoriten
wiederfinden, darf daran nicht hängen.

---

## 11. Was die Mod können soll (Stand 2026-09-05)

Aus dem Starfield-Projekt übernommen, weil es sich dort bewährt hat:

- **`ToggleEquipOnSelect`, Standard 1.** Ein zweiter Druck auf denselben
  Favoriten legt das Ausgerüstete wieder ab. In Starfield ist das eine der
  eigenen Erweiterungen (`ToggleEquipOnSelect=1`, gilt nur für Ausrüstbares,
  nie für Aid oder Munition). Alexander nutzt in Fallout 4 bisher eine
  fremde Mod dafür und deinstalliert sie zugunsten dieser hier — das Verhalten
  gehört also in den Kern und nicht hinter einen Schalter, der standardmäßig
  aus ist.
- Seitentasten vor und zurück, `ResetToFirstPageOnClose`,
  `ExternallyManagedSlots` — dieselbe Begründung, aber noch nicht im Detail
  durchgesprochen.

**Fallout-4-eigen:**

- Die Farbe aus `Fallout4Prefs.ini` (`iHUDColorR/G/B`, dazu `[Pipboy]`), mit
  einer MCM-Überschreibung darüber.
- Die Plätze 10 und 11 liegen auf `-` und `=` (Abschnitt 7) — im Grid
  bekommen sie eine Zelle wie alle anderen.

---

## 12. Wie das Menü seine Zellen hält (2026-09-05, 01:24)

```
menuObj.Cross_mc.EntryHolder_mc.Entry_0 .. Entry_11
    instanceNN (1 child)
    Icon_mc      -- 1 Kind, wenn der Platz belegt ist, sonst 0
    Quickkey_tf  -- der Text der Taste: "1".."9", "0", "-", "="
```

Belegt waren `Entry_2` und `Entry_3` — genau die beiden Plätze mit
Favoriten. Die Zelle trägt ihr Symbol also als geladenen Clip, und der
bleibt stehen, wenn sich die Zuordnung darunter ändert. Das erklaert das
eingefrorene Bild vollständig.

Für das Grid ist das dieselbe Ausgangslage wie in Starfield: Die Icons des
Spiels sind vorhanden und wiederverwendbar, statt eigene Grafik mitzubringen.

**Refresh-Versuche bisher ohne Wirkung:** `kInventoryUpdate`, `kUpdate` und
`kReshow` wurden unter der richtigen Bedingung geprüft (Daten geändert,
Anzeige alt) und bewegen nichts. `kShow`, `kHide`+`kShow` und
`kForceHide`+`kShow` stehen noch aus; sie sind in der Reihenfolge jetzt
vorn.

---

## 13. Wie das Favoritenmenü wirklich funktioniert

**Gelöst am 2026-09-05 gegen 02:55.** `FavoritesMenu.swf` liegt in
`Fallout4 - Interface.ba2`; mit **Archive2** (Teil des Creation Kit) heraus
und mit **JPEXS** (`ffdec-cli.exe -export script`) dekompiliert. Der Code
beantwortet auf einer Seite, was ein Abend Ausprobieren nicht geschafft hat.

### Die Datenkette

```actionscript
// FavoritesMenu.as
public function set favInfoArray(a:Array) : *   { this.Cross_mc.infoArray = a; }
public function set selectedIndex(i:uint) : *   { this.Cross_mc.selectedIndex = i; }
private function onFavEntryClick() : *          { this.BGSCodeObj.useQuickkey(this.Cross_mc.selectedIndex); }

// FavoritesCross.as
public function set infoArray(a:Array) : *      // öffentlich
{
   this._FavoritesInfoA = a;
   this.selectedIndex = this.ClampSelection(this.selectedIndex);
   dispatchEvent(new CustomEvent(SELECTION_UPDATE, ...));
   SetIsDirty();
}

override public function redrawUIComponent() : void
{
   ... entry.Icon_mc.gotoAndStop(info.FavIconType);
}
```

- Das Spiel reicht die zwölf Einträge über **`favInfoArray`** herein, die
  landen in **`Cross_mc.infoArray`**, und der Setter löst das Neuzeichnen
  selbst aus.
- Ein Eintrag ist `{ FavIconType, text, count, ammoText, ammoCount }`.
  `FavIconType` ist eine **Bildnummer** in `Icon_mc`; die Zuordnung steht
  nicht im Skript, `Icon_mc.currentFrame` lässt sich aber lesen und
  mitnehmen.
- Ein leerer Platz ist ein **`null`-Eintrag**, kein fehlender.
- Benutzt wird über **`BGSCodeObj.useQuickkey(index)`** — der Rückruf, den
  die C++-Seite am Menü registriert. Das Gegenstück zu `ProcessUserEvent`
  in Starfield.
- Die Indizes 0 bis 11 sind Kreuzpositionen (`FS_LEFT_3` … `FS_DOWN_3`),
  `12` ist `FS_NONE`. Die Tastenbeschriftung macht `FavoritesEntry`
  selbst: 1–9, dann `0`, `-`, `=`.

### Warum alles davor scheiterte

`SetIsDirty()` wurde angenommen und bewirkte nichts — es zeichnet aus
`_FavoritesInfoA` neu, und die hatte niemand geändert. Alle UI-Nachrichten
(`kInventoryUpdate`, `kUpdate`, `kReshow`, `kShow`) sitzen auf derselben
unveränderten Liste. **Nicht das Neuzeichnen fehlte, sondern die Daten.**

### Was jetzt funktioniert

`RefreshCross()` baut die Liste aus dem Inventar neu und schreibt sie in
`Cross_mc.infoArray`; die Icons der betroffenen Zellen werden über
`GetEntryClip(i).Icon_mc.currentFrame` übernommen. Im Spiel bestätigt: Die
Anzeige folgt sofort, ohne Speichern und Laden.

**Offen:** ob `useQuickkey(index)` nach einem Wechsel den richtigen
Gegenstand benutzt. Falls nicht, löst das Grid die Auswahl selbst aus.

### Werkzeuge, die sich gelohnt haben

| Werkzeug | wofür |
| --- | --- |
| Archive2 (Creation Kit) | SWF aus dem BA2 holen |
| JPEXS / ffdec-cli | SWF nach ActionScript dekompilieren |
| Address Library | Adressen für `REL::ID`, Grundlage von CommonLibF4 |
| Zeichenketten aus fremden DLLs | wie andere Mods dasselbe System anfassen |

---

## 14. Der Seitenwechsel funktioniert — und zwar nur über die Engine

**Gelöst am 2026-09-05, 03:17.** Der Weg ist kürzer als alles, was wir davor
versucht haben:

```cpp
class SetQuickkeyFunctor : public RE::BGSInventoryItem::StackDataWriteFunctor
{
    void WriteDataImpl(RE::TESBoundObject&, Stack& a_stack) override
    { /* ExtraFavorite::quickkeyIndex setzen */ }
};

player->inventoryList->FindAndWriteStackDataForItem(object, compare, set);
```

Ein Favorit umgehängt, und **das Spiel zieht alles andere selbst nach**: die
Anzeige im Kreuz, den Cache in `storedFavTypes` und die dritte Tabelle, die
wir nie gefunden haben. Im Spiel bestätigt: Anzeige stimmt sofort, und der
**erste** Druck auf die Zifferntaste benutzt den richtigen Gegenstand.

### Was dabei zu beachten ist

- `WriteDataImpl` sitzt auf **Vtable-Platz 0**. CommonLibF4 dokumentiert
  Platz 1 — das ist falsch, gemessen an der Vtable von
  `ApplyChangesFunctor` (`REL::ID(319870)`), in der die bekannte Adresse von
  `WriteDataImpl` (`REL::ID(1291190)`) auf Platz 0 steht. Eine gewöhnliche
  C++-Ableitung passt also.
- `shouldSplitStacks = false`, sonst spaltet der Schreibvorgang den Stapel.
- Beim Tauschen zweier Plätze muss einer **zwischengeparkt** werden, sonst
  trifft der Vergleichsfunktor auf halbem Weg den falschen Stapel.

### Was damit hinfällig ist

Alles, was in den Abschnitten 6 bis 13 als Umweg steht: der eigene
Schreibzugriff auf `storedFavTypes`, das Nachschieben von UI-Nachrichten,
`SetIsDirty`, das Melden des `FavoriteChangedEvent` und sogar das Schreiben
von `Cross_mc.infoArray`. Nichts davon wird für den Seitenwechsel gebraucht.

**Wertvoll bleiben die Abschnitte trotzdem**, und zwar aus zwei Gründen. Sie
erklären, *warum* der direkte Weg nicht trägt — es gibt drei Tabellen, und
zwei davon kann man von außen gar nicht sehen. Und `Cross_mc.infoArray`
bleibt der Hebel, mit dem sich das Kreuz gezielt füllen lässt, falls das Grid
später einmal etwas anzeigen soll, das nicht in den zwölf nativen Plätzen
steht.

### Damit ist der Kern entworfen

Ein Seitenwechsel ist: für jeden der zwölf Plätze den Favoriten über
`FindAndWriteStackDataForItem` umhängen. Was bleibt, ist Buchhaltung —
mehrere Seiten verwalten, geparkte Favoriten wiederfinden (Abschnitt 8),
Zustand ins Co-Save — und danach die Oberfläche aus dem Starfield-Projekt.

---

## 15. Die Grundoperation: `ApplyPage` (2026-09-05)

Aus dem Zwei-Platz-Tausch ist die ganze Seite geworden. `ApplyPage(target)`
bekommt zwölf Plätze und sorgt dafür, dass danach in jedem Platz der Gegenstand
liegt, der dort stehen soll. Der Tausch von zwei Plätzen und das Umhängen eines
einzelnen sind darin nur Sonderfälle und stehen nicht mehr eigens im Code.

**Die Regel, nach der die Züge geordnet werden:** Es wird nur auf einen Platz
geschrieben, den niemand mehr braucht. Das ist der Grund für die ganze
Buchhaltung in der Funktion — `occupant` sagt, wer während der Züge wo sitzt,
`settled` sagt, welche Plätze ihren Endstand schon haben. In jedem Durchlauf
werden alle Züge ausgeführt, deren Ziel frei ist; kommt kein Zug mehr durch,
ist ein **Ring** übrig, in dem jeder Platz auf den nächsten wartet.

**Ein Ring wird auf einem freien Platz aufgebrochen** — derselbe Parkplatz wie
beim Zwei-Platz-Tausch, nur an genau der einen Stelle, an der er nötig ist.

**Sind alle zwölf Plätze belegt, gibt es keinen Parkplatz.** Dann bricht die
Funktion den Ring auf einem belegten Platz auf: für einen Moment tragen zwei
Gegenstände denselben Index. Der eigene Vergleichsfunktor stört sich nicht
daran, weil er Gegenstand *und* Index zusammen prüft — ob der Cache der Engine
das übersteht, ist **nicht gemessen**. Der Fall schreibt eine Warnung ins Log;
weicht danach der Cache vom Inventar ab, steht die Stelle fest.

**Was `ApplyPage` noch nicht kann:** einem Gegenstand einen Platz geben, der
gerade keinen hat. Genau das braucht ein Seitenwechsel mit mehr als zwölf
Gegenständen, und genau danach fragt der Rundlauf unten. Bis dahin gilt: Ein
Ziel darf nur Gegenstände nennen, die schon einen Platz haben; Favoriten, die
das Ziel nicht nennt, behalten einen — sie bleiben liegen, wenn das Ziel ihren
Platz frei lässt, sonst rücken sie auf den nächsten freien.

### Der Prüfstand: Rotation (F8)

Jeder Favorit rückt einen Platz weiter, der oberste kommt nach unten. Das ist
**ein einziger langer Ring** und nimmt damit die ganze Funktion in Betrieb;
nach genug Drücken steht alles wieder wie vorher. Mit weniger als zwölf
Favoriten läuft die Parkplatz-Variante, mit genau zwölf die ungemessene.

### Der Rundlauf: Platz wegnehmen und zurückgeben (F7)

Die letzte offene Frage vor dem echten Seitenwechsel. In Fallout 4 ist ein
Favorit nichts als das `ExtraFavorite` auf dem Inventarstapel — es gibt keinen
Zustand „favorisiert, aber ohne Taste". Ein Seitenwechsel über zwölf
Gegenstände hinaus muss also **Favoriten löschen und anlegen**, nicht nur
verschieben.

Beides geht über denselben Schreibweg wie ein Zug, nur mit anderem Funktor:

- **Wegnehmen:** `a_stack.extra->RemoveExtra<RE::ExtraFavorite>()`.
  `ExtraDataList::ClearFavorite` (`REL::ID(254434)`) wäre der Weg der Engine,
  falls das schlichte Entfernen etwas stehen lässt.
- **Zurückgeben:** ein neues `ExtraFavorite`, Typ und Vtable von Hand gesetzt
  (`stl::emplace_vtable`), dann `AddExtra`. Nötig, weil `ExtraFavorite` keinen
  eigenen Konstruktor hat.

Der Rundlauf nimmt beim ersten Druck dem untersten Favoriten seine Taste und
gibt sie beim zweiten zurück. **Vorher speichern.** Zu prüfen ist im Log und
im Spiel: Verschwindet der Eintrag aus dem Kreuz und aus dem Cache? Und kommt
er beim zweiten Druck vollständig zurück — Kreuz, Cache und die Taste, die
danach den richtigen Gegenstand benutzt?

Findet der zweite Druck keinen Stapel ohne Favorit, hat das Entfernen die
`ExtraDataList` mitgenommen; dann muss die Liste beim Anlegen erst erzeugt
werden. Das Log sagt es, weil die Favoritenliste danach unverändert bleibt.

### Was danach dran ist

1. Den Rundlauf im Spiel messen — er entscheidet, ob ein Seitenwechsel mit
   fremden Gegenständen überhaupt über diesen Weg geht.
2. Die Seitenverwaltung: mehrere Seiten im Speicher, Zustand ins Co-Save über
   die F4SE-Serialisierung.
3. Das Grid aus dem Starfield-Projekt (`favorites_grid.cpp`).

---

## 16. Wie das Spiel selbst einen Favoriten setzt (2026-09-05, abends)

Der Rundlauf aus Abschnitt 15 ist im Spiel gescheitert, und zwar dreifach: Der
Favorit war weg, der Gegenstand ließ sich **auch im Pip-Boy nicht mehr
favorisieren**, und mehrfaches Drücken bei offenem Kreuz stürzte ab. Statt
weiter zu probieren — jeder Versuch kostete einen Gegenstand im Spielstand —
ist der Code der Engine nachgelesen worden.

### Das Werkzeug: Peek und f4dis

`Fallout4.exe` ist auf der Platte **verpackt** (Steam, die `.bind`-Sektion).
Ein Disassembler an der Datei liest im Codebereich nur Rauschen. Im Klartext
steht er nur im laufenden Prozess — und dort sitzt das Plugin ohnehin.

- **`src/peek.cpp`** kopiert beim Start oder auf **F10** die gewünschten
  Stellen als Hex neben das Log. Die Funktionsgrenzen kommen aus dem
  **Exception-Verzeichnis** der EXE; das sind reine Daten und vom Packer
  unberührt. Zusätzlich durchsucht es den Code nach `lea reg, [rip + x]` auf
  eine Vtable — so findet man die Stellen, an denen die Engine so ein Objekt
  selbst baut.
- **`tools/f4dis.py`** löst IDs über die Address Library auf, disassembliert
  den Auszug und hängt an jeden Sprung und jedes `lea` die ID des Ziels. Damit
  liest man sich von einer Funktion zur nächsten weiter. Vtables und andere
  Daten liest es direkt aus der EXE — nur der Code ist verpackt.
- Die Einstellungen werden **beim Auslösen** gelesen, nicht beim Laden. Eine
  neue Frage kostet eine Zeile in der INI und einen Tastendruck.

Nebenbei bestätigt: Die Address Library beantwortet eine **unbekannte ID mit
dem Offset des Nachbarn** statt mit einem Fehler (`lower_bound` ohne Prüfung).
Jede aufgelöste Adresse wird deshalb gegen die Sektion geprüft, in der sie
liegen muss.

### Der Fund

```
BGSInventoryItem::SetFavoriteIndex(stackIndex, favoriteIndex)   REL::ID(1349090)
    rcx = das Inventar-Item, edx = der wievielte Stapel, r8b = der Index
```

Die Funktion tut fünf Dinge nacheinander:

1. den passenden Stapel suchen (Vergleichsfunktor nach Stapelnummer),
2. `BGSInventoryItem::WriteStackData` (`REL::ID(224388)`) — führt den
   Schreibfunktor **unter der Inventarsperre** aus, teilt und kopiert dabei
   Stapel nach Bedarf,
3. die Stapel wieder zusammenlegen (`REL::ID(1132179)`),
4. prüfen, ob sich überhaupt etwas geändert hat,
5. und dann **eine Benachrichtigung verschicken** (`REL::ID(178578)`).

**Schritt 5 ist die ganze Geschichte.** Jeder handgeschriebene Versuch hat ihn
ausgelassen. Deshalb blieb ein Gegenstand, dem man den Favoriten von Hand
wegnahm, in einem Zustand hängen, den es im Spiel nicht gibt — die Engine
wusste nichts davon.

### Der zweite Fund: −1 ist ein gültiger Zustand

Der Schreibfunktor darunter ist drei Zeilen lang und ruft
`ExtraDataList::SetFavorite` (`REL::ID(534268)`) auf. Die liest sich so:

| Index | Wirkung |
| --- | --- |
| `0xFE` | das `ExtraFavorite` wird **entfernt** — kein Favorit mehr |
| alles andere | `quickkeyIndex` setzen, das `ExtraFavorite` bei Bedarf **anlegen** |

`0xFF` ist also nicht „kein Favorit", sondern **„Favorit ohne Taste"**. Und
genau diesen Wert schreibt das Spiel selbst, wenn man etwas favorisiert, das
noch keine Taste hat (gefunden im Umschalter, `REL::ID(1508612)`).

**Für die Mod ist das der eigentliche Gewinn:** Ein Seitenwechsel muss nie
etwas löschen. Die Gegenstände der ausgehenden Seite gehen auf −1 und bleiben
Favoriten, die der eingehenden bekommen ihre Taste. Ein abgebrochener Wechsel
hinterlässt schlimmstenfalls einen Favoriten ohne Taste, nie einen kaputten
Gegenstand.

### Was daraus im Code wurde

`SetQuickkeyFunctor`, `MatchQuickkeyFunctor`, `ClearFavoriteFunctor`,
`AddFavoriteFunctor` und `MatchPlainStackFunctor` sind alle weg. Übrig bleibt
**ein** Aufruf:

```cpp
SetFavoriteIndex(item, stackIndex, index);   // 0..11, kNoKey (0xFF), kNotAFavorite (0xFE)
```

Zwei Folgen für den Rest:

- **Der Stapel wird jetzt über seine Nummer adressiert**, nicht über einen
  Vergleichsfunktor. `ReadFavorites` zählt sie beim Durchlaufen mit, und
  `FindStack(object, index)` findet einen geparkten Favoriten wieder.
- **Vor jedem Zug wird neu gelesen.** Schritt 3 oben legt Stapel zusammen und
  kann sie dabei umnummerieren; eine gespeicherte Nummer ist nach einem
  Schreibvorgang nicht mehr verlässlich.

`ApplyPage` bleibt, wie es war — nur der Zug darunter ist ein anderer.

### Der Nachtrag, der eine Nacht gespart hat

`SetFavoriteIndex` allein reichte **nicht**. Im Spiel gemessen: Das Inventar
folgte, `storedFavTypes` nicht, und auf dem Bildschirm passierte gar nichts.
Der Grund steht in der Funktion selbst — sie merkt sich vorher einen Zähler
und verschickt ihre Benachrichtigung nur, wenn er sich geändert hat. Ein
reiner Indexwechsel ändert ihn nicht.

Was alle erreicht, ist der Weg über die Liste:
`BGSInventoryList::FindAndWriteStackDataForItem` verschickt danach das
Ereignis des Inventars. **Beides zusammen ist die Lösung:** der Weg über die
Liste, und darunter der Funktor der Engine, der nichts tut als
`ExtraDataList::SetFavorite` aufzurufen.

```cpp
MatchFavoriteFunctor compare{ from };   // was der Stapel trägt
SetFavoriteFunctor   write{ to };       // ruft REL::ID(534268)
player->inventoryList->FindAndWriteStackDataForItem(object, compare, write);
```

**Im Spiel bestätigt** (2026-09-05, 19:30): parken auf -1, Taste zurückgeben,
Rotation über alle zwölf Plätze. `favorites` und `cache` im Log bleiben
gleich, die Anzeige stimmt, die Taste benutzt den richtigen Gegenstand.

### Was als Nächstes dran ist

1. ~~Das Kreuz bei offenem Menü nachziehen.~~ **Erledigt, Abschnitt 17.**
2. ~~Die Seitenverwaltung.~~ **Erledigt und im Spiel bestätigt, Abschnitt 18.**
3. ~~Das Grid.~~ **Es steht, auf eigenem Menü — Abschnitt 21.** Offen sind
   dort: Tastensteuerung, Zeigerauswahl, das Item-Label über dem Grid, die
   FIS-Symbole (Abschnitt 20) und der Pip-Boy.
4. **Für eine Veröffentlichung** (Abschnitt 19): Tasten über den Weg des
   Spiels statt `GetAsyncKeyState` — Controller, die Belegung des Spielers,
   und keine Auslösung, während jemand in der Konsole tippt. Dazu die Frage
   der Next-Gen-Fassung, vertagt.

---

## 17. Das Kreuz zieht nach (2026-09-05, spät)

Der Schreibweg aus Abschnitt 16 stimmt, aber **auf dem Bildschirm passiert
nichts, solange das Kreuz offen ist**. Es zeichnet aus einer Kopie, die es
beim Öffnen bekommt, und fragt nicht nach. Für einen Seitenwechsel — der
findet ja bei offenem Kreuz statt — wäre das das Ende.

Der Hebel steht seit Abschnitt 13 fest: `Cross_mc.infoArray` ist öffentlich,
und der Setter löst das Neuzeichnen selbst aus. `RefreshCross()` baut die
Liste aus dem Inventar und schreibt sie dorthin.

**Neu ist, wie die Symbole gefunden werden.** `FavIconType` ist eine
Bildnummer, deren Bedeutung nicht im Skript steht; erfinden kann man sie
nicht. Die alte Fassung hat sie zwischen den zwei getauschten Plätzen hin- und
hergeschoben, was bei zwölf Plätzen auf einmal nicht mehr trägt. Jetzt hängt
das Symbol **am Gegenstand**:

- `LearnIcons()` liest die zwölf `Icon_mc.currentFrame` vom Bildschirm und
  legt sie unter dem Gegenstand ab, der gerade dort steht. Das muss laufen,
  **solange Anzeige und Inventar noch übereinstimmen** — also vor der
  Änderung, nicht danach.
- `RefreshCross()` baut die Liste danach neu und gibt jedem Gegenstand sein
  gemerktes Symbol mit.
- Was noch nie zu sehen war, bekommt Bild 1 — die leere Zelle. Lieber leer als
  falsch, und im Log steht es.

Die Merkliste hält die ganze Sitzung. Ein Gegenstand einer anderen Seite war
auf dem Kreuz, als jene Seite dran war, also ist er bekannt, sobald es darauf
ankommt.

**Regel, die dabei wieder gilt:** Scaleform nur aus dem richtigen Faden. Beide
Funktionen laufen ausschließlich in einer UI-Task, wie alles andere, was das
Spiel anfasst.

---

## 18. Die Seiten (2026-09-05, spät)

### Der Seitenwechsel ist zwei Durchgänge geworden

Abschnitt 15 hatte eine Maschinerie, die die Züge so ordnete, dass nur auf
eine Taste geschrieben wird, die niemand mehr braucht — mit Ringen,
Parkplätzen und einem ungemessenen Sonderfall, wenn alle zwölf Tasten belegt
sind. **Das ist alles weg.** Seit -1 ein eigener Zustand ist (Abschnitt 16),
geht es geradeaus:

1. **Alle zwölf parken.** Jeder Favorit geht auf -1. Damit ist jede Taste
   frei, und kein Gegenstand hat dabei etwas verloren außer seiner Taste.
2. **Die Tasten der neuen Seite vergeben.** Für jeden Eintrag den Stapel
   suchen, der geparkt ist (oder gar kein Favorit ist), und ihm die Taste
   geben.

Keine Ringe, kein Parkplatzproblem, und der Fall „alle zwölf belegt" ist keiner
mehr. Ein abgebrochener Wechsel hinterlässt Favoriten ohne Taste — im Pip-Boy
sichtbar, mit einem Handgriff wieder zuzuweisen, nie kaputt.

Der zweite Gewinn: Der alte Weg konnte nur umsortieren, was schon auf den
zwölf Tasten lag. Eine Seite enthält aber Gegenstände, die gerade **nicht**
darauf liegen. Erst damit ist es ein Seitenwechsel.

### Was eine Seite ist

Zwölf Gegenstände. Die Seite, die gerade gespielt wird, steht **nicht** in der
Liste — sie steht im Inventar, und der Spieler darf sie jederzeit über den
Pip-Boy ändern. Deshalb wird sie vor dem Verlassen zurückgelesen
(`RememberCurrentPage`). So gehört ein von Hand vergebener Favorit zu der
Seite, auf der er vergeben wurde.

`PAGEDOWN` und `PAGEUP` blättern, `PageCount` in der neuen `[Pages]`-Sektion
sagt wie viele (1 bis 32, Standard 3). Seite 1 ist das, was der Charakter
hatte, als die Seiten das erste Mal benutzt wurden.

### Die Seiten reisen im Spielstand

F4SE hat ein Co-Save, SFSE nicht — das ist der eine Punkt, an dem die
Fallout-4-Fassung es leichter hat als das Original. Registriert wird in
`F4SEPlugin_Load` (später geht es nicht), gespeichert werden Anzahl, die
gespielte Seite und 12 Form-IDs je Seite. Beim Laden geht jede Form-ID durch
`ResolveFormID`, was einen veränderten Ladeauftrag übersteht; was sich nicht
mehr auflösen lässt, fällt still weg. `RevertCallback` räumt alles ab, denn
ein anderer Spielstand hat andere Seiten.

**Eine Falle, die dabei auffiel:** Die Tasten stehen jetzt in zwei
verschiedenen Sektionen. Eine Einstellung unter der falschen Überschrift wird
als „nicht vorhanden" gelesen, und der Standard gewinnt stillschweigend —
deshalb nimmt der Leser die Sektion als Argument.

### Was noch zu messen ist

Alles davon. Zu prüfen sind: blättern hin und zurück, ein Favorit von Hand
gesetzt und dann geblättert, Spielstand speichern und laden, und was passiert,
wenn ein Gegenstand einer anderen Seite nicht mehr im Inventar ist.

---

## 19. Was die Mod werden soll (Entscheidungen vom 2026-09-05, nachts)

### Eine Mod, mehrere Anzeigen

Die Seiten hängen an **keiner** Zeile Grid-Code — sie arbeiten mit dem
Vanilla-Kreuz. Das Grid ist ein Aufsatz, keine Voraussetzung. Alexanders
Beobachtung dazu: Für Fallout 4 gibt es bisher nichts, was mehrere
Favoritenseiten anbietet; die Seiten allein sind also schon der Grund, warum
jemand die Mod installiert.

**Entschieden:** Eine Veröffentlichung, die **beides** enthält, mit
umschaltbarer Anzeige — Vanilla-Kreuz oder Grid, vielleicht später eine dritte
Form. Umgestellt wird über **MCM**, wenn es geht, sonst über die INI.

Der Name auf Nexus ist unabhängig vom Dateinamen der DLL. Die Seite darf also
sagen, dass es um Seiten geht, ohne dass Repo, Plugin oder INI umbenannt
werden müssen.

### MCM aus einem C++-Plugin

Bei Alexander liegt `Interface\MCM.swf`, MCM ist also installiert. Für ein
Plugin ohne Papyrus geht es so: MCM bekommt eine Konfiguration unter
`Data\MCM\Config\<Mod>\config.json` und schreibt die Auswahl des Spielers nach
`Data\MCM\Settings\<Mod>.ini`. Die liest das Plugin wie jede andere INI —
neu einlesen am besten dann, wenn das Favoritenmenü aufgeht, damit eine
Änderung ohne Neustart ankommt. Zu prüfen, wenn es soweit ist.

### FIS-Symbole (der interessante Teil)

**Wie FIS arbeitet**, nachgesehen in
`Data\Interface\ItemSorter\FIS (FallUI Item Sorter).xml`: Gegenstände werden
zu `[Tag] Name` umbenannt (`tagWrapper="SQUARE_BRACKET"`), und
`FIS Categories.xml` ordnet jedem Tag über `icontag` ein Symbol zu. Die Menüs
von FallUI und DEF_UI schlagen dieses Symbol in einer Symbolbibliothek nach —
bei Alexander `Data\Interface\FallUI_IconLib.swf` und `iconlibs2.swf`.

**Das Favoritenkreuz ist keines dieser Menüs.** Es druckt den Tag, wie er
dasteht: `[Aid] Antibiotics`. Genau so stand es die ganze Nacht in unseren
Logs, ohne dass es jemandem aufgefallen wäre — ein Beleg, der schon vorlag.

**Sofort erledigt:** Der Tag wird aus der Beschriftung genommen
(`StripItemTags=1`, neue `[Display]`-Sektion). Aus `[Aid] Antibiotics` wird
`Antibiotics`.

**Der eigentliche Plan fürs Grid:** die Symbolbibliothek laden und das Symbol
zum Tag selbst zeichnen — dasselbe, was FallUI tut. **Die Zuordnung ist
inzwischen vollständig aufgeklärt, siehe Abschnitt 20.** Die Fragen von
vorhin, der Vollständigkeit halber:

1. Wie heißen die Symbole in `FallUI_IconLib.swf`? Mit JPEXS nachsehen, dem
   Werkzeug, mit dem schon `FavoritesMenu.swf` gelesen wurde. Wenn die
   Linkage-Namen den Tag-Schlüsseln entsprechen, ist die Zuordnung geschenkt.
2. Lässt sich eine fremde SWF in die Bühne von `FavoritesMenu` laden und ein
   Symbol daraus anhängen? Das ist der Weg, den DEF_UI geht.
3. Wenn ja: Geht das auch für die **zwölf Zellen des Vanilla-Kreuzes**? Deren
   `Icon_mc` ist ein MovieClip wie jeder andere. Dann bekämen auch Spieler
   ohne Grid die Symbole.
4. Was passiert ohne FIS? Dann gibt es keine Tags, und alles bleibt, wie es
   ist — die Symbole des Spiels aus `FavIconType`.

Punkt 3 wäre der Fund: FIS-Symbole im normalen Favoritenmenü, was es bisher
nirgends gibt.

---

## 20. Die FIS-Symbole sind vollständig aufgeklärt (2026-09-05, nachts)

Kein Ratespiel nötig — die ganze Kette steht in Dateien, die im Spielordner
liegen, und `tools/swfnames.py` liest sie nach.

### Die Kette

```
Gegenstand   "[Stimpak] Stimpak"          -- so benennt FIS ihn um
   |
Tag          Stimpak                       -- eckige Klammern, tagWrapper="SQUARE_BRACKET"
   |
<tag keyword="Stimpak" icon="M8r.Repo.MedSyringe" colorname="MedicLightRed" />
   |                                       in ItemSorter\FIS (FallUI Item Sorter).xml
Symbol       m_M8r.Repo.MedSyringe         -- Klammer-Präfix "m_" davor
   |
Bibliothek   Interface\FallUI_IconLib.swf  -- 209 Symbole
```

**Geprüft: 241 von 241 Tags finden ihr Symbol.** Das Präfix `m_` vor dem Wert
aus `icon=` ist die einzige Übersetzung, die dazwischen steht.

```
py -3 tools/swfnames.py tags "ItemSorter/FIS (FallUI Item Sorter).xml"
py -3 tools/swfnames.py names FallUI_IconLib.swf --filter Syringe
```

Nebenbei: `iconlibs2.swf` ist eine zweite Bibliothek (294 Symbole, Rüstungen
von `abc`), aber keiner der FIS-Tags zeigt darauf. Für den Anfang reicht
`FallUI_IconLib.swf`.

Ein `colorname` steht bei jedem Tag mit dabei, aufgelöst über
`ItemSorter\ColorSets\`. Farbe ist Kür; erst kommen die Symbole.

### Was das für das Grid heißt

Beim Start einmal die Tag-Konfiguration lesen (welcher Sorter installiert ist,
sagt `ItemSorter\Auto-detect.xml`), daraus Tag → Symbolname bauen, die
Bibliothek in die Bühne laden und pro Zelle das passende Symbol anhängen.
Genau das tut FallUI in seinen eigenen Menüs.

**Was noch offen ist** — aber nur noch Handwerk, keine Erkenntnis:

1. Eine fremde SWF in die Bühne von `FavoritesMenu` laden. Scaleform kann das;
   der Weg ist derselbe, den DEF_UI geht.
2. Ein Symbol daraus an einen Clip hängen.
3. Ob das auch für die zwölf `Icon_mc` des **Vanilla-Kreuzes** geht. Wenn ja,
   bekämen auch Spieler ohne Grid die FIS-Symbole — das wäre der eigentliche
   Fund, denn bisher zeigt das Favoritenmenü sie nirgends.
4. Ohne FIS gibt es keine Tags; dann bleibt alles bei `FavIconType` aus dem
   Spiel. Der Fall muss sauber durchfallen, nicht auffallen.

---

## 21. Das Grid bekommt ein eigenes Menü (2026-09-05, Nacht)

**Der Stand:** Das Grid steht mittig auf dem Bildschirm, auf einer **eigenen
Bühne**, mit **Mauszeiger statt Kamerasteuerung**, und die Zifferntasten des
Spiels funktionieren unverändert weiter. Das ist der Punkt, an dem diese
Sitzung endet.

### Warum ein eigenes Menü

Drei Anläufe auf fremder Leinwand, alle mit derselben Ursache — das Grid war
**Gast**:

| Leinwand | Ergebnis |
| --- | --- |
| Bühne von `FavoritesMenu` | Nur der Streifen sichtbar, den das Kreuz ohnehin neu malt |
| Wurzelclip von `FavoritesMenu` | **Das ganze Menü verschwand**, Panel meldete Position `-107374182` |
| `HUDMenu` | Zeichnet sauber, aber der HUD nimmt keine Eingabe — ein Zeiger dort hieße, der Spielersteuerung die Maus zu entreißen |

Dazu kam die Erkenntnis aus dem Spiel: **Die Maus bewegte weiterhin die
Kamera.** Ein Zeiger auf dem HUD wäre also ein zweiter Eingriff an einer
Stelle gewesen, die mit Favoriten nichts zu tun hat.

Ein Menü ist das, was das Spiel für all das schon hat: Es bekommt die
Eingabe, es kann einen Cursor führen, und es besitzt seine eigene Bühne.
**FavoritesMenuEx** (im Ordner entpackt, siehe unten) macht genau das, und
deshalb reicht ihr F4SE als Anforderung.

### Wie es gebaut ist

- **`tools/build_swf.py`** schreibt `Interface/FavoritesMenuGrid.swf`: **36
  Bytes**, Header plus vier Tags, kein Zeitstrahl, kein ActionScript. Nur
  `FileAttributes` mit dem AS3-Bit (sonst gilt das Movie als AS2 und die
  Anzeigeklassen, die das Plugin über ihren Namen erzeugt, gibt es nicht).
  JPEXS liest die Datei gegen.
- **`src/menu.cpp`** registriert das Menü (`UI::RegisterMenu`), lädt die SWF
  mit `LoadMovieEx` und Hintergrundalpha 0 und meldet über `SetOnReady`, wenn
  gezeichnet werden kann — das Menü taucht erst **nach** seinem Konstruktor
  in der Menüliste des Spiels auf.
- Es öffnet und schließt mit `FavoritesMenu`; das Kreuz wird dabei versteckt.

**Die Flags sind der Kern und waren zweimal falsch:**

```cpp
menuFlags.set(
    kUsesCursor,        // der Zeiger
    kUsesMenuContext,   // nimmt der Kamera die Maus -- das ist die Zeile,
                        // die den ganzen Umbau gerechtfertigt hat
    kUpdateUsesCursor,
    kRequiresUpdate);
```

- **Kein `kPausesGame`:** Ein Favoritenmenü soll schnell sein, nicht das
  Spiel anhalten.
- **Kein `kCustomRendering`.** Das war der Fehler, der eine Runde gekostet
  hat: Es sagt „dieses Menü malt sich selbst", woraufhin das Spiel es aus
  seinem Renderdurchgang auslässt. Das Grid wurde gezeichnet — 40 Kinder,
  richtige Größe, richtige Position, alles im Log — und niemand hat es
  gerendert.

**Zwei Fallstricke, die Zeit gekostet haben:**

1. **Ein Menü ohne benannten Clip hat kein `menuObj`.** Unsere SWF ist leer,
   es gibt nichts zu benennen. Die Bühne hängt an der Wurzel:
   `uiMovie->GetVariable(&root, "root")`, dann `root.stage`. Wer `menuObj`
   verlangt, hält ein offenes Menü für geschlossen.
2. **`GetPrivateProfileIntW` liefert `UINT`.** `GridX=-1` („mittig") kam als
   **4294967295** an, und das Panel stand vier Milliarden Einheiten neben dem
   Bildschirm. Das hat zwei Messrunden entwertet — der HUD war nie das
   Problem. Jede vorzeichenbehaftete Einstellung braucht den Cast.

### Was offen ist

1. **`w`/`a`/`s`/`d` tun im neuen Menü nichts.** Sie gingen vorher an die
   Kreuzlogik; jetzt gehen sie ins Leere. Gewünscht: `w`/`s` blättert Seiten,
   `a`/`d` wandert zwischen den Tasten einer Seite. Das Menü bekommt die
   Eingabe selbst — `IMenu` hat `ProcessMessage`, und für Tasten gibt es den
   Weg über einen eigenen `BSInputEventUser`.
2. **Der Zeiger wählt noch nichts aus.** Mausposition auf Zellen abbilden,
   die getroffene hervorheben, Klick benutzt sie. Zum Benutzen gibt es
   `FavoritesManager::UseQuickkeyItem` (aus den Zeichenketten von
   FavoritesMenuEx). Liegt die getroffene Zelle auf einer anderen Seite,
   erst blättern.
3. **Das Item-Label ist verschwunden.** `ItemName_tf` und `ItemAmmo_tf`
   gehören dem Favoritenmenü, dessen Kreuz jetzt versteckt ist. Sie sollen
   **mittig über dem Grid** stehen, wie in der Starfield-Fassung. Entweder
   die Felder des Spiels weiterverwenden (sie liegen auf dessen Bühne, nicht
   auf unserer) oder ein eigenes Label auf unserer Bühne, gespeist aus dem,
   was der Zeiger gerade trifft. **Das eigene Label ist der bessere Weg** —
   es gehört zur Auswahl, und die machen künftig wir.
4. **Das Design.** Die Zellenfarbe ist inzwischen **aus der Vanilla-SWF
   gelesen**: `FavoritesMenu.swf`, erste `DefineShape4`, Füllung
   `ff ff ff 33` — Weiß mit Alpha 0x33, also ein Fünftel. Das ist der Wert
   der gespielten Seite; die anderen laufen auf der Hälfte. Weiter zu holen
   wäre der Rahmen (eine `LINESTYLE2` in derselben Form) und die
   Textformate. Die entpackte Vanilla-SWF liegt unter
   `Claude Code/FavoritesMenu-vanilla/`.
5. **Die Symbole**, unverändert wie in Abschnitt 20 beschrieben. Auf eigener
   Bühne ist das Laden der Bibliothek jetzt einfacher, weil niemand
   dazwischenfunkt.
6. **Der Pip-Boy.** Das „ASSIGN FAVORITE"-Kreuz ist ein eigenes Menü und
   zeigt noch gar nichts von uns. Beim Zuweisen will man sehen, auf welche
   **Seite** man legt.

### Was nicht mehr gebraucht wird

Der Eingabe-Haken auf `FavoritesManager` (`src/input.cpp`) hat nie ein
Ereignis geliefert. Die Vtable war **richtig** — sie sitzt an Objekt-Offset
`0x10`, nicht vorn, das zeigt der Speicherauszug im Log — die Ereignisse
gehen also woanders lang. Mit eigenem Menü ist die Frage hinfällig; die Datei
bleibt vorerst liegen, weil ihre Messungen dokumentiert sind, und fliegt
raus, sobald die Eingabe über das Menü läuft.

### Werkzeuge, die in dieser Nacht dazugekommen sind

| Werkzeug | wofür |
| --- | --- |
| `tools/f4dis.py` | IDs auflösen, Vtables lesen, Engine-Code disassemblieren |
| `tools/swfnames.py` | Symbole einer SWF auflisten, Sorter-Tags dagegen prüfen |
| `tools/build_swf.py` | die leere Bühne des eigenen Menüs schreiben |
| `src/peek.cpp` | Maschinencode aus dem laufenden Spiel kopieren (F10) |

## 22. Das Grid nimmt die Eingabe (2026-09-06)

**Der Stand:** Die Punkte 1 und 2 aus Abschnitt 21 sind gebaut. `w`/`s`
blättert zwischen den Zeilen, `a`/`d` zwischen den Zwölf einer Zeile, der
Zeiger markiert die Zelle, über der er steht, und Return oder ein Linksklick
benutzt sie. Der Bau ist sauber (`/W4 /WX`) und deployed. **Im Spiel getestet
ist er noch nicht.**

### Warum die Eingabe nicht am Menü hängt

Ein Menü bekommt die Tasten nicht dadurch, dass es ein Menü ist. `MenuControls`
hält ein Feld von `BSInputEventUser*`, läuft es der Reihe nach durch, und der
erste, der ein Ereignis für seines erklärt, beendet den Durchlauf. Genau
dorthin setzt sich das Grid — **an den Anfang** (`handlers.emplace(begin(),
…)`), und `ShouldHandleEvent` antwortet **pro Ereignis**: nur für die fünf
Tasten und den linken Mausknopf. Alles andere — die Ziffern, die Schließtaste,
das Gamepad — läuft unverändert an uns vorbei. Ein pauschales Ja hätte das
Menü unverschließbar gemacht.

FavoritesMenuEx macht es genauso: seine Zeichenketten nennen eine eigene
Klasse `FavoritesMenuExInput` mit je einer Zeile fürs An- und Abmelden.

**Der alte Haken auf `FavoritesManager` ist raus.** Die Vtable war richtig; die
Ereignisse kamen nie, weil der Manager ein *single user* ist und nur bedient
wird, solange sein eigenes Menü die Eingabe hat. `src/input.cpp` ist jetzt der
neue Weg, die Messung steht als Kommentar darin.

### Der Zeiger kostet nichts

`MenuCursor::GetSingleton()` führt den Cursor bereits, weil das Menü ihn mit
`kUsesCursor` angefordert hat — **in Bildschirmpixeln**. Der Panel liegt in
Bühneneinheiten. Beides beantwortet das Menü selbst: `GetViewport()` gibt das
eine (hier 2560x1440), `GetVisibleFrameRect()` das andere (0,0 bis 1280,720).
Damit ist die Umrechnung auflösungsunabhängig, und niemand muss Mausbewegungen
mitzählen.

Gelesen wird einmal pro Bild in `AdvanceMovie` unseres Menüs — auf dem
UI-Thread, wo Scaleform sowieso erlaubt ist. Hat sich der Zeiger nicht bewegt,
passiert nichts; sonst wird die getroffene Zelle bestimmt.

**Die Markierung ist ein eigener Sprite**, einmal gezeichnet und danach nur
noch verschoben. Vierzig Textfelder pro Mausbewegung neu zu bauen wäre der
naheliegende und der falsche Weg.

**Wer zuletzt bewegt hat, hat die Marke.** Der Zeiger meldet sich nur, während
er über einer Zelle steht, also wirft ein Verlassen des Panels keine
Tastenwahl weg. Umgekehrt vergisst eine Taste die letzte Zeigerposition, damit
die stillliegende Maus die Wahl nicht sofort zurückholt.

### `UseQuickkeyItem` — gefunden, aber noch nicht bewiesen

Es gibt keine Address-Library-ID dafür. FavoritesMenuEx trägt die Signatur in
sich:

```
FavoritesManager_useQuickkey (FavoritesManager::UseQuickkeyItem)
E8 ? ? ? ? 83 F8 0C 74 04
```

Ein Aufruf, dessen Antwort mit **zwölf** verglichen wird — der Zahl der
Tasten. `src/use.cpp` sucht das Muster im Codeabschnitt des **laufenden**
Spiels (die EXE ist auf der Platte gepackt), löst das `E8` auf und verlangt,
dass **alle** Fundstellen auf **eine** Funktion zeigen. Sind es mehrere, wird
nichts aufgerufen und die Adressen stehen im Log.

**Der Beweis wird vor dem ersten Aufruf geschrieben.** `peek::Note` legt
0x140 Bytes der Funktion und 0x40 Bytes um die Fundstelle in
`FavoritesMenuGrid.found.txt` neben dem Log — im selben Format, das
`tools/f4dis.py peek` liest. Stürzt der erste Aufruf das Spiel ab, steht
trotzdem da, was aufgerufen wurde.

**Also beim nächsten Start zuerst:**

```
py -3 tools/f4dis.py peek "...\FavoritesMenuGrid.found.txt"
```

Erwartet wird eine Funktion, die `this` und einen Index nimmt. Sieht sie nach
etwas anderem aus, ist der Aufruf in `use::Quickkey` die eine Zeile, die
zurückgebaut werden muss. Das Log nennt zusätzlich die ID, falls die Address
Library die Adresse exakt kennt — dann kann sie in Zukunft fest verdrahtet
werden statt gesucht.

### Neue Einstellungen

Ein Abschnitt `[Controls]`: `GridPageUpKey` (W), `GridPageDownKey` (S),
`GridLeftKey` (A), `GridRightKey` (D), `GridUseKey` (RETURN),
`GridUseOnClick` (1), `GridCloseAfterUse` (1). `ParseVirtualKey` kennt jetzt
auch RETURN/ENTER, ESCAPE und die vier Pfeiltasten.

`deploy.py` überschreibt die installierte INI nie, weil sie die Tasten des
Spielers hält — der Abschnitt wurde ihr diesmal von Hand angehängt.

### Was offen bleibt

- **Punkt 3, das Item-Label.** Ohne es sagt das Grid weiterhin nur *dass* eine
  Zelle belegt ist, nicht *womit*. Die Markierung zeigt also auf etwas
  Namenloses. Das ist jetzt der nächste Schritt, und die Auswahl, die es
  speisen soll, steht bereit: `g_marked` in `main.cpp`.
- **Das Gamepad** ist bewusst außen vor. Es geht weiter ans Kreuz; ein
  falscher Griff dort nähme dem Controller das Menü ganz.
- Punkte 4 bis 6 aus Abschnitt 21 unverändert.

## 23. Der Klick stürzt ab, und warum (2026-09-06)

**Getestet:** WASD läuft durch die Reihen, die Markierung folgt, der Zeiger
markiert. **Der Linksklick hat das Spiel abgestürzt** — und die Vorsorge aus
Abschnitt 22 hat den Fall beantwortet, ohne dass eine zweite Messrunde nötig
war.

### Die Signatur führt woandershin

`FavoritesMenuGrid.found.txt` enthielt die Funktion, die aufgerufen wurde,
`0x1271480` (ID **1330478**). Disassembliert ist sie eindeutig **kein**
`UseQuickkeyItem`:

```
mov  ebx, 0xc                    ; die Antwort, falls nichts passt
mov  rdi, rdx                    ; das zweite Argument ist ein Zeiger
...
cmp  qword ptr [rdi], rax        ; [Objekt] gegen eine bekannte Vtable
je   -> ebx = 0
cmp  qword ptr [rdi], rax        ; die nächste
je   -> ebx = 1
...
```

Ein **Typ-Klassifizierer**: er vergleicht die Vtable eines Formulars gegen ein
Dutzend bekannter und antwortet mit 0…11, zwölf heißt „keine davon". Wir haben
ihm als `rdx` den Tastenindex **1** übergeben — also wurde von Adresse 1
gelesen. Der Absturz ist damit vollständig erklärt.

Die Aufrufstelle `0x126f8eb` liegt in **`ShouldHandleEvent`** (0x126f6c0), nicht
in einem Benutzen-Pfad: dort wird geprüft, ob die Taste überhaupt etwas
Brauchbares hält. Die Signatur aus FavoritesMenuEx ist also nicht falsch
abgeschrieben — sie zeigt nur nicht dorthin, wo wir sie vermutet haben.

### Gesucht wird nicht mehr, gelesen wird

Vtables liegen in `.rdata` und sind **auch aus der gepackten EXE lesbar**. Die
Eingabe-Vtable von `FavoritesManager` (Objektoffset 0x10, RVA `0x2d874f8`) gibt:

| Slot | | RVA | ID |
| --- | --- | --- | --- |
| 1 | `ShouldHandleEvent` | 0x126f6c0 | 1342580 |
| 2–7 | die leeren Vorgaben | 0x1e07xx | |
| 8 | `OnButtonEvent` | **0x126f930** | **1049251** |

`OnButtonEvent` ist die Stelle, an der das Spiel einen Favoriten benutzt, wenn
eine Ziffer gedrückt wird. `use::Find` schreibt sie jetzt bei jedem Start
**ganz** nach `FavoritesMenuGrid.found.txt` (Länge 0 heißt bei `peek::Note`
neuerdings „die ganze Funktion"). Der nächste Schritt ist also kein Raten mehr,
sondern:

```
py -3 tools/f4dis.py peek "...\FavoritesMenuGrid.found.txt"
```

und der Aufruf darin ablesen.

**Bis dahin wird nichts aufgerufen.** Die Adresse kommt aus der INI —
`[Debug] UseQuickkeyID=` oder `UseQuickkeyRVA=` — und ohne sie protokolliert
ein Klick nur, dass er nichts tun kann. Steht die Adresse erst fest, ist sie
ohne Neuübersetzung prüfbar; danach kann sie fest verdrahtet werden.

### Alle Seiten gleich

Die gespielte Seite wird nicht mehr hervorgehoben — kein doppeltes Alpha, keine
gedimmten Zeilennummern. Die Engine gibt die zwölf Tasten immer nur einer Seite,
aber das ist ihre Angelegenheit und nicht die des Spielers: mit Zeiger und
Tasten ist jede Zelle einen Zug entfernt, und eine hervorgehobene Seite würde
sagen, die anderen seien weiter weg.

Folgerichtig fällt zweierlei weg:

- **Der Seitenzähler im Titel.** „Favorites 2 / 3" zählte von einem „hier",
  das es nicht mehr gibt. Es steht nur noch `PageIndicatorText`.
- **Das Blättern mit Bild auf/ab.** Mit dem Grid braucht es das nicht; das
  Benutzen einer Zelle blättert unterwegs von selbst. Die Tasten bleiben für
  `UseGrid=0` erhalten.

**Das bleibt zu bedenken:** Die Zifferntasten des Spiels wirken weiterhin auf
die Seite, die die Engine gerade hält — und die ist auf dem Panel nun nicht
mehr zu erkennen. Wer „3" drückt, benutzt nicht unbedingt, was er unter der
Drei sieht. Das ist der Preis der einheitlichen Darstellung und war eine
bewusste Entscheidung.

### Das Design, aus der Vanilla-SWF statt nach Augenmaß

Die einzige `DefineShape4` in `FavoritesMenu.swf` ist ein Quadrat von 100 × 100
Einheiten:

- Füllung `ff ff ff 33` — Weiß bei einem Fünftel. War schon übernommen.
- **Der Rahmen ist keiner.** Der einzige `LINESTYLE2` ist 1 Einheit breit,
  Farbe `00 ff 00`, Alpha **2 von 255**. Das ist ein Überbleibsel, kein
  Strich: Fallout 4 zeichnet um seine Zellen nichts. `kCellLineAlpha = 0.0`
  war also richtig geraten, und die offene Aufgabe „Rahmen holen" aus
  Abschnitt 21 löst sich auf.

Die vier `DefineEditText` geben das Textformat. Das für `Quickkey_tf`:

| | |
| --- | --- |
| Schrift | `AIN_Font_Bold` (aus `fonts_en.swf` importiert) |
| Höhe | **36** von 100 Zelleneinheiten |
| Farbe | Weiß, voll deckend |
| Ausrichtung | zentriert |
| Kasten | x −5…48, y −2…51 — das linke obere Viertel |

Alles davon ist jetzt als Bruchteil der Zelle in `grid.cpp` hinterlegt
(`kKeyTextSize` 0.36, `kKeyBoxLeft` −0.05, `kKeyBoxTop` −0.02,
`kKeyBoxWidth` 0.53), damit die eine Zahl in der INI weiterhin alles bewegt.
Die Zifferntaste war vorher 0.19 der Zelle und linksbündig — also halb so groß
wie im Spiel und an der falschen Stelle.

**Die Schrift selbst ist nicht übernommen.** `AIN_Font_Bold` liegt in der
Schriftbibliothek des Spiels, und unsere SWF hat 36 Bytes und keine Schrift.
Ob ein Name aus `fonts_en.swf` auf unserer eigenen Bühne ankommt oder eine
Reihe Kästchen ergibt, ist ungemessen. Das ist der nächste Anlauf beim Design
— zusammen mit den Symbolen, die dasselbe Problem in größer sind.

## 24. Das Gitter wird ein Gitter (2026-09-06)

**Getestet:** Kein Absturz mehr beim Klicken. Der Zeiger **markiert aber nichts**
— gemessen wird das jetzt, statt weiter zu vermuten.

### Die Darstellung

Zwei Änderungen, beide aus dem Screenshot heraus entschieden:

- **Jede Zelle bekommt dieselbe Platte**, ob die Taste etwas hält oder nicht.
  Vorher blieb eine leere Taste ein blasser Umriss, und weil auf Seite 2 und 3
  nichts geparkt war, sahen die unteren Reihen aus wie ein Schaden am Gitter.
  Eine leere Taste ist trotzdem eine Taste — sie ist der Platz, auf den etwas
  kann. Was auf einer Taste liegt, wird künftig ihr Symbol sagen, nicht das
  Vorhandensein ihrer Platte.
- **Die Tastennamen stehen einmal über der ersten Reihe**, nicht in jeder
  Zelle. Fallout 4 wiederholt sie pro Zelle, weil es eine Reihe hat; bei drei
  sind dieselben zwölf Zahlen dreimal ein Muster, an dem das Auge vorbeilesen
  muss, um zu sehen, was wirklich dasteht. Eine Spalte beschriftet man am Kopf.

Dafür hat `Metrics` eine `keyRowHeight` bekommen, und `RowTop` geht jetzt über
`KeyRowTop` — Zeichnen und Treffertest lesen weiterhin aus denselben zwei
Zeilen, sonst läge eine Zelle für die Maus woanders als für das Auge.

### Der Zeiger: drei Zahlen statt einer Vermutung

Der Cursor **ist** da (das Spiel zeichnet ihn selbst — ein eigener wäre einer
zu viel gewesen; der Versuch wurde wieder herausgenommen). Er markiert nur
nichts. Dafür gibt es drei Erklärungen, die von außen gleich aussehen: der
Cursor steht still, er zählt in einer anderen Einheit als angenommen, oder das
Ergebnis landet neben dem Panel.

`grid::Pointer` schreibt deshalb **einmal pro geöffnetem Panel** alles auf, was
in die Rechnung eingeht: Cursorposition, sein eigenes Minimum und Maximum, wie
viele Cursor registriert sind, den Viewport, den sichtbaren Rahmen, das
Ergebnis in Bühneneinheiten, die Ausmaße des Panels — und ob das auf eine Zelle
fällt. Eine Zeile, und die drei Fälle sind unterscheidbar.

Gleichzeitig rechnet er nicht mehr über den Viewport, sondern über
`minCursorX`…`maxCursorX`, die der Cursor selbst mitbringt. Das bleibt richtig,
in welcher Einheit er auch zählt — und genau eine solche Annahme hat dieses
Projekt schon einmal einen Abend gekostet (`GridX=-1` als `UINT`).

## 25. Der Zeiger stimmt (2026-09-06)

**Gemessen:**

```
grid: the cursor is at 1919,1073 of 0,0 to 2560,1440 (1 registered);
the viewport is 2560x1440 at 0,0 and the frame 0,0 to 1280,720;
that makes 960,536 on the stage, and the panel runs 313,254 to 967,456
```

Der Cursor zählt also **Bildschirmpixel**, ein Cursor ist registriert, und die
Umrechnung über `minCursorX`…`maxCursorX` trifft. Im Spiel sitzt die Markierung
jetzt unter dem Pfeil. Die vorherige Rechnung über den Viewport war nicht
falsch, aber sie hätte nur zufällig gestimmt; die Grenzen bringt der Cursor
selbst mit.

### Der Dump war ein Bruchstück

`peek::Note` mit Länge 0 nimmt „die Funktion um die Adresse" aus der
Ausnahmetabelle — und die listet eine aus Stücken gebaute Funktion **je Stück**.
Von `OnButtonEvent` kamen so **35 Bytes**, die mitten in der Antwort aufhörten:

```
0x0126f930  push rdi
0x0126f936  xorps xmm0, xmm0
0x0126f939  mov  rdi, rdx
0x0126f93c  ucomiss xmm0, [rdx + 0x38]   ; value == 0 ?
0x0126f940  jne  0x126fa67
0x0126f946  comiss  xmm0, [rdx + 0x3c]   ; heldDownSecs
0x0126f94a  ja   0x126fa67
0x0126f950  mov  rax, [rdx]              ; -- hier war Schluss
```

Man sieht immerhin schon, dass die Funktion `QJustPressed` von Hand prüft. Der
Rest kommt mit einer **festen Länge** von 0x280 statt der Ausnahmetabelle; die
nächste Funktion, die die Vtable nennt, liegt 0x150 dahinter.

### Zwei Dinge weniger auf dem Bild

- **Kein Titel mehr.** „Favorites" über einem Gitter aus Favoriten sagt nichts,
  was das Gitter nicht sagt. Der Streifen darüber fällt mit weg (`titleHeight`
  ist 0, wenn kein Titel da ist) und gehört künftig dem Namen und der Anzahl
  dessen, was markiert ist.
- **Das Crosshair verschwindet**, solange das Favoritenmenü offen ist. Es steht
  in der Mitte des Bildschirms, wo das Gitter ist, und zielt auf nichts. Der
  Name `HUDCrosshair_mc` stammt aus der Vanilla-`HUDMenu.swf`; wer einen
  ersetzten HUD fährt, behält sein Crosshair, und das Log zählt dann die Kinder
  des HUD auf, damit der nächste Name feststeht statt geraten wird.
  Schalter: `[Display] HideCrosshair`.

## 26. Wie Fallout 4 seine Menüs einfärbt (2026-09-06)

**Beobachtung:** Das Grid folgte der HUD-Farbe nur teilweise — Titel und
Zeilennummern waren blau wie der HUD, die Platten und Tastennamen weiß.

Das war so gebaut, und die Begründung war falsch. In `FavoritesMenu.swf` ist
die Zelle weiß (`ff ff ff 33`), und **nichts im ActionScript des Menüs setzt
jemals eine Farbe** — geprüft in `FavoritesMenu.as`, `FavoritesCross.as`,
`FavoritesEntry.as` und `GlobalFunc.as`. Die Färbung macht die Engine:

```cpp
class BSGFxShaderFXTarget : BSGFxDisplayObject, BSTEventSink<ApplyColorUpdateEvent>
    void SetToHUDColor(bool a_useWarningColor)
        CreateAndSetFiltersToHUD(HUDColorTypes::kGameplayHUDColor, 1.0);
```

Ein Anzeigeobjekt wird in so ein Ziel gewickelt, bekommt Filter aufgesetzt und
meldet sich für `ApplyColorUpdateEvent` an, damit die Filter bei jeder
Farbänderung erneuert werden. **Weiß ist nur das, worauf getönt wird.**

Unser Menü ist unser eigenes und hängt an keinem davon. Also wird die Farbe
eine Stufe früher eingesetzt: Platten, Tastennamen, Zeilennummern und die
Markierung zeichnen in der HUD-Farbe des Spielers, die Alphawerte bleiben die
gemessenen (Platte 0.20, Markierung 0.18 gefüllt und 0.9 im Strich — heller,
nicht anders). Neue Einstellung `[Display] GridColor`, gleiche Regel wie beim
Seitenmarker: über Weiß heißt „nimm die HUD-Farbe".

**Der saubere Weg wäre der andere:** `RE::BSGFxShaderFXTarget` ist in
CommonLibF4 vollständig konstruierbar (`BSGFxShaderFXTarget(const
Scaleform::GFx::Value&)`), unser Panel ließe sich damit umwickeln und würde
Powerrüstungs- und Warnfarben von selbst mitmachen. Dagegen sprach für heute
nur, dass es ein Engine-Aufruf über eine Bibliotheks-ID mehr ist — und davon
waren in diesem Projekt schon zwei falsch. Steht als Möglichkeit, nicht als
Schuld.

## 27. `UseQuickkeyItem` steht fest (2026-09-06)

Der vollständige Dump von `FavoritesManager::OnButtonEvent` (0x126f930)
beantwortet alles auf einmal:

```
ucomiss xmm0, [rdx+0x38]    ; value == 0 ?   -- gehandelt wird beim Loslassen
comiss  xmm0, [rdx+0x3c]    ; heldDownSecs
lea     rbx, [rcx - 0x10]   ; der Manager: das Eingabe-Subobjekt liegt bei 0x10
mov     rcx, rdx
call    [rax + 0x10]        ; event->QUserEvent()
mov     rcx, rbx
mov     rdx, rax
call    0x1271480           ; -> Index 0..11, oder 12
cmp     eax, 0xc
je      ...                 ; keine Zifferntaste: Pipboy, Quickcontainer, ...
...
mov     rcx, rbx            ; der Manager
mov     edx, eax            ; der Index
call    0x126fcb0           ; [ID 303130]
test    al, al              ; bool -- false, und das Spiel sagt es hörbar
```

Damit:

| | |
| --- | --- |
| `FavoritesManager::UseQuickkeyItem` | `0x126fcb0`, **ID 303130**, `bool(FavoritesManager*, uint32)` |
| `FavoritesManager::GetQuickkeyIndexFromString` | `0x1271480`, ID 1330478 |

Und die Korrektur zu Abschnitt 23: `0x1271480` ist **kein** Typ-Klassifizierer,
sondern die Übersetzung des Ereignisnamens in einen Index — es vergleicht
`[rdi]` gegen zwölf internierte Zeichenketten (`Quickkey1`…). Der Absturz
bleibt derselbe: ein Index statt eines `BSFixedString`-Zeigers.

Die ID ist jetzt fest verdrahtet; `[Debug] UseQuickkeyID` und `UseQuickkeyRVA`
bleiben als Überschreibung stehen. Das `bool` wird protokolliert und **das Menü
bleibt offen, wenn das Spiel nein sagt** — es hat gerade hörbar abgelehnt, und
darauf zu schließen sähe aus, als wäre etwas passiert.

### Benutzen liegt jetzt auf E und Return

`E` ist, wonach die Hand greift — es aktiviert in diesem Spiel alles andere —
und Return ist, worauf ein Menü hört. `[Controls] GridUseKey` und
`GridUseAltKey`.

### Der Name über dem Gitter

Der Streifen, den der Titel freigemacht hat, trägt jetzt, was die markierte
Zelle hält: Name ohne FIS-Tag, und die Anzahl, wenn es mehr als eine ist.
Gezählt wird über **alle** Stapel — die gespielte Seite hat eine Taste, auf die
das Kreuz zeigen könnte, die anderen haben keine.

`grid::Say` steht neben `grid::Mark` und nicht in `Draw`: die Marke bewegt sich,
ohne dass das Panel neu gezeichnet wird, und das ist der ganze Grund, warum das
Folgen des Zeigers nichts kostet.

### Das Crosshair, zweiter Anlauf

`HUDCrosshair_mc` liegt auf diesem Rechner nicht an der Wurzel des HUD — der
hält `SafeRect_mc`, vier Gruppen, einen `HUDMenuFwCore` aus M8rs Framework und
drei namenlose Instanzen. Also wird jetzt **gesucht**, vier Ebenen tief, und der
gefundene Pfad steht im Log.

Dazu: mehrere Mods streiten sich hier um das Crosshair, und eine davon blendet
es wieder ein. Einmal verstecken reicht darum nicht — es wird auf **jedem Bild**
wieder heruntergedrückt, solange das Menü offen ist, und nur dann geschrieben,
wenn es jemand zurückgeholt hat.

### Symbole: eine Messung vor der Arbeit

Die Kette aus Abschnitt 20 liegt vollständig auf der Platte
(`Data\Interface\ItemSorter\`, `FallUI_IconLib.swf` mit 209 Symbolen). Offen ist
genau eine Frage, und sie entscheidet den ganzen Weg:

**Kann ein Movie ohne ActionScript ein Symbol einer fremden Bibliothek bauen?**

`CreateObject` erzeugt eine Klasse, die die *eigene* Bibliothek des Movies
registriert hat — und unsere hat 36 Bytes und registriert nichts. Der Verdacht
ist also „nein". `[Debug] IconProbe` probiert einen Namen aus und schreibt ins
Log, was zurückkam; kommt ein Anzeigeobjekt heraus, hängt es sichtbar in der
ersten Zelle.

Lautet die Antwort nein, ist der Weg: ein **`ImportAssets2`-Tag** in unsere SWF,
genau so, wie `FavoritesMenu.swf` seine Schriften aus `fonts_en.swf` importiert.
`tools/build_swf.py` schreibt rohe Tags, kann das also. Dann aber zwei Fassungen
— eine mit Import, eine ohne — und beim Laden die passende wählen, sonst wird
eine fehlende Bibliothek zur harten Abhängigkeit.

## 28. Benutzen läuft, das Crosshair sitzt, die Symbole nicht (2026-09-06)

**Drei Antworten aus einem Start:**

```
use: UseQuickkeyItem is 0x126fcb0 (ID 303130)
use: [1] "[Aid] Antibiotics" on page 1 -- the game used it
use: [5] "[Grenade] Dynamite" on page 1 -- the game used it
crosshair: found at HUDMenu.CenterGroup_mc.HUDCrosshair_mc
grid: "m_M8r.Repo.MedSyringe" came back as nothing at all
```

**Benutzen funktioniert** — Klick und Taste, über die Seiten hinweg. Damit sind
die Punkte 1 und 2 aus Abschnitt 21 abgeschlossen und Punkt 3 (das Label) auch.

**Das Crosshair** liegt eine Ebene tief in `CenterGroup_mc`, nicht an der
Wurzel; die Suche über vier Ebenen findet es. Auf diesem Rechner streiten sich
mehrere Mods darum, deshalb wird es jedes Bild wieder heruntergedrückt.

### Die Symbole brauchen einen anderen Weg

`CreateObject("m_M8r.Repo.MedSyringe")` liefert **nichts**. Erwartet: unser
Movie hat 36 Bytes und seine Bibliothek registriert keine einzige Klasse, und
`CreateObject` baut genau das, was die *eigene* Bibliothek kennt.

Damit bleiben zwei Wege, und sie sind nicht gleich teuer:

1. **Zur Laufzeit nachladen**, in die eigene Anwendungsdomäne dieses Movies —
   danach fände `CreateObject` die Klassen der Bibliothek unter ihrem Namen.
   Braucht `flash.display.Loader`, `URLRequest`, `LoaderContext` und vor allem
   `ApplicationDomain.currentDomain`. **Scaleforms AS3 ist nicht Flashs AS3**
   und lässt vieles weg, also ist das eine offene Frage, keine Annahme.
2. **`ImportAssets2` in unsere eigene SWF schreiben**, so wie
   `FavoritesMenu.swf` seine Schriften aus `fonts_en.swf` importiert. Dann
   müsste die SWF beim Start erzeugt werden — aus dem, was tatsächlich
   installiert ist —, sonst zeigt ein Importtag auf eine Datei, die es
   vielleicht nicht gibt, und aus einer optionalen Verzierung würde eine
   Voraussetzung für das ganze Gitter.
   Offen bleibt dabei auch, ob ein importiertes Symbol in einem AS3-Movie
   überhaupt per Namen erzeugbar ist — dort braucht es eine Klasse, und die
   käme aus einem `SymbolClass`-Tag ohne zugehörigen Bytecode.

Deshalb erst die Messung: der nächste Start fragt die vier Klassen und die
aktuelle Domäne einzeln ab und schreibt ins Log, was davon dieser Player
überhaupt kennt. **Was antwortet, entscheidet den Weg.**

### Zur Abhängigkeit, zum Mitschreiben

Wir liefern nichts von FallUI oder FIS mit; wir lesen nur, was der Spieler
liegen hat. `Interface\ItemSorter\` ist ohnehin **FallUIs** Mechanismus — die
`Auto-detect.xml` sagt das in ihrem eigenen Kommentar —, und darin liegen hier
zwei Konfigurationen, FIS und DEF_UI. Fehlt das alles, bleiben die Zellen leer
und das Gitter tut alles andere unverändert. Eine weiche Abhängigkeit auf der
Modseite ist normal; eine harte beim Laden der SWF wäre es nicht.

## 29. Der Ladeweg ist offen (2026-09-06)

```
grid: flash.display.Loader is there
grid: flash.net.URLRequest is there
grid: flash.system.LoaderContext is there
grid: flash.system.ApplicationDomain is there
grid: the current application domain cannot be reached
```

**Alle vier Klassen sind da.** Scaleforms AS3 lässt in diesem Player also
genau das übrig, was zum Nachladen nötig ist — der teure Weg über einen selbst
geschriebenen `ImportAssets2`-Tag entfällt.

Die letzte Zeile war kein Befund, sondern mein Fehler: `Movie::GetVariable`
läuft über den Wurzelpfad des Movies, und `flash.system.*` liegt dort nicht.
Die aktuelle Domäne holt man über das Anzeigeobjekt selbst:

```
root -> loaderInfo -> applicationDomain
```

### `src/icons.cpp`

```
context = new LoaderContext(false, jene Domäne)
loader  = new Loader()
loader.load(new URLRequest("FallUI_IconLib.swf"), context)
```

Das zweite Argument von `LoaderContext` ist der ganze Punkt: ohne es entsteht
eine **Kinddomäne**, und deren Klassen sind nicht die, die `CreateObject`
findet — das sähe von außen aus wie ein fehlgeschlagener Ladevorgang.

Gewartet wird über `contentLoaderInfo.bytesLoaded`/`bytesTotal`, einmal pro
Bild, dieselbe Stelle, an der auch der Zeiger gelesen wird. Eine fehlende Datei
lässt `bytesTotal` für immer auf null, deshalb wird nach 600 Bildern
aufgegeben und das ins Log geschrieben.

Ist die Bibliothek da, wird das Panel **einmal** neu gezeichnet — vorher gab es
die Symbole noch nicht. `IconProbe` hängt dann sein Symbol in die erste Zelle:
das ist der Beweis, dass der Weg trägt, und erst danach lohnt die Zuordnung
Tag → Symbol aus der Sorter-XML.

Die Bibliothek ist 69 KB groß, also kostet ein Laden pro Öffnen des Menüs
nichts Nennenswertes. Unser Menü wird beim Schließen abgebaut, das Movie mit
ihm, und damit auch die geladenen Klassen — `icons::Release` räumt den Loader
weg, bevor das Movie geht.

Neue Einstellung `[Display] IconLibrary`, leer schaltet Symbole ab.

## 30. Der Loader schweigt, also wird er gefragt (2026-09-06)

```
icons: asked for FallUI_IconLib.swf
icons: nothing arrived in 600 frames -- 0 bytes of 0
```

`0 von 0` heißt: der Ladevorgang ist **nie angelaufen**. Warum, sagen Bytes
nicht — und das war der Fehler im ersten Anlauf. Ein Loader hat Ereignisse, und
die tragen den Grund in Worten.

### Aus C++ eine Funktion machen, die ActionScript aufrufen kann

`Scaleform::GFx::Movie::CreateFunction(Value*, FunctionHandler*)` — ein
`FunctionHandler` ist eine Klasse mit einer einzigen virtuellen Methode `Call`,
und was dabei herauskommt, lässt sich an `addEventListener` übergeben wie jede
andere Funktion. Damit hängen jetzt `complete`, `ioError` und `securityError`
am `contentLoaderInfo`, und die Fehlermeldung steht im Klartext im Log.

Der Handler ist ein **statisches** Objekt: `CreateFunction` behält eine
Referenz, solange der Listener hängt, und ein Objekt pro Menüöffnung neu
anzulegen wäre ein Leck mit Ansage. Geschrieben wird darin nur, *was* passiert
ist; gehandelt wird ein Bild später in `Poll`, wo das Menü nachweislich noch
steht.

### Und der Pfad wird nicht mehr geraten

Zwei Dinge auf einmal:

- **Das Movie sagt selbst, woher es kam.** `root.loaderInfo.url` steht jetzt
  im Log — damit ist klar, welche Form ein Pfad in diesem Player hat, und das
  ist genau die Frage, die Raten nicht beantwortet.
- **Vier Formen werden der Reihe nach probiert**, wenn der Name keinen
  Schrägstrich enthält: `FallUI_IconLib.swf`, `Interface/…`, `../…`,
  `Data/Interface/…`. Ein `ioError` schaltet zur nächsten weiter, ein
  `complete` beendet die Suche. Ist der Name mit Pfad angegeben, wird genau
  der genommen und nichts geraten.

Ein Nebenbefund steht auch schon fest: `loader.load(...)` meldet, ob der Aufruf
selbst durchging. Ging er nicht durch, liegt es nicht am Pfad.

## 31. Die Symbole, und was Starfield schon konnte (2026-09-06)

Der Ladeweg trug: `icons: "FallUI_IconLib.swf" is in (complete)`, und die Sonde
hing ihr Symbol in die erste Zelle. Damit war die letzte offene Frage aus
Abschnitt 20 beantwortet, und der Rest ist Lesen. **Nichts davon ist im Spiel
getestet** — die Prüfliste steht am Ende.

### Die Kette, jetzt vollständig

```
"[Aid] Antibiotics"                     der Name, den FIS schreibt
   -> "Aid"                             tags::KeywordOf
   -> M8r.Fo4Misc.MedKit                aus <tag keyword= icon=>
   -> FallUI_IconLib.swf                aus <tags iconLibraryFile=>
   -> m_M8r.Fo4Misc.MedKit              CreateObject, nach dem Laden
```

Drei Dinge machen das unordentlicher, als es aussieht, und alle drei sind auf
einem Rechner mit ein paar hundert Mods echt:

**Es gibt mehr als eine Bibliothek.** Jeder `<tags>`-Block nennt seine eigene,
und der Addon-Ordner hält ein Dutzend — eine pro Mod, die eigene Zeichnungen
mitbringt. Auf diesem Rechner: 402 Schlüsselwörter aus `FallUI_IconLib.swf`,
370 aus `DielloIconLib.swf`, eines aus `4estIconLib.swf`. Geladen wird nur,
was die gezeigte Seite tatsächlich braucht.

**Es gibt mehr als eine Konfiguration.** FIS und DEF_UI liegen beide unter
`Interface\ItemSorter`, und welche aktiv ist, steht in MCM. Also werden alle
gelesen und verschmolzen: nachgeschlagen wird nach dem, wie der Gegenstand
tatsächlich heißt, und wer diesen Namen geschrieben hat, hat sein
Schlüsselwort in eine dieser Dateien geschrieben. Damit erübrigt sich die
Auto-Erkennung ganz.

**Variationen sind die Ausnahme davon.** Das sind alternative Symbolsätze, die
der Spieler in MCM wählt, und die Dateien für alle liegen nebeneinander in
einem Ordner. Alle zu lesen heißt: die zuletzt gelesene gewinnt — und genau so
kam `Stimpak` als **Medkit** heraus, auf einem Rechner, dessen Besitzer
`sVariationMedics=None` stehen hat. Der Ordner wird deshalb übersprungen, und
danach werden genau die gewählten nachgelesen, aus MCMs eigenen Einstellungen
(`MCM\Settings\<Mod>.ini`, sonst `MCM\Config\<Mod>\settings.ini`).

Gegengeprüft an den echten Dateien dieses Rechners: 636 Schlüsselwörter,
`Aid → M8r.Fo4Misc.MedKit`, `Stimpak → M8r.Repo.MedSyringe`,
`GrenBaseball → Diello.Grenade.Baseball` aus Diellos Bibliothek.

**Größe und Farbe.** Ein Symbol wird eingepasst, nicht gedehnt — ein in ein
Quadrat gequetschtes Symbol ist schlimmer als keines, weil es absichtlich
aussieht. Und es wird in der Farbe gemalt, die sein Tag nennt: Multiplikatoren
auf null, die Farbe als Offset, die Transparenz unangetastet. Ein Symbol aus
mehreren Formen nennt eine Farbe je Form (`MedicBrown,MedicSilver`); wir malen
eine, also zählt die erste.

### Drei Dinge aus der Starfield-Fassung

Der Abgleich mit `FavoritesBanks-0.7.6-source` hat drei Sachen ergeben, die
dort längst gelöst sind:

1. **`DefaultPage`** (dort `defaultRow`). Das Grid zeigt alle Seiten gleich,
   also ist „die Seite, auf der du bist" nichts mehr, was man sehen kann — und
   die Zifferntasten des Spiels erreichen nur die Seite, die die Engine hält.
   Bleibt das die zuletzt benutzte, wird aus den Ziffern unsichtbarer Zustand:
   dieselbe Taste tut etwas anderes, je nachdem, was man vor zehn Minuten
   angeklickt hat. Mit `DefaultPage=n` wird beim Schließen des Menüs immer
   dieselbe Seite zurückgelegt. **Standardmäßig aus**, weil es hier nicht
   umsonst ist: ein Seitenwechsel bewegt jeden Favoriten einzeln durch die
   Engine.
2. **`GridClearKey`** (Entf). Gibt die Taste frei, auf der die Marke steht.
   Nichts wird gelöscht und nichts hört auf, Favorit zu sein — der Gegenstand
   geht in denselben Zustand „Favorit ohne Ziffer", den das Spiel selbst
   schreibt.
3. **`GridMoveKey`** (G). Hebt die markierte Zelle auf; der nächste Druck legt
   sie auf die dann markierte, und die beiden tauschen. **Über Seiten hinweg**
   — das ist der Punkt: so wandert etwas von Seite 3 auf Seite 1, ohne den
   Pip-Boy. Beide Enden gehen über die Seitenliste, und die gespielte Seite
   wird danach mit `ApplyPage` zurückgeschrieben, der einen Operation, die
   genug benutzt wurde, um ihr zu trauen. Zwei Sonderfälle verschwinden so.

Dazu `GridWrap`: ob die Enden einer Reihe Türen oder Wände sind.

### Was beim Testen schiefgehen kann

- Ein Symbol, dessen Bibliothek noch lädt, kommt als nichts zurück. Deshalb
  wird nach jeder angekommenen Bibliothek **einmal** neu gezeichnet. Wenn
  Symbole erst nach dem zweiten Öffnen erscheinen, ist das die Stelle.
- `MoveMarked` schreibt bei jedem Tausch, an dem die gespielte Seite beteiligt
  ist, die ganze Seite neu. Das ist laut im Log und langsam, aber es ist der
  Weg, der schon funktioniert.
- Farben sind flach. Mehrfarbige Symbole verlieren dabei etwas; `IconColors=0`
  lässt sie weiß.

## 32. Waffen, Werte und eine zu kleine Zeile (2026-09-06)

Die Symbole standen — für Hilfsmittel, Granaten, Hut, Ringe. **Für Waffen
nicht**, und der Grund stand längst in den Namen: `T60`, `Makeshift Scout
Rifle`, `Sten Mk II` tragen **kein** `[Tag]`. FIS benennt nur um, was sein
Plugin abdeckt.

### Auto-Tagging, wie FallUI es macht

`bAutoTagging = 1` in der MCM-Datei, und `<autoTagger>` nennt zwei Dateien:

| | |
| --- | --- |
| `IndexedTags_${LANGUAGE_CODE}.txt` | Abschnitte aus vier Buchstaben (`WEAP`, `ARMO`, …), darunter `[Tag]` gefolgt von den **exakten** Namen, getrennt durch **0x1f** |
| `AutoTags_${LANGUAGE_CODE}.ini` | `irgendein Text=[Tag]` je Abschnitt, **in der geschriebenen Reihenfolge** als Teilzeichenkette geprüft |

Der Index zuerst, die Regeln als Netz darunter — das ist FIS' eigene Reihenfolge
und sie zählt: eine Regel „Grenade" würde sonst die Baseball-Granate
beanspruchen, die der Index namentlich kennt. Gegengeprüft an den echten
Dateien: 2917 Namen, 299 Regeln, und `Makeshift Scout Rifle → Rifle` über die
Regel `rifle`.

Die Sprache kommt aus `sLanguage` in `Fallout4.ini`. Englische Regeln gegen
deutsche Namen zu prüfen fände gar nichts — und zwar lautlos, was die
schlechteste Art ist, nichts zu finden.

### Und ein Netz darunter

`Sten Mk II` und `T60` kennt auch das Auto-Tagging nicht: Mod-Gegenstände.
`IconFallback` gibt ihnen trotzdem eines, nach Gegenstandsart — Schusswaffe →
`Rifle`, Nahkampf nach Griffart, Rüstung → `Armor` oder `Clothes` je nach
Rüstwert, Hilfsmittel → `Aid`. Alle diese Schlüsselwörter definiert FIS selbst,
also passt die Zeichnung zum Rest statt ein zweiter Stil zu sein. Der grobe
Fall ist die Schusswaffe: die Engine kennt Schusswaffen als **eine** Art,
`WEAPON_TYPE::kGun`, ohne irgendetwas darin, das Pistole von Gewehr trennt.

### Die Zeile über dem Gitter, zweimal so groß und mit Inhalt

Aus `FavoritesMenu.swf` gemessen: `ItemName_tf` ist **AIN_Font_Bold, Größe 28**,
`ItemAmmo_tf` ist **AIN_Font, Größe 24** — beides auf der 1280x720-Bühne.
Unsere Zeile war `Zelle × 0.27`, also **13**. Deshalb wirkte sie zu klein: sie
war es, um mehr als die Hälfte.

Jetzt zwei Zeilen wie im Spiel, in Bühneneinheiten statt als Bruchteil der
Zelle (`LabelSize`, `LabelDetailSize`) — so bleibt die Schrift lesbar, wenn
jemand die Zellen klein macht.

Was in der zweiten Zeile steht, kommt aus `src/detail.cpp`:

- **Waffe:** `DMG` aus `attackDamage`, der Munitionsname und wieviel davon im
  Inventar liegt, dazu Elementarschaden aus `damageTypes`.
- **Rüstung:** die Widerstände aus `armorData.damageTypes`, jeder benannt nach
  dem Aktorwert, den sein `BGSDamageType` schützt — abgekürzt auf die
  Anfangsbuchstaben, so wie der Pip-Boy es tut, wenn ihm der Platz ausgeht.
- **Alles andere:** die Anzahl.

Wichtig dabei: **Instanzdaten vor Basisdaten**. Eine Waffe mit Mods ist eine
andere Waffe als die im Plugin, und der Unterschied liegt im Stapel, als
`ExtraInstanceData` mit einem `TBO_InstanceData` darin. Ohne das zeigte der
Schaden immer den unmodifizierten Wert.

**Ungeprüft und beim Testen anzusehen:** die Widerstandswerte kommen aus einer
`union SharedVal`, die je nach Benutzer eine Ganzzahl oder ein Float führt.
Gelesen wird das Float. Kommen Zahlen heraus, die wie Adressen aussehen, ist es
die andere Hälfte.

### G war eine schlechte Wahl

Die Aufheben-Taste lag auf `G`, und das wirft Granaten — eine andere Mod liest
die Taste **roh**, so wie der Seitenblätterer dieses Plugins es selbst tut, und
sieht deshalb nie, dass ein Menü sie beansprucht hat. Jetzt `INSERT`.

Das ist eine Grenze, die dokumentiert gehört: was das Grid beansprucht, wird
nur dem genommen, was durch die Eingabekette des Spiels läuft. Gegen eine Mod
mit `GetAsyncKeyState` hilft das nicht, und deshalb ist jede Vorgabe hier
besser keine Buchstabentaste.

## 33. Was die Zahlen sagen sollten (2026-09-06)

Die Symbole stehen, die Zeile steht, und beim Lesen fiel auf, was daran noch
nicht stimmt.

### `DMG 1` bei Dynamit

`attackDamage` ist bei einer Wurfwaffe der Schaden, den der geworfene
Gegenstand macht, **wenn er jemanden trifft** — nicht die Explosion. Deshalb
stand da 1, und eine 1 neben einer Granate lässt einen Leser jeder anderen Zahl
auf dem Panel misstrauen.

Der Weg zur echten Zahl ist eine Kette aus reinen Datenlesungen:

```
InstanceData.rangedData->overrideProjectile      (sonst ammo->data.projectile)
   -> BGSProjectile::data.explosionType
   -> BGSExplosion::data.damage
```

Genommen wird sie nur bei `WEAPON_TYPE::kGrenade` und `kMine`; bei allem
anderen bleibt `attackDamage` richtig.

### `ER 0` war eine Fehllesung, keine Rüstung ohne Widerstand

Der Wert in `damageTypes` steckt in einer `union SharedVal` aus Ganzzahl und
Float, und nirgends steht, welche Hälfte gemeint ist. Ich hatte das Float
gelesen. Das Float einer Ganzzahl ist eine **Denormale** — eine Zahl knapp über
null —, also kam sie durch die „größer als null"-Prüfung und wurde als `0`
gedruckt. Genau so sah es aus wie eine Rüstung ohne Widerstand statt wie ein
Lesefehler.

Jetzt wird die **Ganzzahl** gelesen, und das Float nur dort, wo die Ganzzahl
keine sein kann: ein echter Widerstand ist eine kleine Zahl, alles über 100000
ist die andere Hälfte, die durchscheint. Nullen fallen ganz weg — der Pip-Boy
zählt jede Art auf, weil er für jede eine Spalte hat; eine Zeile muss ihre
Wörter verdienen.

### Das Blättern im Pip-Boy war ein glatter Verlust

Abschnitt 23 hat die Seitentasten abgeschaltet, „weil das Grid sie überflüssig
macht". Das stimmt **nur, solange das Grid offen ist**. Im Pip-Boy schreibt
„Favorit zuweisen" in die Seite, die die Engine gerade hält — ohne die Tasten
kann man also nur noch auf eine einzige Seite zuweisen.

Jetzt sind sie überall aktiv außer bei offenem Grid, und dort, wo das Panel die
Seite nicht zeigen kann, sagt es die Eckmeldung des Spiels: `PageMessage` leer
heißt jetzt nicht mehr „nie", sondern „nur wenn man es sonst nicht sehen
könnte". Ein lautloser Seitenwechsel im Pip-Boy ist eine Falle.

### Die Tastenleiste

Unter dem Panel steht jetzt, worauf es hört — `WASD) MOVE   E) USE   INS) PICK
UP   DEL) CLEAR   TAB) CLOSE` —, gebaut aus den Tasten, wie sie **tatsächlich
gebunden** sind. `KeyHintExtra` ist freier Text, weil die Schließtaste dem
Spiel gehört und nicht uns: was das Menü geöffnet hat, schließt es auch.

Dazu `KeyRowGap`: Tastennamen und erste Zellenreihe standen ein Haar
auseinander und lasen sich als ein gedrängter Block.

### Zur Schriftfrage

Alles auf dem Panel wird in derselben Schrift geschrieben, und die kommt aus
dem Menü selbst: `Cross_mc.GetEntryClip(0).Quickkey_tf.getTextFormat().font`,
also **AIN_Font_Bold** — dieselbe, die das Spiel für seine eigenen
Tastennummern nimmt. Sie wird bei jedem Öffnen frisch gemessen und ist damit
immer eine, die der Spieler wirklich hat. Neu: das steht jetzt einmal im Log,
und `GridFont` kann sie überschreiben. Falls die Zahlen links und oben anders
aussehen, liegt es nicht an der Schrift, sondern an der Größe.

### Was offen bleibt

**Symbole in der Zeile.** Der Wunsch — Munitionstyp, Widerstände, Schadensart
je mit eigenem Zeichen, so wie die Karte im Pip-Boy — braucht mehr als ein
Textfeld: die Zeile müsste aus Abschnitten gebaut werden, jeder mit optionalem
Symbol, jeder gemessen (`textWidth`) und zusammen mittig gesetzt. Das Symbol
für die Munition ist dabei das einfache, weil Munition ein Gegenstand mit Namen
ist und damit durch dieselbe Tag-Kette läuft wie alles andere. Widerstände und
Schadensarten haben in FIS keine Zeichen; die des Pip-Boys liegen in seiner
eigenen SWF.

**Die Zeile „Powerful | Quick | Instigating".** Die legendäre Wirkung und die
Modnamen, die der Pip-Boy unter dem Namen zeigt. Sie kommen aus den
Instanznamensregeln des Gegenstands und sind der nächste ernsthafte Brocken.

## 34. Mittig wovon? (2026-09-06)

Der Wunsch, die Reihenzahlen auszublenden, hatte einen Grund, und der Grund war
ein Fehler von mir: **die Zeilen über und unter dem Gitter waren über das ganze
Panel zentriert**, also einschließlich der Zahlenspalte links. Die Spalte sitzt
nur auf einer Seite, also stand jede dieser Zeilen um ihre halbe Breite neben
der Mitte dessen, was sie beschreibt — sichtbar, sobald eine zweite Zeile
darunter kam.

Zentriert wird jetzt auf die zwölf Zellen (`CellsLeft`, `CellsWidth`), nicht auf
das Panel. Damit ist `ShowPageNumbers` wieder das, was es sein sollte: eine
Geschmacksfrage, kein Ausweg.

Nebenbei wurde `CellLeft` auf dieselben zwei Zeilen zurückgeführt — Zeichnen,
Treffertest und Beschriftung lesen die Geometrie damit weiterhin aus einer
einzigen Quelle.

## 35. Der Schalter saß im Aufruf, nicht neben ihm (2026-09-06)

Das Ablegen über `ActorEquipManager::UnequipObject` ist gescheitert, und das
Log war dabei präzise: der getragene Stapel wurde gefunden
(`is worn on stack 0 of 1`), der Manager lehnte ab. Vier verschiedene
Argumentkombinationen, alle abgelehnt.

**Die ToggleEquip-Mod von alexj hat den Weg gezeigt.** Ihr eigener Code sagt,
wie sie es macht — die REL-IDs stehen als Konstanten in ihrer DLL und lassen
sich allesamt benennen:

| ID im Thunk | ist |
| --- | --- |
| 303130 | `FavoritesManager::UseQuickkeyItem` — plus `0x1b3`, die gehookte Stelle |
| 303410 | `PlayerCharacter` — der Vergleich prüft „ist der Akteur der Spieler" |
| 501899 | `BGSInventoryInterface` — nur fürs Protokollieren |

Und bei `UseQuickkeyItem + 0x1b3` steht:

```
mov  rdx, [rip + ...]          ; der Spieler
mov  rcx, [rip + ...]          ; der ActorEquipManager
mov  byte ptr [rsp+0x30], 0    ; das zweite Boolean
lea  r8,  [rsp+0x40]           ; das Inventar-Handle, zwei Zeilen vorher gebaut
mov  r9d, ebp                  ; der Index
mov  byte ptr [rsp+0x28], 0    ; das erste -- dieses hier
mov  qword ptr [rsp+0x20], 0   ; kein Equip-Slot
call 0xe1c750                  ; [ID 332489]
```

`true` an dieser Stelle heißt **„und nimm es wieder ab, wenn es schon an
ist"**. Das Spiel übergibt immer `false`. Die Signatur deckt sich exakt mit dem
RTTI-Namen aus der Mod:

```
bool (ActorEquipManager*, Actor*, InventoryInterface::Handle&,
      uint32, BGSEquipSlot*, bool, bool)
```

### Warum gehookt und nicht direkt gerufen

Das `Handle` ist ein `uint32` aus einer Agent-Tabelle, und `UseQuickkeyItem`
baut es zwei Befehle vorher (ID 868852) und gibt es zwei Befehle später wieder
frei (ID 1214840). Es selbst zu besorgen hieße, drei weitere Engine-Aufrufe
nachzubauen, um am Ende denselben Aufruf zu machen. Der Hook ist der kürzere
und ehrlichere Weg.

**Er wirkt nur, wenn dieses Plugin es verlangt.** `use::Quickkey` setzt eine
Flagge für die Dauer des einen Aufrufs; alles andere, was durch dieselbe
Stelle läuft — die Zifferntasten des Spiels voran —, bleibt unberührt.

Nebeneffekt, und ein guter: **die Regeln gehören wieder der Engine.** Die
Power-Armor-Sonderbehandlung aus Abschnitt 34 ist wieder raus. Sie war nötig,
solange wir den Equip-Manager selbst riefen; jetzt entscheidet dieselbe
Funktion wie bei einem Ziffernkorb, und was sie dort verweigert, verweigert sie
auch hier.

`F4SE::AllocTrampoline(64)` in `F4SEPlugin_Load` — der eine Aufruf ist der
einzige, den dieses Plugin umleitet.

## 36. Eine Richtung, und der Rest fliegt raus (2026-09-06)

Entscheidung: **die Mod ist das Grid.** Das Vanilla-Menü sitzt woanders, sieht
anders aus und gehorcht anderen Regeln; es parallel zu bedienen hieße, jede
Änderung zweimal zu machen und zweimal zu testen. Andere Anordnungen können
später dazukommen — dann aber in unserem System, nicht als zweiter Weg durch
denselben Code.

### Was aus dem Code verschwunden ist

| | warum |
| --- | --- |
| `UseGrid` und der ganze Zweig dahinter | es gibt nur noch einen Weg |
| Der Seitenmarker am Kreuz (`ShowPageIndicator`, `PageIndicator*`, `DressField`, `PageWording`, `ReleaseIndicator`) | er war die Anzeige *für* das Vanilla-Kreuz; das Panel sagt es selbst |
| `GridMenu` | drei fremde Leinwände wurden probiert, das eigene Menü hat gewonnen (Abschnitt 21) |
| `GridInMenuRoot` | dokumentierte Sackgasse, als Schalter aufbewahrt, nie benutzt |
| `GridProbeStage` | hat seine Frage beantwortet: der Viewport war nie das Problem |
| `InventoryProbeKey`, `FavoriteRoundTripKey`, `RotateFavoritesKey` samt `RotateFavorites` und `FavoriteRoundTrip` | die Werkbank aus der Erkundungszeit; `ApplyPage` ist inzwischen der bewiesene Weg |
| `UseQuickkeyID` / `UseQuickkeyRVA` | die Adresse steht fest und ist im Spiel bestätigt |
| Der automatische Funktionsdump bei jedem Start | Entwicklerarbeit, die niemand ausliefert |

`CrossFont` bleibt: die Schrift des Panels wird weiterhin aus dem Kreuz
gemessen, weil das die einzige ist, von der feststeht, dass das Menü sie hat.
Das Kreuz selbst bleibt versteckt.

`peek` bleibt auch, aber ausdrücklich als Werkzeug: der `[Debug]`-Abschnitt der
INI sagt jetzt in seinem ersten Satz, dass er zum Arbeiten an der Mod da ist
und nicht zum Spielen. Leer ist er wirkungslos.

`main.cpp` ist dabei um rund 250 Zeilen kürzer geworden, `use.cpp` um ein
Drittel.

### Die INI

Neu geschrieben statt geflickt. Fünfzehn Einstellungen sind verschwunden, der
Rest ist nach dem geordnet, was ein Spieler sucht: erst wo und wie groß, dann
die Schrift darüber, dann die Symbole, dann die Seiten, dann die Tasten. Der
`[Debug]`-Abschnitt steht am Ende und ist als solcher gekennzeichnet.

Die installierte INI wurde dabei **mit den Werten des Spielers** neu
geschrieben, nicht überschrieben — was er verstellt hatte, steht weiterhin
verstellt drin.

Der README behauptete noch, das sei kein funktionierender Mod, und versprach,
dass nichts in `Data\Interface` liegt. Beides stimmt nicht mehr: die 36 Bytes
unserer eigenen Bühne liegen dort, gehören aber keinem Vanilla-Menü und können
mit nichts kollidieren. Das steht jetzt so da.

## 37. Was diese Sitzung gebracht hat, und was zu prüfen bleibt (2026-09-06)

Eine lange Sitzung. Der Reihe nach, damit die nächste weiß, worauf sie steht.

### Im Spiel bestätigt

- Benutzen per Klick und Taste, über Seiten hinweg.
- Symbole für Hilfsmittel, Granaten, Kleidung, Ringe — **und** für Waffen,
  nachdem das Auto-Tagging dazukam.
- Farben des Panels folgen der HUD-Farbe.
- Das Crosshair verschwindet (`HUDMenu.CenterGroup_mc.HUDCrosshair_mc`).
- Ab- statt Anlegen beim zweiten Druck, für Waffen wie für Kleidung.
- Der Zeiger trifft die Zellen, gemessen und bestätigt.

### Gebaut, aber ungeprüft

| | |
| --- | --- |
| `INSERT` | Zelle aufheben, nächster Druck tauscht — auch über Seiten |
| `DELETE` | Taste freigeben, Gegenstand bleibt Favorit |
| `DefaultPage=1` | beim Schließen zurück auf Seite 1 legen |
| `GridWrap=0` | Marke stoppt am Rand statt umzulaufen |
| Wiederholung | 400 ms Anlauf, dann 90 ms je Schritt |
| Alle Aufräumarbeiten | INI neu geschrieben, halber Code entfernt — dass nichts Stilles kaputtgegangen ist, zeigt erst ein Lauf |

**Beim nächsten Start zuerst ins Log sehen.** Diese Zeilen gehören dorthin:

```
settings: N pages, 48 cells, page keys 0x22 and 0x21
tags: N keywords out of M libraries, plus N names and M rules ...
use: UseQuickkeyItem is 0x126fcb0, and its equip call comes through here
input: the grid listens first of N handlers; ...
grid: everything is written in "AIN_Font_Bold", taken from ...
```

Fehlt eine davon, ist beim Aufräumen etwas mitgegangen.

### Ein wiederkehrendes Muster, das sich gelohnt hat

Viermal in dieser Sitzung sah ein Fehlschlag nach etwas anderem aus, als er
war, und viermal hat dieselbe Antwort geholfen: **nicht die nächste Vermutung
bauen, sondern messen.**

- „Das Symbol kommt als nichts zurück" → vier Klassennamen einzeln abfragen,
  statt den teuren Weg zu bauen. Alle vier waren da.
- „0 Bytes von 0" → der Loader hat Ereignisse; sie tragen den Grund in Worten.
- „`ER 0`" → eine `union`, deren Float-Hälfte für eine Ganzzahl eine
  Denormale ergibt: knapp über null, also durch jede Prüfung.
- „Abgelehnt" beim Ablegen → vier Argumentkombinationen in **einem**
  Tastendruck, statt vier Spielstarts.

Und einmal hat der Blick in eine fremde Mod mehr gebracht als jede Messung:
ToggleEquip trägt ihre REL-IDs als Konstanten in der DLL, und die ließen sich
allesamt benennen (Abschnitt 35).

### Was als Nächstes ansteht

In dieser Reihenfolge sinnvoll:

1. Die ungeprüften Tasten durchspielen.
2. Die Wertezeile aus Abschnitten bauen, damit Symbole hineinpassen — das
   Munitionssymbol zuerst, es läuft durch dieselbe Tag-Kette wie alles andere.
3. Die legendäre Zeile aus den Instanznamensregeln.
4. Erst danach an das Gamepad und den Pip-Boy denken.

## 38. Das Gamepad (2026-09-06, später)

Bisher stand im Handoff, das Gamepad bleibe außen vor: „ein falscher Griff
dort nähme dem Controller das Menü ganz." Der Satz war richtig — er beschreibt
aber eine Gefahr, keine Unmöglichkeit. Sie lässt sich benennen und dann
umgehen.

### Was die Gefahr genau ist

Unser Handler steht **vorn** in `MenuControls::handlers`, und was er für seines
erklärt, sieht danach niemand mehr. Auf der Tastatur ist das harmlos: die
Schließtaste wird nie beansprucht. Auf dem Controller wäre es das nicht, denn
das Menü schließt sich über eine Taste, die genauso ein Knopf ist wie die, mit
denen man durch die Zellen läuft.

Die Vanilla-SWF sagt, welche. `FavoritesMenu.as`:

```actionscript
if((param1 == "Cancel" || param1 == "Quickkeys") && !param2)
{
   this.BGSCodeObj.closeMenu();
}
```

Zwei Benutzerereignisse, und geschlossen wird beim **Loslassen**. Welche
Knöpfe das sind, steht nicht im Code, sondern in den Bindungen des Spielers —
also wird nachgesehen statt geraten: `input::Install` läuft beim Start einmal
durch alle `ControlMap::controlMaps`, nimmt aus jedem die Gamepad-Zuordnungen
und sammelt jede, deren `eventID` `Quickkeys` oder `Cancel` heißt. Diese Codes
landen zusammen mit `B`, `START` und `BACK` in einer Sperrliste, die
`Claimed` vor allem anderen fragt. Ein umbelegter Controller ist damit
genauso sicher wie ein unveränderter, und die INI kann die Sperre nicht
aufheben — sie steht nicht als Einstellung da.

### Was das Kreuz am Controller sonst tut

Aus derselben Datei, und es erklärt, warum wir dem Kreuz nichts wegnehmen, was
es braucht: `FavoritesCross.ProcessUserEvent` kennt nur `PrimaryAttack` und
`Quickkey1`…`Quickkey12`. Das Laufen durch die zwölf Felder läuft **nicht**
über Benutzerereignisse, sondern über Scaleform-Tastenereignisse an das
fokussierte Objekt (`onKeyUp` mit `Keyboard.UP`, `DOWN`, `LEFT`, `RIGHT`,
`ENTER`) — die Engine übersetzt Steuerkreuz und Stick dorthin. Unser Handler
sitzt davor, also kommt beides gar nicht erst an: kein doppeltes Benutzen,
keine zweite Auswahl hinter dem versteckten Kreuz.

### Die Zahlen

`idCode` eines Gamepad-Ereignisses ist die **nackte XInput-Maske**, nicht
`BS_BUTTON_CODE`. Der Unterschied ist genau das `0x10000`, das
`GetBSButtonCode` erst dazusetzt. Also `A = 0x1000`, `X = 0x4000`,
`Y = 0x8000`, `B = 0x2000`, Steuerkreuz `0x1`/`0x2`/`0x4`/`0x8`,
Schultern `0x100`/`0x200`, Sticks `0x40`/`0x80`, Start `0x10`, Back `0x20`.
Die beiden Trigger sind Bethesdas eigene Erfindung — XInput hat für sie keine
Bits —, `0x9` und `0xA`, und stehen absichtlich in keiner Vorgabe: ein Trigger
ist eine Achse, und eine gehaltene Achse in einem Menü, das Dinge benutzt, ist
ein schlechter Gedanke.

### Der linke Stick

Er kommt nicht als Knopf, sondern als `ThumbstickEvent` (`idCode` `0xB`), und
er meldet, **wo er steht**, nicht dass etwas passiert ist. Das Schrittwerk der
Tasten — Druck, Pause, gleichmäßiger Lauf — musste also hier noch einmal
gebaut werden, mit einer eigenen Uhr statt der `heldDownSecs`, die das
Ereignis nicht hat.

Zwei Dinge, die nicht offensichtlich waren:

- **Der tote Bereich hat zwei Kanten**, 0.55 hinein und 0.35 heraus. Mit einer
  einzigen Zahl zittert die Marke genau am Rand zwischen zwei Zellen.
- **Alle Stickereignisse werden beansprucht oder keines.** Nur die
  ausgelenkten zu nehmen hieße, die Rückkehr in die Mitte nie zu sehen — die
  Marke liefe weiter, nachdem der Daumen losgelassen hat.

Der Preis ist, dass man mit offenem Menü nicht gehen kann. Das ist derselbe
Preis, den `W` und `S` auf der Tastatur längst kosten, deshalb ist es ein
Schalter (`GridGamepadStick`) und kein Sonderfall.

### Die Tastenleiste folgt der Hand

`ShouldHandleEvent` wird für **jedes** Ereignis gefragt, auch für die, die wir
nicht wollen. Damit ist es die einzige Stelle, die den ganzen Strom sieht, und
also die billigste, an der sich merken lässt, womit zuletzt gespielt wurde.
Ein Druck, keine Bewegung: ein Stick, der in der Ruhelage driftet, würde sonst
für immer behaupten, er sei das Gerät in Benutzung.

Die Leiste unter dem Panel nennt daraufhin Controllertasten oder Tasten. Der
Wechsel zeichnet das Panel neu, was für eine Textzeile viel ist — er passiert
aber, wenn jemand etwas anderes in die Hand nimmt, nicht während er es
benutzt. `KeyHintExtraGamepad` steht neben `KeyHintExtra`, weil „TAB) CLOSE"
an einem Controller eine Lüge ist.

### Nebenbei gefunden

`g_stepped` war ein `std::array<float, 5>`, indiziert mit einer Aufzählung, die
inzwischen **sieben** Werte hat. Jeder Druck auf `DELETE` oder `INSERT` schrieb
zwei Floats hinter das Feld. Es lag im Datensegment neben anderen Globalen und
ist deshalb nie aufgefallen. Jetzt leitet sich die Größe aus der Aufzählung
ab, statt danebenzustehen.

### Was zu prüfen bleibt

Der Bau ist sauber (`/W4 /WX`) und deployed, **gespielt ist er nicht**. In
dieser Reihenfolge:

1. **Schließt sich das Menü noch mit dem Controller?** Alles andere ist
   Bequemlichkeit, das hier ist die Frage. Das Log sagt vorher, was gesperrt
   wurde — die Zeile `input:` endet jetzt mit `… stay with the game`.
2. Steuerkreuz durch die Zellen, in beide Richtungen, über Seiten hinweg.
3. Linker Stick dasselbe, und ob die Wiederholung beim Loslassen aufhört.
4. `A` benutzt, `X` hebt auf und legt ab, `Y` gibt frei.
5. Die Leiste: Controllertaste drücken, sie muss `^<v>) MOVE   A) USE …`
   sagen; Tastatur anfassen, sie muss zurückwechseln.

Findet sich in Schritt 1 etwas, ist `GridGamepad=0` die vollständige Rücknahme
— und der nächste Schritt wäre, in derselben Sperrliste nachzusehen, was das
Log als `stay with the game` aufgezählt hat.

## 39. Die Symbole der Knöpfe stehen in einer Schrift (2026-09-07)

Die Steuerung lief auf Anhieb. Was falsch aussah, war die Leiste darunter:
`^<v>) MOVE   A) USE   X) PICK UP   Y)` — ausgeschriebene Behelfsnamen, wo das
Spiel überall sonst gezeichnete Knöpfe zeigt. Und für einen anderen Controller
wären sie schlicht falsch.

### Wo die Symbole liegen

Nicht in Bildern, sondern in einer **Schrift**. `Data\Interface\FontConfig.txt`
sagt es in zwei Zeilen:

```
fontlib "Interface\fonts_en.swf"
map "$Controller_Buttons" = "Controller  Buttons" Normal
map "$Controller_Buttons_inverted" = "Controller  Buttons inverted" Normal
```

Der Schriftname hat **zwei Leerzeichen** in der Mitte. Ein Textfeld in dieser
Schrift, in das man ein `A` schreibt, zeigt den A-Knopf.

Dass die Vanilla-Menüs genau das tun, steht in ihren eigenen SWFs. In
`ContainerMenu.swf` liegt ein `DefineEditText` mit
`$Controller_Buttons_inverted` und dem Inhalt

```html
<p align="center"><font face="Controller  Buttons inverted" size="16" …>X</font></p>
```

**Achtung, Falle:** dieses `X` ist Autorentext, keine Zuordnung. Dieselben
Felder tragen daneben `TEXT`, `110` und `370/175` — alles Platzhalter, die zur
Laufzeit ersetzt werden. Wer die Zuordnung aus diesen Buchstaben abliest,
liest Blindtext.

### Wie die Zuordnung wirklich gemessen wurde

`fonts_en.swf` liegt in `Fallout4 - Interface.ba2`. Ein BA2 vom Typ `GNRL` ist
in vierzig Zeilen Python gelesen: Kopf, Dateisätze mit Versatz und Größe,
Namenstabelle am Ende, Daten per zlib. Danach die SWF entpacken und ihre
`DefineFont3`-Tags durchgehen.

Die Schrift hat **57 Glyphen**: Leerzeichen, `A`–`Z`, `a`–`z`, `{`, `|`, `}`
und ein geschütztes Leerzeichen. Damit ist die Zeichentabelle bekannt — aber
nicht, welcher Buchstabe welcher Knopf ist.

Also **gezeichnet**. Die Glyphenumrisse in `DefineFont3` sind gewöhnliche
SHAPE-Sätze; ein kleiner Parser macht daraus Pfade, ein Rasterer in reinem
Python (kein Pillow nötig, `zlib` reicht für PNG) macht daraus ein Blatt mit
allen 57 nebeneinander. Ein Blick darauf beantwortet alles auf einmal:

> **`A`–`Z` ist der Xbox-Satz, `a`–`z` der PlayStation-Satz.**

| | Xbox | PlayStation |
| --- | --- | --- |
| Bestätigen | `A` (Ⓐ) | `a` (✕) |
| Zweiter Knopf | `B` (Ⓑ) | `d` (○) |
| Dritter | `C` (Ⓧ) | `c` (□) |
| Vierter | `D` (Ⓨ) | `b` (△) |
| Kreuz links / rechts / runter / hoch | `T` `U` `V` `W` | `t` `u` `v` `w` |
| Kreuz ganz | `P` | `s` |
| Linke Schulter / rechte | `G` / `L` | `g` / `m` |
| Linker Trigger / rechter | `I` / `N` | `j` / `o` |
| Linker Stick / rechter | `F` / `K` | `f`/`i` / `l` |
| Start / Zurück | `O` (≡) / `E` | `p` (OPTIONS) / `e` (SHARE) |
| Nicht belegt | `R` (?) | — |

Die Richtungen sind in beiden Sätzen **gleich sortiert** — links, rechts,
runter, hoch —, und das ist auch die Probe: die PlayStation-Kreuze markieren
die gedrückte Richtung, indem sie deren Arm **hohl** lassen statt ihn zu
füllen. Programmatisch abgetastet (ein Pixel je Arm) ergibt `t` genau den
hohlen linken Arm, `u` den rechten, und so weiter — dieselbe Reihenfolge wie
die Pfeile `T`–`W`. Zwei unabhängige Wege, dieselbe Antwort.

Welcher Satz gilt, entscheidet nicht die Mod, sondern das Spiel:
`ControlMap::pcGamePadMapType` ist `kDirectX` oder `kOrbis` — dasselbe Feld,
nach dem sich die Vanilla-Menüs richten.

### Was das im Code heißt

Zwei Schriften in einer Zeile gehen nur über Markup, also ist die Hinweiszeile
jetzt `htmlText` statt `text`:

```html
<font face='Controller  Buttons' size='20'>A</font>) USE
```

Drei Dinge, die dabei zu beachten waren:

- **Kein zweites `setTextFormat`.** Das Feld setzt sein Format sonst über
  jeden Lauf und malt das `face` aus dem Markup wieder weg. `defaultTextFormat`
  **vor** dem Zuweisen reicht, und was das Markup nicht selbst sagt, erbt es
  von dort.
- **Höhe nach dem größten Lauf.** Ein Symbol steht 145 % der Schriftgröße, und
  ein Feld, das nur für die Wörter hoch genug ist, schneidet es oben ab. Die
  Höhe kommt jetzt aus `hintTallest`.
- **Escapen.** Alles, was nicht Markup sein soll, geht durch `Escape` —
  `KeyHintExtra` steht in der INI und darf ein `&` enthalten.

Und die Klammer fällt weg, wenn ein Knopf gezeichnet wird: `E) USE` liest sich
als Taste, `Ⓐ) USE` liest sich als Fehler. Vier Kreuz-Richtungen nebeneinander
wären außerdem viermal dieselbe Tinte für denselben Satz — liegen die vier auf
dem Steuerkreuz, steht **ein** Kreuzsymbol da, so wie das Spiel es auch macht.

### Was zu prüfen bleibt

- Ob `face` mit `embedFonts` wirklich greift. Es ist der Weg, den die
  Vanilla-SWFs gehen, aber unsere Bühne ist eine eigene. Kommen Kästchen statt
  Knöpfe, ist `GamepadGlyphFont` leer zu setzen — dann steht wieder `LB`,
  `RT` und so weiter da — und der nächste Versuch wäre `$Controller_Buttons`
  statt des ausgeschriebenen Namens.
- Ob `Controller  Buttons` oder `… inverted` besser zum Panel passt. Gemessen
  ist die nicht invertierte: weiße Scheibe, Buchstabe ausgespart. Auf dunklem
  Grund ist das die richtige.
- Die Größe: 145 % ist geschätzt nach dem, was die Vanilla-Felder tun (16 bis
  20 gegen 18). `GamepadGlyphSize` verstellt sie ohne Neubau.

## 40. Drei Nachbesserungen aus dem ersten Spielen (2026-09-07)

Die Steuerung und die Symbole standen; drei Dinge fielen beim Spielen auf.

### Die Schließen-Taste stand als Text da

`B) CLOSE` kam aus `KeyHintExtraGamepad` — reiner Text, weil beim Bau der
Zeile für die Tastatur galt: welche Taste das Menü schließt, gehört dem Spiel
und wir erfahren es nie.

Für den Controller stimmte das nicht mehr. `Install` **muss** diesen Knopf
kennen, sonst könnte es ihn nicht in Ruhe lassen (Abschnitt 38) — er lag also
längst da und wurde nur nicht weitergegeben. `input::PadCloseButton` gibt jetzt
die Bindung von `Cancel` heraus, und die Zeile zeichnet sie wie jeden anderen
Knopf. `KeyHintExtraGamepad` bleibt als Auffanglösung für den Fall, dass die
Bindungen nicht zu lesen waren.

Merksatz für den Rest der Mod: was eine Vorsichtsmaßnahme herausfinden musste,
ist danach eine bekannte Tatsache.

### Zeiger und Tasten haben um dieselbe Zelle gestritten

Die Marke lief mit `w`/`a`/`s`/`d`, und im nächsten Bild nahm der ruhende
Mauszeiger sie wieder an sich. `ForgetPointer` sollte das verhindern, tut es
aber nicht: es setzt die gemerkte Position auf den kleinsten Wert, und im
nächsten Bild ist die Zeigerposition davon verschieden — also gilt sie als
Bewegung. Aufgefallen ist es nur deshalb selten, weil `grid::At` außerhalb des
Panels nichts zurückgibt: lag der Zeiger daneben, passierte nichts. Lag er über
einer Zelle, war die Tastatur wirkungslos.

Die Antwort ist kein besseres Vergessen, sondern ein Zustand: **der Zeiger
schläft.** Er schläft ein, sobald eine Taste oder ein Knopf benutzt wird
(Maustasten ausgenommen — das ist der Zeiger, der für sich selbst spricht),
und wacht auf, wenn sich die Maus bewegt oder der **rechte** Stick ausgelenkt
wird. Der rechte Stick wird deshalb auch nicht beansprucht: er ist der, mit
dem das Spiel seinen Cursor bewegt.

Solange er schläft, wird er **weder gezeichnet noch gefragt** — `TrackPointer`
steigt vorher aus.

### Der Cursor ist ein eigenes Menü

Zum Ausblenden: Fallout 4 zeichnet den Mauszeiger als `CursorMenu`, ein Menü
wie jedes andere (`RE::CursorMenu`, `MENU_NAME` `"CursorMenu"`). Damit ist es
derselbe Griff wie beim Crosshair — `menuObj.visible` auf falsch — und
dieselbe Vorsicht: jedes Bild neu, weil die Engine ihren Cursor
zurückstellt, wann es ihr passt, und beim Schließen des Menüs wieder auf
sichtbar.

**Nicht** über `MenuCursor::UnregisterCursor` gegangen. Das wäre der
naheliegende Weg und der gefährliche: `registeredCursors` ist ein Zähler, den
sich alle Menüs teilen, und wer ihn einmal zu oft senkt, nimmt dem ganzen
Spiel den Zeiger.

### Deckkraft

Die zweite Zeile über dem Gitter stand auf 0.75 und die Symbole erbten,
was ihre Bibliothek mitbrachte. Beides jetzt voll deckend: über der Wüste ist
eine gedimmte Zeile eine Zeile, die man suchen muss. Was die zweite Zeile zur
zweiten macht, ist ihre Größe, nicht ihre Blässe. Alle übrigen Transparenzen —
die Zellplatten, die Marke, die Hinweiszeile — bleiben, wie sie waren.

### Was zu prüfen bleibt

- Ob `CursorMenu.menuObj` wirklich das Anzeigeobjekt ist. Ist es das nicht,
  steht `pointer: no CursorMenu to hide` im Log und der Zeiger bleibt sichtbar
  — der Rest (schlafen, nicht gefragt werden) wirkt trotzdem.
- Ob der rechte Stick den Cursor tatsächlich bewegt. Falls das Spiel dafür den
  linken nimmt, weckt der rechte den Zeiger, ohne ihn zu bewegen — dann wäre
  `GridGamepadStick=0` die Lösung, oder das Wecken müsste an den linken Stick.

## 41. Was das Log erzählte (2026-09-07)

Drei Meldungen aus dem Spiel, und das Log beantwortete zwei davon sofort. Das
ist der Grund, warum so viel darin steht.

### Nuka-Cola stand auf jeder Seite

Im Log, beim Umschalten auf Seite 1 (die Waffenseite):

```
move: "[Nuka_Cola] Nuka-Cola" 8 -> no key
...
favorites (after the page): [1]T60x1 ... [7]Hunting Shotgunx1 [8][Nuka_Cola] Nuka-Colax1
cross: rewritten from the inventory -- ... [8]Nuka-Cola/54
```

Also: die Taste **wurde** geräumt, und danach lag der Gegenstand wieder darauf.
Und zwei Zahlen, die nicht zusammenpassen — `x1` in unserer Zählung, `/54` im
Kreuz.

Das ist die Stapelspaltung aus Abschnitt 10, nur von der anderen Seite gesehen.
Es gibt **zwei** Nuka-Cola-Stapel, und **beide** tragen `quickkeyIndex` 8.
`FindAndWriteStackDataForItem` schreibt aber nur den **ersten** Treffer. Also
räumte jeder Seitenwechsel einen davon und ließ den anderen liegen — und der
hielt die Taste über jede Seite hinweg fest.

`WriteFavorite` fragt jetzt nach: solange `Carries` sagt, dass die Taste noch
irgendwo hängt, wird noch einmal geschrieben, höchstens zwölfmal. Nur beim
Räumen — `a_from` unter zwölf ist eine echte Taste; mit `kNoKey` oder
`kNotAFavorite` in derselben Schleife würde man das halbe Inventar
favorisieren. Brauchte es mehr als eine Runde, steht das im Log.

### Die Zurück-Taste blieb Text

Die Ursache stand ebenfalls im Log, in der Zeile, die extra dafür da ist:

```
input: ... START BACK LB B 0xffff stay with the game
```

`0xffff`. Die Bindungstabellen sind voll von Platzhaltern für „nicht belegt",
und es ist nicht überall derselbe: `-1`, `0` und `0xffff` kommen alle vor. Die
Prüfung fing die ersten beiden ab und ließ den dritten durch, und weil in
diesem Spiel gar keine `Cancel`-Bindung fürs Gamepad steht, wurde `0xffff` zur
Schließen-Taste — ein Knopf, den es nicht gibt, also kein Symbol, also der
Auffangtext.

Jetzt: `Buttonish` lässt nur die sechzehn Zahlen durch, die überhaupt Knöpfe
sind, und die Vorgabe für die Schließen-Taste ist **B** statt null. B ist, was
das Vanilla-Menü auf einem Serien-Controller beantwortet, und steht ohnehin
unverrückbar auf der Sperrliste. Die `input:`-Zeile sagt jetzt am Ende, welcher
Knopf der Ausgang ist.

Nebenbei gelernt: `Quickkeys` liegt hier auf **LB**. Wer je vermutet, das Menü
öffne sich mit dem Steuerkreuz, hat damit ein Gegenbeispiel.

### Der Absturz — nicht bewiesen, aber entschärft

Das Log endet mit

```
[11:20:04] FavoritesMenu opened
[11:20:04] menu: FavoritesMenuGrid is up
```

und nichts danach. Kein `grid: 4 pages`, also fiel es zwischen dem Bau des
Menüs und der ersten Zeichnung. In genau dieses Fenster fällt `AdvanceMovie`,
und damit `TrackPointer` — das Neue von Abschnitt 40. Buffout hat nichts
geschrieben, ein Stack liegt also nicht vor. **Die Ursache ist damit nicht
bewiesen.**

Was aber sicher falsch war: das Anzeigeobjekt von `CursorMenu` lag in einer
globalen `GFx::Value` und wurde jedes Bild beschrieben. Das ist eine Referenz
in den Speicher eines **fremden** Films, und wann dieses Menü kommt und geht,
entscheidet das Spiel, nicht wir. Jetzt wird es bei jedem Zugriff frisch
geholt — ein Hash-Lookup, der niemandem etwas schuldet.

Dazu ein Schalter: `GridHidePointer=0` nimmt den ganzen Griff heraus, ohne
etwas neu zu bauen. Stürzt es weiter ab, ist das der erste Versuch, und dann
liegt es nicht am Zeiger.

### Die Symbole waren zu einfarbig

Der Grund stand die ganze Zeit als Kommentar im Code, seit die Symbole
gebaut wurden (Abschnitt 31):

> „A symbol built from several shapes names a colour for each, separated by
> commas -- RadAway is brown and silver. We paint one flat colour, so the
> first is the one that counts."

Genau das sah man: im Pip-Boy ist RadAway ein brauner Beutel mit silberner
Kappe, im Gitter war es ein brauner Fleck. `colorname` trägt eine Liste, ein
Eintrag je Teil des Symbols, und wir haben die Liste am ersten Komma
abgeschnitten.

Jetzt bleibt die ganze Liste erhalten (`tags::Icon::colors`), und `PaintParts`
malt die Kinder des Symbols der Reihe nach: Kind *i* in Farbe *i*, von hinten
nach vorn — die Reihenfolge, in der Flash stapelt und in der die
Konfiguration schreibt. Weniger Teile als Farben nimmt, was da ist; mehr Teile
als Farben malt den Rest in der letzten genannten, weil eine Liste, die ausgeht,
„und der Rest auch" gemeint hat.

**Diese Reihenfolge ist die eine Annahme darin.** Deshalb schreibt sie sich
einmal ins Log:

```
icons: a symbol of N parts against M colours
```

Sehen die Symbole falsch herum eingefärbt aus, ist die Reihenfolge umzudrehen
— das ist eine Zeile —, und `IconColors=0` ist bis dahin die Rücknahme.

## 42. Der Absturz hatte einen Namen, die Farben auch (2026-09-07)

### Der Absturz: das Panel überlebte sein Menü

Zweimal dieselbe Signatur im Log — und die Signatur ist die halbe Antwort:

```
FavoritesMenu opened
menu: FavoritesMenuGrid is up
                                <- hier hört es auf
```

Kein `grid: 4 pages`. Der Sturz liegt also zwischen dem Bau des Menüs und der
ersten Zeichnung. Und die Zeile, die es erklärt, steht **davor**, beim
Schließen:

```
FavoritesMenu closed
page: back to 1 because the menu closed
...
grid: 4 pages; ... with 46 children      <- gezeichnet, nachdem geschlossen wurde
```

Der Ablauf: Schließen stellt `RestoreDefaultPage` als UI-Aufgabe ein, die
Aufgabe schaltet eine Seite, und `GoToPage` zeichnet danach das Panel. Sie
läuft **nach** dem Schließereignis, während der Film schon auf dem Weg
hinaus ist — aber noch gefunden wird. Also wurde das Panel in ein Menü
gebaut, das gleich zerstört wird, und seine Anzeigeobjekte lagen danach in
unseren Globalen, ihren Film überlebend. Beim nächsten Öffnen beginnt
`grid::Draw` mit `Release()`, und `Release()` greift nach genau diesen
Objekten, um sie von einer Bühne zu nehmen, die es nicht mehr gibt.

Ein Zeiger in freigegebenen Speicher, ein Bild später. Dass es nur manchmal
sofort abstürzte, ist die übliche Gnade solcher Fehler.

**`ShowGrid` fragt jetzt zuerst `g_favoritesMenuOpen`.** Das Schließereignis
hat bereits gesagt, was gilt; danach wird nicht mehr gezeichnet. Ein
Einzeiler, aber kein Gürtel-und-Hosenträger: er ist die Reparatur.

Aufgefallen ist es jetzt, weil `DefaultPage=1` in der INI des Spielers steht.
Mit `DefaultPage=0` läuft `RestoreDefaultPage` nicht, und der Fehler schlief.

Der Verdacht aus Abschnitt 41 — der zwischengespeicherte `CursorMenu`-Wert —
war also **nicht** die Ursache. Falsch war er trotzdem, und er bleibt
korrigiert.

### Die Farben: wir haben alle Paletten gelesen

Der Mehrfarb-Pfad aus Abschnitt 41 lief nachweislich (`icons: a symbol of 2
parts against 2 colours`), und trotzdem war alles rosa. Die Ursache lag eine
Ebene tiefer.

`Interface\ItemSorter\ColorSets` enthält mehrere Dateien, die **dieselben**
Farbnamen **verschieden** definieren:

```xml
Default.xml : <color name='MedicBrown' alias='brown'   />
Simple.xml  : <color name='MedicBrown' alias='Medical' />
              <color name='brown' hex='ffd98e' />   ... und zwanzig weitere
                                                     auf denselben Wert
```

Wir lasen alle drei und ließen den letzten gewinnen. Der letzte ist
alphabetisch `Simple.xml`, und `Simple.xml` ist genau das: eine Palette, die
fast alles auf einen Farbton legt. Das Gitter war nicht falsch eingefärbt, es
war korrekt nach der falschen Palette eingefärbt.

Das ist derselbe Fehler wie bei den Variations, nur ohne die Warnung: der
Ordner `Variations` wurde übersprungen, weil die XML ihn als Alternativsatz
**deklariert**. Für `ColorSets` tut das keine Datei — die Wahl steht allein in
FallUIs MCM:

```
Data\MCM\Settings\FallUIIconLibrary.ini
[MainSettings]
sIconsColorSet=Default.xml
```

Also wird der Ordner jetzt ebenso übersprungen und genau die eine gewählte
Datei gelesen, zuletzt, damit sie gewinnt. Fehlt die Einstellung, gilt
`Default.xml` — das, was FallUI ausgewählt ausliefert. Das Log sagt, welche
Palette es wurde.

**Die Lehre, und sie gilt über diesen Fall hinaus:** „alle Konfigurationen
lesen und zusammenführen" (Abschnitt 0) stimmt für Dateien, die einander
**ergänzen**, und ist falsch für Dateien, die einander **ersetzen**. Zwei
solche Ordner sind jetzt bekannt. Wer einen dritten findet, erkennt ihn daran,
dass mehrere Dateien denselben Namen verschieden definieren.

### Nebenbei bestätigt

- `favorites: "[Nuka_Cola] Nuka-Cola" carried 3 on 3 stacks at once` — die
  Stapelspaltung aus Abschnitt 41 war nicht zweifach, sondern dreifach, und
  die Schleife räumt sie.
- `... START BACK LB B stay with the game, and B is the way out` — kein
  `0xffff` mehr, und die Schließen-Taste ist benannt.

## 43. Ein Schalter, der hängen bleibt, nimmt dem Spiel die Steuerung (2026-09-07)

Gemeldet: nach einem Controller-Screenshot (Xbox-Taste plus RT) war die
Steuerung von Fallout 4 vollständig weg. Das klingt nach einem fremden
Problem und ist mit hoher Wahrscheinlichkeit unseres.

### Warum ein Schalter nicht reicht

`input::Listen` wird vom Schließereignis des Favoritenmenüs ausgeschaltet.
Kommt dieses Ereignis nicht — weil das Fenster den Fokus verliert, weil ein
Overlay dazwischenfährt, weil das Menü auf einem anderen Weg verschwindet —,
bleibt der Schalter an. Und dieser Handler steht **vorn** in
`MenuControls::handlers`.

Was dann verschluckt wird: `w`, `a`, `s`, `d`, `E`, Return, Einfügen,
Entfernen, das Steuerkreuz, drei Knöpfe — **und der linke Stick**, denn den
beanspruchen wir ganz oder gar nicht (Abschnitt 38). Also: kein Gehen, kein
Umsehen mit der Tastatur, kein Gehen mit dem Controller. Für den Rest der
Sitzung.

Ein boolescher Schalter kann diesen Fall nicht abfangen, weil genau der
Vorgang fehlt, der ihn umlegen würde. **Ein Herzschlag kann es.**

`TrackPointer` läuft in `AdvanceMovie` unseres eigenen Menüs, also nur, wenn
das Panel wirklich gezeichnet wird. Es stempelt jetzt als Allererstes
`input::Alive()`. `ShouldHandleEvent` beansprucht nichts mehr, wenn dieser
Stempel älter als eine halbe Sekunde ist. Kein verpasstes Ereignis und kein
verlorener Fensterfokus kann das fälschen: hört das Panel auf zu zeichnen,
hört der Anspruch auf.

### Und das, was außerhalb liegt

Zwei Dinge greifen aus dem Menü hinaus: das versteckte Crosshair und der
versteckte Mauszeiger. Beide wurden bisher nur im Schließereignis
zurückgegeben — mit demselben Fehler dahinter. Ein hängengebliebener
`CursorMenu.visible = false` heißt: kein Mauszeiger mehr, in keinem Menü.

`menu::SetOnGone` hängt jetzt im **Destruktor** von `GridMenu`. Der läuft,
wie das Menü auch verschwindet. Dort werden `Listen(false)`, der Zeiger und
das Crosshair zurückgegeben — und ausdrücklich **nichts** aus unserem eigenen
Film angefasst: der ist ja gerade das, was zerstört wird (Abschnitt 42 sagt,
was das sonst kostet).

**Die Regel dahinter, für alles Weitere:** was diese Mod außerhalb ihres
eigenen Menüs anfasst, wird im Destruktor zurückgegeben, nicht im
Schließereignis. Das Ereignis ist der gewöhnliche Weg, nicht der einzige.

### Nicht unsere Meldung

Im vierten Screenshot stand unten „[Grenade] Fragmentation Grenade". Das ist
nicht von uns: diese Mod schreibt genau **eine** HUD-Meldung, und die hat die
Form `<Wort> <n> / <m>` — nie einen Gegenstandsnamen. Im Spiel liegt
`VisibleFavorites.dll`, die Favoriten am Körper anzeigt und ihre
`VisibleFavorites.ini` in vierundzwanzig `[SlotN]`-Blöcken mit je einem `Fav=`
führt. Ein Seitenwechsel schreibt alle zwölf Tasten neu; dass eine Mod, die
auf Favoriten hört, darauf etwas sagt, ist die naheliegende Erklärung. Zu
prüfen mit `bEnableOverlay=0` in deren INI.

## 44. Die Zeile unten war unsere, und die Farbe fehlte an einer zweiten Zeichnung (2026-09-07)

### „[Grenade] Fragmentation Grenade" unter dem Gitter

Der Verdacht des Spielers stimmte: das sah aus wie das alte Favoritenmenü,
**weil es das alte Favoritenmenü war.**

Die Log-Zeile, die es seit Tagen sagt:

```
grid: the menu holds Cross_mc (-417,-480 418x419)  ItemName_tf (-408,-561 400x41)  ItemAmmo_tf (-358,-523 300x36)
```

Drei Objekte, nicht eines. Wir haben nur `Cross_mc` versteckt; `ItemName_tf`
und `ItemAmmo_tf` wurden **verschoben** — unter unser Panel. Das stammt aus
der Zeit, als das Panel noch keine eigene Beschriftung hatte (Abschnitt 21),
und ist seit Abschnitt 32 überflüssig: das Panel schreibt seine zwei Zeilen
selbst.

Übrig blieb ein Paar Textfelder, die weiterhin der Auswahl des **Kreuzes**
folgen, nicht unserer Marke. Ein Seitenwechsel schreibt alle zwölf Tasten neu,
das Kreuz aktualisiert seine Auswahl, und unter dem Gitter stand plötzlich der
Name irgendeines Gegenstands.

Jetzt werden alle drei versteckt und in `Release` zurückgegeben. `moveLabel`
ist weg.

### Der Nachbearbeitungsschritt: nichts zu tun

Nachgesehen, wie versprochen. `Data\MCM\Config\FallUIIconLibrary\config.json`
beschreibt die Einstellungen selbst:

```json
{"type":"dropdown","text":"$Preset",
 "options":["$Custom","$Normal","$Pastel","$Intense","$Grayscale","$Classic"],
 "id":"iIconsColorPostEffect:MainSettings"}
```

Dasselbe Feld ist an zwei Stellen im Menü: oben als **Voreinstellung**, und
im Zweig „Custom" als eigentlicher Nacheffekt (`$None`, `$Grayscale`,
`$Pastel`, `$Intense`). Der Spieler steht auf **1**, und oben heißt 1
`$Normal` — keine Voreinstellung mit Effekt. Helligkeit und Sättigung stehen
beide auf 100, also neutral.

**Es gibt also nichts nachzubauen.** Wer hier eine Farbmatrix eingebaut
hätte, hätte einen Effekt nachgeahmt, den das Spiel gar nicht anwendet. Der
Rest an Abweichung musste woandersher kommen — und kam.

### `subicon`

Die Konfiguration kennt zwei Muster, und wir kannten nur eines:

```xml
<tag keyword="MedPills"        icon="M8r.Repo.MedPills"  colorname="MedicSilver,MedicOrange" />
<tag keyword="DrugPillsPurple" icon="M8r.Repo.MedPills" subicon="M8r.Repo.MedPills2" colorname="DrugSilver,MedicPurple" />
```

Beide tragen zwei Farben, und sie meinen **Verschiedenes**:

- ohne `subicon`: **eine** Zeichnung aus mehreren Teilen, die Liste läuft an
  den Teilen entlang (das ist, was Abschnitt 41 gebaut hat, und es war
  richtig — die Log-Zeile `a symbol of 2 parts against 2 colours` betraf
  genau diesen Fall);
- mit `subicon`: **zwei** Zeichnungen übereinander — Pillen in Silber mit
  ihrer farbigen Hälfte darauf, ein Medikoffer mit Werkzeug darüber. Erste
  Farbe für die erste, zweite für die zweite.

`subicon` haben wir schlicht nie gelesen. Die zweite Zeichnung wurde nie
erzeugt, also hatte ihre Farbe nichts, worauf sie hätte gehen können — und
die Zelle blieb einfarbig, egal wie viele Farben in der Liste standen. Zwölf
Tags im Hauptbestand haben eines.

`Place` erzeugt jetzt eine Zeichnung und gibt sie zurück, `Symbol` benutzt es
zweimal und färbt nach dem Muster, das die Anwesenheit eines `subicon`
vorgibt.

**Die Lehre:** zwei Farben in der Liste waren nie ein einzelner Fall. Wer
sich künftig fragt, warum eine Zelle blasser aussieht als im Pip-Boy, sieht
zuerst im Tag nach, ob dort ein zweites Attribut steht, das wir nicht lesen.

## 45. Mittig wovon, zweiter Teil (2026-09-07)

Abschnitt 34 hat die Zeilen **über** dem Gitter auf die Zellen zentriert statt
auf das Panel. Dieselbe Frage eine Ebene höher war offen geblieben: das Panel
selbst stand mittig auf der Bühne — und das Panel ist nicht das Gitter.

Zwei Gründe, warum die zwei Mitten auseinanderliegen:

- Die Seitenzahlen stehen **nur links**. Ist die Spalte da, sitzen die Zellen
  um ihre halbe Breite zu weit rechts.
- Über dem Gitter stehen zwei Zeilen und die Tastennamen, darunter nur die
  Hinweiszeile. Der obere Block ist deutlich höher als der untere, also sitzen
  die Zellen um die halbe Differenz zu tief.

Gerechnet für die Einstellungen im Log (Zelle 48, vier Seiten, Seitenzahlen
aus, Bühne 1280x720): das Panel war 340 hoch und stand bei y=190, die Zellen
begannen 107 darin und waren 201 hoch — ihre Mitte lag also bei 397 statt bei
360. **37 Einheiten zu tief**, gut fünf Prozent der Bildhöhe. Waagerecht
stimmte es bei diesem Spieler zufällig, weil `ShowPageNumbers=0` die Spalte
auf null setzt; mit Zahlen wären es 13 Einheiten nach rechts gewesen.

Jetzt wird die Ecke des Panels aus den Zellen zurückgerechnet:

```cpp
left = (stageWidth  - CellsWidth(m)) / 2 - CellsLeft(m);
top  = (stageHeight - cellsHeight)   / 2 - cellsTop;
```

Alles andere hängt am Panel und folgt von selbst. Ein ausdrückliches `GridX`
oder `GridY` bleibt die Ecke des Panels — wer eine Zahl hinschreibt, meint
eine Stelle, keine Ausrichtung.

Die Log-Zeile nannte übrigens bis eben nicht die tatsächliche Position,
sondern rechnete die alte Formel noch einmal aus. Sie sagt jetzt `left` und
`top`, sonst hätte die nächste Messung an dieser Stelle gelogen.

## 46. Der Name in der Hand, nicht der im Plugin (2026-09-07)

Vier Meldungen auf einmal, und alle vier hatten dieselbe Ursache:

- T60 Pistol zeigte ein Gewehr statt einer Pistole
- Liberator ein Gewehr statt einer Schrotflinte
- Righteous Authority eine Laserpistole statt eines Lasergewehrs
- und ein an der Werkbank umgebautes Sten Mk II änderte sein Symbol nie

Der Auszug aus `LogIcons` sagt es in drei Zeilen:

```
icon: "T60"       -> [Rifle] M8r.Fo4Wpn.AK
icon: "Laser"     -> [LaserPistol] M8r.WpnLaser.LaserGun
icon: "Liberator" -> [Rifle] M8r.Fo4Wpn.AK
```

**„T60".** Das Pip-Boy nennt dasselbe Ding „T60 Pistol". Wir lasen den Namen
aus dem Basisobjekt — `TESFullName::GetFullName` —, und das ist der Name im
Plugin. Für eine Waffe ist das kaum ein Name: das Plugin sagt „T60", „Laser",
„Hunting Shotgun"; das Spiel sagt „T60 Pistol", „Righteous Authority",
„Rapid Advanced Hunting Shotgun". Den Unterschied macht die **Instanz** — was
angebaut ist und was die Namensregeln daraus machen.

Und da das Auto-Tagging des Sorters **den Namen liest**, entscheidet dieser
Unterschied über Pistole oder Gewehr.

Er erklärt auch den vierten Fall von selbst, und das ist die Probe: ein
Basisname kann sich nicht ändern, also konnte sich nichts ändern, was wir aus
ihm ableiten. Eine an der Werkbank umgebaute Waffe war für uns dieselbe Waffe.

Die Engine macht das auf Zuruf: `BGSInventoryItem::GetDisplayFullName`, dem
man sagt, welcher Stapel gemeint ist. Die Stapel hängen als Kette, die Nummer
wird also erlaufen. `detail::DisplayName` tut beides und fällt auf den
Pluginnamen zurück, wenn nichts davon im Inventar liegt — eine Seite, die
sich an etwas erinnert, das der Spieler weggeworfen hat.

Trägt jemand zweimal dasselbe Basisobjekt mit verschiedenen Anbauten, gewinnt
der erste Stapel mit Instanzdaten. Das ist dieselbe Wahl, die `Describe` für
die zweite Zeile längst trifft, und **dass die beiden übereinstimmen, ist
mehr wert, als dass eine von beiden schlauer wäre.**

### Was daraus folgt

Überall dort, wo diese Mod einen Gegenstand *benennt*, ist ab jetzt der
angezeigte Name gemeint und nicht der aus dem Plugin. Wer eine neue Stelle
baut, die etwas aus einem Namen ableitet — Symbol, Farbe, Sortierung —, nimmt
`detail::DisplayName`. `TESFullName::GetFullName` auf einem Basisobjekt ist
in dieser Mod nur noch der Notnagel innerhalb dieser einen Funktion.

## 47. Das Kreuz im Pip-Boy, vermessen (2026-09-07)

Gefunden, und zwar an einer Stelle, die niemand geraten hätte:

```
PipboyMenu.instance36.ModalFadeRect_mc.instance402.Cross_mc   216,114  418x420
```

**Ein Kind des Dimmers.** `ModalFadeRect_mc` ist nicht bloß der graue Schleier
hinter dem Dialog — der ganze Dialog hängt darin. Deshalb war die erste
Vermessung, die den Dimmer *sichtbar* meldete und trotzdem kein Kreuz fand,
kein Widerspruch: da war ein Modal offen, aber nicht dieses.

Der Aufbau, wie er auf dieser Installation wirklich aussieht:

```
PipboyMenu.instance36                          (die Inventarseite)
  ModalFadeRect_mc                 0,0   885x707   der Dimmer, seitengroß
    ModalFadeInputCatcher          0,0   885x707
    instance402                    0,0   454x587   der Dialog selbst
      Background_mc              200,97  454x553
      Header_tf                  225,60  400x51    "ASSIGN FAVORITE"
      Cross_mc                   216,114 418x420   die zwölf Felder
        EntryHolder_mc             0,0   418x419
        Selection_mc             180,360  59x59
      SelectionName_tf           208,546 435x41
      SelectionAmmo_tf           275,599 300x36
      FallUI_textEnhanced80/81                     Zutaten von FallUI
```

Bühne: **876x757**. Der frühere Lauf mit 1244x700 war der in der Power Armor —
Abschnitt 42 hatte die beiden vertauscht, hier steht es richtig herum.

### Was das für ein Gitter heißt

- **Erreichbar ist es.** `FindByName` aus `main.cpp` — dasselbe Werkzeug, das
  das Crosshair im HUD sucht — findet `Cross_mc` bei Tiefe 5. Über Namen der
  Zwischenstufen darf nichts laufen: `instance36` hieß in einem anderen Lauf
  `instance8`, und `instance402` ist genauso vergänglich. Vom Menü aus suchen,
  nicht durchhangeln.
- **Der Platz ist knapp, aber er reicht.** Der Dialog ist 454 breit, das Kreuz
  418x420. Zwölf Spalten in 418 Einheiten heißen rund **33 pro Zelle** — die
  INI lässt 24 bis 96 zu, das passt. Vier Seiten zu 35 sind 140 in der Höhe,
  bei 420 verfügbar. Es ist also eher zu viel Platz als zu wenig.
- **Der Rahmen ist schon da.** `Header_tf`, `SelectionName_tf` und
  `SelectionAmmo_tf` sind genau die Zeilen, die unser Panel sich selbst
  zeichnet. Im Pip-Boy müssten wir sie nicht bauen, sondern füllen.

### Und was dort etwas anderes ist

Im HUD **benutzt** man eine Zelle. Im Pip-Boy **belegt** man sie: das Kreuz
ist ein Zuweisungsdialog. Ein Gitter an dieser Stelle bedeutet also nicht
"dasselbe nochmal", sondern das, was heute gar nicht geht — einen Gegenstand
direkt auf Seite 3, Taste 7 legen, ohne vorher durch die Seiten zu blättern.
Das ist der eigentliche Gewinn, und es ist ein anderer Gewinn als der im HUD.

Offen, und erst im Spiel zu beantworten: ob die Eingabe des Dialogs sich
genauso abfangen lässt wie die des Favoritenmenüs, und was passiert, wenn man
`Cross_mc` versteckt — der Dialog hört womöglich auf dessen Auswahl.

## 48. Das Gitter steht im Pip-Boy (2026-09-07)

In drei Schritten, jeder für sich prüfbar, jeder einzeln zurückzunehmen.

### Schritt 1: das Kreuz verstecken, und sonst nichts

Die eine Frage, auf der alles Weitere steht: **arbeitet der Dialog weiter,
wenn sein eigenes Kreuz unsichtbar ist?** Wäre die Antwort nein gewesen,
hätte das Gitter *neben* dem Kreuz stehen müssen und die Schritte 2 und 3
sähen völlig anders aus.

Die Antwort ist ja. Gefunden über `FindByName` vom Menü aus — die
Zwischennamen taugen nichts, derselbe Clip hieß in drei Läufen `instance402`,
`instance389` und die Seite einmal `instance8`, einmal `instance36`.

### Schritt 2: zeichnen, ohne Eingabe

`grid::Draw` kann das Panel jetzt woanders aufhängen als an der Bühne. Überall
sonst ist die Bühne der Bildschirm und das Panel wird darauf zentriert; im
Pip-Boy ist die Bühne der kleine Schirm am Modell, und der Dialog hat ein
eigenes Koordinatensystem. Also übergibt der Aufrufer beides: den Clip zum
Anhängen und das Rechteck zum Füllen. Die Zentrierrechnung aus Abschnitt 45
merkt den Unterschied nicht — sie bekommt statt der Bühne eben ein Rechteck.

Das Rechteck ist das des Kreuzes, **aus dem Kreuz gelesen**, in den
Koordinaten dessen, was es hält. Zwölf Spalten in seine 418 Einheiten ergeben
eine Zelle von rund 33.

**Es kam an.** Der Pip-Boy zeichnet, was wir hinzufügen — das war die teuerste
der offenen Fragen, weil er auf eine Textur am Modell rendert und nicht auf
eine HUD-Bühne.

Zwei Dinge sind dort aus: unsere zwei Textzeilen (der Dialog hat
`SelectionName_tf` und `SelectionAmmo_tf` bereits) und die Tastenleiste.
Symbole fehlen noch, weil die Bibliotheken in *unser* Menü geladen sind und
nicht in den Pip-Boy.

### Schritt 3: nichts bauen

Der Trick ist, **nichts** zu bauen. Das Kreuz ist noch da, hört noch zu und
weist noch zu — es ist bloß unsichtbar. Der Spieler bewegt seine Auswahl mit
denselben Tasten wie immer und drückt dasselbe Accept, und das Spiel weist zu,
wie es immer zugewiesen hat.

Es fehlten nur zwei Dinge: **es zu sehen**, und **eine Seite, auf die es
geht**.

Das Sehen: `selectedIndex` vom Kreuz lesen und diese Taste markieren.
Die Seite: die Seitentasten drehen im Pip-Boy längst die Seiten der Engine —
das tun sie seit Abschnitt 22 und aus genau diesem Grund. Wer also blättert
und dann zuweist, weist auf die Seite zu, die gerade steht.

Damit kann man einen Gegenstand direkt auf Seite 3, Taste 7 legen. Das ging
vorher überhaupt nicht, und es ist keine Zeile Zuweisungscode dafür
geschrieben worden.

Gelesen wird zehnmal in der Sekunde aus der Tastaturschleife heraus, als
UI-Aufgabe. Das Kreuz wird dafür **festgehalten** — gegen die Regel aus
Abschnitt 42, und mit Absicht: es zu suchen heißt, einen Baum mit tausend
Knoten zu durchlaufen, und das tut man nicht zehnmal je Sekunde. Losgelassen
wird es beim Herunternehmen des Gitters und beim Schließen des Pip-Boys — die
einzigen zwei Wege, auf denen dieser Film darunter weggehen kann.

### Was noch fehlt

- **Symbole.** `icons::Want` lädt in das Menü, auf dem gezeichnet wird; im
  Pip-Boy ist das ein anderer Film. Erst prüfen, ob die Anwendungsdomäne dort
  überhaupt erreichbar ist (Abschnitt 29 war die Arbeit dafür im eigenen
  Menü).
- **Die Zellen sind leer.** Ohne Symbol steht dort im Moment nichts — im HUD
  fällt das nicht auf, weil dort immer eines da ist.
- **Der Aufruf hängt an einer Taste.** Von selbst kommt das Gitter noch
  nicht; dafür müsste das Erscheinen des Dialogs bemerkt werden, und die
  einzige verlässliche Stelle dafür ist bislang die Tastaturschleife.

## 49. Wie viele Seiten, und wie sie in den Dialog passen (2026-09-07)

### Es gibt keine Acht

Gefragt wurde, ob acht Seiten das Maximum von Fallout 4 seien. Nein — es gibt
in Fallout 4 überhaupt keine Seiten. Das Spiel kennt **zwölf Schnelltasten**,
sonst nichts; die Seiten sind eine Schicht dieser Mod, und sie liegen im
Mitspeicher von F4SE. Die Engine hält immer genau eine davon, und ein
Seitenwechsel schreibt die zwölf Tasten neu (Abschnitt 15).

Die einzige Grenze ist deshalb unsere eigene: `PageCount` nimmt **1 bis 32**.
Acht ist nichts Besonderes.

### Was das für den Dialog heißt

Die Zelle wurde bislang allein aus der Breite gerechnet — zwölf Spalten in
die 418 Einheiten des Kreuzes, also rund 33. Das ging gut, solange vier Reihen
davon 138 der verfügbaren 419 brauchten. Bei acht Seiten sind es 278, immer
noch bequem; bei zwölf wäre es vorbei.

Also fitten jetzt **beide Richtungen**, und die Zelle ist die kleinere der
zwei Antworten. Gerechnet für das Rechteck des Kreuzes:

| Seiten | Zelle | Zellen |
| --- | --- | --- |
| 4 | 33.0 | 418 x 138 |
| 8 | 33.0 | 418 x 278 |
| 12 | 31.5 | 400 x 400 |
| 16 | 23.3 | 301 x 403 |
| 32 | 12.0 | 166 x **446** |

Bei 32 greift der Boden von 12 Einheiten und es läuft über. Das ist Absicht:
unter zwölf Einheiten ist eine Zelle kein Bild mehr, sondern ein Punkt, und
ein unlesbares Gitter, das hineinpasst, ist schlechter als ein lesbares, das
ansteht. Wer zweiunddreißig Seiten fährt, sieht sie im Pip-Boy nicht alle.

### Und eine Ausnahme von Abschnitt 45

Auf dem Bildschirm werden die **Zellen** zentriert und alles andere hängt
sich daran — dort ist über den Zellen immer Platz. Im Dialog ist er das
nicht: das Rechteck ist alles, was es gibt, und eine mittig gesetzte Zeile
kletterte über das, was der Dialog darüber schreibt. Dort wird deshalb der
ganze Block zentriert, vom oberen Rand des Panels bis zur untersten Reihe.

Dieselbe Regel wäre an beiden Stellen falsch gewesen, und der Grund steht in
je einem Satz an beiden Stellen im Code.
