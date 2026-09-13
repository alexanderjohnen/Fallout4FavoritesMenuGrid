# Nexus page

## Short description

Shows your favorites as a grid of up to eight pages instead of a twelve-key
cross - in the favorites menu and in the Pip-Boy's ASSIGN FAVORITE dialog.
Move favorites between keys and pages, clear them, equip and unequip them.
A Fallout 4 port of Favorites Menu Grid for Starfield.

## Full description

**FAVORITES MENU GRID**

*Every page of favorites at once, as a grid you can point at*

Fallout 4's favorites menu gives you twelve keys. Favorites Menu Grid gives
you up to eight pages of twelve, drawn together as one grid - in the favorites
menu, and in the Pip-Boy when you assign a favorite. Mouse, keyboard and
controller.

**Disclaimer:** This is a port of my Favorites Menu Grid for Starfield, which
is a fork of SrSator's *Favorites Banks - Multiple Favorite Pages*. Fallout 4
does its favorites very differently, so this is a rewrite rather than a
recompile - but the idea and the name are theirs, and you should try and
endorse the original. Please be aware that this mod is made with Claude AI.
Use at your own risk and back up your saves. This paragraph is written by me;
most of the rest is written by Claude, and I keep editing what I do not like.

**REQUIRES FALLOUT 4 1.10.163** (the original runtime, not Next-Gen). Also
needs F4SE and the Address Library for F4SE Plugins. Nothing else - no item
sorter, no UI mod - although you will want one for the icons, see below.

**THE GRID**

One row per page, one cell per key. Set the number of pages in the INI (up to
eight). The key numbers stand above the columns, the page numbers beside the
rows.

Point at a cell to mark it, or walk with W/A/S/D or the D-pad. The marked
item's name stands above the grid, and under it what it does: damage and
ammunition for a weapon, resistances for armour - the game's own numbers.

Use the marked cell with E, Return, a click, or A on the controller. A cell
on another page turns to that page first, so every cell on every page is one
move away.

Using a weapon or armour you already have equipped takes it off again - the
engine's own holstering, so power armour and the rest follow the engine's
rules. Aid is used, never toggled.

**MOVING AND CLEARING**

INSERT (X on the controller) picks up the marked favorite; the next press puts
it down on the marked cell, and the two change places - across pages too.
That is how something gets from page 3 to page 1 without the Pip-Boy.

DELETE (Y) frees the marked key. Nothing is deleted: the item stays a
favorite, just without a key, the same state the game gives a favorite you
never assigned a digit to.

**THE PIP-BOY**

Assigning a favorite in the Pip-Boy shows the same grid in place of the
twelve-key cross. Point or walk to any cell on any page and press Accept or
click; the favorite goes there. Everything else about assigning is the
game's own.

**ICONS**

If you have FallUI's Item Sorter and Icon Library (or DEF_UI), every cell
shows the item's icon in the colour your sorter configuration and colour set
ask for - the same symbol you see in FallUI's menus, read from the files you
already have. Icon addons (Diello, 4estGimp and the rest) are read as well.
Nothing of that artwork is shipped with this mod. Without a sorter the cells
show names, and everything else works the same.

In the Pip-Boy the icons are made by FallUI's own icon library, so they match
the inventory list exactly.

**HOW THE PAGES WORK**

The game has twelve real favorite keys. One page at a time occupies them;
the other pages are stored beside your save, in F4SE's co-save, so every
character keeps their own. Turning to a page moves its favorites into the
real keys through the engine's own routines - which is why the digits 1-0,
the ammo counters and the equipped state all keep working exactly as unmodded.

`DefaultPage` in the INI names a page that goes back into the real keys every
time the favorites menu or the Pip-Boy closes, so the digits in gameplay
always mean one fixed page. Off by default, which leaves whichever page you
last used.

**INSTALLATION**

Vortex or Mod Organizer 2, or copy the folders into Data by hand:

    Data\F4SE\Plugins\FavoritesMenuGrid.dll
    Data\F4SE\Plugins\FavoritesMenuGrid.ini
    Data\Interface\FavoritesMenuGrid.swf

The .swf is a 36-byte empty movie of this mod's own. It replaces no vanilla
file, so it cannot collide with DEF_UI, FallUI, HUDFramework or anything else
that ships an Interface file. The grid is drawn into it from C++.

All settings are in FavoritesMenuGrid.ini, which explains every option in
place. It is read once at startup.

**COMPATIBILITY**

Mods that replace FavoritesMenu.swf or rework the favorites menu
(FavoritesMenuEx and the like) have not been tested with this and are not
expected to get along with it - pick one. Mods that only read or write
favorites are a different matter: this mod tells the game's favorites system
about every change it makes, the way the Pip-Boy does. Visible Favorites is
confirmed to follow it; sorters and FallUI are what it was built against.

**KNOWN LIMITS**

- Fallout 4 1.10.163 only. Next-Gen (1.10.980 and later) needs its own
  address IDs and is not supported yet.
- Tested on one setup, with FallUI and FIS installed. Without them the mod is
  built to fall back to names, but that path has not been played. Keep a save
  backup.
- The small badge some sorter icons carry in one corner (the pills beside a
  bottle) is not drawn.
- A favorite you no longer carry loses its key on the next page turn; it is
  not kept faint the way the Starfield version does it.

**CREDITS AND LICENCE**

- SrSator for Favorites Banks, which the Starfield original is built on.
- The F4SE team, the CommonLibF4 contributors, and the Address Library.
- m8r98a4f2 for FallUI and its icon library, which this mod reads and, in the
  Pip-Boy, asks.
- Bethesda Game Studios for Fallout 4.

Source code is GPL-3.0-or-later, inherited from the Starfield mod. The full
source of this build is on GitHub and uploaded beside the binary, as that
licence requires.
