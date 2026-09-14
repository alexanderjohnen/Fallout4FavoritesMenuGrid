#pragma once

// A measurement, not a feature: says in the log what the Pip-Boy menu
// itself is handed while our grid stands in its assign dialog. Nothing
// is changed on the way through.
//
// Why: the grid's own handler (input.cpp) turns the D-pad into the same
// steps as W/S/A/D, and still the mark jumped at the pad and not at the
// keys (HANDOFF 65). The one path that differs is the menu's own:
// IMenu::HandleEvent turns button events into Scaleform key events for
// whatever holds the focus. Whether the pad's presses, the stick, and the
// A button go through there -- and in what shape -- is what this writes
// down, one line per press.
namespace probe
{
	// Hooks the Pip-Boy menu's input slots. `a_active` says whether to
	// speak; it is asked on every event and should be cheap.
	void WatchPipboyMenu(bool (*a_active)());
}
