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
// else -- the digits, the close key -- goes past untouched, to the same
// places as before.
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

	// Which kind of thing the player last touched. The line under the panel
	// has to name keys somebody actually has, and a controller player has
	// none of the ones an INI written for a keyboard names.
	enum class Device
	{
		kNone,
		kKeyboard,
		kGamepad
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
		int move{ VK_INSERT };

		// The left mouse button uses whatever the pointer marks. Its own
		// switch, because a pointer is the one part of this a player may
		// well want off.
		bool useOnClick{ true };
	};

	// The same seven things, on a controller.
	//
	// The numbers are the ones the event carries: XInput's own button mask,
	// which is what `BS_BUTTON_CODE` spells with a 0x10000 in front of it.
	// The two triggers are Bethesda's own invention, 0x9 and 0xA, and are
	// left out of the defaults -- a trigger is an axis, and a held axis in a
	// menu that uses things is a bad idea.
	//
	// What is *not* here matters as much as what is. B, Start and Back are
	// never claimed, whatever an INI says, and neither is whatever the game
	// has bound "Quickkeys" and "Cancel" to on this player's controller: one
	// of those closes this menu, and a menu a controller cannot close is
	// worse than no controller support at all. Install resolves them from
	// the game's own bindings rather than assuming.
	struct Pad
	{
		bool enabled{ true };

		int pageUp{ 0x0001 };    // D-pad up
		int pageDown{ 0x0002 };  // D-pad down
		int slotLeft{ 0x0004 };  // D-pad left
		int slotRight{ 0x0008 };  // D-pad right

		int use{ 0x1000 };    // A -- the button that means yes everywhere else
		int move{ 0x4000 };   // X
		int clear{ 0x8000 };  // Y

		// The left stick walks the mark as well. It costs the player walking
		// while the menu is up, which is the same price w/a/s/d already
		// charges on a keyboard, so it is one switch and not a special case.
		bool stick{ true };
	};

	// Joins the front of the handler array, once. Nothing is claimed until
	// Listen is on.
	void Install();

	void SetKeys(const Keys& a_keys);
	void SetPad(const Pad& a_pad);

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

	// The panel says it is still there, once a frame.
	//
	// Listen is switched off by the menu's close event, and an event that
	// never arrives leaves it switched on -- and this handler stands in
	// front of everyone else. A player would then find w, a, s, d, E,
	// Return, Insert, Delete, the D-pad, three face buttons **and the left
	// stick** all swallowed, everywhere, for the rest of the session. That
	// is not a bug you shrug at; that is a game you have to restart.
	//
	// So a switch is not enough. Nothing is claimed unless the grid's own
	// menu drew a frame in the last half-second, which no missed event and
	// no lost window focus can fake.
	void Alive();

	// What the player last pressed something on. Answered from the input
	// thread and read from the UI one, so it is an atomic and nothing more:
	// whoever asks gets the last device, not a promise about the next.
	[[nodiscard]] Device LastDevice();

	// What the panel should call a controller button. Empty for a button
	// that has no name worth printing.
	[[nodiscard]] std::string PadName(int a_button);

	// The game's own symbol for a controller button -- one character of the
	// font Fallout 4 keeps its button art in. Empty when that font has none
	// for this button, and then PadName is what is left.
	//
	// `a_orbis` picks the PlayStation set over the Xbox one. Both live in the
	// same font: A-Z is one controller, a-z the other.
	[[nodiscard]] std::string PadGlyph(int a_button, bool a_orbis);

	// The whole D-pad as one symbol, and the left stick as another -- what
	// the game writes when a hint means "any direction" rather than one.
	[[nodiscard]] std::string PadGlyphDPad(bool a_orbis);
	[[nodiscard]] std::string PadGlyphStick(bool a_orbis);

	// Which controller button leaves this menu, as the player has it bound.
	// Install already had to find it in order to keep its hands off it, so
	// the line under the panel can name it instead of guessing at a B.
	// Zero when the bindings could not be read.
	[[nodiscard]] int PadCloseButton();

	// Whether the mouse pointer still has a say.
	//
	// Two ways of choosing a cell in one menu get in each other's way: the
	// keys move the mark, and a pointer resting over some other cell takes
	// it back on the next frame. So the pointer goes to sleep the moment a
	// key or a button is used, and wakes when the thing that owns it moves
	// -- the mouse, or the right stick, which is the one the game leaves for
	// the cursor.
	[[nodiscard]] bool PointerAwake();
}
