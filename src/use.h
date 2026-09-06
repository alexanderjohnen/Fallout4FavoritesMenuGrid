#pragma once

// Using what a cell holds.
//
// The cross has one way to use a favorite -- the engine's own
// FavoritesManager::UseQuickkeyItem, which takes a key index and does the
// rest: a stimpak is drunk, a weapon is drawn, a grenade is thrown. Every
// difference between kinds of item lives in there, so calling it is the only
// answer that stays right.
//
// It has no Address Library ID we know of, and the signature FavoritesMenuEx
// carries for it -- `E8 ? ? ? ? 83 F8 0C 74 04`, a call whose answer is
// compared with twelve -- was tried and led somewhere else: to a classifier
// that walks a form's vtable against a dozen known ones and answers with a
// number, twelve meaning "none of them". Handed a key index instead of a
// pointer, it read from address 1 and took the game with it.
//
// So the search was given up in favour of reading. FavoritesManager brings
// the input handler that uses a favorite when a digit is pressed, and its
// address is not a guess: it is the eighth slot of the vtable the living
// object carries at offset 0x10. That function is written out whole, next to
// the log, and the call inside it is read there rather than here.
//
// Until an address is named in the INI, nothing is called at all. Fallout4
// .exe is packed on disk, so all of this can only happen in the running game.
namespace use
{
	// Writes out what the manager brings to the input, and takes the address
	// of UseQuickkeyItem from the INI if one is named there.
	void Find(const std::filesystem::path& a_settings);

	[[nodiscard]] bool Ready();

	// The key index, 0 to 11. Runs on the UI thread, like everything that
	// touches the inventory. False is the game's own no.
	bool Quickkey(std::uint32_t a_index);
}
