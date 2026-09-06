// Favorites Menu Grid for Fallout 4 -- the working core.
//
// What is left here is what turned out to be true. The exploration that
// produced it is written up in HANDOFF.md, and the probes themselves are in
// the history if they are ever wanted again: menu display lists, UI message
// experiments, Cross_mc.infoArray writing, the FavoriteChangedEvent, the
// vtable dump. None of them are needed to move a favorite.
//
// A favorite lives on the inventory stack as ExtraFavorite::quickkeyIndex.
// FavoritesManager::storedFavTypes is a copy the engine keeps, and there is
// a third table nobody outside can see. Writing any of them directly leaves
// the others behind. Writing through the engine updates all three:
//
//     BGSInventoryList::FindAndWriteStackDataForItem(object, compare, write)
//
// That is the whole page switch, and everything below serves it.

#include "PCH.h"

#include "detail.h"
#include "grid.h"
#include "icons.h"
#include "input.h"
#include "menu.h"
#include "peek.h"
#include "tags.h"
#include "use.h"

namespace
{
	// ---- Settings -------------------------------------------------------
	//
	// The keys are in the INI because guessing them cost four rounds: F5 is
	// quicksave, F9 is quickload, and Special K sits on F8 and F9 here.

	int g_inventoryKey = VK_F6;
	// On again: the round trip no longer takes anything away, it parks a
	// favorite at -1 and gives the key back, both through the engine.
	int g_roundTripKey = VK_F7;
	int g_rotateKey = VK_F8;
	int g_nextPageKey = VK_NEXT;
	int g_previousPageKey = VK_PRIOR;

	// How many pages the twelve keys are shared between.
	int g_pageCount = 3;

	// Whether the "[Tag]" that FIS puts in front of an item name is dropped
	// before the cross shows it.
	bool g_stripItemTags = true;

	// The whole set of pages, drawn beside the cross.
	bool g_useGrid = true;
	grid::Placement g_gridWhere;

	// Walking the grid and using what is under the mark. Keys rather than
	// fixed letters: w and s are only obvious to someone who never moved
	// them.
	input::Keys g_gridKeys;

	// The cross closes the menu when a key is used, and a grid that stayed
	// open afterwards would be the one place in the game where using a
	// favorite leaves you standing in a menu.
	bool g_closeAfterUse = true;

	// Whether cells carry icons at all.
	bool g_useIcons = true;

	// The line of keys under the panel, and whatever else should stand in it.
	// The closing key belongs to the game rather than to us -- it is whatever
	// the player bound the favorites menu to -- so it is text, not a binding.
	bool g_showHint = true;
	std::string g_hintExtra = "TAB) CLOSE";

	// Empty means the font the cross labels its own keys with, which is the
	// game's own and always present.
	std::string g_gridFont;

	// And whether something nobody has a symbol for still gets one, by what
	// kind of thing it is.
	bool g_iconFallback = true;

	// The page that goes back into the engine's twelve keys when the menu
	// closes, counted from 1. Zero leaves whatever page was last used there.
	//
	// The grid shows every page alike, so "the page you are on" is not a
	// thing the player can see any more -- and the game's own digit keys can
	// only ever reach the page the engine holds. Leaving that to be whichever
	// page was last touched turns the digits into invisible state: the same
	// key does something different depending on what was clicked ten minutes
	// ago. Restoring one chosen page on every close makes them mean one fixed
	// thing. The Starfield version arrived at this and calls it defaultRow.
	//
	// It is off by default because it is not free here: a page switch moves
	// every favorite through the engine, once per key, and doing that on
	// every close of the menu is a cost the player should choose.
	int g_defaultPage = 0;

	// Whether the ends of a row and of the stack are walls or doors. There is
	// no right answer, which is why it is a setting.
	bool g_wrapNavigation = true;

	// Frees the key the mark sits on. The item stays a favorite -- it goes to
	// the same "favorited, no key" state the game itself writes when you
	// favorite something from the Pip-Boy without assigning a digit.
	int g_clearKey = VK_DELETE;

	// Picks a cell up and puts it down again somewhere else.
	//
	// Not a letter, and that is deliberate: a mod that reads the keyboard
	// directly -- the way this plugin's own page keys do -- never sees that a
	// menu claimed the key, so a grenade went flying every time a cell was
	// picked up. What the grid claims is only taken from things that go
	// through the game's own input handlers.
	int g_moveKey = VK_INSERT;

	// The crosshair belongs to the HUD, and the HUD has no idea the favorites
	// menu is open.
	bool g_hideCrosshair = true;

	// The page marker inside the favorites menu.
	bool g_showPageIndicator = true;
	std::string g_indicatorText = "Favorites";
	double g_indicatorX = 0.0;
	double g_indicatorY = 4.0;
	double g_indicatorSize = 18.0;
	// Wide enough for any wording anyone is likely to put in the INI; the
	// text is centred in it, so what is not used costs nothing.
	constexpr double kIndicatorWidth = 320.0;

	// Empty means "the one the cross labels its own keys with".
	std::string g_indicatorFont;
	// What the measuring settled on, so it is only logged when it changes.
	std::string g_indicatorFontInUse = "?";
	// The cross is measured on every draw but only reported once.
	bool g_boundsLogged = false;
	// Anything above white means "take the colour the player set for the HUD".
	std::uint32_t g_indicatorColor = 0x1000000;

	// The grid's own, on the same terms. It is a colour of its own because
	// the grid is the whole panel now and the marker inside the favorites
	// menu is a different thing on a different canvas.
	std::uint32_t g_gridColor = 0x1000000;

	// The cross shows no page of its own, so turning one is announced the
	// way the game announces everything else. The wording is a setting
	// because the game is not played in English everywhere; the numbers are
	// appended to it.
	std::string g_pageMessage = "Favorites";

	// Writes the pieces of engine code named in the INI next to the log --
	// see peek.h. The settings are read at that moment, so a new question
	// needs no restart of the game.
	int g_peekKey = VK_F10;

	[[nodiscard]] std::filesystem::path GetSettingsPath()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		const auto length = GetModuleFileNameW(
			nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path() / L"Data" / L"F4SE" /
			L"Plugins" / L"FavoritesMenuGrid.ini";
	}

	// The other direction from ParseVirtualKey: what to call a key on screen.
	// Short, the way the game labels its own -- "E)" and "INS)", not
	// "E key" -- because the line has twelve cells' worth of width and five
	// things to say in it.
	[[nodiscard]] std::string KeyName(int a_key)
	{
		if ((a_key >= 'A' && a_key <= 'Z') || (a_key >= '0' && a_key <= '9')) {
			return std::string(1, static_cast<char>(a_key));
		}
		if (a_key >= VK_F1 && a_key <= VK_F24) {
			return std::format("F{}", a_key - VK_F1 + 1);
		}
		switch (a_key) {
		case VK_RETURN:
			return "ENTER";
		case VK_ESCAPE:
			return "ESC";
		case VK_SPACE:
			return "SPACE";
		case VK_TAB:
			return "TAB";
		case VK_INSERT:
			return "INS";
		case VK_DELETE:
			return "DEL";
		case VK_PRIOR:
			return "PGUP";
		case VK_NEXT:
			return "PGDN";
		case VK_HOME:
			return "HOME";
		case VK_END:
			return "END";
		case VK_UP:
			return "UP";
		case VK_DOWN:
			return "DOWN";
		case VK_LEFT:
			return "LEFT";
		case VK_RIGHT:
			return "RIGHT";
		default:
			return {};
		}
	}

	// Data\Interface, where every menu movie and every sorter configuration
	// lives. Derived from the INI's own path rather than looked up, so it
	// follows the game wherever it is installed.
	[[nodiscard]] std::filesystem::path GetInterfacePath()
	{
		return GetSettingsPath().parent_path().parent_path().parent_path() /
			L"Interface";
	}

	[[nodiscard]] std::wstring NormalizeKeyName(std::wstring a_value)
	{
		std::wstring normalized;
		normalized.reserve(a_value.size());
		for (const auto character : a_value) {
			if (!std::iswspace(character) && character != L'_') {
				normalized.push_back(
					static_cast<wchar_t>(std::towupper(character)));
			}
		}
		return normalized;
	}

	// Same spelling of key names as the Starfield mod, so a player who knows
	// one INI knows the other.
	[[nodiscard]] std::optional<int> ParseVirtualKey(const std::wstring& a_raw)
	{
		const auto value = NormalizeKeyName(a_raw);
		if (value.empty()) {
			return std::nullopt;
		}
		if (value == L"NONE" || value == L"DISABLED") {
			return 0;
		}
		if (value.size() == 1) {
			const auto character = value.front();
			if ((character >= L'A' && character <= L'Z') ||
				(character >= L'0' && character <= L'9')) {
				return static_cast<int>(character);
			}
		}
		if (value.starts_with(L"F") && value.size() <= 3) {
			const auto number = std::wcstol(value.c_str() + 1, nullptr, 10);
			if (number >= 1 && number <= 24) {
				return VK_F1 + static_cast<int>(number - 1);
			}
		}

		const std::array<std::pair<std::wstring_view, int>, 28> names{
			std::pair{ L"RETURN"sv, VK_RETURN },
			std::pair{ L"ENTER"sv, VK_RETURN },
			std::pair{ L"ESCAPE"sv, VK_ESCAPE },
			std::pair{ L"ESC"sv, VK_ESCAPE },
			std::pair{ L"UP"sv, VK_UP },
			std::pair{ L"DOWN"sv, VK_DOWN },
			std::pair{ L"LEFT"sv, VK_LEFT },
			std::pair{ L"RIGHT"sv, VK_RIGHT },
			std::pair{ L"PAGEUP"sv, VK_PRIOR },
			std::pair{ L"PGUP"sv, VK_PRIOR },
			std::pair{ L"PAGEDOWN"sv, VK_NEXT },
			std::pair{ L"PGDN"sv, VK_NEXT },
			std::pair{ L"HOME"sv, VK_HOME },
			std::pair{ L"END"sv, VK_END },
			std::pair{ L"INSERT"sv, VK_INSERT },
			std::pair{ L"DELETE"sv, VK_DELETE },
			std::pair{ L"SPACE"sv, VK_SPACE },
			std::pair{ L"TAB"sv, VK_TAB },
			std::pair{ L"NUMPAD0"sv, VK_NUMPAD0 },
			std::pair{ L"NUMPAD1"sv, VK_NUMPAD1 },
			std::pair{ L"NUMPAD2"sv, VK_NUMPAD2 },
			std::pair{ L"NUMPAD3"sv, VK_NUMPAD3 },
			std::pair{ L"NUMPAD4"sv, VK_NUMPAD4 },
			std::pair{ L"NUMPAD5"sv, VK_NUMPAD5 },
			std::pair{ L"NUMPAD6"sv, VK_NUMPAD6 },
			std::pair{ L"NUMPAD7"sv, VK_NUMPAD7 },
			std::pair{ L"NUMPAD8"sv, VK_NUMPAD8 },
			std::pair{ L"NUMPAD9"sv, VK_NUMPAD9 }
		};
		for (const auto& [name, code] : names) {
			if (value == name) {
				return code;
			}
		}

		wchar_t* end = nullptr;
		const auto numeric = std::wcstol(value.c_str(), &end, 0);
		if (end && *end == L'\0' && numeric >= 0 && numeric <= 0xFF) {
			return static_cast<int>(numeric);
		}
		return std::nullopt;
	}

	// The INI is wide, the menu and the HUD want bytes, and a wording may
	// well carry an umlaut -- so it goes through UTF-8 rather than through a
	// cast that would drop half of it.
	[[nodiscard]] std::string ReadText(
		const std::filesystem::path& a_path,
		const wchar_t* a_section,
		const wchar_t* a_key,
		const wchar_t* a_fallback)
	{
		std::wstring value(128, L'#');
		value.resize(GetPrivateProfileStringW(
			a_section,
			a_key,
			a_fallback,
			value.data(),
			static_cast<DWORD>(value.size()),
			a_path.c_str()));
		if (value.empty()) {
			return {};
		}

		const auto needed = WideCharToMultiByte(
			CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (needed <= 1) {
			return {};
		}
		std::string narrow(static_cast<std::size_t>(needed) - 1, ' ');
		WideCharToMultiByte(
			CP_UTF8, 0, value.c_str(), -1, narrow.data(), needed, nullptr, nullptr);
		return narrow;
	}

	void LoadSettings()
	{
		const auto path = GetSettingsPath();
		std::error_code error;
		if (!std::filesystem::exists(path, error)) {
			logger::info("settings: no INI, using the defaults");
			return;
		}

		// The section matters: a key written under the wrong heading is
		// read as absent and the default quietly wins.
		const auto read = [&](const wchar_t* a_section,
							   const wchar_t* a_key,
							   int& a_target) {
			std::wstring value(64, L'\0');
			const auto length = GetPrivateProfileStringW(
				a_section,
				a_key,
				L"",
				value.data(),
				static_cast<DWORD>(value.size()),
				path.c_str());
			value.resize(length);
			if (value.empty()) {
				return;
			}
			if (const auto parsed = ParseVirtualKey(value)) {
				a_target = *parsed;
			} else {
				logger::warn("settings: could not read the key for that entry");
			}
		};

		read(L"Pages", L"NextPageKey", g_nextPageKey);
		read(L"Pages", L"PreviousPageKey", g_previousPageKey);

		read(L"Controls", L"GridPageUpKey", g_gridKeys.pageUp);
		read(L"Controls", L"GridPageDownKey", g_gridKeys.pageDown);
		read(L"Controls", L"GridLeftKey", g_gridKeys.slotLeft);
		read(L"Controls", L"GridRightKey", g_gridKeys.slotRight);
		read(L"Controls", L"GridUseKey", g_gridKeys.use);
		read(L"Controls", L"GridUseAltKey", g_gridKeys.useAlt);
		read(L"Controls", L"GridClearKey", g_clearKey);
		read(L"Controls", L"GridMoveKey", g_moveKey);
		g_wrapNavigation =
			GetPrivateProfileIntW(L"Controls", L"GridWrap", 1, path.c_str()) != 0;

		read(L"Debug", L"InventoryProbeKey", g_inventoryKey);
		read(L"Debug", L"FavoriteRoundTripKey", g_roundTripKey);
		read(L"Debug", L"RotateFavoritesKey", g_rotateKey);
		read(L"Debug", L"PeekKey", g_peekKey);

		// What the cross shows.
		g_gridKeys.useOnClick =
			GetPrivateProfileIntW(
				L"Controls", L"GridUseOnClick", 1, path.c_str()) != 0;
		g_closeAfterUse =
			GetPrivateProfileIntW(
				L"Controls", L"GridCloseAfterUse", 1, path.c_str()) != 0;

		g_stripItemTags = GetPrivateProfileIntW(
							  L"Display", L"StripItemTags", 1, path.c_str()) != 0;
		g_useGrid =
			GetPrivateProfileIntW(L"Display", L"UseGrid", 1, path.c_str()) != 0;
		// GetPrivateProfileIntW answers with a UINT, so a -1 that means
		// "centred" comes back as 4294967295 and puts the panel four billion
		// units off screen. The cast is the whole fix, and it cost an
		// evening's worth of wrong conclusions.
		g_gridWhere.x = static_cast<int>(
			GetPrivateProfileIntW(L"Display", L"GridX", -1, path.c_str()));
		g_gridWhere.y = static_cast<int>(
			GetPrivateProfileIntW(L"Display", L"GridY", -1, path.c_str()));
		g_gridWhere.probeStage =
			GetPrivateProfileIntW(L"Display", L"GridProbeStage", 0, path.c_str()) != 0;
		g_gridWhere.inMenuRoot =
			GetPrivateProfileIntW(L"Display", L"GridInMenuRoot", 0, path.c_str()) != 0;
		g_gridWhere.canvas = ReadText(path, L"Display", L"GridMenu", L"HUDMenu");
		g_useIcons =
			GetPrivateProfileIntW(L"Display", L"UseIcons", 1, path.c_str()) != 0;
		g_iconFallback =
			GetPrivateProfileIntW(L"Display", L"IconFallback", 1, path.c_str()) != 0;
		g_gridWhere.iconColors =
			GetPrivateProfileIntW(L"Display", L"IconColors", 1, path.c_str()) != 0;
		g_gridWhere.labelSize = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Display", L"LabelSize", 28, path.c_str())),
			8,
			72);
		g_gridWhere.detailSize = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Display", L"LabelDetailSize", 22, path.c_str())),
			8,
			72);
		g_showHint =
			GetPrivateProfileIntW(L"Display", L"ShowKeyHints", 1, path.c_str()) != 0;
		g_hintExtra = ReadText(path, L"Display", L"KeyHintExtra", L"TAB) CLOSE");
		g_gridFont = ReadText(path, L"Display", L"GridFont", L"");
		g_gridWhere.hintSize = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Display", L"KeyHintSize", 18, path.c_str())),
			8,
			48);
		g_gridWhere.corners =
			GetPrivateProfileIntW(L"Display", L"GridCorners", 0, path.c_str()) != 0;
		g_gridWhere.cornerArm = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Display", L"GridCornerLength", 20, path.c_str())),
			5,
			200) / 100.0;
		g_gridWhere.cornerThickness = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Display", L"GridCornerThickness", 1, path.c_str())),
			1,
			16);
		g_gridWhere.cornerOutset = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Display", L"GridCornerOutset", 6, path.c_str())),
			0,
			64);
		g_gridWhere.labelGap = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Display", L"LabelGap", 16, path.c_str())),
			0,
			96);
		g_gridWhere.showRowLabels =
			GetPrivateProfileIntW(
				L"Display", L"ShowPageNumbers", 1, path.c_str()) != 0;
		g_gridWhere.keyRowGap = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Display", L"KeyRowGap", 8, path.c_str())),
			0,
			64);
		g_gridWhere.iconFit = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Display", L"IconFit", 78, path.c_str())),
			20,
			100) / 100.0;
		g_gridWhere.cellSize = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Display", L"GridCellSize", 48, path.c_str())),
			24,
			96);
		g_gridWhere.backdrop =
			GetPrivateProfileIntW(L"Display", L"GridBackdrop", 0, path.c_str()) != 0;
		g_hideCrosshair =
			GetPrivateProfileIntW(
				L"Display", L"HideCrosshair", 1, path.c_str()) != 0;
		g_showPageIndicator =
			GetPrivateProfileIntW(
				L"Display", L"ShowPageIndicator", 1, path.c_str()) != 0;
		g_indicatorX = static_cast<int>(GetPrivateProfileIntW(
			L"Display", L"PageIndicatorX", 0, path.c_str()));
		g_indicatorY = static_cast<int>(GetPrivateProfileIntW(
			L"Display", L"PageIndicatorY", 4, path.c_str()));
		g_indicatorSize = static_cast<int>(GetPrivateProfileIntW(
			L"Display", L"PageIndicatorSize", 18, path.c_str()));
		g_indicatorColor = static_cast<std::uint32_t>(GetPrivateProfileIntW(
			L"Display", L"PageIndicatorColor", 0x1000000, path.c_str()));
		g_gridColor = static_cast<std::uint32_t>(GetPrivateProfileIntW(
			L"Display", L"GridColor", 0x1000000, path.c_str()));
		g_indicatorText =
			ReadText(path, L"Display", L"PageIndicatorText", L"Favorites");
		g_indicatorFont = ReadText(path, L"Display", L"PageIndicatorFont", L"");

		// Off unless someone asks for it: the corner message lands wherever
		// the player's HUD mods put it, which is why the page is written
		// into the menu instead.
		g_pageMessage = ReadText(path, L"Pages", L"PageMessage", L"");

		// Not a key, so it is read on its own.
		g_pageCount = static_cast<int>(GetPrivateProfileIntW(
			L"Pages", L"PageCount", g_pageCount, path.c_str()));
		g_pageCount = std::clamp(g_pageCount, 1, 32);
		g_defaultPage = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Pages", L"DefaultPage", 0, path.c_str())),
			0,
			g_pageCount);

		logger::info(
			"settings: {} pages on {:#04x} and {:#04x}; keys {:#04x} "
			"(inventory), {:#04x} (round trip), {:#04x} (rotate)",
			g_pageCount,
			g_nextPageKey,
			g_previousPageKey,
			g_inventoryKey,
			g_roundTripKey,
			g_rotateKey);
	}

	// ---- Reading the favorites -------------------------------------------

	// The digit keys run 1..9, then 0, and the cross adds - and = for the
	// last two. FavoritesEntry in the menu's own ActionScript labels them
	// the same way.
	[[nodiscard]] std::string KeyLabel(std::size_t a_index)
	{
		if (a_index < 9) {
			return std::string(1, static_cast<char>('1' + a_index));
		}
		return a_index == 9 ? "0" : a_index == 10 ? "-" : "=";
	}

	inline constexpr std::uint8_t kNoKey = 0xFF;
	inline constexpr std::uint8_t kNotAFavorite = 0xFE;

	// What a stack carries: its key, kNoKey when it is favorited without
	// one, kNotAFavorite when it is no favorite at all.
	[[nodiscard]] std::uint8_t FavoriteOf(const RE::BGSInventoryItem::Stack& a_stack)
	{
		if (a_stack.extra) {
			if (const auto* favorite = a_stack.extra->GetByType<RE::ExtraFavorite>()) {
				return static_cast<std::uint8_t>(favorite->quickkeyIndex);
			}
		}
		return kNotAFavorite;
	}

	struct FavoriteSlot
	{
		RE::TESBoundObject* object{ nullptr };
		std::uint32_t count{ 0 };
	};

	// The twelve slots as the inventory has them. This is the source of
	// truth; the manager's array only follows it.
	[[nodiscard]] std::array<FavoriteSlot, 12> ReadFavorites()
	{
		std::array<FavoriteSlot, 12> slots{};
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList) {
			return slots;
		}

		// ForEachStack does not lock, so every caller runs as a UI task.
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				const auto carried = FavoriteOf(a_stack);
				if (carried < 12) {
					slots[carried] = FavoriteSlot{ a_item.object, a_stack.count };
				}
				return true;
			});
		return slots;
	}

	void LogFavorites(std::string_view a_reason)
	{
		const auto slots = ReadFavorites();
		std::string line;
		for (std::size_t index = 0; index < slots.size(); ++index) {
			line += slots[index].object
				? std::format(
					  "[{}]{}x{} ",
					  KeyLabel(index),
					  RE::TESFullName::GetFullName(*slots[index].object),
					  slots[index].count)
				: std::format("[{}]- ", KeyLabel(index));
		}
		logger::info("favorites ({}): {}", a_reason, line);

		// The manager's copy alongside, because the two drifting apart is
		// the symptom of a write that went past the engine.
		if (const auto* manager = RE::FavoritesManager::GetSingleton()) {
			std::string cache;
			for (std::size_t index = 0; index < 12; ++index) {
				const auto* form = manager->storedFavTypes[index];
				cache += form
					? std::format(
						  "[{}]{} ",
						  KeyLabel(index),
						  RE::TESFullName::GetFullName(*form))
					: std::format("[{}]- ", KeyLabel(index));
			}
			logger::info("cache          : {}", cache);
		}
	}

	// ---- Making the cross catch up ---------------------------------------
	//
	// The cross draws from a copy it is handed when it opens, so a favorite
	// that moves underneath it changes nothing on screen until the menu is
	// closed and opened again. A page switch happens with the cross open, so
	// that is not something the mod can live with.
	//
	// FavoritesMenu.swf, decompiled with JPEXS, says where the copy sits:
	//
	//     public function set infoArray(a:Array) : *      // on Cross_mc
	//     {
	//        this._FavoritesInfoA = a;
	//        ... SetIsDirty();
	//     }
	//
	//     override public function redrawUIComponent() : void
	//     {
	//        ... entry.Icon_mc.gotoAndStop(info.FavIconType);
	//     }
	//
	// The setter is public and triggers the redraw itself. Every earlier
	// attempt failed because it asked the menu to redraw data nobody had
	// changed -- the data was missing, not the redraw.
	//
	// An entry is { FavIconType, text, count, ammoText, ammoCount }, and an
	// empty key is a null entry. FavIconType is a frame number in Icon_mc
	// whose meaning is not in the script. It is therefore never invented:
	// the frame showing on a cell right now is read off the screen and
	// remembered for the item standing there, so it can travel with the item
	// to its new key.

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

	// FIS (FallUI Item Sorter) renames items to "[Tag] Name" -- square
	// brackets by its own configuration -- and the menus of FallUI and
	// DEF_UI turn that tag into an icon from their icon library. The cross
	// is not one of those menus, so it prints the tag as it stands. Until
	// the grid can draw the icon itself, the tag is dropped from the name,
	// which is what the label under the cells is for.
	[[nodiscard]] std::string_view WithoutTag(std::string_view a_name)
	{
		if (!g_stripItemTags || a_name.empty() || a_name.front() != '[') {
			return a_name;
		}
		const auto close = a_name.find(']');
		if (close == std::string_view::npos) {
			return a_name;
		}
		auto rest = a_name.substr(close + 1);
		while (!rest.empty() && rest.front() == ' ') {
			rest.remove_prefix(1);
		}
		return rest.empty() ? a_name : rest;
	}

	// Frame 1 is the empty icon, so an item nobody has seen yet draws a
	// blank cell rather than the wrong picture.
	inline constexpr double kEmptyIcon = 1.0;

	// What icon an item draws with. Learned from the screen, never guessed,
	// and kept for the whole session -- an item on another page was on the
	// cross when that page was showing.
	std::unordered_map<RE::TESBoundObject*, double> g_iconOfObject;

	// An open menu with a usable movie, by name.
	[[nodiscard]] RE::IMenu* GetMenu(std::string_view a_name)
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return nullptr;
		}
		const RE::BSFixedString menuName{ a_name };
		const auto menu = ui->GetMenu(menuName);
		// A movie is enough. Our own menu has no named clip -- there is
		// nothing in its movie to name -- so requiring menuObj here is what
		// made it look shut while it was open.
		if (!menu || !menu->uiMovie) {
			return nullptr;
		}
		return menu.get();
	}

	// The open cross, or nothing.
	[[nodiscard]] RE::IMenu* GetFavoritesMenu()
	{
		return GetMenu("FavoritesMenu");
	}

	[[nodiscard]] bool GetCross(RE::IMenu* a_menu, RE::Scaleform::GFx::Value& a_cross)
	{
		return a_menu->menuObj.GetMember("Cross_mc", &a_cross) &&
			a_cross.IsObject();
	}

	// Reads the twelve icons off the screen and files them under the items
	// standing there. Has to run while the display still agrees with the
	// inventory -- so before a change, not after it.
	void LearnIcons()
	{
		auto* menu = GetFavoritesMenu();
		if (!menu) {
			return;
		}
		RE::Scaleform::GFx::Value cross;
		if (!GetCross(menu, cross)) {
			return;
		}

		const auto slots = ReadFavorites();
		for (std::size_t index = 0; index < slots.size(); ++index) {
			if (!slots[index].object) {
				continue;
			}
			const RE::Scaleform::GFx::Value argument{ static_cast<int>(index) };
			RE::Scaleform::GFx::Value entry;
			RE::Scaleform::GFx::Value icon;
			if (cross.Invoke("GetEntryClip", &entry, &argument, 1) &&
				entry.IsObject() && entry.GetMember("Icon_mc", &icon)) {
				const auto frame = ReadNumber(icon, "currentFrame", kEmptyIcon);
				if (frame != kEmptyIcon) {
					g_iconOfObject[slots[index].object] = frame;
				}
			}
		}
	}

	// Hands the cross a fresh list built from the inventory. Quiet when the
	// menu is closed: then there is nothing to catch up, and the next open
	// brings the current state anyway.
	void RefreshCross()
	{
		auto* menu = GetFavoritesMenu();
		if (!menu) {
			return;
		}
		RE::Scaleform::GFx::Value cross;
		if (!GetCross(menu, cross)) {
			logger::warn("cross: no Cross_mc");
			return;
		}

		const auto slots = ReadFavorites();

		RE::Scaleform::GFx::Value array;
		menu->uiMovie->CreateArray(&array);
		std::string written;
		for (std::size_t index = 0; index < slots.size(); ++index) {
			auto* object = slots[index].object;
			if (!object) {
				// An empty key is a null entry; redrawUIComponent checks
				// for exactly that and parks the icon on frame 1.
				array.PushBack(RE::Scaleform::GFx::Value(nullptr));
				written += std::format("[{}]- ", KeyLabel(index));
				continue;
			}

			const auto found = g_iconOfObject.find(object);
			const auto frame = found != g_iconOfObject.end() ? found->second
															 : kEmptyIcon;
			const auto name =
				std::string(WithoutTag(RE::TESFullName::GetFullName(*object)));

			RE::Scaleform::GFx::Value entry;
			menu->uiMovie->CreateObject(&entry);
			entry.SetMember("FavIconType", RE::Scaleform::GFx::Value(frame));
			entry.SetMember("text", RE::Scaleform::GFx::Value(name.c_str()));
			entry.SetMember(
				"count", RE::Scaleform::GFx::Value(slots[index].count));
			array.PushBack(entry);

			written += std::format("[{}]{}/{} ", KeyLabel(index), name, frame);
		}

		if (!cross.SetMember("infoArray", array)) {
			logger::warn("cross: the infoArray setter was refused");
			return;
		}
		logger::info("cross: rewritten from the inventory -- {}", written);
	}

	// ---- Writing a favorite, the way the game does -----------------------
	//
	// Read out of the running game with tools/f4dis.py, after two attempts
	// that wrote past the engine and cost an item each.
	//
	// The engine's own write is three instructions: take the index out of
	// the functor, take the ExtraDataList off the stack, and hand both to
	//
	//     ExtraDataList::SetFavorite(list, index)      REL::ID(534268)
	//
	// which reads:
	//
	//     0xFE          take the ExtraFavorite away -- not a favorite at all
	//     anything else set quickkeyIndex, creating the ExtraFavorite if the
	//                   stack has none
	//
	// So 0xFF is not "no favorite" but "a favorite without a key", and that
	// is the state the game itself writes when you favorite something that
	// has no key yet. A page switch therefore never has to delete anything:
	// the outgoing page parks at -1 and stays favorited.
	//
	// The other half is who does the writing. BGSInventoryItem::SetFavoriteIndex
	// (REL::ID(1349090)) is the game's own caller, and it was tried first --
	// it writes correctly, but the display and FavoritesManager::storedFavTypes
	// stayed on the old state, because it only notifies when the write
	// changed the shape of the stacks. What does reach everyone is the path
	// through the list:
	//
	//     BGSInventoryList::FindAndWriteStackDataForItem(object, compare, write)
	//
	// which dispatches the inventory's own event afterwards. That is what
	// made moving work in the first place, so the write goes through it and
	// only the functor underneath is the engine's.

	class SetFavoriteFunctor : public RE::BGSInventoryItem::StackDataWriteFunctor
	{
	public:
		explicit SetFavoriteFunctor(std::uint8_t a_index) noexcept :
			index(a_index)
		{
			// The engine's own functor sets both of these to false in its
			// constructor: the whole stack moves, nothing is split off.
			shouldSplitStacks = false;
			transferEquippedToSplitStack = false;
		}

		void WriteDataImpl(
			RE::TESBoundObject&,
			RE::BGSInventoryItem::Stack& a_stack) override
		{
			if (!a_stack.extra) {
				return;
			}
			using func_t = void (*)(RE::ExtraDataList*, std::uint8_t);
			static REL::Relocation<func_t> setFavorite{ REL::ID(534268) };
			setFavorite(a_stack.extra.get(), index);
		}

		std::uint8_t index;
	};

	class MatchFavoriteFunctor :
		public RE::BGSInventoryItem::StackDataCompareFunctor
	{
	public:
		explicit MatchFavoriteFunctor(std::uint8_t a_index) noexcept :
			index(a_index)
		{}

		bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
		{
			return FavoriteOf(a_stack) == index;
		}

		std::uint8_t index;
	};

	// Writes one favorite: the stack of a_object that carries a_from ends up
	// carrying a_to. Both are keys 0..11, kNoKey for a parked favorite or
	// kNotAFavorite for a stack that has none.
	bool WriteFavorite(
		RE::TESBoundObject* a_object,
		std::uint8_t a_from,
		std::uint8_t a_to)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList || !a_object) {
			return false;
		}

		MatchFavoriteFunctor compare{ a_from };
		SetFavoriteFunctor write{ a_to };
		player->inventoryList->FindAndWriteStackDataForItem(
			a_object, compare, write);
		return true;
	}

	[[nodiscard]] std::string KeyName(std::uint8_t a_index)
	{
		if (a_index < 12) {
			return KeyLabel(a_index);
		}
		return a_index == kNoKey ? "no key" : "not a favorite";
	}

	// Moves the favorite on one key to another, or off the keys entirely
	// when a_to is negative.
	bool MoveFavorite(RE::TESBoundObject* a_object, int a_from, int a_to)
	{
		if (a_from < 0 || a_from >= 12 || !a_object) {
			return false;
		}

		const auto to = a_to < 0 ? kNoKey : static_cast<std::uint8_t>(a_to);
		if (!WriteFavorite(a_object, static_cast<std::uint8_t>(a_from), to)) {
			return false;
		}

		logger::info(
			"move: \"{}\" {} -> {}",
			RE::TESFullName::GetFullName(*a_object),
			KeyLabel(static_cast<std::size_t>(a_from)),
			KeyName(to));
		return true;
	}

	// ---- Setting all twelve keys at once ---------------------------------
	//
	// Since -1 turned out to be a state of its own (section 16), a page
	// switch has no puzzle left in it. The first pass parks all twelve
	// favorites, which frees every key without anything losing its favorite
	// status; the second hands the keys to the items of the new page. No
	// rings to break, no parking spot to find, and an interrupted switch
	// leaves favorites without keys rather than anything broken.
	//
	// What came before this -- ordering the moves so that a key is only ever
	// written when nobody needs it any more -- is in the history, and in
	// section 15 of the HANDOFF. It worked, but it could only rearrange what
	// was already on the twelve keys, and a page holds items that are not.

	using Page = std::array<RE::TESBoundObject*, 12>;

	// Does this item have a stack carrying that index?
	[[nodiscard]] bool Carries(RE::TESBoundObject* a_object, std::uint8_t a_index)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList || !a_object) {
			return false;
		}

		bool found = false;
		player->inventoryList->ForEachStack(
			[&](RE::BGSInventoryItem& a_item) { return a_item.object == a_object; },
			[&](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack& a_stack) {
				if (FavoriteOf(a_stack) != a_index) {
					return true;
				}
				found = true;
				return false;
			});
		return found;
	}

	// Which stack of this item can be given a key: a parked one first, then
	// one that is no favorite at all. Nothing means the item is not in the
	// inventory any more.
	[[nodiscard]] std::optional<std::uint8_t> FindFree(RE::TESBoundObject* a_object)
	{
		for (const auto carried : { kNoKey, kNotAFavorite }) {
			if (Carries(a_object, carried)) {
				return carried;
			}
		}
		return std::nullopt;
	}

	// Applies a whole arrangement: afterwards the item named in slot i holds
	// key i. Items the target does not name keep their favorite and lose
	// their key -- which is exactly what the items of the outgoing page are
	// supposed to do.
	void ApplyPage(const Page& a_target)
	{
		// While the display still agrees with the inventory.
		LearnIcons();

		const auto current = ReadFavorites();
		for (std::size_t key = 0; key < current.size(); ++key) {
			if (current[key].object) {
				MoveFavorite(current[key].object, static_cast<int>(key), -1);
			}
		}

		for (std::size_t slot = 0; slot < a_target.size(); ++slot) {
			auto* object = a_target[slot];
			if (!object) {
				continue;
			}
			const auto from = FindFree(object);
			if (!from) {
				logger::warn(
					"page: \"{}\" is not in the inventory any more -- [{}] stays empty",
					RE::TESFullName::GetFullName(*object),
					KeyLabel(slot));
				continue;
			}
			WriteFavorite(object, *from, static_cast<std::uint8_t>(slot));
		}

		RefreshCross();
		LogFavorites("after the page");
	}

	// The test bench: every favorite moves up one key and the topmost one
	// wraps around. Nothing a page switch needs, but it exercises the whole
	// of ApplyPage in one press and enough presses bring everything back.
	void RotateFavorites()
	{
		const auto current = ReadFavorites();

		std::vector<std::size_t> occupied;
		for (std::size_t key = 0; key < current.size(); ++key) {
			if (current[key].object) {
				occupied.push_back(key);
			}
		}
		if (occupied.size() < 2) {
			logger::warn("rotate: fewer than two favorites");
			return;
		}

		Page target{};
		for (std::size_t index = 0; index < occupied.size(); ++index) {
			target[occupied[(index + 1) % occupied.size()]] =
				current[occupied[index]].object;
		}
		ApplyPage(target);
	}

	// ---- The pages themselves --------------------------------------------
	//
	// A page is twelve items. The one being played is not kept in the list
	// while it is in use -- the inventory holds it, and the player can
	// change it at any time through the Pip-Boy -- so it is read back from
	// the twelve keys before the page is left. That way a favorite assigned
	// by hand belongs to the page it was assigned on.

	std::vector<Page> g_pages;
	std::size_t g_currentPage = 0;

	// Whether the favorites menu is open, for the input thread to read. The
	// UI's own answer needs the UI thread.
	std::atomic_bool g_favoritesMenuOpen{ false };

	// The cell the grid is pointing at. Empty until the pointer finds one or
	// a key is pressed: a menu that opens with something already chosen
	// invites using it by accident.
	std::optional<grid::Spot> g_marked;

	// The cell that has been picked up and is waiting to be put down.
	std::optional<grid::Spot> g_held;

	// Defined below, with the rest of the page marker: they need the pages,
	// and the pages need to show themselves.
	void ShowPageIndicator();
	void ShowGrid();

	// Defined with the rest of the choosing, further down: the panel has to
	// be able to say what is marked while it is being drawn, and the marking
	// needs the pages that are declared here.
	[[nodiscard]] detail::Lines Describe(const std::optional<grid::Spot>& a_spot);

	void EnsurePages()
	{
		if (g_pages.size() != static_cast<std::size_t>(g_pageCount)) {
			g_pages.resize(static_cast<std::size_t>(g_pageCount));
		}
		if (g_currentPage >= g_pages.size()) {
			g_currentPage = 0;
		}
	}

	// Writes the twelve keys as they are now into the page being played.
	void RememberCurrentPage()
	{
		EnsurePages();
		const auto current = ReadFavorites();
		auto& page = g_pages[g_currentPage];
		for (std::size_t key = 0; key < current.size(); ++key) {
			page[key] = current[key].object;
		}
	}

	// Says which page is being played, in the game's own corner message.
	void AnnouncePage()
	{
		// With the grid up the panel says which page is which by showing all
		// of them, so nothing has to be announced. Everywhere else -- the
		// Pip-Boy above all, where a favorite is assigned into whichever page
		// the engine holds -- the player has no way at all to see it, and a
		// silent page switch there is a trap. So the corner message is on by
		// default in exactly that case.
		auto lead = g_pageMessage;
		if (lead.empty()) {
			if (g_useGrid && g_favoritesMenuOpen.load()) {
				return;
			}
			lead = g_indicatorText.empty() ? "Favorites" : g_indicatorText;
		}
		const auto line =
			std::format("{} {} / {}", lead, g_currentPage + 1, g_pages.size());
		RE::SendHUDMessage::ShowHUDMessage(line.c_str(), nullptr, false, false);
	}

	void GoToPage(std::size_t a_page)
	{
		EnsurePages();
		if (a_page >= g_pages.size()) {
			return;
		}
		if (a_page == g_currentPage) {
			logger::info("page {} is already the one being played", a_page + 1);
			return;
		}

		RememberCurrentPage();
		const auto target = g_pages[a_page];
		g_currentPage = a_page;

		logger::info("page: switching to {} of {}", a_page + 1, g_pages.size());
		ApplyPage(target);
		ShowPageIndicator();
		ShowGrid();
		AnnouncePage();
	}

	void TurnPage(int a_by)
	{
		EnsurePages();
		const auto count = static_cast<int>(g_pages.size());
		if (count < 2) {
			logger::warn("page: there is only one page");
			return;
		}
		const auto next = (static_cast<int>(g_currentPage) + a_by % count + count) % count;
		GoToPage(static_cast<std::size_t>(next));
	}

	// ---- Saying which page is showing ------------------------------------
	//
	// Not through the game's corner notification. Players rebuild the HUD to
	// taste -- FallUI and friends move, restyle and hide those messages --
	// and the favorites menu is the one place that usually stays as it is.
	// So the page is written into the menu itself, as a text field hung on
	// Cross_mc. Hanging it there rather than on the stage means it follows
	// the cross wherever another mod has put it.

	RE::Scaleform::GFx::Value g_indicator;

	// The colour the player set for the HUD, so the page looks like it
	// belongs to the game rather than to us.
	[[nodiscard]] std::uint32_t HUDColor()
	{
		const auto channel = [](const char* a_name, std::uint32_t a_fallback) {
			auto* collection = RE::INIPrefSettingCollection::GetSingleton();
			const auto setting = collection ? collection->GetSetting(a_name) : nullptr;
			if (!setting ||
				setting->GetType() != RE::Setting::SETTING_TYPE::kInt) {
				return a_fallback;
			}
			return static_cast<std::uint32_t>(
				std::clamp(setting->GetInt(), 0, 255));
		};

		return (channel("iHUDColorR:Interface", 0x12) << 16) |
			(channel("iHUDColorG:Interface", 0xFF) << 8) |
			channel("iHUDColorB:Interface", 0x7D);
	}

	[[nodiscard]] std::string PageWording(std::string_view a_lead)
	{
		if (a_lead.empty()) {
			return std::format("{} / {}", g_currentPage + 1, g_pages.size());
		}
		return std::format(
			"{} {} / {}", a_lead, g_currentPage + 1, g_pages.size());
	}

	void ReleaseIndicator()
	{
		g_indicator = RE::Scaleform::GFx::Value();
	}

	// Which font the cross draws its own key labels with. Whatever the menu
	// already uses is one it certainly has.
	[[nodiscard]] std::string CrossFont(RE::Scaleform::GFx::Value& a_cross)
	{
		const RE::Scaleform::GFx::Value first{ 0 };
		RE::Scaleform::GFx::Value entry;
		RE::Scaleform::GFx::Value label;
		RE::Scaleform::GFx::Value format;
		RE::Scaleform::GFx::Value font;

		if (a_cross.Invoke("GetEntryClip", &entry, &first, 1) && entry.IsObject() &&
			entry.GetMember("Quickkey_tf", &label) && label.IsObject() &&
			label.Invoke("getTextFormat", &format) && format.IsObject() &&
			format.GetMember("font", &font) && font.IsString()) {
			return font.GetString();
		}
		return {};
	}

	// Writes the text and gives it a font that is really there.
	//
	// This was learned twice. The Starfield version of this mod has the same
	// paragraph next to the same code: leaving the font unset falls back to
	// whatever Scaleform picks, and naming one the movie does not have draws
	// a row of boxes -- which is exactly what the first attempt looked like
	// on screen. So the choice is **measured**: a field with a usable font
	// reports a textWidth for a known string, one without reports nothing.
	//
	// The first candidate is the font the cross labels its own keys with,
	// which is the one answer that stays right when an interface mod brings
	// its own fonts along.
	bool DressField(
		RE::IMenu* a_menu,
		RE::Scaleform::GFx::Value& a_cross,
		RE::Scaleform::GFx::Value& a_field,
		const std::string& a_text)
	{
		std::vector<std::string> candidates;
		if (!g_indicatorFont.empty()) {
			candidates.push_back(g_indicatorFont);
		}
		if (auto own = CrossFont(a_cross); !own.empty()) {
			candidates.push_back(std::move(own));
		}
		// The names the game gives its own fonts in FontConfig.txt.
		candidates.emplace_back("$MAIN_Font_Bold");
		candidates.emplace_back("$MAIN_Font");
		// Last resort: whatever the movie falls back to on its own.
		candidates.emplace_back();

		for (const auto& font : candidates) {
			RE::Scaleform::GFx::Value format;
			a_menu->uiMovie->CreateObject(&format, "flash.text.TextFormat");
			if (!format.IsObject()) {
				return false;
			}
			if (!font.empty()) {
				format.SetMember("font", RE::Scaleform::GFx::Value(font.c_str()));
			}
			format.SetMember("size", RE::Scaleform::GFx::Value(g_indicatorSize));
			format.SetMember(
				"color",
				RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(
					g_indicatorColor <= 0xFFFFFF ? g_indicatorColor : HUDColor())));
			format.SetMember("align", RE::Scaleform::GFx::Value("center"));
			format.SetMember("bold", RE::Scaleform::GFx::Value(true));

			// The fonts of a menu are embedded in its movie, so a named one
			// only draws when the field is told to look there.
			a_field.SetMember(
				"embedFonts", RE::Scaleform::GFx::Value(!font.empty()));
			a_field.SetMember("text", RE::Scaleform::GFx::Value(a_text.c_str()));
			// After the text, not before: defaultTextFormat only reaches
			// what is typed afterwards.
			a_field.Invoke("setTextFormat", nullptr, &format, 1);

			const auto width = ReadNumber(a_field, "textWidth", 0.0);
			if (width > 0.0) {
				a_field.SetMember("defaultTextFormat", format);
				if (font != g_indicatorFontInUse) {
					logger::info(
						"indicator: drawing with {} ({:.1f} wide)",
						font.empty() ? "the movie's own fallback" : font,
						width);
					g_indicatorFontInUse = font;
				}
				return true;
			}
			logger::info(
				"indicator: {} draws nothing here",
				font.empty() ? "the movie's own fallback" : font);
		}
		return false;
	}

	// What the cross actually covers on the stage.
	//
	// Its own x and y are the origin of its coordinate space, and that origin
	// is not the middle of what you see -- measured in game it sits well
	// above the drawn cross, which is why the marker first landed beside the
	// upper cells instead of under them. getBounds answers where the thing
	// really is, whatever the symbol was built like.
	struct Bounds
	{
		double x{ 0.0 };
		double y{ 0.0 };
		double width{ 0.0 };
		double height{ 0.0 };
	};

	[[nodiscard]] Bounds CrossBounds(
		RE::Scaleform::GFx::Value& a_cross,
		RE::Scaleform::GFx::Value& a_stage)
	{
		Bounds bounds;

		RE::Scaleform::GFx::Value rect;
		if (a_cross.Invoke("getBounds", &rect, &a_stage, 1) && rect.IsObject()) {
			bounds.x = ReadNumber(rect, "x", 0.0);
			bounds.y = ReadNumber(rect, "y", 0.0);
			bounds.width = ReadNumber(rect, "width", 0.0);
			bounds.height = ReadNumber(rect, "height", 0.0);
		}
		if (bounds.width > 0.0 && bounds.height > 0.0) {
			return bounds;
		}

		// Nothing drawn yet, or a build that will not answer: the origin is
		// at least in the right corner of the screen.
		bounds.x = ReadNumber(a_cross, "x", 0.0);
		bounds.y = ReadNumber(a_cross, "y", 0.0);
		return bounds;
	}

	// Builds the field the first time and writes the page every time. Quiet
	// when the menu is closed -- there is nothing to write on.
	//
	// The field goes on the stage, not into Cross_mc. Hung on the cross it
	// looked right and cost the menu its exit: FavoritesMenu stayed open
	// from the moment the child was added. The cross counts on its children
	// being its own, so the marker is placed beside it instead -- converted
	// to stage coordinates, so it still follows wherever the cross sits.
	void ShowPageIndicator()
	{
		// With the grid up the page stands in its title, and the cross it
		// would be measured against is hidden anyway.
		if (!g_showPageIndicator || g_useGrid || g_pages.empty()) {
			return;
		}
		auto* menu = GetFavoritesMenu();
		if (!menu) {
			ReleaseIndicator();
			return;
		}
		RE::Scaleform::GFx::Value cross;
		if (!GetCross(menu, cross)) {
			return;
		}
		RE::Scaleform::GFx::Value stage;
		if (!menu->menuObj.GetMember("stage", &stage) ||
			!stage.IsDisplayObject()) {
			logger::warn("indicator: the menu has no stage");
			g_showPageIndicator = false;
			return;
		}

		if (!g_indicator.IsObject()) {
			menu->uiMovie->CreateObject(&g_indicator, "flash.text.TextField");
			if (!g_indicator.IsObject()) {
				logger::warn("indicator: the menu would not make a text field");
				g_showPageIndicator = false;
				return;
			}

			// It is a label, not a control: nothing about it should react to
			// the mouse or take focus away from the cross.
			g_indicator.SetMember("selectable", RE::Scaleform::GFx::Value(false));
			g_indicator.SetMember("mouseEnabled", RE::Scaleform::GFx::Value(false));
			g_indicator.SetMember("multiline", RE::Scaleform::GFx::Value(false));
			g_indicator.SetMember("wordWrap", RE::Scaleform::GFx::Value(false));

			// A fixed, generous box with the text centred in it, rather than
			// autoSize. Left to size itself the field came up cut off on the
			// first draw and only sorted itself out on the second, because
			// the box was still the default width when the text arrived.
			g_indicator.SetMember(
				"width", RE::Scaleform::GFx::Value(kIndicatorWidth));
			g_indicator.SetMember(
				"height", RE::Scaleform::GFx::Value(g_indicatorSize + 8.0));
			// The fonts of a menu are embedded in its movie, so the field
			// has to be told to use them rather than one from the system.
			g_indicator.SetMember("embedFonts", RE::Scaleform::GFx::Value(true));

			if (!stage.Invoke("addChild", nullptr, &g_indicator, 1)) {
				logger::warn("indicator: the stage would not take the field");
				ReleaseIndicator();
				g_showPageIndicator = false;
				return;
			}
			logger::info("indicator: added to the stage");
		}

		const auto bounds = CrossBounds(cross, stage);
		if (!g_boundsLogged) {
			logger::info(
				"indicator: the cross covers {:.0f},{:.0f} to {:.0f},{:.0f}",
				bounds.x,
				bounds.y,
				bounds.x + bounds.width,
				bounds.y + bounds.height);
			g_boundsLogged = true;
		}

		// Centred on the cross and, by default, just below it.
		g_indicator.SetMember(
			"x",
			RE::Scaleform::GFx::Value(
				bounds.x + bounds.width / 2.0 - kIndicatorWidth / 2.0 +
				g_indicatorX));
		g_indicator.SetMember(
			"y",
			RE::Scaleform::GFx::Value(bounds.y + bounds.height + g_indicatorY));

		if (!DressField(menu, cross, g_indicator, PageWording(g_indicatorText))) {
			logger::warn("indicator: no font in this menu draws anything");
			g_showPageIndicator = false;
		}
	}

	// ---- The grid --------------------------------------------------------
	//
	// The page being played is not read out of the page list -- it lives in
	// the inventory, where the player may have changed it since. Every other
	// row comes from the list.
	// What a thing gets when neither its name nor the sorter's own auto-tagging
	// knows it. A mod-added weapon is nothing FIS has ever heard of, and a
	// blank cell says less than a plain one saying "gun" -- the player knows
	// what their own favorites are, they only need to find them again.
	//
	// Every keyword here is one FIS defines itself, so the artwork matches the
	// rest of the grid rather than being a second style.
	[[nodiscard]] std::string_view FallbackKeyword(RE::TESBoundObject* a_object)
	{
		switch (a_object->GetFormType()) {
		case RE::ENUM_FORM_ID::kWEAP:
			{
				auto* weapon = a_object->As<RE::TESObjectWEAP>();
				if (!weapon) {
					return {};
				}
				switch (weapon->weaponData.type.get()) {
				case RE::WEAPON_TYPE::kHandToHand:
					return "Unarmed";
				case RE::WEAPON_TYPE::kOneHandSword:
				case RE::WEAPON_TYPE::kOneHandDagger:
				case RE::WEAPON_TYPE::kOneHandAxe:
				case RE::WEAPON_TYPE::kOneHandMace:
					return "MeleeOneHand";
				case RE::WEAPON_TYPE::kTwoHandSword:
				case RE::WEAPON_TYPE::kTwoHandAxe:
				case RE::WEAPON_TYPE::kBow:
				case RE::WEAPON_TYPE::kStaff:
					return "MeleeTwoHand";
				case RE::WEAPON_TYPE::kGrenade:
					return "Grenade";
				case RE::WEAPON_TYPE::kMine:
					return "Mine";
				case RE::WEAPON_TYPE::kGun:
				default:
					// The engine knows guns as one kind, with nothing in it
					// that separates a pistol from a rifle, so this is the
					// rougher half of an already rough answer.
					return "Rifle";
				}
			}

		case RE::ENUM_FORM_ID::kARMO:
			{
				auto* armor = a_object->As<RE::TESObjectARMO>();
				return armor && armor->armorData.rating > 0 ? "Armor" : "Clothes";
			}

		case RE::ENUM_FORM_ID::kALCH:
			return "Aid";
		case RE::ENUM_FORM_ID::kAMMO:
			return "Ammo";
		case RE::ENUM_FORM_ID::kNOTE:
			return "Note";
		default:
			return {};
		}
	}

	// The line under the panel, built from the keys as they are actually
	// bound rather than from what they were bound to when this was written.
	[[nodiscard]] std::string BuildHint()
	{
		if (!g_showHint) {
			return {};
		}

		std::string line;
		const auto add = [&line](const std::string& a_key, std::string_view a_what) {
			if (a_key.empty()) {
				return;
			}
			if (!line.empty()) {
				line += "      ";
			}
			line += std::format("{}) {}", a_key, a_what);
		};

		// Walking first: it is the one thing a player will try without being
		// told, and seeing it named says the rest of the line is trustworthy.
		const auto up = KeyName(g_gridKeys.pageUp);
		const auto left = KeyName(g_gridKeys.slotLeft);
		const auto down = KeyName(g_gridKeys.pageDown);
		const auto right = KeyName(g_gridKeys.slotRight);
		if (!up.empty() && !left.empty() && !down.empty() && !right.empty()) {
			add(up + left + down + right, "MOVE");
		}

		add(KeyName(g_gridKeys.use), "USE");
		add(KeyName(g_gridKeys.move), "PICK UP");
		add(KeyName(g_gridKeys.clear), "CLEAR");

		if (!g_hintExtra.empty()) {
			if (!line.empty()) {
				line += "      ";
			}
			line += g_hintExtra;
		}
		return line;
	}

	// Which icon libraries the page being drawn actually needs. Only these
	// are asked for: a player with a dozen addon libraries installed has no
	// use for eleven of them on any given screen.
	std::set<std::string> g_wantedLibraries;

	[[nodiscard]] std::vector<grid::Page> BuildGridPages()
	{
		EnsurePages();
		const auto live = ReadFavorites();
		g_wantedLibraries.clear();

		std::vector<grid::Page> rows(g_pages.size());
		for (std::size_t row = 0; row < g_pages.size(); ++row) {
			for (std::size_t slot = 0; slot < 12; ++slot) {
				auto* object = row == g_currentPage ? live[slot].object
													: g_pages[row][slot];
				auto& cell = rows[row][slot];
				cell.label = KeyLabel(slot);
				if (!object) {
					continue;
				}

				// The whole name first: the tag in front of it is what says
				// which icon this is, and stripping it is the last step, not
				// the first.
				const std::string full{ RE::TESFullName::GetFullName(*object) };
				cell.name = std::string(WithoutTag(full));
				if (!g_useIcons) {
					continue;
				}
				// The tag in the name first; where a sorter never renamed the
				// thing -- which is most of a heavily modded game -- the same
				// answer the sorter's own auto-tagging would give.
				auto keyword = tags::KeywordOf(full);
				if (keyword.empty()) {
					keyword = tags::AutoKeywordOf(full, object->GetFormType());
				}
				if (keyword.empty() && g_iconFallback) {
					keyword = FallbackKeyword(object);
				}
				if (const auto* icon = tags::Find(keyword)) {
					// The "m_" is the only translation between what the
					// configuration writes and what the library exports.
					cell.symbol = "m_" + icon->symbol;
					cell.color = icon->color;
					if (!icon->library.empty()) {
						g_wantedLibraries.insert(icon->library);
					}
				}
			}
		}
		return rows;
	}

	void ShowGrid()
	{
		if (!g_useGrid) {
			return;
		}
		auto* menu = GetFavoritesMenu();
		if (!menu) {
			grid::Release();
			return;
		}
		RE::Scaleform::GFx::Value cross;
		if (!GetCross(menu, cross)) {
			return;
		}

		// Whatever the marker measured, or the cross's own font if the
		// marker is switched off and nothing has been measured yet. Reported
		// once: everything on the panel is written in it, and a font that
		// quietly fell back to the player's default would look like a design
		// decision rather than a miss.
		auto font = g_indicatorFontInUse;
		if (font == "?") {
			font = CrossFont(cross);
		}
		if (!g_gridFont.empty()) {
			font = g_gridFont;
		}
		static bool saidFont = false;
		if (!saidFont) {
			saidFont = true;
			logger::info(
				"grid: everything is written in \"{}\", taken from {}",
				font,
				g_gridFont.empty() ? "the cross's own key labels" : "the INI");
		}

		// The keys, so the line under the panel can name them.
		g_gridWhere.hint = BuildHint();

		// Drawn on the HUD by default: the favorites menu only paints a
		// strip around its own cross, so anything of ours outside that never
		// reaches the screen.
		auto* canvas = GetMenu(g_gridWhere.canvas);
		if (!canvas) {
			logger::warn("grid: {} is not open to draw on", g_gridWhere.canvas);
			return;
		}

		const auto pages = BuildGridPages();
		for (const auto& library : g_wantedLibraries) {
			icons::Want(canvas, library);
		}

		// Above the keys stands what the mark is on, not a title: the word
		// "Favorites" over a grid of favorites said nothing the grid does
		// not, and a page number would count from a "here" that no longer
		// exists now that every page is drawn alike.
		grid::Draw(
			canvas,
			menu,
			font,
			pages,
			g_marked,
			g_gridColor <= 0xFFFFFF ? g_gridColor : HUDColor(),
			g_gridWhere);

		// The panel was built from scratch, so what was said above it and
		// what was being carried have to be said and shown again.
		const auto lines = Describe(g_marked);
		grid::Say(lines.name, lines.what);
		grid::Hold(g_held);
	}

	// ---- The crosshair steps aside ---------------------------------------
	//
	// It sits in the middle of the screen, which is where the grid is, and it
	// aims at nothing while a menu is open. The HUD is not ours and is not
	// asked to change: one clip is put out of sight and put back, the same
	// way the cross is.

	RE::Scaleform::GFx::Value g_hiddenCrosshair;

	// The vanilla HUD holds HUDCrosshair_mc at its root. A modded one does
	// not: this machine's holds SafeRect_mc, four groups, an
	// HUDMenuFwCore of M8r's framework and three nameless instances -- and
	// the crosshair somewhere inside one of them. So it is looked for rather
	// than reached for, by name, a few levels down. Anything deeper than this
	// is not a HUD any more.
	constexpr int kCrosshairDepth = 4;

	[[nodiscard]] bool FindByName(
		RE::Scaleform::GFx::Value& a_where,
		const char* a_name,
		int a_depth,
		RE::Scaleform::GFx::Value& a_found,
		std::string& a_path)
	{
		if (a_depth < 0 || !a_where.IsObject()) {
			return false;
		}
		if (a_where.GetMember(a_name, &a_found) && a_found.IsDisplayObject()) {
			a_path += std::format(".{}", a_name);
			return true;
		}

		RE::Scaleform::GFx::Value count;
		if (!a_where.GetMember("numChildren", &count)) {
			return false;
		}
		const auto total =
			count.IsNumber() ? static_cast<int>(count.GetNumber()) : count.GetInt();
		for (int index = 0; index < total; ++index) {
			const RE::Scaleform::GFx::Value at{ index };
			RE::Scaleform::GFx::Value child;
			if (!a_where.Invoke("getChildAt", &child, &at, 1) ||
				!child.IsDisplayObject()) {
				continue;
			}
			RE::Scaleform::GFx::Value name;
			const auto step = child.GetMember("name", &name) && name.IsString()
				? std::format(".{}", name.GetString())
				: std::format("[{}]", index);
			auto below = a_path + step;
			if (FindByName(child, a_name, a_depth - 1, a_found, below)) {
				a_path = below;
				return true;
			}
		}
		return false;
	}

	void HideCrosshair()
	{
		if (!g_hideCrosshair || g_hiddenCrosshair.IsDisplayObject()) {
			return;
		}
		auto* hud = GetMenu("HUDMenu");
		if (!hud || !hud->menuObj.IsObject()) {
			return;
		}

		std::string path = "HUDMenu";
		if (FindByName(
				hud->menuObj,
				"HUDCrosshair_mc",
				kCrosshairDepth,
				g_hiddenCrosshair,
				path)) {
			g_hiddenCrosshair.SetMember("visible", RE::Scaleform::GFx::Value(false));
			static bool said = false;
			if (!said) {
				said = true;
				logger::info("crosshair: found at {}", path);
			}
			return;
		}

		g_hiddenCrosshair = RE::Scaleform::GFx::Value();
		static bool listed = false;
		if (listed) {
			return;
		}
		listed = true;
		logger::info(
			"crosshair: no HUDCrosshair_mc within {} levels of the HUD -- it "
			"stays",
			kCrosshairDepth);
	}

	// Once is not enough on a machine where several mods have an opinion
	// about the crosshair: one of them puts it back, and it is back. So it is
	// pushed down again on every frame the menu is up -- one write, and only
	// when something else has undone the last one.
	void KeepCrosshairDown()
	{
		if (!g_hiddenCrosshair.IsDisplayObject()) {
			return;
		}
		RE::Scaleform::GFx::Value shown;
		if (g_hiddenCrosshair.GetMember("visible", &shown) && shown.IsBoolean() &&
			!shown.GetBoolean()) {
			return;
		}
		g_hiddenCrosshair.SetMember("visible", RE::Scaleform::GFx::Value(false));
	}

	void ShowCrosshair()
	{
		if (g_hiddenCrosshair.IsDisplayObject()) {
			g_hiddenCrosshair.SetMember("visible", RE::Scaleform::GFx::Value(true));
		}
		g_hiddenCrosshair = RE::Scaleform::GFx::Value();
	}

	// ---- Choosing a cell -------------------------------------------------
	//
	// Two ways to the same mark: the pointer, which the menu carries because
	// it asked for a cursor, and the keys, which walk from wherever the mark
	// stands. They do not fight over it -- whichever moved last has it -- and
	// the pointer only ever speaks when it is over a cell, so leaving the
	// panel with the mouse does not throw away what the keys chose.

	// The last place the pointer was seen, so a mouse lying still does not
	// overwrite a choice made with the keys sixty times a second.
	double g_pointerX = std::numeric_limits<double>::lowest();
	double g_pointerY = std::numeric_limits<double>::lowest();

	void ForgetPointer()
	{
		g_pointerX = std::numeric_limits<double>::lowest();
		g_pointerY = std::numeric_limits<double>::lowest();
	}

	// What the marked cell holds, in the two lines the game itself uses:
	// the name, and under it what the thing does. Everything but the lookup
	// lives in detail.cpp -- a weapon with mods on it is a different weapon
	// from the one in the plugin, and that is its business, not this file's.
	[[nodiscard]] detail::Lines Describe(const std::optional<grid::Spot>& a_spot)
	{
		if (!a_spot) {
			return {};
		}
		EnsurePages();
		if (a_spot->page >= g_pages.size() || a_spot->slot >= 12) {
			return {};
		}

		// The page being played lives in the inventory, not in the list.
		const auto live = ReadFavorites();
		auto* object = a_spot->page == g_currentPage
			? live[a_spot->slot].object
			: g_pages[a_spot->page][a_spot->slot];
		return detail::Describe(object, g_stripItemTags);
	}

	void SetMark(const std::optional<grid::Spot>& a_spot)
	{
		g_marked = a_spot;
		grid::Mark(g_marked);
		const auto lines = Describe(g_marked);
		grid::Say(lines.name, lines.what);
	}

	// Every frame the grid is up.
	void TrackPointer()
	{
		KeepCrosshairDown();

		// The library arrives some frames after it was asked for, and the
		// panel was drawn before that. So when it lands, the panel is drawn
		// again -- once -- and this time its cells can carry symbols.
		if (auto* canvas = GetMenu(g_gridWhere.canvas)) {
			icons::Poll(canvas, []() { ShowGrid(); });
		}

		if (!g_useGrid) {
			return;
		}
		auto* canvas = GetMenu(g_gridWhere.canvas);
		if (!canvas) {
			return;
		}

		double x = 0.0;
		double y = 0.0;
		if (!grid::Pointer(canvas, x, y) || (x == g_pointerX && y == g_pointerY)) {
			return;
		}
		g_pointerX = x;
		g_pointerY = y;

		const auto over = grid::At(x, y);
		if (!over || over == g_marked) {
			return;
		}
		SetMark(over);
	}

	// One step with the keys. Wrapping around in both directions: twelve
	// keys and three pages are a ring, not a page of text, and a mark that
	// stops at the edge means reaching for the mouse.
	void MoveMark(int a_pages, int a_slots)
	{
		EnsurePages();
		if (g_pages.empty()) {
			return;
		}

		// The first press lands on the key the player is already playing
		// rather than somewhere they have to look for.
		const auto from = g_marked.value_or(grid::Spot{ g_currentPage, 0 });
		const auto rows = static_cast<int>(g_pages.size());
		constexpr int slots = 12;

		const auto step = [](int a_from, int a_by, int a_count, bool a_wrap) {
			const auto to = a_from + a_by;
			if (a_wrap) {
				return (to % a_count + a_count) % a_count;
			}
			return std::clamp(to, 0, a_count - 1);
		};

		const grid::Spot to{
			static_cast<std::size_t>(
				step(static_cast<int>(from.page), a_pages, rows, g_wrapNavigation)),
			static_cast<std::size_t>(
				step(static_cast<int>(from.slot), a_slots, slots, g_wrapNavigation))
		};

		SetMark(to);
		// The keys have the mark now; the mouse takes it back by moving.
		ForgetPointer();
	}

	// Using what is marked. A cell on another page is used by going there
	// first -- the engine is what hands out the twelve keys, and it only
	// ever hands out one page of them.
	void UseMarked()
	{
		if (!g_marked) {
			logger::info("use: nothing is marked");
			return;
		}
		EnsurePages();

		const auto spot = *g_marked;
		if (spot.page >= g_pages.size() || spot.slot >= 12) {
			return;
		}

		// The page being played is only in the inventory, so it is read back
		// before anything on it is looked up.
		RememberCurrentPage();
		auto* object = g_pages[spot.page][spot.slot];
		if (!object) {
			logger::info(
				"use: [{}] on page {} holds nothing",
				KeyLabel(spot.slot),
				spot.page + 1);
			return;
		}

		if (!use::Ready()) {
			logger::warn(
				"use: the engine's own UseQuickkeyItem was never found -- "
				"\"{}\" stays where it is",
				RE::TESFullName::GetFullName(*object));
			return;
		}

		if (spot.page != g_currentPage) {
			GoToPage(spot.page);
		}

		const auto used = use::Quickkey(static_cast<std::uint32_t>(spot.slot));
		logger::info(
			"use: [{}] \"{}\" on page {} -- the game {}",
			KeyLabel(spot.slot),
			RE::TESFullName::GetFullName(*object),
			spot.page + 1,
			used ? "used it" : "would not");

		// A refusal leaves the menu open. The game has just said no out
		// loud, and closing on top of that would look like something
		// happened.
		if (g_closeAfterUse && used) {
			if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage("FavoritesMenu", RE::UI_MESSAGE_TYPE::kHide);
			}
		}
	}

	// Picks the marked cell up, or puts the held one down on the marked one.
	//
	// Both ends go through the page list rather than through the inventory:
	// the list is where a page that is not being played exists at all, and
	// the page that is being played is written back from it afterwards by
	// ApplyPage -- the one operation in this plugin that has been exercised
	// enough to trust. Two special cases disappear that way, and with them
	// the chance of the two halves disagreeing.
	void MoveMarked()
	{
		if (!g_marked) {
			return;
		}
		EnsurePages();
		const auto here = *g_marked;
		if (here.page >= g_pages.size() || here.slot >= 12) {
			return;
		}

		RememberCurrentPage();

		if (!g_held) {
			if (!g_pages[here.page][here.slot]) {
				logger::info("move: nothing on that key to pick up");
				return;
			}
			g_held = here;
			grid::Hold(g_held);
			logger::info(
				"move: holding \"{}\" from [{}] on page {}",
				RE::TESFullName::GetFullName(*g_pages[here.page][here.slot]),
				KeyLabel(here.slot),
				here.page + 1);
			return;
		}

		const auto from = *g_held;
		g_held.reset();
		grid::Hold(g_held);
		if (from == here) {
			logger::info("move: put back where it was");
			return;
		}

		// An exchange, not an insertion: whatever was on the target key goes
		// to where the held one came from. Anything else would have to push
		// a third item somewhere, and there is no somewhere.
		std::swap(g_pages[from.page][from.slot], g_pages[here.page][here.slot]);
		logger::info(
			"move: [{}] on page {} and [{}] on page {} have changed places",
			KeyLabel(from.slot),
			from.page + 1,
			KeyLabel(here.slot),
			here.page + 1);

		// Only the page in the engine's keys has to be written out; the
		// others are the list itself.
		if (from.page == g_currentPage || here.page == g_currentPage) {
			ApplyPage(g_pages[g_currentPage]);
		}
		ShowGrid();
	}

	// Frees the key the mark sits on. Nothing is deleted and nothing is
	// unfavorited: the item goes to the state the game itself writes for a
	// favorite with no digit, which is what "this key is free" means here.
	void ClearMarked()
	{
		if (!g_marked) {
			return;
		}
		EnsurePages();

		const auto spot = *g_marked;
		if (spot.page >= g_pages.size() || spot.slot >= 12) {
			return;
		}

		RememberCurrentPage();
		auto* object = g_pages[spot.page][spot.slot];
		if (!object) {
			return;
		}

		logger::info(
			"clear: [{}] on page {} held \"{}\"",
			KeyLabel(spot.slot),
			spot.page + 1,
			RE::TESFullName::GetFullName(*object));

		// The page being played holds its keys in the inventory; every other
		// page holds them only in our own list, where forgetting one is the
		// whole operation.
		g_pages[spot.page][spot.slot] = nullptr;
		if (g_held == g_marked) {
			g_held.reset();
			grid::Hold(g_held);
		}
		if (spot.page == g_currentPage) {
			MoveFavorite(object, static_cast<int>(spot.slot), -1);
			RefreshCross();
		}

		ShowGrid();
	}

	// Puts one chosen page back into the engine's twelve keys. Called when
	// the favorites menu closes, so that the game's own digit keys always
	// mean the same page -- see g_defaultPage for why that matters now that
	// every page is drawn alike.
	void RestoreDefaultPage()
	{
		if (g_defaultPage <= 0) {
			return;
		}
		EnsurePages();

		const auto wanted = static_cast<std::size_t>(g_defaultPage - 1);
		if (wanted >= g_pages.size() || wanted == g_currentPage) {
			return;
		}
		logger::info("page: back to {} because the menu closed", g_defaultPage);
		GoToPage(wanted);
	}

	// Whether the panel is on screen. Read from the input thread, so it asks
	// the one thing that is safe to ask there.
	[[nodiscard]] bool GridIsShowing()
	{
		return g_useGrid && g_favoritesMenuOpen.load();
	}

	// From the input thread. Everything the actions do touches Scaleform or
	// the inventory, so none of it happens here.
	void OnAction(input::Action a_action)
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		switch (a_action) {
		case input::Action::kPageUp:
			tasks->AddUITask([]() { MoveMark(-1, 0); });
			break;
		case input::Action::kPageDown:
			tasks->AddUITask([]() { MoveMark(1, 0); });
			break;
		case input::Action::kSlotLeft:
			tasks->AddUITask([]() { MoveMark(0, -1); });
			break;
		case input::Action::kSlotRight:
			tasks->AddUITask([]() { MoveMark(0, 1); });
			break;
		case input::Action::kUse:
			tasks->AddUITask([]() { UseMarked(); });
			break;
		case input::Action::kClear:
			tasks->AddUITask([]() { ClearMarked(); });
			break;
		case input::Action::kMove:
			tasks->AddUITask([]() { MoveMarked(); });
			break;
		}
	}

	// ---- Keeping the pages in the save -----------------------------------
	//
	// F4SE has a co-save, which SFSE does not, so the pages travel with the
	// save game instead of lying in a file beside it. Form IDs are written
	// as they are and resolved on the way back in, which is what survives a
	// changed load order.

	constexpr std::uint32_t kSaveUniqueID = 'FMGD';
	constexpr std::uint32_t kPagesRecord = 'PAGE';
	constexpr std::uint32_t kSaveVersion = 1;

	void SaveCallback(const F4SE::SerializationInterface* a_intfc)
	{
		RememberCurrentPage();

		if (!a_intfc->OpenRecord(kPagesRecord, kSaveVersion)) {
			logger::error("save: could not open the record");
			return;
		}

		const auto count = static_cast<std::uint32_t>(g_pages.size());
		const auto current = static_cast<std::uint32_t>(g_currentPage);
		a_intfc->WriteRecordData(&count, sizeof(count));
		a_intfc->WriteRecordData(&current, sizeof(current));

		for (const auto& page : g_pages) {
			for (const auto* object : page) {
				const std::uint32_t formID = object ? object->formID : 0;
				a_intfc->WriteRecordData(&formID, sizeof(formID));
			}
		}
		logger::info("save: {} pages, playing {}", count, current + 1);
	}

	void LoadCallback(const F4SE::SerializationInterface* a_intfc)
	{
		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;

		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kPagesRecord) {
				continue;
			}
			if (version != kSaveVersion) {
				logger::warn("load: a record of version {} is not read", version);
				continue;
			}

			std::uint32_t count = 0;
			std::uint32_t current = 0;
			a_intfc->ReadRecordData(&count, sizeof(count));
			a_intfc->ReadRecordData(&current, sizeof(current));
			if (count == 0 || count > 32) {
				logger::warn("load: {} pages is not a number to trust", count);
				return;
			}

			g_pages.assign(count, Page{});
			g_currentPage = current < count ? current : 0;

			std::size_t restored = 0;
			for (auto& page : g_pages) {
				for (auto& entry : page) {
					std::uint32_t formID = 0;
					a_intfc->ReadRecordData(&formID, sizeof(formID));
					if (formID == 0) {
						continue;
					}
					// A changed load order moves form IDs; the co-save knows
					// where they went.
					const auto resolved = a_intfc->ResolveFormID(formID);
					if (!resolved) {
						continue;
					}
					entry = RE::TESForm::GetFormByID<RE::TESBoundObject>(*resolved);
					if (entry) {
						++restored;
					}
				}
			}

			logger::info(
				"load: {} pages, playing {}, {} entries found again",
				count,
				g_currentPage + 1,
				restored);
		}
	}

	// A new game, or another save loaded over this one: everything the old
	// one knew is gone.
	void RevertCallback(const F4SE::SerializationInterface*)
	{
		g_pages.clear();
		g_currentPage = 0;
		g_iconOfObject.clear();
		logger::info("revert: the pages are cleared");
	}

	// ---- Taking a key away and giving it back ----------------------------
	//
	// The other half of a page switch. An item of the outgoing page loses
	// its key but stays a favorite, at -1, and an item of the incoming page
	// takes a key -- whether it had one before or not. Both go through
	// SetFavoriteIndex, so nothing here happens behind the engine's back.
	//
	// The probe is a round trip on one key: the first press takes the lowest
	// favorite off its key, the second gives it back.

	RE::TESBoundObject* g_probeObject = nullptr;
	int g_probeIndex = -1;

	void FavoriteRoundTrip()
	{
		if (g_probeIndex < 0) {
			const auto slots = ReadFavorites();
			for (std::size_t key = 0; key < slots.size(); ++key) {
				if (slots[key].object) {
					g_probeObject = slots[key].object;
					g_probeIndex = static_cast<int>(key);
					break;
				}
			}
			if (g_probeIndex < 0) {
				logger::warn("round trip: there is no favorite to take away");
				return;
			}

			LearnIcons();
			MoveFavorite(g_probeObject, g_probeIndex, -1);
			RefreshCross();
			logger::info(
				"round trip: \"{}\" is parked -- press again to give the key back",
				RE::TESFullName::GetFullName(*g_probeObject));
			LogFavorites("after parking one favorite");
			return;
		}

		LearnIcons();
		WriteFavorite(
			g_probeObject, kNoKey, static_cast<std::uint8_t>(g_probeIndex));
		RefreshCross();
		logger::info(
			"round trip: \"{}\" is back on [{}]",
			RE::TESFullName::GetFullName(*g_probeObject),
			KeyLabel(static_cast<std::size_t>(g_probeIndex)));
		LogFavorites("after giving the key back");

		g_probeObject = nullptr;
		g_probeIndex = -1;
	}

	// ---- Input -----------------------------------------------------------

	[[nodiscard]] bool IsGameForeground()
	{
		const auto foreground = GetForegroundWindow();
		if (!foreground) {
			return false;
		}
		DWORD processID = 0;
		GetWindowThreadProcessId(foreground, &processID);
		return processID == GetCurrentProcessId();
	}

	[[nodiscard]] bool IsKeyDown(int a_key)
	{
		return a_key != 0 && (GetAsyncKeyState(a_key) & 0x8000) != 0;
	}

	// The thread only reads the keyboard. Everything that touches the game
	// runs as a UI task, on the thread the engine expects -- the same rule
	// the Starfield project arrived at.
	void KeyboardPollingLoop()
	{
		bool previousInventory = false;
		bool previousRoundTrip = false;
		bool previousRotate = false;
		bool previousPeek = false;
		bool previousNext = false;
		bool previousBack = false;

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			if (!IsGameForeground()) {
				previousInventory = false;
				previousRoundTrip = false;
				previousRotate = false;
				previousPeek = false;
				previousNext = false;
				previousBack = false;
				continue;
			}

			const auto inventory = IsKeyDown(g_inventoryKey);
			const auto roundTrip = IsKeyDown(g_roundTripKey);
			const auto rotate = IsKeyDown(g_rotateKey);
			const auto peekNow = IsKeyDown(g_peekKey);
			const auto nextPage = IsKeyDown(g_nextPageKey);
			const auto previousPage = IsKeyDown(g_previousPageKey);
			const auto* tasks = F4SE::GetTaskInterface();

			if (tasks && inventory && !previousInventory) {
				tasks->AddUITask([]() { LogFavorites("probe"); });
			}
			if (tasks && roundTrip && !previousRoundTrip) {
				tasks->AddUITask([]() { FavoriteRoundTrip(); });
			}
			if (tasks && rotate && !previousRotate) {
				tasks->AddUITask([]() { RotateFavorites(); });
			}
			// Turning pages by hand is what the grid did away with -- while
			// the grid is up. Everywhere else the keys still matter, and the
			// Pip-Boy is the place it matters most: assigning a favorite
			// there writes into whichever page the engine holds, so without
			// these keys a player could only ever assign to one of them.
			// Switching them off outright was a straight loss.
			if (tasks && !GridIsShowing() && nextPage && !previousNext) {
				tasks->AddUITask([]() { TurnPage(1); });
			}
			if (tasks && !GridIsShowing() && previousPage && !previousBack) {
				tasks->AddUITask([]() { TurnPage(-1); });
			}
			// Reading memory only, so this one needs no UI task.
			if (peekNow && !previousPeek) {
				peek::Run(GetSettingsPath());
			}

			previousInventory = inventory;
			previousRoundTrip = roundTrip;
			previousRotate = rotate;
			previousPeek = peekNow;
			previousNext = nextPage;
			previousBack = previousPage;
		}
	}

	// ---- Plugin ----------------------------------------------------------

	class MenuWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuWatch* GetSingleton()
		{
			static MenuWatch singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent& a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			static const RE::BSFixedString favoritesMenu("FavoritesMenu");
			if (a_event.menuName == favoritesMenu) {
				logger::info(
					"FavoritesMenu {}", a_event.opening ? "opened" : "closed");
				g_favoritesMenuOpen = a_event.opening;

				// The field belongs to the movie that is going away, so it
				// is dropped on close and built again on the next open.
				if (!a_event.opening) {
					input::Listen(false);
					icons::Release();
					ShowCrosshair();
					g_marked.reset();
					g_held.reset();
					ForgetPointer();
					ReleaseIndicator();
					grid::Release();
					if (g_useGrid) {
						menu::Hide();
					}
					// After the menu is out of the way: the page switch
					// rewrites the twelve keys and refreshes the cross, and
					// there is no reason for either to happen behind a menu
					// that is still on screen.
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddUITask([]() { RestoreDefaultPage(); });
					}
				} else if (g_useGrid) {
					// Our own menu carries the grid. It answers with
					// SetOnReady once its movie is loaded, which is when
					// there is something to draw on.
					menu::Show();
				} else if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddUITask([]() { ShowPageIndicator(); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		MenuWatch() = default;
	};

	void OnMessage(F4SE::MessagingInterface::Message* a_message)
	{
		if (!a_message ||
			a_message->type != F4SE::MessagingInterface::kGameDataReady) {
			return;
		}

		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			logger::error("no UI singleton at kGameDataReady");
			return;
		}

		ui->GetEventSource<RE::MenuOpenCloseEvent>()->RegisterSink(
			MenuWatch::GetSingleton());

		peek::Run(GetSettingsPath());

		// Which tag means which icon. Read once: the files are the player's
		// mod setup, and that does not change while the game runs.
		if (g_useIcons) {
			tags::Load(GetInterfacePath());
		}

		menu::Register();
		menu::SetOnReady([]() {
			ShowGrid();
			HideCrosshair();
			// Only now: the keys mean pages and cells while the grid is up,
			// and walking again the moment it is gone.
			input::Listen(true);
		});
		menu::SetOnAdvance(&TrackPointer);

		// The one call the grid cannot make up for itself. Looked for once,
		// here, so a failure is in the log before anyone clicks anything.
		use::Find(GetSettingsPath());

		g_gridKeys.clear = g_clearKey;
		g_gridKeys.move = g_moveKey;
		input::SetKeys(g_gridKeys);
		input::SetOnAction(&OnAction);
		input::Install();

		std::thread(KeyboardPollingLoop).detach();
		logger::info("ready -- the keys are in FavoritesMenuGrid.ini");
	}

	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= std::format("{}.log", PLUGIN_LOG_NAME);

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
			path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(
	const F4SE::QueryInterface* a_f4se,
	F4SE::PluginInfo* a_info)
{
	InitializeLog();
	LoadSettings();
	logger::info(
		"{} {}.{}.{}",
		PLUGIN_NAME,
		PLUGIN_VERSION_MAJOR,
		PLUGIN_VERSION_MINOR,
		PLUGIN_VERSION_PATCH);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = PLUGIN_LOG_NAME;
	a_info->version = PLUGIN_VERSION_MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in the editor");
		return false;
	}

	// Only the original runtime for now. Address IDs differ on 1.10.980 and
	// later, and a wrong ID would not fail loudly -- it would read whatever
	// happens to sit at that address.
	const auto version = a_f4se->RuntimeVersion();
	if (version != F4SE::RUNTIME_1_10_163) {
		logger::critical("unsupported runtime v{}", version.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::critical("could not register the message listener");
		return false;
	}

	// The pages live in the co-save. Registering has to happen here, in
	// Load, not later.
	if (const auto serialization = F4SE::GetSerializationInterface()) {
		serialization->SetUniqueID(kSaveUniqueID);
		serialization->SetSaveCallback(SaveCallback);
		serialization->SetLoadCallback(LoadCallback);
		serialization->SetRevertCallback(RevertCallback);
	} else {
		logger::error("no serialization interface -- the pages will not be kept");
	}

	logger::info("loaded");
	return true;
}
