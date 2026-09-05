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
		double titleHeight{ 18.0 };
		std::size_t nameRoom{ 10 };
	};

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
		m.keySize = floorAt(a_cell * 0.19, 8.0);
		m.rowLabelSize = floorAt(a_cell * 0.23, 9.0);
		m.titleSize = floorAt(a_cell * 0.27, 10.0);
		m.titleHeight = m.titleSize + 5.0;
		// Roughly what fits at this size in a condensed face.
		m.nameRoom = static_cast<std::size_t>(
			std::max(4.0, a_cell / (m.nameSize * 0.48)));
		return m;
	}

	constexpr std::size_t kSlots = 12;

	// Fallout 4 draws its panels as thin translucent plates with no border
	// at all -- look at the cross itself. So the cells are barely there, and
	// only the page being played is lifted out; an outline is kept for that
	// one alone, because it is the one thing that has to be unmistakable.
	constexpr double kCellAlpha = 0.07;
	constexpr double kCellLineAlpha = 0.0;
	constexpr double kCurrentAlpha = 0.20;
	constexpr double kCurrentLineAlpha = 0.45;
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

	RE::Scaleform::GFx::Value g_panel;
	// Kept so the cross can be put back the way it was found.
	RE::Scaleform::GFx::Value g_hiddenCross;

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
		double a_alpha)
	{
		const std::array stroke{
			RE::Scaleform::GFx::Value(1.0),
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
	void Label(
		RE::IMenu* a_canvas,
		const std::string& a_font,
		std::string_view a_text,
		double a_x,
		double a_y,
		double a_width,
		double a_size,
		std::uint32_t a_color,
		double a_alpha)
	{
		RE::Scaleform::GFx::Value field;
		a_canvas->uiMovie->CreateObject(&field, "flash.text.TextField");
		if (!field.IsDisplayObject()) {
			return;
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
			format.SetMember("align", RE::Scaleform::GFx::Value("center"));
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
}

void grid::Draw(
	RE::IMenu* a_canvas,
	RE::IMenu* a_favorites,
	const std::string& a_font,
	const std::string& a_title,
	const std::vector<Page>& a_pages,
	std::size_t a_current,
	std::uint32_t a_color,
	const Placement& a_where)
{
	if (!a_canvas || !a_canvas->uiMovie || !a_favorites || a_pages.empty()) {
		return;
	}
	RE::Scaleform::GFx::Value stage;
	if (!a_canvas->menuObj.GetMember("stage", &stage) || !stage.IsDisplayObject()) {
		logger::warn("grid: the menu has no stage");
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

	const auto m = MetricsFor(a_where.cellSize);
	const auto width = PanelWidth(m);
	const auto height = m.padding * 2.0 + m.titleHeight +
		RowHeight(m) * static_cast<double>(a_pages.size());

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

	if (!a_title.empty()) {
		Label(
			a_canvas,
			a_font,
			a_title,
			0.0,
			m.padding,
			width,
			m.titleSize,
			a_color,
			1.0);
	}

	for (std::size_t row = 0; row < a_pages.size(); ++row) {
		const auto rowTop = m.padding + m.titleHeight +
			RowHeight(m) * static_cast<double>(row);
		const bool playing = row == a_current;

		Label(
			a_canvas,
			a_font,
			std::to_string(row + 1),
			m.padding - 4.0,
			rowTop + m.cell / 2.0 - m.rowLabelSize,
			m.rowLabelWidth,
			m.rowLabelSize,
			a_color,
			playing ? 1.0 : 0.5);

		for (std::size_t slot = 0; slot < kSlots; ++slot) {
			const auto cellLeft = m.padding + m.rowLabelWidth +
				(m.cell + m.gap) * static_cast<double>(slot);
			const auto& cell = a_pages[row][slot];
			const bool taken = !cell.name.empty();

			Fill(
				graphics,
				cellLeft,
				rowTop,
				m.cell,
				m.cell,
				a_color,
				playing ? kCurrentAlpha : kCellAlpha);
			Outline(
				graphics,
				cellLeft,
				rowTop,
				m.cell,
				m.cell,
				a_color,
				playing ? kCurrentLineAlpha : kCellLineAlpha);

			// The key it sits on, in the corner, so the panel can be read
			// against the cross without counting.
			Label(
				a_canvas,
				a_font,
				cell.label,
				cellLeft + 2.0,
				rowTop + 2.0,
				m.cell - 4.0,
				m.keySize,
				a_color,
				taken ? 0.9 : 0.35);

			if (taken) {
				// Inside the cell, along its bottom edge: a name below the
				// cell would push the rows apart and break the lattice.
				Label(
					a_canvas,
					a_font,
					Shorten(cell.name, m.nameRoom),
					cellLeft,
					rowTop + m.cell - m.nameHeight - 1.0,
					m.cell,
					m.nameSize,
					a_color,
					0.9);
			}
		}
	}

	// What was asked for, and what the movie made of it. The two drifting
	// apart is the only way to tell a layout mistake from a drawing one.
	logger::info(
		"grid: {} pages, playing {}; stage {:.0f}x{:.0f}, panel asked for "
		"{:.0f}x{:.0f} at {:.0f},{:.0f}, reports {:.0f}x{:.0f} at {:.0f},{:.0f} "
		"with {:.0f} children",
		a_pages.size(),
		a_current + 1,
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
