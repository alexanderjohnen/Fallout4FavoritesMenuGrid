#pragma once

// Copying the engine's own machine code out of the running game.
//
// Fallout4.exe is packed on disk -- the .bind section is Steam's wrapper --
// so a disassembler reading the file finds noise where the code should be.
// Only the running process has it in the clear, and a plugin is already in
// there. So the bytes are copied out here and read outside:
//
//     PeekIDs=534268:0x120            in FavoritesMenuGrid.ini
//     PeekRVAs=0x1a7210:0x200
//     PeekVtableRefs=1064496
//     py -3 tools/f4dis.py peek "...\FavoritesMenuGrid.peek.txt"
//
// The settings are read every time, not once at startup, so a new question
// costs an edit and a key press rather than a restart of the game.
//
// Reading only. Nothing in here writes to the game.
namespace peek
{
	// PeekIDs and PeekRVAs name a place, either by Address Library ID or as
	// an offset from the start of the module, and optionally how much to
	// copy: `534268:0x120`. Without a length, the function around the
	// address is written out. PeekVtableRefs takes IDs of vtables and finds
	// every place in the code that points at one -- that is where the engine
	// builds such an object for itself.
	void Run(const std::filesystem::path& a_settings);
}
