#include "PCH.h"

#include "grid.h"

namespace
{
	// Everything is derived from the cell, so one number in the INI moves
	// the whole panel without any of it going out of proportion. Text is
	// kept above a floor: a grid too small to read is worse than one that
	// takes up room.
	struct Metrics
	{
		double cell{ 48.0 };
		double gap{ 3.0 };
		double rowLabelWidth{ 30.0 };
		double padding{ 10.0 };
		double nameHeight{ 11.0 };
		double nameSize{ 9.0 };
		double keySize{ 9.0 };
		double rowLabelSize{ 11.0 };
		double titleSize{ 13.0 };
		double detailSize{ 11.0 };
		double titleHeight{ 18.0 };
		double keyRowHeight{ 22.0 };
		double hintHeight{ 0.0 };
		std::size_t nameRoom{ 10 };
	};

	// The key number, at the size the game gives it: an edit text of
	// AIN_Font_Bold, height 36 of the 100 units its cell is wide, white and
	// fully opaque. Kept as a fraction of the cell, so one number in the INI
	// still moves the whole panel.
	//
	// It stands once, above the first row, and not in every cell. Fallout 4
	// repeats it in each cell because it has one row; with three, the same
	// twelve numbers three times over are a pattern the eye has to read past
	// to see what is actually there. A column is labelled at its head.
	constexpr double kKeyTextSize = 0.36;

	// The second line is the same colour, quieter. The game draws its own
	// ammunition line in a smaller face rather than a dimmer one, but the
	// game has a backdrop behind it and we have the wasteland.
	constexpr double kDetailAlpha = 0.75;
	// The key line is a reminder rather than a statement, and sits quieter
	// still.
	constexpr double kHintAlpha = 0.6;

	// The marked cell: the same plate with more light in it.
	constexpr double kMarkFillAlpha = 0.18;
	constexpr double kMarkLineAlpha = 0.9;
	// The cell being carried: more light again, and no outline, so that the
	// mark stays the thing with an edge.
	constexpr double kHoldFillAlpha = 0.40;
	constexpr double kKeyAlpha = 1.0;

	[[nodiscard]] Metrics MetricsFor(double a_cell)
	{
		const auto floorAt = [](double a_value, double a_least) {
			return a_value < a_least ? a_least : a_value;
		};

		Metrics m;
		m.cell = a_cell;
		m.gap = floorAt(a_cell * 0.06, 2.0);
		m.rowLabelWidth = a_cell * 0.55;
		m.padding = a_cell * 0.2;
		m.nameSize = floorAt(a_cell * 0.19, 8.0);
		m.nameHeight = m.nameSize + 2.0;
		m.keySize = floorAt(a_cell * kKeyTextSize, 8.0);
		m.rowLabelSize = floorAt(a_cell * 0.23, 9.0);
		m.titleSize = floorAt(a_cell * 0.27, 10.0);
		m.titleHeight = m.titleSize + 5.0;
		m.keyRowHeight = m.keySize + m.gap + 2.0;
		m.hintHeight = 0.0;
		// Roughly what fits at this size in a condensed face.
		m.nameRoom = static_cast<std::size_t>(
			std::max(4.0, a_cell / (m.nameSize * 0.48)));
		return m;
	}

	constexpr std::size_t kSlots = 12;

	// The cell, measured out of FavoritesMenu.swf rather than matched by eye.
	// Its one DefineShape4 is a square of 100 by 100 units filled with
	// ff ff ff 33 -- white at a fifth -- and the only line style in it is one
	// unit of 00 ff 00 at alpha 2 of 255. That is not a border, it is a
	// leftover: Fallout 4 draws its cells as thin sheets of light with no
	// outline at all, and they work by brightening what is behind them.
	//
	// The white is not the colour the player sees. Nothing in the menu's own
	// ActionScript sets a colour anywhere -- the engine does it, by wrapping
	// a display object in a BSGFxShaderFXTarget and calling SetToHUDColor,
	// which hangs filters on it and renews them on every ApplyColorUpdateEvent.
	// White is simply what a tint is applied to. Our menu is our own and is
	// wired to none of that, so what arrives white in the vanilla movie is
	// drawn here in the player's own HUD colour: same intention, one step
	// earlier.
	//
	// Every page is drawn the same. The engine hands the twelve keys to one
	// page at a time, but that is its business rather than the player's: with
	// the pointer and the keys, every cell on the panel is one move away, and
	// picking one page out would say that the others are further off.
	constexpr double kCellAlpha = 0.20;
	// Only drawn when a backdrop is asked for.
	constexpr double kPanelAlpha = 0.72;

	[[nodiscard]] double PanelWidth(const Metrics& a_m)
	{
		return a_m.padding * 2.0 + a_m.rowLabelWidth +
			(a_m.cell + a_m.gap) * static_cast<double>(kSlots) - a_m.gap;
	}

	// One cell, one gap -- the same gap that separates the cells in a row.
	// The pages are meant to read as one lattice rather than as three
	// stacked strips, so the spacing has to be the same in both directions.
	[[nodiscard]] double RowHeight(const Metrics& a_m)
	{
		return a_m.cell + a_m.gap;
	}

	// Where one cell begins inside the panel. The drawing and the hit test
	// have to agree to the unit, so they read it from the same two lines.
	[[nodiscard]] double CellLeft(const Metrics& a_m, std::size_t a_slot)
	{
		return a_m.padding + a_m.rowLabelWidth +
			(a_m.cell + a_m.gap) * static_cast<double>(a_slot);
	}

	// The head of the columns, where the twelve key names stand.
	[[nodiscard]] double KeyRowTop(const Metrics& a_m)
	{
		return a_m.padding + a_m.titleHeight;
	}

	[[nodiscard]] double RowTop(const Metrics& a_m, std::size_t a_row)
	{
		return KeyRowTop(a_m) + a_m.keyRowHeight +
			RowHeight(a_m) * static_cast<double>(a_row);
	}

	RE::Scaleform::GFx::Value g_panel;
	// The outline around the chosen cell. A child of its own, so choosing
	// costs a move rather than forty new text fields.
	RE::Scaleform::GFx::Value g_marker;
	// And around the cell that has been picked up.
	RE::Scaleform::GFx::Value g_holder;
	// What the marked cell holds, above the grid: its name, and under it in a
	// quieter size what it does. Both are kept, because the mark moves
	// without the panel being drawn again -- the whole point of a mark that
	// only moves.
	RE::Scaleform::GFx::Value g_note;
	RE::Scaleform::GFx::Value g_detail;
	// Kept so the cross can be put back the way it was found.
	RE::Scaleform::GFx::Value g_hiddenCross;

	// The layout of the panel as it was last drawn, so a point on the stage
	// can be turned back into a cell without asking the movie anything.
	Metrics g_metrics;
	double g_left{ 0.0 };
	double g_top{ 0.0 };
	std::size_t g_rows{ 0 };
	// The pointer is reported once per panel: what the cursor says, what the
	// movie says, and what the two make together. A mark that never appears
	// looks the same whether the cursor stands still, counts in units nobody
	// expected, or lands somewhere off the panel -- and these three numbers
	// tell them apart in one opening of the menu.
	bool g_pointerReported{ false };

	// Scaleform hands numbers over as Int as often as as Number, so both
	// have to be accepted or every read comes back as the fallback.
	[[nodiscard]] double ReadNumber(
		const RE::Scaleform::GFx::Value& a_object,
		const char* a_member,
		double a_fallback)
	{
		RE::Scaleform::GFx::Value value;
		if (!a_object.IsObject() || !a_object.GetMember(a_member, &value)) {
			return a_fallback;
		}
		if (value.IsNumber()) {
			return value.GetNumber();
		}
		if (value.IsInt()) {
			return static_cast<double>(value.GetInt());
		}
		if (value.IsUInt()) {
			return static_cast<double>(value.GetUInt());
		}
		return a_fallback;
	}

	// ---- Drawing ---------------------------------------------------------

	void Fill(
		RE::Scaleform::GFx::Value& a_graphics,
		double a_x,
		double a_y,
		double a_width,
		double a_height,
		std::uint32_t a_color,
		double a_alpha)
	{
		const std::array fill{
			RE::Scaleform::GFx::Value(static_cast<double>(a_color)),
			RE::Scaleform::GFx::Value(a_alpha)
		};
		a_graphics.Invoke("beginFill", nullptr, fill.data(), fill.size());
		const std::array rect{
			RE::Scaleform::GFx::Value(a_x),
			RE::Scaleform::GFx::Value(a_y),
			RE::Scaleform::GFx::Value(a_width),
			RE::Scaleform::GFx::Value(a_height)
		};
		a_graphics.Invoke("drawRect", nullptr, rect.data(), rect.size());
		a_graphics.Invoke("endFill");
	}

	void Outline(
		RE::Scaleform::GFx::Value& a_graphics,
		double a_x,
		double a_y,
		double a_width,
		double a_height,
		std::uint32_t a_color,
		double a_alpha,
		double a_thickness = 1.0)
	{
		const std::array stroke{
			RE::Scaleform::GFx::Value(a_thickness),
			RE::Scaleform::GFx::Value(static_cast<double>(a_color)),
			RE::Scaleform::GFx::Value(a_alpha)
		};
		a_graphics.Invoke("lineStyle", nullptr, stroke.data(), stroke.size());
		const std::array rect{
			RE::Scaleform::GFx::Value(a_x),
			RE::Scaleform::GFx::Value(a_y),
			RE::Scaleform::GFx::Value(a_width),
			RE::Scaleform::GFx::Value(a_height)
		};
		a_graphics.Invoke("drawRect", nullptr, rect.data(), rect.size());
		const std::array clear{ RE::Scaleform::GFx::Value(0.0) };
		a_graphics.Invoke("lineStyle", nullptr, clear.data(), clear.size());
	}

	// ---- Icons -----------------------------------------------------------

	// Paints white artwork in one flat colour: every multiplier at zero and
	// the colour as an offset. Transparency is left alone -- its multiplier
	// stays at one -- so only what was drawn takes the colour.
	void Paint(
		RE::IMenu* a_canvas,
		RE::Scaleform::GFx::Value& a_icon,
		std::uint32_t a_color)
	{
		const std::array parts{
			RE::Scaleform::GFx::Value(0.0),
			RE::Scaleform::GFx::Value(0.0),
			RE::Scaleform::GFx::Value(0.0),
			RE::Scaleform::GFx::Value(1.0),
			RE::Scaleform::GFx::Value(static_cast<double>((a_color >> 16) & 0xFF)),
			RE::Scaleform::GFx::Value(static_cast<double>((a_color >> 8) & 0xFF)),
			RE::Scaleform::GFx::Value(static_cast<double>(a_color & 0xFF)),
			RE::Scaleform::GFx::Value(0.0)
		};
		RE::Scaleform::GFx::Value paint;
		a_canvas->uiMovie->CreateObject(
			&paint,
			"flash.geom.ColorTransform",
			parts.data(),
			static_cast<std::uint32_t>(parts.size()));
		if (!paint.IsObject()) {
			return;
		}

		// A transform is read, changed and written back. Changing the one the
		// object hands out does nothing at all -- it is a copy, and this is
		// the one place in Flash where that matters.
		RE::Scaleform::GFx::Value transform;
		if (a_icon.GetMember("transform", &transform) && transform.IsObject()) {
			transform.SetMember("colorTransform", paint);
			a_icon.SetMember("transform", transform);
		}
	}

	// One cell's icon, if its tag named one and its library is in. The
	// symbol is scaled to fit rather than stretched: an icon squeezed into a
	// square is worse than none, because it still looks deliberate.
	void Symbol(
		RE::IMenu* a_canvas,
		const grid::Cell& a_cell,
		double a_left,
		double a_top,
		const Metrics& a_m,
		const grid::Placement& a_where)
	{
		if (a_cell.symbol.empty()) {
			return;
		}

		RE::Scaleform::GFx::Value icon;
		a_canvas->uiMovie->CreateObject(&icon, a_cell.symbol.c_str());
		if (!icon.IsDisplayObject()) {
			return;
		}

		const auto width = ReadNumber(icon, "width", 0.0);
		const auto height = ReadNumber(icon, "height", 0.0);
		if (width <= 0.0 || height <= 0.0) {
			return;
		}

		const auto room = a_m.cell * a_where.iconFit;
		const auto scale = std::min(room / width, room / height);
		icon.SetMember("scaleX", RE::Scaleform::GFx::Value(scale));
		icon.SetMember("scaleY", RE::Scaleform::GFx::Value(scale));
		icon.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
		icon.SetMember(
			"x", RE::Scaleform::GFx::Value(a_left + (a_m.cell - width * scale) / 2.0));
		icon.SetMember(
			"y", RE::Scaleform::GFx::Value(a_top + (a_m.cell - height * scale) / 2.0));

		if (a_where.iconColors && a_cell.color <= 0xFFFFFF) {
			Paint(a_canvas, icon, a_cell.color);
		}

		g_panel.Invoke("addChild", nullptr, &icon, 1);
	}

	// ---- Text ------------------------------------------------------------

	// A name has to fit its cell. Cutting it with an ellipsis says that
	// something was cut, which a hard truncation does not.
	[[nodiscard]] std::string Shorten(const std::string& a_text, std::size_t a_room)
	{
		if (a_text.size() <= a_room) {
			return a_text;
		}
		if (a_room <= 1) {
			return {};
		}
		return a_text.substr(0, a_room - 1) + "...";
	}

	// Adds a label to the panel. The font is handed in already measured --
	// naming one the movie does not have draws a row of boxes, which this
	// project has now learned twice.
	RE::Scaleform::GFx::Value Label(
		RE::IMenu* a_canvas,
		const std::string& a_font,
		std::string_view a_text,
		double a_x,
		double a_y,
		double a_width,
		double a_size,
		std::uint32_t a_color,
		double a_alpha,
		const char* a_align = "center")
	{
		RE::Scaleform::GFx::Value field;
		a_canvas->uiMovie->CreateObject(&field, "flash.text.TextField");
		if (!field.IsDisplayObject()) {
			return field;
		}

		field.SetMember("selectable", RE::Scaleform::GFx::Value(false));
		field.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
		field.SetMember("multiline", RE::Scaleform::GFx::Value(false));
		field.SetMember("wordWrap", RE::Scaleform::GFx::Value(false));
		field.SetMember("width", RE::Scaleform::GFx::Value(a_width));
		field.SetMember("height", RE::Scaleform::GFx::Value(a_size + 6.0));
		field.SetMember("x", RE::Scaleform::GFx::Value(a_x));
		field.SetMember("y", RE::Scaleform::GFx::Value(a_y));
		field.SetMember("alpha", RE::Scaleform::GFx::Value(a_alpha));
		field.SetMember("embedFonts", RE::Scaleform::GFx::Value(!a_font.empty()));

		RE::Scaleform::GFx::Value format;
		a_canvas->uiMovie->CreateObject(&format, "flash.text.TextFormat");
		if (format.IsObject()) {
			if (!a_font.empty()) {
				format.SetMember(
					"font", RE::Scaleform::GFx::Value(a_font.c_str()));
			}
			format.SetMember("size", RE::Scaleform::GFx::Value(a_size));
			format.SetMember(
				"color",
				RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(a_color)));
			format.SetMember("align", RE::Scaleform::GFx::Value(a_align));
			field.SetMember("defaultTextFormat", format);
		}

		field.SetMember(
			"text", RE::Scaleform::GFx::Value(std::string(a_text).c_str()));
		// After the text: defaultTextFormat only reaches what is typed
		// afterwards, so the format is applied a second time.
		if (format.IsObject()) {
			field.Invoke("setTextFormat", nullptr, &format, 1);
		}

		g_panel.Invoke("addChild", nullptr, &field, 1);
		return field;
	}
}

void grid::Release()
{
	if (g_hiddenCross.IsDisplayObject()) {
		g_hiddenCross.SetMember("visible", RE::Scaleform::GFx::Value(true));
		g_hiddenCross = RE::Scaleform::GFx::Value();
	}
	if (g_panel.IsDisplayObject()) {
		RE::Scaleform::GFx::Value parent;
		if (g_panel.GetMember("parent", &parent) && parent.IsDisplayObject()) {
			parent.Invoke("removeChild", nullptr, &g_panel, 1);
		}
	}
	g_panel = RE::Scaleform::GFx::Value();
	g_marker = RE::Scaleform::GFx::Value();
	g_holder = RE::Scaleform::GFx::Value();
	g_note = RE::Scaleform::GFx::Value();
	g_detail = RE::Scaleform::GFx::Value();
	// Without a panel there are no cells, and a hit test against the layout
	// of a panel that is gone would answer for cells nobody can see.
	g_rows = 0;
}

void grid::Draw(
	RE::IMenu* a_canvas,
	RE::IMenu* a_favorites,
	const std::string& a_font,
	const std::vector<Page>& a_pages,
	const std::optional<Spot>& a_marked,
	std::uint32_t a_color,
	const Placement& a_where)
{
	if (!a_canvas || !a_canvas->uiMovie || !a_favorites || a_pages.empty()) {
		return;
	}
	// A menu that was loaded without a named clip has no menuObj at all --
	// ours is an empty movie, so there is nothing to name. Its root answers
	// just as well, and that is what carries the stage.
	RE::Scaleform::GFx::Value stage;
	if (!a_canvas->menuObj.IsObject() ||
		!a_canvas->menuObj.GetMember("stage", &stage) ||
		!stage.IsDisplayObject()) {
		RE::Scaleform::GFx::Value root;
		if (a_canvas->uiMovie->GetVariable(&root, "root") && root.IsObject()) {
			root.GetMember("stage", &stage);
		}
	}
	if (!stage.IsDisplayObject()) {
		logger::warn("grid: the canvas has no stage");
		return;
	}

	// Everything is drawn from scratch, children and all. A page switch
	// changes most cells anyway, and rebuilding is one code path instead of
	// two that have to agree.
	Release();

	a_canvas->uiMovie->CreateObject(&g_panel, "flash.display.Sprite");
	if (!g_panel.IsDisplayObject()) {
		logger::warn("grid: the movie would not make a sprite");
		return;
	}
	g_panel.SetMember("name", RE::Scaleform::GFx::Value("FavoritesMenuGrid"));
	g_panel.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));

	auto& parent = a_where.inMenuRoot ? a_favorites->menuObj : stage;
	if (!parent.Invoke("addChild", nullptr, &g_panel, 1)) {
		logger::warn("grid: the panel was not taken in");
		g_panel = RE::Scaleform::GFx::Value();
		return;
	}
	logger::info(
		"grid: hung in {}", a_where.inMenuRoot ? "the menu's root" : "the stage");

	// What the movie is actually allowed to paint on.
	//
	// The panel comes out clipped to a box around the cross, and so did the
	// page marker before it, so the suspicion is that the menu is given a
	// viewport rather than the whole screen. These numbers settle it: the
	// viewport is where the movie may draw, the scissor is what survives of
	// that, and the visible frame rect is the same thing in stage units.
	{
		static bool reported = false;
		if (!reported) {
			reported = true;
			RE::Scaleform::GFx::Viewport view{};
			a_canvas->uiMovie->GetViewport(&view);
			const auto frame = a_canvas->uiMovie->GetVisibleFrameRect();
			logger::info(
				"grid: viewport {}x{} at {},{} of a {}x{} buffer; scissor "
				"{}x{} at {},{}; visible frame {:.0f},{:.0f} to {:.0f},{:.0f}",
				view.width,
				view.height,
				view.left,
				view.top,
				view.bufferWidth,
				view.bufferHeight,
				view.scissorWidth,
				view.scissorHeight,
				view.scissorLeft,
				view.scissorTop,
				frame.x1,
				frame.y1,
				frame.x2,
				frame.y2);
		}
	}

	// What else the menu has, once. The cross is only part of it: the item
	// name and count are drawn by a clip of their own, and with the grid in
	// the middle they have to travel too.
	{
		static bool listed = false;
		if (!listed) {
			listed = true;
			const auto total =
				static_cast<int>(ReadNumber(a_favorites->menuObj, "numChildren", 0.0));
			std::string names;
			for (int index = 0; index < total; ++index) {
				const RE::Scaleform::GFx::Value at{ index };
				RE::Scaleform::GFx::Value child;
				RE::Scaleform::GFx::Value name;
				if (a_favorites->menuObj.Invoke("getChildAt", &child, &at, 1) &&
					child.IsDisplayObject() &&
					child.GetMember("name", &name) && name.IsString()) {
					names += std::format(
						"{} ({:.0f},{:.0f} {:.0f}x{:.0f})  ",
						name.GetString(),
						ReadNumber(child, "x", 0.0),
						ReadNumber(child, "y", 0.0),
						ReadNumber(child, "width", 0.0),
						ReadNumber(child, "height", 0.0));
				}
			}
			logger::info("grid: the menu holds {}", names);
		}
	}

	// The name and the ammo count are the menu's own fields, and they stay
	// the menu's: the game keeps writing them, we only say where. They sit
	// where the cross used to be, which with the grid in the middle is
	// nowhere useful, so they move under the panel.
	//
	// Their coordinates are the menu's, not the stage's -- its children run
	// from around -400,-560 -- so the menu is asked where its own origin
	// lands and the difference is subtracted.
	const auto moveLabel = [&](const char* a_name, double a_stageX, double a_stageY) {
		RE::Scaleform::GFx::Value field;
		if (!a_favorites->menuObj.GetMember(a_name, &field) ||
			!field.IsDisplayObject()) {
			return;
		}

		const std::array<RE::Scaleform::GFx::Value, 2> zero{
			RE::Scaleform::GFx::Value(0.0), RE::Scaleform::GFx::Value(0.0)
		};
		RE::Scaleform::GFx::Value point;
		a_favorites->uiMovie->CreateObject(
			&point, "flash.geom.Point", zero.data(), 2);
		RE::Scaleform::GFx::Value origin;
		if (!point.IsObject() ||
			!a_favorites->menuObj.Invoke("localToGlobal", &origin, &point, 1) ||
			!origin.IsObject()) {
			return;
		}

		field.SetMember(
			"x",
			RE::Scaleform::GFx::Value(a_stageX - ReadNumber(origin, "x", 0.0)));
		field.SetMember(
			"y",
			RE::Scaleform::GFx::Value(a_stageY - ReadNumber(origin, "y", 0.0)));
	};

	// The cross steps aside. Fallout 4 puts it in the bottom right corner,
	// and a panel in the middle plus a cross in the corner would be two
	// readings of the same twelve keys.
	if (a_favorites->menuObj.GetMember("Cross_mc", &g_hiddenCross) &&
		g_hiddenCross.IsDisplayObject()) {
		g_hiddenCross.SetMember("visible", RE::Scaleform::GFx::Value(false));
	} else {
		g_hiddenCross = RE::Scaleform::GFx::Value();
	}

	RE::Scaleform::GFx::Value graphics;
	if (!g_panel.GetMember("graphics", &graphics) || !graphics.IsObject()) {
		logger::warn("grid: the sprite has nothing to draw with");
		Release();
		return;
	}

	auto m = MetricsFor(a_where.cellSize);
	// The two lines above the grid are the game's own sizes rather than a
	// fraction of a cell, so the band they need is measured from them.
	m.titleSize = a_where.labelSize;
	m.detailSize = a_where.detailSize;
	m.titleHeight =
		m.titleSize + m.detailSize + m.gap * 2.0 + a_where.labelGap;
	if (!a_where.showRowLabels) {
		m.rowLabelWidth = 0.0;
	}
	m.keyRowHeight = m.keySize + a_where.keyRowGap;
	m.hintHeight = a_where.hint.empty() ? 0.0 : a_where.hintSize + m.gap * 2.0;
	const auto width = PanelWidth(m);
	const auto height = m.padding * 2.0 + m.titleHeight + m.keyRowHeight +
		RowHeight(m) * static_cast<double>(a_pages.size()) + m.hintHeight;

	// In the middle of the screen by default, where the eye already is.
	const auto stageWidth = ReadNumber(stage, "stageWidth", 1280.0);
	const auto stageHeight = ReadNumber(stage, "stageHeight", 720.0);
	const auto left =
		a_where.x < 0.0 ? (stageWidth - width) / 2.0 : a_where.x;
	const auto top =
		a_where.y < 0.0 ? (stageHeight - height) / 2.0 : a_where.y;
	// Setting x and y is not always enough. On the HUD they read back as
	// nonsense afterwards, so the older names are tried as well and the
	// result is logged -- a panel of the right size in the wrong place looks
	// exactly like a panel that was never drawn.
	const auto place = [&](const char* a_x, const char* a_y) {
		g_panel.SetMember(a_x, RE::Scaleform::GFx::Value(left));
		g_panel.SetMember(a_y, RE::Scaleform::GFx::Value(top));
		return ReadNumber(g_panel, a_x, -1.0e9);
	};

	auto placed = place("x", "y");
	if (std::abs(placed - left) > 1.0) {
		const auto second = place("_x", "_y");
		logger::warn(
			"grid: x came back as {:.0f} instead of {:.0f}; _x gives {:.0f}",
			placed,
			left,
			second);
		placed = second;
	}

	// What the pointer will be measured against. Kept from the drawing
	// rather than worked out again, so a cell can never be somewhere else
	// for the mouse than it is for the eye.
	g_metrics = m;
	g_left = left;
	g_top = top;
	g_rows = a_pages.size();
	g_pointerReported = false;

	// The stage, outlined. The panel is demonstrably drawn where it should
	// be and only part of it arrives, so the question is no longer where the
	// panel is but which parts of the stage reach the screen at all. An
	// outline all the way round, with a mark in every corner and in the
	// middle, answers that in one screenshot.
	if (a_where.probeStage) {
		Fill(graphics, -left, -top, stageWidth, 2.0, a_color, 1.0);
		Fill(graphics, -left, -top + stageHeight - 2.0, stageWidth, 2.0, a_color, 1.0);
		Fill(graphics, -left, -top, 2.0, stageHeight, a_color, 1.0);
		Fill(graphics, -left + stageWidth - 2.0, -top, 2.0, stageHeight, a_color, 1.0);
		constexpr double mark = 24.0;
		Fill(graphics, -left, -top, mark, mark, a_color, 1.0);
		Fill(graphics, -left + stageWidth - mark, -top, mark, mark, a_color, 1.0);
		Fill(graphics, -left, -top + stageHeight - mark, mark, mark, a_color, 1.0);
		Fill(
			graphics,
			-left + stageWidth - mark,
			-top + stageHeight - mark,
			mark,
			mark,
			a_color,
			1.0);
		Fill(
			graphics,
			-left + stageWidth / 2.0 - mark / 2.0,
			-top + stageHeight / 2.0 - mark / 2.0,
			mark,
			mark,
			a_color,
			1.0);
	}

	// A plate behind everything only on request. The game draws no such
	// slab anywhere, and the cells read well enough without one.
	if (a_where.backdrop) {
		Fill(graphics, 0.0, 0.0, width, height, 0x000000, kPanelAlpha);
		Outline(graphics, 0.0, 0.0, width, height, a_color, 0.5);
	}

	// Above the keys, the middle of the panel: what the mark is on, and under
	// it what it does. Built even when there is nothing to say, because the
	// mark moves without the panel being drawn again and the fields have to
	// be there when it does.
	g_note = Label(
		a_canvas, a_font, {}, 0.0, m.padding, width, m.titleSize, a_color, 1.0);
	g_detail = Label(
		a_canvas,
		a_font,
		{},
		0.0,
		m.padding + m.titleSize + m.gap,
		width,
		m.detailSize,
		a_color,
		kDetailAlpha);

	// The twelve key names, once, at the head of their columns. They are the
	// only thing on the panel that is the same on every page, so they are the
	// one thing that is not repeated per page.
	for (std::size_t slot = 0; slot < kSlots; ++slot) {
		Label(
			a_canvas,
			a_font,
			a_pages.front()[slot].label,
			CellLeft(m, slot),
			KeyRowTop(m),
			m.cell,
			m.keySize,
			a_color,
			kKeyAlpha);
	}

	for (std::size_t row = 0; row < a_pages.size(); ++row) {
		const auto rowTop = RowTop(m, row);

		if (a_where.showRowLabels) {
			Label(
				a_canvas,
				a_font,
				std::to_string(row + 1),
				m.padding - 4.0,
				rowTop + m.cell / 2.0 - m.rowLabelSize,
				m.rowLabelWidth,
				m.rowLabelSize,
				a_color,
				1.0);
		}

		// Every cell gets the same plate, whether a key holds something or
		// not. An empty key is still a key: it is where something can be
		// put, and a lattice with holes in it reads as damage rather than as
		// room. What lies on a key is said by its icon, not by whether its
		// plate is there at all.
		for (std::size_t slot = 0; slot < kSlots; ++slot) {
			const auto cellLeft = CellLeft(m, slot);
			Fill(graphics, cellLeft, rowTop, m.cell, m.cell, a_color, kCellAlpha);
			Symbol(a_canvas, a_pages[row][slot], cellLeft, rowTop, m, a_where);
		}
	}

	// The keys, under the panel, the way the game writes its own along the
	// bottom of a screen. A grid that answers to five keys and says none of
	// them is a grid nobody finds the keys of.
	if (!a_where.hint.empty()) {
		Label(
			a_canvas,
			a_font,
			a_where.hint,
			0.0,
			RowTop(m, a_pages.size()) + m.gap,
			width,
			a_where.hintSize,
			a_color,
			kHintAlpha);
	}

	// The chosen cell, drawn once and afterwards only moved. It is added
	// last, so it lies over the plates rather than under them, and it is a
	// brighter version of the same white sheet the game itself uses -- a
	// colour of its own here would say "this cell is different", where what
	// is meant is "this cell is the one".
	a_canvas->uiMovie->CreateObject(&g_marker, "flash.display.Sprite");
	if (g_marker.IsDisplayObject()) {
		g_marker.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
		g_marker.SetMember("visible", RE::Scaleform::GFx::Value(false));
		RE::Scaleform::GFx::Value pen;
		if (g_marker.GetMember("graphics", &pen) && pen.IsObject()) {
			// Brighter, not different. The game marks a cell by giving it
			// more of the same light, and a colour of its own here would
			// say "this cell is not like the others" where what is meant is
			// "this cell is the one".
			Fill(pen, 0.0, 0.0, m.cell, m.cell, a_color, kMarkFillAlpha);
			Outline(pen, 0.0, 0.0, m.cell, m.cell, a_color, kMarkLineAlpha, 2.0);
		}
		g_panel.Invoke("addChild", nullptr, &g_marker, 1);
	}


	// The cell being carried, if one is. Built the same way as the mark and
	// filled rather than outlined: it is not where you are looking, it is
	// what you are holding.
	a_canvas->uiMovie->CreateObject(&g_holder, "flash.display.Sprite");
	if (g_holder.IsDisplayObject()) {
		g_holder.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
		g_holder.SetMember("visible", RE::Scaleform::GFx::Value(false));
		RE::Scaleform::GFx::Value pen;
		if (g_holder.GetMember("graphics", &pen) && pen.IsObject()) {
			Fill(pen, 0.0, 0.0, m.cell, m.cell, a_color, kHoldFillAlpha);
		}
		g_panel.Invoke("addChild", nullptr, &g_holder, 1);
	}

	Mark(a_marked);

	// What was asked for, and what the movie made of it. The two drifting
	// apart is the only way to tell a layout mistake from a drawing one.
	// Centred under the panel, the name above the ammo line. The fields are
	// 400 and 300 wide, so half of each comes off the middle.
	moveLabel("ItemName_tf", left + width / 2.0 - 200.0, top + height + m.gap);
	moveLabel(
		"ItemAmmo_tf", left + width / 2.0 - 150.0, top + height + m.gap + 26.0);

	logger::info(
		"grid: {} pages; stage {:.0f}x{:.0f}, panel asked for "
		"{:.0f}x{:.0f} at {:.0f},{:.0f}, reports {:.0f}x{:.0f} at {:.0f},{:.0f} "
		"with {:.0f} children",
		a_pages.size(),
		stageWidth,
		stageHeight,
		width,
		height,
		(stageWidth - width) / 2.0,
		(stageHeight - height) / 2.0,
		ReadNumber(g_panel, "width", -1.0),
		ReadNumber(g_panel, "height", -1.0),
		ReadNumber(g_panel, "x", -1.0),
		ReadNumber(g_panel, "y", -1.0),
		ReadNumber(g_panel, "numChildren", -1.0));
}

bool grid::Pointer(RE::IMenu* a_canvas, double& a_x, double& a_y)
{
	const auto* cursor = RE::MenuCursor::GetSingleton();
	if (!cursor || !a_canvas || !a_canvas->uiMovie) {
		return false;
	}

	// The cursor counts screen pixels and the panel is laid out in the
	// movie's own units, of which there are 1280 by 720. What has to be
	// known is the range the cursor moves in, and the cursor carries it
	// itself: minCursorX to maxCursorX. That is better than the viewport,
	// because it stays right whatever unit the cursor turns out to count in
	// -- and this project has already lost an evening to assuming one.
	RE::Scaleform::GFx::Viewport view{};
	a_canvas->uiMovie->GetViewport(&view);
	const auto frame = a_canvas->uiMovie->GetVisibleFrameRect();

	const auto span = [](double a_low, double a_high, double a_fallback) {
		return a_high > a_low ? a_high - a_low : a_fallback;
	};
	const auto width = span(cursor->minCursorX, cursor->maxCursorX, view.width);
	const auto height = span(cursor->minCursorY, cursor->maxCursorY, view.height);
	if (width <= 0.0 || height <= 0.0) {
		return false;
	}

	const auto low = [](double a_min, double a_max) {
		return a_max > a_min ? a_min : 0.0;
	};
	a_x = frame.x1 +
		(cursor->cursorPosX - low(cursor->minCursorX, cursor->maxCursorX)) *
			(frame.x2 - frame.x1) / width;
	a_y = frame.y1 +
		(cursor->cursorPosY - low(cursor->minCursorY, cursor->maxCursorY)) *
			(frame.y2 - frame.y1) / height;

	if (!g_pointerReported) {
		g_pointerReported = true;
		const auto over = At(a_x, a_y);
		logger::info(
			"grid: the cursor is at {},{} of {},{} to {},{} ({} registered); "
			"the viewport is {}x{} at {},{} and the frame {:.0f},{:.0f} to "
			"{:.0f},{:.0f}; that makes {:.0f},{:.0f} on the stage, and the "
			"panel runs {:.0f},{:.0f} to {:.0f},{:.0f} -- {}",
			cursor->cursorPosX,
			cursor->cursorPosY,
			cursor->minCursorX,
			cursor->minCursorY,
			cursor->maxCursorX,
			cursor->maxCursorY,
			cursor->registeredCursors,
			view.width,
			view.height,
			view.left,
			view.top,
			frame.x1,
			frame.y1,
			frame.x2,
			frame.y2,
			a_x,
			a_y,
			g_left,
			g_top,
			g_left + PanelWidth(g_metrics),
			g_top + RowTop(g_metrics, g_rows),
			over ? std::format("on page {} key {}", over->page + 1, over->slot + 1)
				 : std::string("on no cell"));
	}

	return true;
}

std::optional<grid::Spot> grid::At(double a_x, double a_y)
{
	if (g_rows == 0) {
		return std::nullopt;
	}

	const auto& m = g_metrics;
	const auto fromLeft = a_x - g_left - m.padding - m.rowLabelWidth;
	const auto fromTop = a_y - g_top - RowTop(m, 0);
	if (fromLeft < 0.0 || fromTop < 0.0) {
		return std::nullopt;
	}

	const auto column = m.cell + m.gap;
	const auto slot = static_cast<std::size_t>(fromLeft / column);
	const auto row = static_cast<std::size_t>(fromTop / RowHeight(m));
	if (slot >= kSlots || row >= g_rows) {
		return std::nullopt;
	}

	// Inside the cell, not in the gap beside or below it. A grid answers
	// for what it draws; the space between two cells belongs to neither.
	if (fromLeft - static_cast<double>(slot) * column > m.cell ||
		fromTop - static_cast<double>(row) * RowHeight(m) > m.cell) {
		return std::nullopt;
	}

	return Spot{ row, slot };
}

void grid::Mark(const std::optional<Spot>& a_spot)
{
	if (!g_marker.IsDisplayObject()) {
		return;
	}
	if (!a_spot || a_spot->page >= g_rows || a_spot->slot >= kSlots) {
		g_marker.SetMember("visible", RE::Scaleform::GFx::Value(false));
		return;
	}

	g_marker.SetMember(
		"x", RE::Scaleform::GFx::Value(CellLeft(g_metrics, a_spot->slot)));
	g_marker.SetMember(
		"y", RE::Scaleform::GFx::Value(RowTop(g_metrics, a_spot->page)));
	g_marker.SetMember("visible", RE::Scaleform::GFx::Value(true));
}

void grid::Say(std::string_view a_name, std::string_view a_what)
{
	const auto write = [](RE::Scaleform::GFx::Value& a_field, std::string_view a_text) {
		if (a_field.IsDisplayObject()) {
			a_field.SetMember(
				"text", RE::Scaleform::GFx::Value(std::string(a_text).c_str()));
		}
	};
	write(g_note, a_name);
	write(g_detail, a_what);
}

void grid::Hold(const std::optional<Spot>& a_spot)
{
	if (!g_holder.IsDisplayObject()) {
		return;
	}
	if (!a_spot || a_spot->page >= g_rows || a_spot->slot >= kSlots) {
		g_holder.SetMember("visible", RE::Scaleform::GFx::Value(false));
		return;
	}

	g_holder.SetMember(
		"x", RE::Scaleform::GFx::Value(CellLeft(g_metrics, a_spot->slot)));
	g_holder.SetMember(
		"y", RE::Scaleform::GFx::Value(RowTop(g_metrics, a_spot->page)));
	g_holder.SetMember("visible", RE::Scaleform::GFx::Value(true));
}
