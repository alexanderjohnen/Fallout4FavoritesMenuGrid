#pragma once

// Copying the engine's own machine code out of the running game.
//
// Fallout4.exe is packed on disk -- the .bind section is Steam's wrapper --
// so a disassembler reading the file finds noise where the code should be.
// Only the running process has it in the clear, and a plugin is already in
// there. So the bytes are copied out here and read outside:
//
//     PeekIDs=779526,1291190          in FavoritesMenuGrid.ini
//     PeekVtableRefs=1064496
//     py -3 tools/f4dis.py peek "...\FavoritesMenuGrid.peek.txt"
//
// Reading only. Nothing in here writes to the game.
namespace peek
{
	// Both arguments are comma-separated lists of Address Library IDs, as
	// they come out of the INI. IDs are dumped as whole functions; vtable
	// IDs are looked for in the code section, and every function that
	// mentions one is dumped -- that is how the engine's own users of a
	// functor are found.
	void Run(const std::wstring& a_ids, const std::wstring& a_vtableRefs);
}
