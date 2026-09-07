#pragma once

// What to say about the thing under the mark.
//
// The Starfield version shows this beside its grid and it is the difference
// between a wall of icons and something worth reading: hovering a weapon says
// what it fires, how much of that is left and what it hits for; hovering a
// piece of armour says what it resists.
//
// All of it is read out of the item, not out of a menu -- the favorites menu
// carries no such card, and a page that is not the one being played has no
// menu entry at all. Where a stack carries instance data -- a weapon with
// mods on it is a different weapon from the one in the plugin -- that is
// preferred over the base form, because it is what the player is holding.
namespace detail
{
	struct Lines
	{
		std::string name;  // what it is called, without the sorter's tag
		std::string what;  // what it does, in one line, or empty
	};

	// Runs on the UI thread: it walks the player's inventory.
	[[nodiscard]] Lines Describe(RE::TESBoundObject* a_object, bool a_stripTags);

	// What the game itself calls this thing.
	//
	// **Not** TESFullName on the base object. That is the name in the
	// plugin, and for a weapon it is barely a name at all: the plugin says
	// "T60", "Laser", "Hunting Shotgun", and the game shows "T60 Pistol",
	// "Righteous Authority", "Rapid Advanced Hunting Shotgun". The
	// difference is the instance -- what is bolted to it and what the naming
	// rules make of that -- and it is the difference between a pistol icon
	// and a rifle icon, because the sorter's auto-tagging reads the name.
	//
	// It is also why an icon never changed when a weapon was rebuilt at a
	// workbench: the base name cannot change, so nothing we read from it
	// could.
	//
	// The engine will do all of that on request -- BGSInventoryItem::
	// GetDisplayFullName, given which stack is meant. Carrying two of the
	// same base form with different mods on them, the first stack with
	// instance data wins; that is the same choice Describe already makes for
	// the second line, and the two agreeing matters more than either being
	// clever.
	[[nodiscard]] std::string DisplayName(RE::TESBoundObject* a_object);
}
