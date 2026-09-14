#pragma once

// Stands between the game's input and the two things that would turn a
// pad press into a Scaleform arrow while our grid is in the Pip-Boy's
// assign dialog.
//
// Why: the grid's handler (input.cpp) already turns the D-pad into the
// same steps as W/S/A/D. But the game has paths of its own that make a
// Keyboard.RIGHT out of the same press for whatever holds the focus --
// the cross, since it has to hold the focus for E -- and the cross walks
// that arrow on release, in its own shape. Measured 2026-09-14: every
// D-pad press was two steps, ours on press and the cross's on release;
// the left stick reached the cross the same way. W/S never become
// arrows, which is why the keyboard was always smooth.
//
// Two doors. The menu's own input slots (IMenu::HandleEvent), and the
// GFxConvertHandler in MenuControls' list, which converts input events
// into Scaleform ones for the movies -- our handler sits first in that
// list and claims the press, and the list runs on regardless. Keeping
// the menu alone was measured to change nothing (12:21); the convert
// handler is the second door.
//
// While our grid stands there, neither is handed the D-pad's directions
// or the left stick. Everything else -- A, which the menu takes as Accept
// and assigns with, and B, which closes -- goes through untouched.
namespace probe
{
	// Hooks both. `a_active` says whether our grid stands in the dialog;
	// it is asked on every event.
	void WatchPipboyMenu(bool (*a_active)());

	// Says in the log whether the hooks are still in the vtables, and who
	// is there instead when they are not. Asked when the grid goes up.
	void CheckPipboyMenu();
}
