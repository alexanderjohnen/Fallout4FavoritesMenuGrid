#pragma once

// The grid: every page at once, drawn beside the cross.
//
// Built out of display objects made from C++, not by replacing
// FavoritesMenu.swf. The vanilla movie lives in a BA2 and any interface mod
// that ships its own copy would collide with ours; everything here is
// additive instead -- a sprite on the stage that the menu neither knows nor
// cares about. The Starfield version of this mod arrived at the same answer
// for the same reason.
//
// The cross keeps working exactly as it does. The grid only shows what the
// twelve keys hold on every page, so a player can see where a page switch
// is going before making it.
namespace grid
{
	// One key of one page.
	struct Cell
	{
		std::string label;  // the key: 1..9, 0, -, =
		std::string name;   // what lies on it, empty for a free key
	};

	using Page = std::array<Cell, 12>;

	// Draws or redraws the panel, in the middle of the screen, and puts the
	// cross out of sight while it is up: two ways to read the same twelve
	// keys, in two corners, is one too many.
	//
	// `a_font` is a font the menu really has -- which one is settled by
	// measuring. `a_title` goes above the rows, where the page marker would
	// otherwise be.
	struct Placement
	{
		// Below zero means "centred on the stage".
		double x{ -1.0 };
		double y{ -1.0 };
		// Outlines the whole stage and its corners, to see which parts of it
		// reach the screen at all.
		bool probeStage{ false };

		// Whether the panel hangs in the menu's own root clip rather than
		// beside it on the stage. The root clip is what the game draws; a
		// sibling of it only seems to come through where the menu repaints
		// anyway, which is what cut the panel in half.
		bool inMenuRoot{ true };
	};

	void Draw(
		RE::IMenu* a_menu,
		const std::string& a_font,
		const std::string& a_title,
		const std::vector<Page>& a_pages,
		std::size_t a_current,
		std::uint32_t a_color,
		const Placement& a_where);

	// Forgets the panel. The display objects belong to the movie that is
	// going away, so this runs when the menu closes.
	void Release();
}
