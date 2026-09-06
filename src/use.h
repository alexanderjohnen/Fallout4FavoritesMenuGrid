#pragma once

// Using what a cell holds.
//
// The cross has one way to use a favorite -- the engine's own
// FavoritesManager::UseQuickkeyItem, which takes a key index and does the
// rest: a stimpak is drunk, a weapon is drawn, a grenade is thrown. Every
// difference between kinds of item lives in there, so calling it is the only
// answer that stays right.
//
// It has no Address Library ID we know of. FavoritesMenuEx finds it by the
// bytes around a call to it, and names it in its own strings:
//
//     FavoritesManager_useQuickkey (FavoritesManager::UseQuickkeyItem)
//     E8 ? ? ? ? 83 F8 0C 74 04
//
// That is a call whose answer is compared with twelve -- the number of keys
// -- which is what makes the place unmistakable among the millions of bytes
// of code. Fallout4.exe is packed on disk, so this can only be done in the
// running game, and the bytes that were found are written out before the
// first call is ever made: see peek::Note. A crash on the way then still
// leaves behind what was called.
namespace use
{
	// Looks for the function, once, and writes down what it found.
	void Find();

	[[nodiscard]] bool Ready();

	// The key index, 0 to 11. Runs on the UI thread, like everything that
	// touches the inventory.
	void Quickkey(std::uint32_t a_index);
}
