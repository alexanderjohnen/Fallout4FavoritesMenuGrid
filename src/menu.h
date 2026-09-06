#pragma once

// A menu of our own.
//
// Everything the grid ran into on borrowed canvas comes back to one cause:
// it was a guest. In the favorites menu the drawing was clipped to the strip
// the cross repaints, and a child in its root clip made the menu vanish
// outright. On the HUD it draws fine, but the HUD is not a menu -- it takes
// no input, so a pointer there would mean wrestling the camera away from the
// player controls, at a place that has nothing to do with favorites.
//
// A menu is the thing the game already has for all of that. It gets the
// input, it can carry a cursor, and it owns its own stage. FavoritesMenuEx
// does exactly this, and it is why its wheel sits in the middle of the
// screen with a pointer in it while needing nothing but F4SE.
//
// What we bring is an empty movie -- tools/build_swf.py writes it, 36 bytes,
// no timeline and no ActionScript. The grid is still drawn from C++, exactly
// as it is now; only the canvas changes owner. And because the movie belongs
// to no vanilla menu, no other interface mod can collide with it.
namespace menu
{
	inline constexpr auto kName = "FavoritesMenuGrid";

	// Tells the game the menu exists. Has to happen before it is opened, and
	// it only takes.
	void Register();

	// Called once the menu is up and its movie is loaded, on the UI thread.
	// The menu is not in the game's menu list yet while it is being built,
	// so drawing has to wait for this rather than for Show() returning.
	void SetOnReady(void (*a_ready)());

	// Called on every frame the menu is drawn, on the UI thread. That is
	// where the pointer is read: the game moves its own cursor and nobody
	// has to be told when, so asking once a frame is both the simplest way
	// and the one that cannot fall behind.
	void SetOnAdvance(void (*a_advance)());

	// Opens and closes it. Both are messages to the UI queue, so they take
	// effect on the game's own terms rather than immediately.
	void Show();
	void Hide();

	[[nodiscard]] bool IsOpen();
}
