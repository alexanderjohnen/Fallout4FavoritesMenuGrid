#pragma once

// Bringing an icon library into our own movie.
//
// The cells should carry the same symbols the player already sees in FallUI's
// menus. The chain is known and written up in section 20 of the handoff: an
// item's name carries a tag, the sorter's XML maps that tag to a symbol name,
// and the symbol lives in a library movie under Data\Interface. Nothing of it
// is ours and nothing of it is shipped -- it is read where the player already
// has it, which is exactly what FallUI's own auto-detect does.
//
// The one thing in the way was that our movie knows no such symbol:
// CreateObject builds a class the movie's own library registers, and ours is
// 36 bytes and registers none. Measured, not assumed -- it came back as
// nothing at all.
//
// So the library is loaded at runtime, into the application domain this movie
// already lives in. Then its classes are registered where CreateObject looks,
// and a symbol can be made by name like any other. Scaleform's AS3 is not
// Flash's, so every piece of that was asked for by name first: Loader,
// URLRequest, LoaderContext and ApplicationDomain are all there. Only the
// current domain could not be read through the movie's variable path -- it is
// not reached that way but through the root's own loaderInfo.
namespace icons
{
	// Starts the load. Returns false when a piece is missing, and says in the
	// log which one.
	bool Begin(RE::IMenu* a_canvas, const std::string& a_library);

	// Every frame while the menu is up. Calls a_ready once, when the symbols
	// can be made.
	void Poll(RE::IMenu* a_canvas, void (*a_ready)());

	[[nodiscard]] bool Ready();

	// The loader belongs to the movie that is going away.
	void Release();
}
