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
// The cross keeps working exactly as it does. The grid shows what the twelve
// keys hold on every page, and shows every page alike: the engine hands the
// keys to one page at a time, but with the pointer and the keys every cell is
// one move away, so picking one page out would only say that the others are
// further off.
namespace grid
{
	// One key of one page.
	struct Cell
	{
		std::string label;   // the key: 1..9, 0, -, =
		std::string name;    // what lies on it, empty for a free key
		std::string symbol;  // the icon's class name, empty when there is none
		std::uint32_t color{ 0x1000000 };  // above white: leave the icon as it is
	};

	using Page = std::array<Cell, 12>;

	// One cell of the panel: which page it belongs to, which of the twelve
	// keys it is. What the pointer lands on and what the keys walk between
	// are the same thing, so they are the same type.
	struct Spot
	{
		std::size_t page{ 0 };
		std::size_t slot{ 0 };

		[[nodiscard]] bool operator==(const Spot&) const = default;
	};

	// Draws or redraws the panel, in the middle of the screen, and puts the
	// cross out of sight while it is up: two ways to read the same twelve
	// keys, in two corners, is one too many.
	//
	// `a_font` is a font the menu really has -- which one is settled by
	// measuring. `a_title` goes above the rows and says what the marked cell
	// holds; it carried the word "Favorites" once, which said nothing a grid
	// of favorites does not.
	struct Placement
	{
		// Below zero means "centred on the stage".
		double x{ -1.0 };
		double y{ -1.0 };
		// Outlines the whole stage and its corners, to see which parts of it
		// reach the screen at all.
		bool probeStage{ false };

		// Hanging the panel in the menu's own root clip was tried and is a
		// dead end: the favorites menu vanished entirely and the panel
		// reported a position of -107374182. Bethesda's UI components do not
		// take kindly to foreign children -- the same lesson the cross gave
		// when a text field kept the menu from ever closing. Kept only as a
		// switch, never as the default.
		bool inMenuRoot{ false };

		// A plate behind the whole panel. Fallout 4 draws its interface
		// without one -- the cross has no backdrop either -- and a slab is
		// what makes an addition look like an addition.
		bool backdrop{ false };

		// The one number the whole layout is built from. Starfield's grid
		// uses 66 on the same 1280x720 stage both games author their menus
		// on, and comes out wider than this one -- its cells hold icons,
		// where ours still hold a cut-off name.
		double cellSize{ 48.0 };

		// How much of a cell an icon fills, with its proportions kept. The
		// game's own cell gives its icon nearly the whole square; a little
		// air keeps the plates readable as a lattice.
		double iconFit{ 0.78 };

		// Whether an icon is painted in the colour its tag asks for. FIS
		// names one per keyword -- aid red, weapons blue -- and the artwork
		// is white so that something can. Off leaves it white.
		bool iconColors{ true };

		// Which menu the panel is drawn on. The favorites menu paints only a
		// strip around its own cross, so anything of ours outside that never
		// arrives; the HUD covers the whole screen and is repainted all over.
		std::string canvas{ "HUDMenu" };
	};

	// `a_canvas` is drawn on, `a_favorites` is the menu the cross lives in;
	// they are only the same menu when the canvas is the favorites menu
	// itself.
	void Draw(
		RE::IMenu* a_canvas,
		RE::IMenu* a_favorites,
		const std::string& a_font,
		const std::string& a_title,
		const std::vector<Page>& a_pages,
		const std::optional<Spot>& a_marked,
		std::uint32_t a_color,
		const Placement& a_where);

	// Where the game's own pointer sits, in the stage units the panel is
	// laid out in. The menu carries a cursor because it asked for one, so
	// there is nothing to track: the engine already knows, and it knows it
	// in screen pixels, which is what has to be converted.
	[[nodiscard]] bool Pointer(RE::IMenu* a_canvas, double& a_x, double& a_y);

	// Which cell a point falls on. Nothing when it lands in a gap, on the
	// margin, or off the panel altogether -- a near miss is not a choice.
	[[nodiscard]] std::optional<Spot> At(double a_x, double a_y);

	// Writes above the grid what the marked cell holds. Separate from Draw
	// because the mark moves without the panel being drawn again -- that is
	// what makes following the pointer cheap.
	void Say(std::string_view a_text);

	// Marks the cell that has been picked up and is waiting to be put down
	// somewhere else. Drawn apart from the mark, because while a cell is
	// held there are two places worth looking at: where it came from and
	// where it would go.
	void Hold(const std::optional<Spot>& a_spot);

	// Picks a cell out. Cheap enough for every frame: the outline is a child
	// of its own that only ever moves, so nothing is drawn again for it.
	void Mark(const std::optional<Spot>& a_spot);

	// Forgets the panel. The display objects belong to the movie that is
	// going away, so this runs when the menu closes.
	void Release();
}
