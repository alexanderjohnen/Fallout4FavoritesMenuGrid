# Favorites Menu Grid - F4SE

A Fallout 4 port of [Favorites Menu Grid for Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid):
all of your favorite pages at once, on a grid you can point at.

Still being built, and tested only on one machine so far. See
[HANDOFF.md](HANDOFF.md) for where the work stands and how each piece was
arrived at.

## What it does

- **Several pages of twelve favorites**, drawn together, one row per page. The
  pages travel in the save game through F4SE's co-save, so every character
  keeps their own.
- **Every cell is one move away.** The mouse or the keys reach any key on any
  page, and using one turns to its page on the way. There is no scrolling
  between pages while the grid is up.
- **The game's own favorites.** Everything is written into Fallout 4's twelve
  real favorite slots through the engine's own routines, so the number keys,
  the ammo counters and the equipped state keep working the way the game
  intends.
- **Pick up and put down.** A cell can be exchanged with another, across
  pages, without going through the Pip-Boy. A second press on something you
  are already holding takes it off again.
- **What a thing is and what it does.** The name of the marked item stands
  above the grid, and under it what it does: damage and ammunition for a
  weapon, resistances for a piece of armour.
- **Icons from the setup you already have.** If an item sorter and an icon
  library are installed, the cells carry the same symbols you see in your
  other menus. Nothing of that is shipped with this mod -- it is read where
  you already have it, the way FallUI's own auto-detect does, and everything
  works without it.
- **The game's own look.** Fallout 4 lets you colour the interface, and the
  grid follows that colour rather than bringing its own.

## What it puts in your game folder

- `F4SE\Plugins\FavoritesMenuGrid.dll` and its INI.
- `Interface\FavoritesMenuGrid.swf` -- 36 bytes, an empty stage with no
  timeline and no ActionScript. It belongs to no vanilla menu, so it replaces
  nothing and cannot collide with DEF_UI, FallUI, HUDFramework or anything
  else that owns those files. The grid itself is drawn into it from C++.

## Requirements

- Fallout 4 **1.10.163** (the original runtime)
- [F4SE](https://f4se.silverlock.org/)
- [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327)

Later runtimes (1.10.980 and up) need their own address IDs and are not
supported yet.

## Building

```
git clone --recurse-submodules <this repository>
set VCPKG_ROOT=C:\Dev\vcpkg
cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
py -3 deploy.py
```

`deploy.py` copies the DLL, its PDB and the movie into the game found at
`FALLOUT4_DATA`, or at the default Steam install. It never overwrites an INI
that is already there, because that one holds your keys.

## Licence

GPL-3.0-or-later, inherited from the Starfield mod this is a port of, which is
itself a fork of [Favorites Banks](https://www.nexusmods.com/starfield/mods/17906)
by Sator. CommonLibF4 is MIT and is not affected by that.
