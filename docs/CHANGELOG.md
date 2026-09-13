# Changelog

## 1.1.0 — 2026-09-13 (test release)

One DLL for Fallout 4 1.10.163, Next-Gen (1.10.980/984) and 1.11.x. Addresses are resolved for the running game by CommonLibF4RD and the Runtime Database (required on Next-Gen and 1.11.x, optional on 1.10.163). The two functions this mod used to name by fixed number are now found by their relationship to functions the library knows, and refused rather than guessed when that fails.

Played on 1.10.163, where nothing changes from 1.0.1. Not played on Next-Gen or 1.11.x: this is the build for testers. Before the first start, put an empty file named FavoritesMenuGrid.trace next to FavoritesMenuGrid.dll; the library then writes what it resolved, and that file plus FavoritesMenuGrid.log (and a crash log, if any) is what a report needs.

Includes the 1.0.1 fix below.

## 1.0.1 — 2026-09-13

Fixed: assigning a favorite in the Pip-Boy with the Accept key (E / Return) sometimes used or equipped the item instead of assigning it. The mouse was not affected. Cause: FallUI moves the keyboard focus to the inventory list every time it rebuilds the list, and the grid rebuilds it on every page change; when that happened between choosing a cell and pressing Accept, the key went to the list. The grid now keeps the focus on the assign dialog while it is showing.

Fixed: a yellow rectangle could remain on the Pip-Boy screen after the assign dialog closed.

## 1.0.0 — 2026-09-13

First release.
