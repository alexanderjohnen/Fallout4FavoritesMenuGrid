# Favorites Menu Grid (Fallout 4) — Arbeitsstand

Stand: 2026-09-05. Portierung von
[Favorites Menu Grid für Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid).
**Abschnitt 0 ist der Einstieg** — dort steht, was gilt und was offen ist.

---

## 0. Aktueller Stand

**Meilenstein 3 steht, in der Nacht zum 2026-09-06:** Das Grid hat ein
**eigenes Menü** mit eigener Bühne, mittig auf dem Bildschirm, mit
**Mauszeiger statt Kamerasteuerung** — und die Zifferntasten des Spiels
funktionieren weiter. **Abschnitt 21 ist der Einstieg für die nächste
Sitzung:** dort steht, was steht, was offen ist und welche Fallstricke
Stunden gekostet haben.

**Meilenstein 2 steht, im Spiel bestätigt am 2026-09-05 gegen 19:30.** Ein
Favorit lässt sich parken und wieder auf eine Taste legen, und die Rotation
setzt alle zwölf Plätze auf einmal. Anzeige, Cache und Tastendruck folgen.
Das Kreuz zieht seit demselben Abend auch bei offenem Menü nach
(Abschnitt 17).

**Der Stand vom 2026-09-05, abends:** `ApplyPage` setzt alle zwölf Plätze auf
einmal (Abschnitt 15) und ist mit der Rotation auf F8 im Spiel bestätigt. Der
Schreibvorgang darunter ist am selben Abend noch einmal ersetzt worden: Er
geht jetzt über **`BGSInventoryItem::SetFavoriteIndex`**, die Funktion, die
das Spiel selbst benutzt (Abschnitt 16). Der Weg dahin führte durch den
Maschinencode der Engine, weil zwei geratene Versuche je einen Gegenstand im
Spielstand gekostet haben.

Vier Tasten aus der INI: F6 schreibt die Favoriten ins Log, F8 dreht sie um
einen Platz weiter, F7 parkt einen Favoriten und gibt ihm die Taste zurück,
F10 kopiert Engine-Code neben das Log.

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

**Zweiter Lauf, 23:43 Uhr — die Oberflächenfrage ist ganz beantwortet.**
`FavoritesMenu` nimmt den Sprite ebenfalls an, seine Bühne ist 1280 x 720,
und `menuObj` hat **`ProcessUserEvent`** (`HUDMenu` hat es nicht). Damit steht
auch der Auswahlweg des Starfield-Grids in Fallout 4 offen: Auswahl durch den
eigenen Pfad des Menüs schicken, statt selbst auszurüsten. Die Probe zielt
seitdem nur noch auf `FavoritesMenu` — in `HUDMenu` blieb das Rechteck sonst
die ganze Sitzung stehen.

Zwei Kleinigkeiten aus demselben Lauf: `stageWidth`/`stageHeight` kamen als
`-1` zurück, weil Scaleform sie als Int statt als Number liefert — dafür gibt
es jetzt `ReadNumber`, wie im Starfield-Projekt.

**Dritter Lauf, 23:50 Uhr — Meilenstein 0 ist komplett.** Mit gesetzten
Favoriten liest der Dump sie sauber aus:

```
slot  2 form FE0A7F12 type  43 "Hunting Shotgun"
slot  3 form 00023736 type  48 "Stimpak"
slot  8 form FE034F9A type  43 "T60"
```

Typ 43 ist `0x2B` = WEAP, Typ 48 ist `0x30` = ALCH. Namen, Typen und
FormIDs passen zusammen, `allowStimpakUse` liest 1. **`REL::ID(198281)` und
das Speicherlayout aus dem Header gelten auf 1.10.163.** Damit ist der
Datenzugriff auf die zwölf Plätze gesichert — lesend.

Eine Kleinigkeit bleibt offen: `weaponLoadedAmmo` steht durchgehend auf `-1`,
auch mit favorisierter Schrotflinte. Entweder wird das Feld nur für eine
geladene Waffe gepflegt, oder der Offset dieses Arrays stimmt nicht. Es wird
erst gebraucht, wenn der Munitionsstand angezeigt werden soll — dann
nachmessen.

**Quellcode ist öffentlich:**
`https://github.com/alexanderjohnen/Fallout4FavoritesMenuGrid` (GPL-3.0-or-later).

### Was offen ist

1. ~~Die drei Fragen aus Abschnitt 2.~~ **Alle drei beantwortet** (siehe
   oben). ~~Meilenstein 1: schreiben statt nur lesen.~~ **Gelöst** — der
   Seitenwechsel läuft über die Engine, Abschnitt 14.
2. **Der Kern.** Die Grundoperation steht: `ApplyPage` setzt alle zwölf
   Plätze auf einmal, Abschnitt 15. Offen ist davon der Fall, dass ein
   Gegenstand einen Platz bekommt, der vorher keinen hatte — dafür gibt es
   den Rundlauf auf F7, und er ist als Nächstes im Spiel zu messen.
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
