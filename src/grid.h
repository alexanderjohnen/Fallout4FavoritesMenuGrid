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

	// Draws or redraws the panel. `a_font` is a font the menu really has --
	// which one is settled by measuring, next to the page marker.
	void Draw(
		RE::IMenu* a_menu,
		const std::string& a_font,
		const std::vector<Page>& a_pages,
		std::size_t a_current,
		std::uint32_t a_color);

	// Forgets the panel. The display objects belong to the movie that is
	// going away, so this runs when the menu closes.
	void Release();
}
