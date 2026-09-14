#pragma once

// Stands between the Pip-Boy menu and its own input while our grid is in
// the assign dialog.
//
// Why: the grid's handler (input.cpp) already turns the D-pad into the
// same steps as W/S/A/D. But the menu has a second path of its own:
// IMenu::HandleEvent turns a pad press into a Scaleform arrow for whatever
// holds the focus -- the cross, since it has to hold the focus for E --
// and the cross walks that arrow on release, in its own shape. Measured
// 2026-09-14: every D-pad press was two steps, ours on press and the
// cross's on release; the left stick reached the menu the same way. W/S
// never produce those arrows, which is why the keyboard was always smooth.
//
// So while our grid stands there, the menu is not handed the D-pad's
// directions or the left stick. Everything else -- A, which the menu takes
// as Accept and assigns with, and B, which closes -- goes through untouched.
namespace probe
{
	// Hooks the Pip-Boy menu's input slots. `a_active` says whether our
	// grid stands in the dialog; it is asked on every event.
	void WatchPipboyMenu(bool (*a_active)());
}

namespace probe
{
	// Says in the log whether the hooks are still in the vtable, and who
	// is there instead when they are not. Asked when the grid goes up in
	// the dialog: on 2026-09-14 the hooks were installed and then never
	// called, so somebody writes those slots after us.
	void CheckPipboyMenu();
}
