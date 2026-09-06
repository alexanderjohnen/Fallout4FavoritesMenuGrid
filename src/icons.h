#pragma once

// Bringing icon libraries into our own movie.
//
// The cells should carry the same symbols the player already sees in FallUI's
// menus. Which symbol belongs to which item is `tags`; getting the artwork
// within reach is this.
//
// The obstacle was that our movie knows no such symbol: CreateObject builds a
// class the movie's own library registers, and ours is 36 bytes and registers
// none. Measured, not assumed -- it came back as nothing at all. So a library
// is loaded at runtime into the application domain this movie already lives
// in, after which its classes are registered where CreateObject looks. Every
// piece of that was asked for by name first, because Scaleform's AS3 is not
// Flash's: Loader, URLRequest, LoaderContext and ApplicationDomain are all
// there, and only the current domain is not reachable through the movie's
// variable path -- it comes off the root's own loaderInfo.
//
// There is more than one library, because every mod that brings its own
// artwork brings its own SWF. They are asked for one at a time and each
// answers for itself; a player who has none of them simply gets cells without
// icons.
namespace icons
{
	// Asks for a library, unless it is already in or already asked for. The
	// path is what a tag configuration declared -- bare beside the other
	// interface movies, or relative to Interface.
	void Want(RE::IMenu* a_canvas, const std::string& a_library);

	// Every frame while the menu is up. Calls a_changed once for each library
	// that arrives, because the panel was drawn before it did.
	void Poll(RE::IMenu* a_canvas, void (*a_changed)());

	[[nodiscard]] bool Has(const std::string& a_library);

	// The loaders belong to the movie that is going away, and so do the
	// classes they registered.
	void Release();
}
