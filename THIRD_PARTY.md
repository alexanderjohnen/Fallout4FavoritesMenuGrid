# Third-party code and data

Favorites Menu Grid for Fallout 4 is GPL-3.0-or-later (see LICENSE). What it
is built with, and what it reads at runtime, is listed here.

## Compiled in

| Component | Licence | Used for |
| --- | --- | --- |
| [CommonLibF4](https://github.com/alandtse/CommonLibF4) (community branch, with the one-line patch under `patches/`) | MIT | the game's types and functions |
| [spdlog](https://github.com/gabime/spdlog) | MIT | the log file |
| [fmt](https://github.com/fmtlib/fmt) | MIT | formatting |
| [boost-stl-interfaces](https://www.boost.org/) | BSL-1.0 | required by CommonLibF4 |
| [rsm-mmio](https://github.com/Ryan-rsm-McKenzie/mmio) | MIT | required by CommonLibF4 |

## Required at runtime, not shipped

- [F4SE](https://f4se.silverlock.org/) and the
  [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327).

## Read at runtime, not shipped

- Item sorter tag configurations and colour sets under
  `Data\Interface\ItemSorter` (FallUI Item Sorter, DEF_UI, and their addon
  libraries) and the icon libraries they name. In the favorites menu these
  are loaded into this mod's own movie; in the Pip-Boy, FallUI's own
  `IconLibrary` is asked to draw them. None of that artwork or configuration
  is part of this mod, and all of it remains its authors'.
- The game's own menus (`FavoritesMenu`, `PipboyMenu`, `HUDMenu`) are drawn
  on, not replaced. `Interface\FavoritesMenuGrid.swf` is an empty movie
  written by `tools/build_swf.py`; it contains nothing of Bethesda's.

## Origin

A port of [Favorites Menu Grid for Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid),
itself a fork of Favorites Banks by SrSator (GPL-3.0-or-later).
