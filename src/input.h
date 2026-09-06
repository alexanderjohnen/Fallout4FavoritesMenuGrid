#pragma once

// The keys of the grid, taken before anyone else sees them.
//
// The first attempt hooked FavoritesManager's own input handlers. The vtable
// was right -- it sits at object offset 0x10, which the memory dump in the
// log shows -- and not one event ever arrived: the manager is a single user,
// and the game only feeds it while its own menu owns the input. With a menu
// of our own that question is settled differently.
//
// MenuControls keeps an array of input event users and walks it in order,
// and the first one that owns an event ends the walk. So the grid stands at
// the front of that array and claims exactly the keys it needs. Everything
// else -- the digits, the close key, the gamepad -- goes past untouched, to
// the same places as before.
//
// FavoritesMenuEx does the same thing: its strings name a class of its own,
// `FavoritesMenuExInput`, with a line for registering and one for
// unregistering. A menu does not get the keys by being a menu.
namespace input
{
	// What the grid does with the keys it claims. Named rather than spelled
	// as key codes, because the player decides which key means which.
	enum class Action
	{
		kPageUp,
		kPageDown,
		kSlotLeft,
		kSlotRight,
		kUse,
		kClear,
		kMove
	};

	// Virtual key codes, the way the INI spells them.
	struct Keys
	{
		int pageUp{ 'W' };
		int pageDown{ 'S' };
		int slotLeft{ 'A' };
		int slotRight{ 'D' };
		// Two of them. E is what a player's hand reaches for, because it is
		// what activates everything else in this game; Return is what a menu
		// answers to. Neither is worth taking away from the other.
		int use{ 'E' };
		int useAlt{ VK_RETURN };

		// Frees the key the mark sits on.
		int clear{ VK_DELETE };

		// Picks the marked cell up, or puts the held one down on it.
		int move{ 'G' };

		// The left mouse button uses whatever the pointer marks. Its own
		// switch, because a pointer is the one part of this a player may
		// well want off.
		bool useOnClick{ true };
	};

	// Joins the front of the handler array, once. Nothing is claimed until
	// Listen is on.
	void Install();

	void SetKeys(const Keys& a_keys);

	// How a held key walks on. The first step is the press; after a_delay it
	// keeps going, one step every a_interval, both in seconds.
	//
	// Only the four that move the mark repeat. Using, clearing and picking up
	// are single acts, and a held key that used a stimpak eleven times a
	// second would be a bug with a body count.
	void SetRepeat(double a_delay, double a_interval);

	// Runs on the game's input thread: decide here and leave the doing to a
	// UI task. Nothing behind this may touch Scaleform or the inventory.
	void SetOnAction(void (*a_action)(Action));

	// On while the grid is up, off the rest of the time -- outside the
	// favorites menu, w and s are walking again.
	void Listen(bool a_on);
}
