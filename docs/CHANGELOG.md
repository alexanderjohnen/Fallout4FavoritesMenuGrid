# Changelog

## 1.2.0 — 2026-09-19

Fixed: after assigning a favorite in the Pip-Boy, the inventory list stopped taking up and down (stick or W/S) until a tab change rebuilt it. The dialog frees the list's keys as it closes, and the grid locked them again in the same frame. Reported by Hitman136.

New: GridBackdrop, a dark plate behind the grid in percent (0 to 100, off by default), for skies that hide the icons. With a backdrop, the Vault-Tec emblem and "Q.A.O.S. - Quick Access Operating System" stand over the grid while nothing is marked.

New: PauseGame=1 stops the game while the favorites menu is open, the way the Pip-Boy does. Off by default: the menu is meant to be as quick as the game's own.

New: the game's own icons. An item no sorter has tagged - or every item, without a sorter - shows the picture the vanilla favorites cross would show, on the frame the game's own routine picks (weapon type keywords on the actual instance, the item keywords the game uses for gloves, helmets, clothes, chems, alcohol, food, repair kits and med bags, and the survival effect keywords), tinted in your HUD colour. Loaded from the game's FavoritesMenu.swf; nothing is shipped. Not in the Pip-Boy, where FallUI draws the icons. Seen on one machine only, with FallUI.

New: IconColors=2 paints every icon in your HUD colour, the way the cross tints its own; 1 keeps the sorter's colours, 0 leaves the artwork white.

Played on 1.10.163 with FallUI. Not played on Next-Gen or 1.11.x; the vanilla icons are off there.

## 1.1.1 — 2026-09-14

Fixed: with a controller, the mark in the Pip-Boy's assign dialog jumped between cells, a held stick ran through the pages, and the inventory list behind the dialog scrolled along. Cause: the game turned every D-pad press and stick push into an arrow key for the dialog's own hidden cross as well, and the cross walked it in its own shape; the grid then followed the cross. While the grid is in the dialog the D-pad's directions and the left stick now go to the grid alone. Up and down step one page per press or push; left and right still repeat while held. Keyboard and mouse are unchanged.

Played on 1.10.163 with FallUI. Not played on Next-Gen or 1.11.x; the notes for 1.1.0 below still apply there.

This is the release for everyone: 1.0.x is not continued.

## 1.1.0 — 2026-09-13 (test release)

One DLL for Fallout 4 1.10.163, Next-Gen (1.10.980/984) and 1.11.x. Addresses are resolved for the running game by CommonLibF4RD and the Runtime Database (required on Next-Gen and 1.11.x, optional on 1.10.163). The two functions this mod used to name by fixed number are now found by their relationship to functions the library knows, and refused rather than guessed when that fails.

Played on 1.10.163, where nothing changes from 1.0.1. Not played on Next-Gen or 1.11.x: this is the build for testers. Before the first start, put an empty file named FavoritesMenuGrid.trace next to FavoritesMenuGrid.dll; the library then writes what it resolved, and that file plus FavoritesMenuGrid.log (and a crash log, if any) is what a report needs.

Includes the 1.0.1 fix below.

## 1.0.1 — 2026-09-13

Fixed: assigning a favorite in the Pip-Boy with the Accept key (E / Return) sometimes used or equipped the item instead of assigning it. The mouse was not affected. Cause: FallUI moves the keyboard focus to the inventory list every time it rebuilds the list, and the grid rebuilds it on every page change; when that happened between choosing a cell and pressing Accept, the key went to the list. The grid now keeps the focus on the assign dialog while it is showing.

Fixed: a yellow rectangle could remain on the Pip-Boy screen after the assign dialog closed.

## 1.0.0 — 2026-09-13

First release.
