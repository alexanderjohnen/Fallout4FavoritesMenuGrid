#include "PCH.h"

#include "grid.h"

namespace
{
	// Sized for a square cell that will hold an icon once the icons are
	// there, with the name on a line underneath. The numbers are the whole
	// layout: everything else is derived from them.
	constexpr double kCellSize = 64.0;
	constexpr double kCellGap = 3.0;
	constexpr double kRowLabelWidth = 30.0;
	constexpr double kPadding = 10.0;
	constexpr double kNameHeight = 14.0;
	constexpr double kNameSize = 9.0;
	constexpr double kKeySize = 10.0;
	constexpr double kRowLabelSize = 12.0;
	constexpr double kTitleSize = 15.0;
	constexpr double kTitleHeight = 22.0;

	constexpr std::size_t kSlots = 12;

	// A cell is drawn as a plate of the HUD colour at these strengths, so
	// the whole panel keeps to one colour and reads as one thing.
	constexpr double kPanelAlpha = 0.72;
	constexpr double kCellAlpha = 0.16;
	constexpr double kCellLineAlpha = 0.45;
	constexpr double kCurrentAlpha = 0.32;
	constexpr double kCurrentLineAlpha = 1.0;

	RE::Scaleform::GFx::Value g_panel;
	// Kept so the cross can be put back the way it was found.
	RE::Scaleform::GFx::Value g_hiddenCross;

	[[nodiscard]] double PanelWidth()
	{
		return kPadding * 2.0 + kRowLabelWidth +
			(kCellSize + kCellGap) * static_cast<double>(kSlots) - kCellGap;
	}

	[[nodiscard]] double RowHeight()
	{
		return kCellSize + kNameHeight + kCellGap;
	}

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
		RE::IMenu* a_menu,
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
		a_menu->uiMovie->CreateObject(&field, "flash.text.TextField");
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
		a_menu->uiMovie->CreateObject(&format, "flash.text.TextFormat");
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
	RE::IMenu* a_menu,
	const std::string& a_font,
	const std::string& a_title,
	const std::vector<Page>& a_pages,
	std::size_t a_current,
	std::uint32_t a_color)
{
	if (!a_menu || !a_menu->uiMovie || a_pages.empty()) {
		return;
	}
	RE::Scaleform::GFx::Value stage;
	if (!a_menu->menuObj.GetMember("stage", &stage) || !stage.IsDisplayObject()) {
		logger::warn("grid: the menu has no stage");
		return;
	}

	// Everything is drawn from scratch, children and all. A page switch
	// changes most cells anyway, and rebuilding is one code path instead of
	// two that have to agree.
	Release();

	a_menu->uiMovie->CreateObject(&g_panel, "flash.display.Sprite");
	if (!g_panel.IsDisplayObject()) {
		logger::warn("grid: the movie would not make a sprite");
		return;
	}
	g_panel.SetMember("name", RE::Scaleform::GFx::Value("FavoritesMenuGrid"));
	g_panel.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
	if (!stage.Invoke("addChild", nullptr, &g_panel, 1)) {
		logger::warn("grid: the stage would not take the panel");
		g_panel = RE::Scaleform::GFx::Value();
		return;
	}

	// The cross steps aside. Fallout 4 puts it in the bottom right corner,
	// and a panel in the middle plus a cross in the corner would be two
	// readings of the same twelve keys.
	if (a_menu->menuObj.GetMember("Cross_mc", &g_hiddenCross) &&
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

	const auto width = PanelWidth();
	const auto height = kPadding * 2.0 + kTitleHeight +
		RowHeight() * static_cast<double>(a_pages.size());

	// In the middle of the screen, where the eye already is.
	const auto stageWidth = ReadNumber(stage, "stageWidth", 1280.0);
	const auto stageHeight = ReadNumber(stage, "stageHeight", 720.0);
	g_panel.SetMember("x", RE::Scaleform::GFx::Value((stageWidth - width) / 2.0));
	g_panel.SetMember(
		"y", RE::Scaleform::GFx::Value((stageHeight - height) / 2.0));

	// The plate behind everything, dark rather than coloured: the cells and
	// the text carry the colour, and a coloured plate would fight them.
	Fill(graphics, 0.0, 0.0, width, height, 0x000000, kPanelAlpha);
	Outline(graphics, 0.0, 0.0, width, height, a_color, 0.5);

	if (!a_title.empty()) {
		Label(
			a_menu,
			a_font,
			a_title,
			0.0,
			kPadding,
			width,
			kTitleSize,
			a_color,
			1.0);
	}

	for (std::size_t row = 0; row < a_pages.size(); ++row) {
		const auto top = kPadding + kTitleHeight +
			RowHeight() * static_cast<double>(row);
		const bool playing = row == a_current;

		Label(
			a_menu,
			a_font,
			std::to_string(row + 1),
			kPadding - 4.0,
			top + kCellSize / 2.0 - kRowLabelSize,
			kRowLabelWidth,
			kRowLabelSize,
			a_color,
			playing ? 1.0 : 0.5);

		for (std::size_t slot = 0; slot < kSlots; ++slot) {
			const auto left = kPadding + kRowLabelWidth +
				(kCellSize + kCellGap) * static_cast<double>(slot);
			const auto& cell = a_pages[row][slot];
			const bool taken = !cell.name.empty();

			Fill(
				graphics,
				left,
				top,
				kCellSize,
				kCellSize,
				a_color,
				playing ? kCurrentAlpha : kCellAlpha);
			Outline(
				graphics,
				left,
				top,
				kCellSize,
				kCellSize,
				a_color,
				playing ? kCurrentLineAlpha : kCellLineAlpha);

			// The key it sits on, in the corner, so the panel can be read
			// against the cross without counting.
			Label(
				a_menu,
				a_font,
				cell.label,
				left + 2.0,
				top + 2.0,
				kCellSize - 4.0,
				kKeySize,
				a_color,
				taken ? 0.9 : 0.35);

			if (taken) {
				Label(
					a_menu,
					a_font,
					Shorten(cell.name, 13),
					left,
					top + kCellSize - kNameHeight,
					kCellSize,
					kNameSize,
					a_color,
					0.95);
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
