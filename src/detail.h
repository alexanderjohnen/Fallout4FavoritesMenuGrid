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
}
