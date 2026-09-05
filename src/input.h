#pragma once

// Taking the favorites input away from the cross.
//
// While the grid is up, the keys should mean what the grid needs: w and s
// between pages, a and d between the keys of a page -- and, the point of the
// whole thing, a pointer that goes straight to a cell. The cross has its own
// idea of all of that, and it gets there first: FavoritesManager is an input
// event user, and the game hands it every event while the menu is open.
//
// So its own handlers are redirected through here. Nothing is disabled
// wholesale -- the digits, the close key and the gamepad keep working
// through the game's own path -- but what the grid claims never reaches the
// cross.
//
// Two things fall out of the same hook. FavoritesManager ignores mouse
// movement (its handler for it is the empty default), which means the events
// arrive and go nowhere: exactly what a pointer of our own needs.
//
// FavoritesMenuEx pointed the way -- its strings name
// `FavoritesManager__inputEventUser__ShouldHandleEvent` -- and the vtable was
// measured rather than trusted: CommonLibF4 lists four vtables for
// FavoritesManager, and the one carrying the input handlers is the second.
// Of its nine entries only two are the manager's own; the rest are the empty
// defaults, sitting together in memory, which is how the right one was
// recognised.
namespace input
{
	struct Hooks
	{
		// Return true to swallow the event, so the cross never sees it.
		bool (*button)(const RE::ButtonEvent&){ nullptr };

		// Mouse movement, in whatever units the device reports.
		void (*mouse)(std::int32_t a_deltaX, std::int32_t a_deltaY){ nullptr };
	};

	// Runs on the game's input thread: decide here, and leave the doing to a
	// UI task. Nothing here may touch Scaleform or the inventory.
	void Install(const Hooks& a_hooks);
}
