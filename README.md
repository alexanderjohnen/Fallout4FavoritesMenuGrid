# Favorites Menu Grid - F4SE

A Fallout 4 port of [Favorites Menu Grid for Starfield](https://github.com/alexanderjohnen/StarfieldFavoritesMenuGrid):
several rows of twelve favorites, drawn all at once as a grid you can click.

**This is not a working mod yet.** The repository currently holds milestone 0 -
a plugin that only reads and logs, so the two questions the headers cannot
answer get answered in the running game. See [HANDOFF.md](HANDOFF.md) for the
state of the work.

## What the port aims for

- **The game's own favorites.** The grid writes into Fallout 4's twelve real
  favorite slots, so the number keys, the ammo counters and the equipped state
  keep working the way the game intends.
- **No replaced interface file.** The grid is drawn into the menu at runtime,
  as an addition to the stage. Nothing in `Data\Interface` is shipped, so it
  sits alongside DEF_UI, FallUI, HUDFramework and anything else that owns
  those files.
- **The game's own look.** Fallout 4 lets you colour the interface, and the
  grid follows that colour rather than bringing its own.

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

`deploy.py` copies the DLL and its PDB into `Data\F4SE\Plugins` of the game
found at `FALLOUT4_DATA`, or of the default Steam install.

## Licence

GPL-3.0-or-later, inherited from the Starfield mod this is a port of, which is
itself a fork of [Favorites Banks](https://www.nexusmods.com/starfield/mods/17906)
by Sator. CommonLibF4 is MIT and is not affected by that.
