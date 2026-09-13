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

	int g_nextPageKey = VK_NEXT;
	int g_previousPageKey = VK_PRIOR;

	// How many pages the twelve keys are shared between.
	// Eight pages is ninety-six keys, and that is already more than anyone
	// will remember the shape of. The cap is not a technical one -- the
	// engine holds twelve keys and knows nothing of pages, and the co-save
	// would carry any number -- it is a judgement: past this the grid stops
	// being a thing you read at a glance and becomes a thing you search,
	// and searching is what the Pip-Boy is for.
	constexpr int kMostPages = 8;
	int g_pageCount = 3;

	// Whether the "[Tag]" that FIS puts in front of an item name is dropped
	// before the cross shows it.
	bool g_stripItemTags = true;

	grid::Placement g_gridWhere;

	// Walking the grid and using what is under the mark. Keys rather than
	// fixed letters: w and s are only obvious to someone who never moved
	// them.
	input::Keys g_gridKeys;

	// And the same seven things on a controller. Its own struct because the
	// numbers are not the same numbers: a keyboard event carries a virtual
	// key code, a gamepad event carries an XInput bit.
	input::Pad g_gridPad;

	// The cross closes the menu when a key is used, and a grid that stayed
	// open afterwards would be the one place in the game where using a
	// favorite leaves you standing in a menu.
	bool g_closeAfterUse = true;

	// Whether cells carry icons at all.
	bool g_useIcons = true;

	// Writes, once per opening, what every cell resolved to: the keyword its
	// name gave, the symbol that keyword names, and the colours that came
	// out of the palette. A tool, not a feature -- but the one that turns
	// "this looks wrong" into a line somebody can check against the XML.
	bool g_logIcons = false;
	std::atomic_bool g_logIconsDue{ false };

	// How many levels of the Pip-Boy's display tree to write out, once per
	// opening. Zero is off. It is a lot of log for one run and exactly the
	// right amount for the run that has to answer where a grid could go.
	int g_surveyDepth = 0;

	// Whether the mouse pointer goes out of sight while the keys have the
	// mark. Its own switch, because it is the one part of this that reaches
	// outside our own menu.
	bool g_hidePointer = true;

	// The line of keys under the panel, and whatever else should stand in it.
	// The closing key belongs to the game rather than to us -- it is whatever
	// the player bound the favorites menu to -- so it is text, not a binding.
	bool g_showHint = true;
	std::string g_hintExtra = "TAB) CLOSE";
	std::string g_hintExtraPad = "B) CLOSE";

	// The game's own button art, which is a font rather than a set of
	// pictures. Empty switches it off and the line spells the buttons out.
	std::string g_glyphFont = "Controller  Buttons";
	// A symbol is set larger than the words beside it -- the game sets its
	// own icons at 16 to 20 against text at 18 -- in hundredths of the hint
	// size.
	int g_glyphScale = 145;

	// Which line is on the panel now, so a switch of device can be noticed
	// without rebuilding the wording sixty times a second to compare it.
	input::Device g_hintDevice = input::Device::kNone;

	// Empty means the font the cross labels its own keys with, which is the
	// game's own and always present.
	std::string g_gridFont;

	// And whether something nobody has a symbol for still gets one, by what
	// kind of thing it is.
	bool g_iconFallback = true;

	// Whether a second press on something already worn takes it off again.
	bool g_toggleEquip = true;

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

	// How long a direction has to be held before the mark walks on by itself,
	// and how fast it walks then. Milliseconds in the INI, seconds here.
	double g_repeatDelay = 0.40;
	double g_repeatInterval = 0.09;

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

	// The colour of the whole panel. Anything above white means "take the
	// colour the player set for the HUD".
	std::uint32_t g_gridColor = 0x1000000;

	// Writes the pieces of engine code named in the INI next to the log --
	// see peek.h. The settings are read at that moment, so a new question
	// needs no restart of the game.
	int g_peekKey = VK_F10;

	// Surveys the Pip-Boy where it stands, rather than when it opens.
	//
	// The first survey ran on the menu's opening and found no cross at all,
	// which is the answer: ASSIGN FAVORITE is built when it is asked for and
	// does not exist before. So the survey has to be triggered by a hand,
	// with the thing on screen.
	int g_surveyKey = 0;

	// Hides and shows the Pip-Boy's own assign-a-favorite cross, to find out
	// whether the dialog still works without it. See TogglePipboyCross.
	int g_crossKey = 0;

	// Whether the grid takes the Pip-Boy's assign dialog over by itself.
	// Always: the grid is the mod, in the Pip-Boy as much as in the menu.
	// It spent a week off, under Debug as PipboyAuto, while the takeover
	// crashed; the crash was the icon libraries loaded into FallUI's
	// movie (section 63), and the switch went with it.
	constexpr bool g_pipboyAuto = true;

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

	// The gamepad's own spelling. A separate parser from ParseVirtualKey and
	// not an extension of it: "A" means the letter on a keyboard and the
	// button under the thumb on a controller, and the two are different
	// numbers. One table that had to guess which was meant would be wrong
	// half the time and silently.
	//
	// The numbers are XInput's button bits, which is what the event carries.
	[[nodiscard]] std::optional<int> ParsePadButton(const std::wstring& a_raw)
	{
		const auto value = NormalizeKeyName(a_raw);
		if (value.empty()) {
			return std::nullopt;
		}
		if (value == L"NONE" || value == L"DISABLED") {
			return 0;
		}

		const std::array<std::pair<std::wstring_view, int>, 26> names{
			std::pair{ L"DPADUP"sv, 0x0001 },
			std::pair{ L"UP"sv, 0x0001 },
			std::pair{ L"DPADDOWN"sv, 0x0002 },
			std::pair{ L"DOWN"sv, 0x0002 },
			std::pair{ L"DPADLEFT"sv, 0x0004 },
			std::pair{ L"LEFT"sv, 0x0004 },
			std::pair{ L"DPADRIGHT"sv, 0x0008 },
			std::pair{ L"RIGHT"sv, 0x0008 },
			std::pair{ L"START"sv, 0x0010 },
			std::pair{ L"BACK"sv, 0x0020 },
			std::pair{ L"SELECT"sv, 0x0020 },
			std::pair{ L"LSTICK"sv, 0x0040 },
			std::pair{ L"LS"sv, 0x0040 },
			std::pair{ L"L3"sv, 0x0040 },
			std::pair{ L"RSTICK"sv, 0x0080 },
			std::pair{ L"RS"sv, 0x0080 },
			std::pair{ L"R3"sv, 0x0080 },
			std::pair{ L"LB"sv, 0x0100 },
			std::pair{ L"LSHOULDER"sv, 0x0100 },
			std::pair{ L"RB"sv, 0x0200 },
			std::pair{ L"RSHOULDER"sv, 0x0200 },
			// Bethesda's own two, which XInput has no bits for.
			std::pair{ L"LT"sv, 0x0009 },
			std::pair{ L"RT"sv, 0x000A },
			std::pair{ L"A"sv, 0x1000 },
			// B is never claimed, so it is deliberately not spellable here;
			// X and Y are.
			std::pair{ L"X"sv, 0x4000 },
			std::pair{ L"Y"sv, 0x8000 }
		};
		for (const auto& [name, code] : names) {
			if (value == name) {
				return code;
			}
		}

		wchar_t* end = nullptr;
		const auto numeric = std::wcstol(value.c_str(), &end, 0);
		if (end && *end == L'\0' && numeric >= 0 && numeric <= 0xFFFF) {
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

		// The same, in the gamepad's own numbers.
		const auto readPad = [&](const wchar_t* a_key, int& a_target) {
			std::wstring value(64, L'\0');
			const auto length = GetPrivateProfileStringW(
				L"Controls",
				a_key,
				L"",
				value.data(),
				static_cast<DWORD>(value.size()),
				path.c_str());
			value.resize(length);
			if (value.empty()) {
				return;
			}
			if (const auto parsed = ParsePadButton(value)) {
				a_target = *parsed;
			} else {
				logger::warn(
					"settings: could not read the gamepad button for that "
					"entry");
			}
		};


		// ---- What a player turns ----------------------------------------
		//
		// Everything below is in the INI because somebody might reasonably
		// want it different. What is *not* below used to be, and was taken
		// out for the release: the size of every line of type, the length
		// and weight of the corner marks, how much of a cell an icon fills,
		// how fast a held key repeats, the name of the font the button
		// symbols live in, and a button for each of the seven things a
		// controller does. Those were the numbers this mod was built with,
		// not questions a player has -- and an INI that asks fifty-nine
		// questions is one nobody reads. They are constants now, at the
		// values the measuring settled on.

		read(L"Controls", L"GridPageUpKey", g_gridKeys.pageUp);
		read(L"Controls", L"GridPageDownKey", g_gridKeys.pageDown);
		read(L"Controls", L"GridLeftKey", g_gridKeys.slotLeft);
		read(L"Controls", L"GridRightKey", g_gridKeys.slotRight);
		read(L"Controls", L"GridUseKey", g_gridKeys.use);
		read(L"Controls", L"GridClearKey", g_clearKey);
		read(L"Controls", L"GridMoveKey", g_moveKey);

		const auto yes = [&](const wchar_t* a_section,
							  const wchar_t* a_key,
							  bool a_fallback) {
			return GetPrivateProfileIntW(
					   a_section, a_key, a_fallback ? 1 : 0, path.c_str()) != 0;
		};

		g_gridKeys.useOnClick = yes(L"Controls", L"GridUseOnClick", true);
		g_closeAfterUse = yes(L"Controls", L"GridCloseAfterUse", true);
		g_toggleEquip = yes(L"Controls", L"GridToggleEquip", true);
		g_wrapNavigation = yes(L"Controls", L"GridWrap", true);
		g_hidePointer = yes(L"Controls", L"GridHidePointer", true);
		g_gridPad.enabled = yes(L"Controls", L"GridGamepad", true);
		g_gridPad.stick = yes(L"Controls", L"GridGamepadStick", true);

		g_gridWhere.cellSize = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Grid", L"GridCellSize", 48, path.c_str())),
			24,
			96);

		g_gridWhere.showRowLabels = yes(L"Grid", L"ShowPageNumbers", true);
		g_gridWhere.corners = yes(L"Grid", L"GridCorners", false);
		g_useIcons = yes(L"Grid", L"UseIcons", true);
		g_gridWhere.iconColors = yes(L"Grid", L"IconColors", true);
		g_iconFallback = yes(L"Grid", L"IconFallback", true);
		g_stripItemTags = yes(L"Grid", L"StripItemTags", true);
		g_hideCrosshair = yes(L"Grid", L"HideCrosshair", true);
		g_showHint = yes(L"Grid", L"ShowKeyHints", true);
		g_hintExtra = ReadText(path, L"Grid", L"KeyHintExtra", L"TAB) CLOSE");

		// Left in the code and out of the INI: for working on the mod, not
		// for playing it. Written up in the handoff instead.
		read(L"Debug", L"PeekKey", g_peekKey);
		read(L"Debug", L"SurveyKey", g_surveyKey);
		read(L"Debug", L"PipboyCrossKey", g_crossKey);
		g_logIcons = yes(L"Debug", L"LogIcons", false);
		g_surveyDepth = std::clamp(
			static_cast<int>(GetPrivateProfileIntW(
				L"Debug", L"SurveyPipboy", 0, path.c_str())),
			0,
			12);

		// Off unless someone asks for it: the corner message lands wherever
		// the player's HUD mods put it, which is why the page is written
		// into the menu instead.

		// Not a key, so it is read on its own.
		g_pageCount = static_cast<int>(GetPrivateProfileIntW(
			L"Pages", L"PageCount", g_pageCount, path.c_str()));
		g_pageCount = std::clamp(g_pageCount, 1, kMostPages);
		g_defaultPage = std::clamp(
			static_cast<int>(
				GetPrivateProfileIntW(L"Pages", L"DefaultPage", 0, path.c_str())),
			0,
			g_pageCount);

		logger::info(
			"settings: {} pages, {} cells, page keys {:#04x} and {:#04x}",
			g_pageCount,
			static_cast<int>(g_gridWhere.cellSize),
			g_nextPageKey,
			g_previousPageKey);
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

	// Every object that carries a key, in the order the inventory is walked.
	//
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
			SetFavorite(*a_stack.extra, index);
		}

		// ExtraDataList::SetFavorite, written out from its own bytes rather
		// than called by number (OG ID 534268, disassembled 2026-09-13 and
		// kept in the handoff, section 64):
		//
		//     extra = GetByType(list, kFavorite)
		//     if (index == 0xFE)  RemoveExtra(list, kFavorite)
		//     else if (extra)     extra->quickkeyIndex = index
		//     else                new ExtraFavorite{ index }; AddExtra(list, it)
		//
		// Every piece of that the library has on every runtime; the number
		// it replaces was the only one for this function, and the Runtime
		// Database has no bridge for it. What the engine does around it --
		// the list's lock, a debug check -- the library's list operations
		// do their own way, which is the part the game has to confirm.
		static void SetFavorite(RE::ExtraDataList& a_list, std::uint8_t a_index)
		{
			if (a_index == kNotAFavorite) {
				a_list.RemoveExtra(RE::EXTRA_DATA_TYPE::kFavorite);
				return;
			}
			if (auto* extra = a_list.GetByType<RE::ExtraFavorite>()) {
				extra->quickkeyIndex = static_cast<std::int8_t>(a_index);
				return;
			}
			// Through the game's own heap, which is what F4_HEAP_REDEFINE_NEW
			// on BSExtraData means: the list will free it one day.
			auto* extra = new RE::ExtraFavorite();
			F4SE::stl::emplace_vtable(extra);
			extra->type = RE::EXTRA_DATA_TYPE::kFavorite;
			extra->quickkeyIndex = static_cast<std::int8_t>(a_index);
			a_list.AddExtra(extra);
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
	// Whether any stack of this object carries that key. Declared here
	// because clearing a key has to ask until the answer is no.
	[[nodiscard]] bool Carries(RE::TESBoundObject* a_object, std::uint8_t a_index);

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

		// Once is not always enough. FindAndWriteStackDataForItem writes the
		// **first** stack that matches, and a favorited stack can be split
		// in two (section 10) -- one Nuka-Cola of fifty-four and one of one,
		// both carrying key 8. Clearing the key then cleared one of them,
		// the other kept it, and the item stood on every page at once.
		//
		// So a key is taken off every stack that has it. Only when clearing:
		// a_from below twelve is a real key, where kNoKey and kNotAFavorite
		// would match half the inventory and this would favorite all of it.
		auto rounds = 0;
		do {
			player->inventoryList->FindAndWriteStackDataForItem(
				a_object, compare, write);
			++rounds;
		} while (a_from < 12 && rounds < 12 && Carries(a_object, a_from));

		if (rounds > 1) {
			logger::info(
				"favorites: \"{}\" carried {} on {} stacks at once",
				RE::TESFullName::GetFullName(*a_object),
				KeyLabel(a_from),
				rounds);
		}
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

		// Deliberately silent. Turning a page writes twenty-four of these,
		// and a page is turned every time the mark crosses a row now, so a
		// line each would bury everything else in the log. What the twelve
		// keys ended up holding is said once, by LogFavorites, after the
		// whole page has been written.
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
	// Takes one key off everything that carries it.
	//
	// Everything, not the one thing the twelve slots report: two different
	// objects can hold the same key, and then which one is reported is
	// whichever stack the walk saw last (section 54).
	void ClearKey(std::uint8_t a_key)
	{
		std::vector<RE::TESBoundObject*> carried;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList) {
			return;
		}
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item,
				RE::BGSInventoryItem::Stack& a_stack) {
				if (FavoriteOf(a_stack) == a_key) {
					carried.push_back(a_item.object);
				}
				return true;
			});
		for (auto* object : carried) {
			MoveFavorite(object, a_key, -1);
		}
	}

	// The engine keeps its own copy of the twelve, and UseQuickkeyItem reads
	// that copy rather than the inventory (section 53). Whatever moves a key
	// has to bring it back into agreement.
	void SyncFavoritesCache()
	{
		auto* manager = RE::FavoritesManager::GetSingleton();
		if (!manager) {
			return;
		}
		const auto now = ReadFavorites();
		for (std::size_t key = 0; key < now.size(); ++key) {
			manager->storedFavTypes[key] = now[key].object;
		}
	}

	// Tells everyone else that the twelve keys changed.
	//
	// The page switch writes through the inventory list, and the list sends
	// its own event afterwards -- which is what keeps the engine's display
	// and FavoritesManager in step (see WriteFavorite). It is not the event
	// the Pip-Boy sends when a favorite is assigned there: that one is
	// InventoryInterface::FavoriteChangedEvent, out of BGSInventoryInterface,
	// and it is what other plugins listen for. VisibleFavorites is one: it
	// draws the favorites on the player's body, and after a page switch it
	// kept drawing the old page until the next equip event gave it a reason
	// to look again. Holstering, as it turned out.
	//
	// So the same event is sent here, once per item that holds a key, with
	// the inventory item the Pip-Boy would name. Only after the menu has
	// closed and the page has settled, not on every row the mark crosses:
	// FavoritesManager listens to this event too and keeps twelve buffered
	// geometries beside its twelve keys, and nobody needs those reloaded
	// ten times while a page is being chosen.
	void AnnounceFavorites()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* inventory = RE::BGSInventoryInterface::GetSingleton();
		if (!player || !player->inventoryList || !inventory) {
			return;
		}

		// The event source is a private base of the interface, at 0x60 --
		// the header says so, and there is no accessor.
		auto* source = reinterpret_cast<
			RE::BSTEventSource<RE::InventoryInterface::FavoriteChangedEvent>*>(
			reinterpret_cast<std::uintptr_t>(inventory) + 0x60);

		std::vector<RE::BGSInventoryItem*> keyed;
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item,
				RE::BGSInventoryItem::Stack& a_stack) {
				// Once per item, however many of its stacks carry a key;
				// returning false here would end the whole walk.
				if (FavoriteOf(a_stack) < 12 &&
					(keyed.empty() || keyed.back() != &a_item)) {
					keyed.push_back(&a_item);
				}
				return true;
			});

		for (auto* item : keyed) {
			RE::InventoryInterface::FavoriteChangedEvent event{ item };
			source->Notify(event);
		}
		logger::info("favorites: {} keyed items announced", keyed.size());
	}

	void ApplyPage(const Page& a_target)
	{
		// While the display still agrees with the inventory.
		LearnIcons();

		// Everything that carries a key, not one thing per key.
		//
		// ReadFavorites answers with **one** object per key, because that is
		// what a key means. The inventory does not agree: two different
		// objects can both carry key 3, and then which one the twelve slots
		// report is whichever stack the walk happened to see last.
		//
		// That is what made a page switch look right and act wrong. The grid
		// said Righteous Authority on page 2, the engine's own copy said
		// Righteous Authority -- and the Sten Mk II from page 1 still had
		// key 3 on its stack, so the game equipped the Sten. Clearing one
		// object per key left the other one holding it, every time.
		//
		// So the whole inventory is swept for keys first, and every pair it
		// finds is cleared. WriteFavorite then takes each one off all of its
		// own stacks as well (a favorited stack can be split, section 41).
		std::vector<std::pair<RE::TESBoundObject*, std::uint8_t>> carried;
		if (auto* player = RE::PlayerCharacter::GetSingleton();
			player && player->inventoryList) {
			player->inventoryList->ForEachStack(
				[](RE::BGSInventoryItem&) { return true; },
				[&](RE::BGSInventoryItem& a_item,
					RE::BGSInventoryItem::Stack& a_stack) {
					const auto key = FavoriteOf(a_stack);
					if (key < 12) {
						carried.emplace_back(a_item.object, key);
					}
					return true;
				});
		}
		for (const auto& [object, key] : carried) {
			MoveFavorite(object, static_cast<int>(key), -1);
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

		// And the engine's own copy of the twelve.
		//
		// This is the bug that made a page switch look like it had worked
		// and then equipped something else entirely. Section 6 says it:
		// `storedFavTypes` is not the truth, it is an image of the truth --
		// and `UseQuickkeyItem` reads the image. The log made it plain in
		// the end, because the cache line printed exactly one value for a
		// whole session while the inventory underneath it changed twenty
		// times: it was frozen at whatever page the character had when the
		// save was loaded.
		//
		// So every page switch used the right index into the wrong page. Aid
		// sometimes worked because using something up is forgiving; a weapon
		// simply equipped whatever the stale image held at that key, which
		// with DefaultPage on is page one.
		//
		// Rewriting it is legitimate precisely because it is an image: the
		// inventory is what was changed, and this is brought back into
		// agreement with it.
		SyncFavoritesCache();
		RefreshCross();
		LogFavorites("after the page");
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

	// The same, for the Pip-Boy, so the polling thread can ask without
	// touching the UI's own tables.
	std::atomic_bool g_pipboyOpen{ false };

	// Ticks of the keyboard loop still to wait before the default page is
	// put back; zero means nothing is pending. Twenty of them at 25 ms is
	// half a second -- long enough for a queued equip to have happened, short
	// enough that nobody gets back to the digits first.
	constexpr int kRestoreDelayTicks = 20;
	std::atomic_int g_restoreIn{ 0 };

	// The cell the grid is pointing at. Empty until the pointer finds one or
	// a key is pressed: a menu that opens with something already chosen
	// invites using it by accident.
	std::optional<grid::Spot> g_marked;

	// The cell that has been picked up and is waiting to be put down.
	std::optional<grid::Spot> g_held;

	// Defined below, with the rest of the page marker: they need the pages,
	// and the pages need to show themselves.
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

		// One thing, one place. The engine keeps a key to an object within
		// its twelve, but the other pages are ours, and nothing stopped an
		// item assigned on the page being played from still being written
		// down on another -- where the next switch gave it a key again, and
		// the player saw the same item on two rows. Where it was put last
		// is where it belongs; the older entry goes free.
		for (std::size_t other = 0; other < g_pages.size(); ++other) {
			if (other == g_currentPage) {
				continue;
			}
			for (auto& held : g_pages[other]) {
				if (!held) {
					continue;
				}
				const auto twice = std::ranges::find(page, held);
				if (twice != page.end()) {
					logger::info(
						"page: \"{}\" is on page {} now, so it leaves page {}",
						RE::TESFullName::GetFullName(*held),
						g_currentPage + 1,
						other + 1);
					held = nullptr;
				}
			}
		}
	}

	// Which page the twelve keys are really holding.
	//
	// g_currentPage is a belief, and a belief can be wrong. The twelve keys
	// live inside the game's own save; the page list lives beside it in the
	// co-save; and nothing makes the two agree at load time. When they do
	// not, everything built on top is built on sand: RememberCurrentPage
	// writes the live twelve into the wrong page, a use of "the page being
	// played" reaches into the inventory for somebody else's items, and a
	// switch to the page that is already live is skipped as needless.
	//
	// That last one is what the player saw. Load a save, open the grid,
	// click a cell on page 1 -- if the keys really hold page 3 while we
	// believe page 1, no switch happens at all and page 3's item is used.
	// Every time, the same item, until something turns a page for real.
	//
	// So the belief is checked against the inventory instead of trusted.
	// Nothing is rearranged here: this only decides which page we are
	// looking at.
	void ReconcileCurrentPage()
	{
		EnsurePages();
		if (g_pages.size() < 2) {
			return;
		}

		const auto live = ReadFavorites();
		const auto holds = [&](std::size_t a_page) {
			for (std::size_t key = 0; key < 12; ++key) {
				if (g_pages[a_page][key] != live[key].object) {
					return false;
				}
			}
			return true;
		};

		if (holds(g_currentPage)) {
			return;
		}
		for (std::size_t page = 0; page < g_pages.size(); ++page) {
			if (page == g_currentPage || !holds(page)) {
				continue;
			}
			logger::warn(
				"page: the twelve keys hold page {} and we believed page {} "
				"-- the inventory is the one that counts",
				page + 1,
				g_currentPage + 1);
			g_currentPage = page;
			return;
		}

		// No page owns them. The player assigned a favorite by hand since we
		// last looked, which is theirs to do -- the keys belong to the page
		// we are on, and RememberCurrentPage writes them there.
		logger::info(
			"page: the twelve keys match no stored page -- they are taken as "
			"page {}",
			g_currentPage + 1);
	}

	// The corner message that used to say which page was being played is
	// gone. It came from the days when nothing else said it; the panel says
	// it now, on both screens it appears on, and a HUD message on top of
	// that is one more thing flashing at somebody who is reading a grid.

	// The twelve keys, frame after frame, across a page switch and the use
	// that follows it.
	//
	// Everything measured so far has been a single instant, and every
	// instant read correctly. But the player's own account has the shape of
	// something settling rather than something wrong: the first press after
	// a switch uses the old page's item, the second press uses the right
	// one. Whatever the engine reads catches up on its own between those two
	// presses, and nothing has ever watched it do that.
	//
	// So this says the same two lines every frame for a while: the inventory
	// as it stands, and the engine's own copy of the twelve. If something
	// puts the old page back after we wrote the new one, it happens in one
	// of these frames and it will be in the log with a frame number on it.
	void WatchTheKeys(int a_frames)
	{
		if (a_frames <= 0) {
			return;
		}
		LogFavorites(std::format("watching, {} to go", a_frames));
		if (auto* tasks = F4SE::GetTaskInterface()) {
			tasks->AddUITask([a_frames]() { WatchTheKeys(a_frames - 1); });
		}
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
		ShowGrid();
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
				return armor && armor->data.rating > 0 ? "Armor" : "Clothes";
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
	// Whose names the line should carry. Not simply the last device: with
	// the gamepad switched off there is nothing to say about it, and the
	// keyboard is what a player who cannot use the pad still has.
	[[nodiscard]] input::Device HintDevice()
	{
		if (g_gridPad.enabled && input::LastDevice() == input::Device::kGamepad) {
			return input::Device::kGamepad;
		}
		return input::Device::kKeyboard;
	}

	// The line is markup now, so anything that is not meant as markup has to
	// say so. Three characters, which is all Flash's parser cares about.
	[[nodiscard]] std::string Escape(std::string_view a_text)
	{
		std::string out;
		out.reserve(a_text.size());
		for (const auto character : a_text) {
			switch (character) {
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			default:
				out += character;
			}
		}
		return out;
	}

	// Which of the two sets of button art the game is showing. Its own
	// answer, not ours: the same field decides what the vanilla menus draw.
	[[nodiscard]] bool PadIsOrbis()
	{
		const auto* controls = RE::ControlMap::GetSingleton();
		return controls &&
			controls->pcGamePadMapType == RE::PC_GAMEPAD_TYPE::kOrbis;
	}

	// One button, drawn in the game's own font, at a size of its own.
	[[nodiscard]] std::string Glyph(std::string_view a_character)
	{
		if (a_character.empty() || g_glyphFont.empty()) {
			return {};
		}
		return std::format(
			"<font face='{}' size='{}'>{}</font>",
			g_glyphFont,
			static_cast<int>(g_gridWhere.hintSize * g_glyphScale / 100.0),
			a_character);
	}

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
			// "E) USE" for a key, "(A) USE" for a button. The bracket is
			// what a written-out key needs to read as a key; a drawn button
			// already is one, and a bracket after it looks like a mistake.
			// Markup is the one thing a key name never starts with.
			line += a_key.starts_with('<')
				? std::format("{} {}", a_key, a_what)
				: std::format("{}) {}", a_key, a_what);
		};

		// Whose keys to name. A controller player has no INS and no DEL, and
		// a line that names them is worse than no line: it says the mod has
		// not noticed what they are holding.
		const auto pad = HintDevice() == input::Device::kGamepad;
		const auto orbis = pad && PadIsOrbis();

		// A button the game has art for is drawn, not spelled. Only when it
		// has none -- the two triggers have their own symbols, an unbound
		// button has none at all -- does the short name stand in.
		const auto name = [pad, orbis](int a_key, int a_button) {
			if (!pad) {
				return Escape(KeyName(a_key));
			}
			if (auto drawn = Glyph(input::PadGlyph(a_button, orbis)); !drawn.empty()) {
				return drawn;
			}
			return Escape(input::PadName(a_button));
		};

		// Walking first: it is the one thing a player will try without being
		// told, and seeing it named says the rest of the line is trustworthy.
		//
		// On a controller the four directions are one thing with one symbol,
		// the way the game writes them -- four separate D-pads in a row would
		// be four times the ink for the same sentence. Only when the four have
		// been moved off the D-pad does each get named on its own.
		const auto dpad = pad && g_gridPad.pageUp == 0x0001 &&
			g_gridPad.pageDown == 0x0002 && g_gridPad.slotLeft == 0x0004 &&
			g_gridPad.slotRight == 0x0008;
		if (dpad) {
			auto walk = Glyph(input::PadGlyphDPad(orbis));
			if (g_gridPad.stick) {
				walk += Glyph(input::PadGlyphStick(orbis));
			}
			add(walk, "MOVE");
		} else {
			const auto up = name(g_gridKeys.pageUp, g_gridPad.pageUp);
			const auto left = name(g_gridKeys.slotLeft, g_gridPad.slotLeft);
			const auto down = name(g_gridKeys.pageDown, g_gridPad.pageDown);
			const auto right = name(g_gridKeys.slotRight, g_gridPad.slotRight);
			if (!up.empty() && !left.empty() && !down.empty() && !right.empty()) {
				add(up + left + down + right, "MOVE");
			} else if (pad && g_gridPad.stick) {
				// The four may be off and the stick still on, and then the
				// stick is the whole of it.
				add(Glyph(input::PadGlyphStick(orbis)), "MOVE");
			}
		}

		add(name(g_gridKeys.use, g_gridPad.use), "USE");
		add(name(g_gridKeys.move, g_gridPad.move), "PICK UP");
		add(name(g_gridKeys.clear, g_gridPad.clear), "CLEAR");

		// Leaving. On a keyboard this is text, because the key that closes
		// the favorites menu belongs to the game and we never see it named.
		// On a controller we do: Install had to find that button in order to
		// keep its hands off it, so it can be drawn like any other.
		auto said = false;
		if (pad) {
			if (const auto close = input::PadCloseButton(); close != 0) {
				if (auto drawn = Glyph(input::PadGlyph(close, orbis));
					!drawn.empty()) {
					add(drawn, "CLOSE");
					said = true;
				}
			}
		}

		const auto& extra = pad ? g_hintExtraPad : g_hintExtra;
		if (!said && !extra.empty()) {
			if (!line.empty()) {
				line += "      ";
			}
			line += Escape(extra);
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
		// What is drawn is what is true: an item on the live keys is drawn
		// nowhere else. Otherwise an item just assigned in the Pip-Boy kept
		// showing on its old row until the next page switch remembered.
		//
		// Struck from the book, not written into it. For one build (7740b28)
		// this called RememberCurrentPage here, which also copies the live
		// keys into the page being played -- and with that in place Accept
		// in the Pip-Boy's dialog used the item instead of assigning it,
		// every time, found by bisecting on 2026-09-13. Why a write into
		// our own book at draw time should reach the dialog's Accept is
		// not understood; that it does is measured, so the book is only
		// written where it always was, on a page switch.
		for (std::size_t other = 0; other < g_pages.size(); ++other) {
			if (other == g_currentPage) {
				continue;
			}
			for (auto& held : g_pages[other]) {
				if (held && std::ranges::any_of(live, [&](const auto& a_key) {
						return a_key.object == held;
					})) {
					held = nullptr;
				}
			}
		}
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
				//
				// The name the *game* shows, not the one in the plugin. A
				// weapon is named by what is bolted to it -- "T60" in the
				// plugin is "T60 Pistol" in the hand -- and the sorter's
				// auto-tagging reads that name to decide the icon. See
				// detail::DisplayName.
				const std::string full{ detail::DisplayName(object) };
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
				const auto* icon = tags::Find(keyword);
				cell.keyword = std::string(keyword);
				if (g_logIconsDue.load()) {
					std::string colors;
					if (icon) {
						for (const auto value : icon->colors) {
							colors += colors.empty() ? "" : ",";
							colors += value <= 0xFFFFFF
								? std::format("#{:06x}", value)
								: "-";
						}
					}
					logger::info(
						"icon: \"{}\" -> [{}] {}{} {}",
						cell.name,
						keyword,
						icon ? icon->symbol : std::string("(none)"),
						icon && !icon->subsymbol.empty()
							? " +" + icon->subsymbol
							: std::string(),
						colors.empty() ? "(no colour)" : colors);
				}
				if (icon) {
					// The "m_" is the only translation between what the
					// configuration writes and what the library exports.
					cell.symbol = "m_" + icon->symbol;
					if (!icon->subsymbol.empty()) {
						cell.subsymbol = "m_" + icon->subsymbol;
					}
					cell.colors = icon->colors;
					if (!icon->library.empty()) {
						g_wantedLibraries.insert(icon->library);
					}
				}
			}
		}
		g_logIconsDue.store(false);
		return rows;
	}

	// ---- Surveying the Pip-Boy -------------------------------------------
	//
	// The next place the grid could stand is the Pip-Boy's own assign-a-
	// favorite cross, and getting there begins with knowing what is there --
	// on *this* machine. Three things make that unlike the HUD menu, and all
	// three argue for measuring rather than assuming:
	//
	//   * The Pip-Boy is not one movie. `PipboyMenu.swf` is the frame, and
	//     the pages are loaded into it: `Pipboy_InvPage.swf` carries the
	//     inventory, and it is that file which holds `Cross_mc` -- the same
	//     twelve-slot cross class the favorites menu uses. The UI knows a
	//     menu called "PipboyMenu"; the page is a child clip inside it, so
	//     it has to be walked to, not asked for.
	//   * The stage is not the screen. Out in the world the Pip-Boy draws
	//     onto a texture on the model, so "the middle of the stage" means
	//     the middle of that little screen, and the safe area is the one
	//     the page defines rather than the one the game reports.
	//   * The file on this machine is already a replacement. A layout read
	//     out of the vanilla SWF would be a layout nobody here has.
	//
	// So: no assumptions, one survey. It writes the display tree of the
	// Pip-Boy once per opening, marking anything that looks like the cross,
	// and everything after this depends on what it finds.
	void SurveyBranch(
		RE::Scaleform::GFx::Value& a_where, const std::string& a_path, int a_depth)
	{
		if (a_depth < 0 || !a_where.IsObject()) {
			return;
		}
		RE::Scaleform::GFx::Value count;
		if (!a_where.GetMember("numChildren", &count)) {
			return;
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
			const auto called = child.GetMember("name", &name) && name.IsString()
				? std::string(name.GetString())
				: std::format("[{}]", index);
			const auto here = a_path + "." + called;

			// Size and place, because that is what a grid would have to fit
			// into, and visibility, because half of a Pip-Boy is built and
			// hidden.
			RE::Scaleform::GFx::Value shown;
			const auto visible = !child.GetMember("visible", &shown) ||
				!shown.IsBoolean() || shown.GetBoolean();
			logger::info(
				"pipboy: {} {:.0f},{:.0f} {:.0f}x{:.0f}{}",
				here,
				ReadNumber(child, "x", 0.0),
				ReadNumber(child, "y", 0.0),
				ReadNumber(child, "width", 0.0),
				ReadNumber(child, "height", 0.0),
				visible ? "" : " (hidden)");

			SurveyBranch(child, here, a_depth - 1);
		}
	}

	// Which menus are up, before anything is walked.
	//
	// The second survey found the Pip-Boy's inventory page with its modal
	// dimmer *visible* -- so ASSIGN FAVORITE was open -- and still no cross
	// anywhere in that tree. A thing that is on screen and not in the tree
	// we are walking is in somebody else's tree, so the first question is
	// whose. The engine keeps the answer in a map it also locks; the lock is
	// taken, because this runs while the game is drawing that very map.
	void SurveyMenus()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		std::string open;
		{
			const RE::BSAutoReadLock lock{ RE::UI::GetMenuMapRWLock() };
			for (const auto& [name, entry] : ui->menuMap) {
				if (!entry.menu) {
					continue;
				}
				open += open.empty() ? "" : ", ";
				open += name.c_str();
				if (!entry.menu->uiMovie) {
					open += " (no movie)";
				}
			}
		}
		logger::info("pipboy: the menus up are {}", open.empty() ? "none" : open);
	}

	// All defined further down, with the things they were written for.
	[[nodiscard]] std::string CrossFont(RE::Scaleform::GFx::Value& a_cross);
	[[nodiscard]] std::vector<grid::Page> BuildGridPages();
	[[nodiscard]] std::uint32_t HUDColor();

	// Walks a display tree looking for a named child. Defined further down,
	// with the crosshair it was written for.
	[[nodiscard]] bool FindByName(
		RE::Scaleform::GFx::Value& a_where,
		const char* a_name,
		int a_depth,
		RE::Scaleform::GFx::Value& a_found,
		std::string& a_path);

	// The Pip-Boy's own display object, whichever of the two ways it
	// answers to.
	[[nodiscard]] bool PipboyRoot(RE::Scaleform::GFx::Value& a_root)
	{
		auto* pipboy = GetMenu("PipboyMenu");
		if (!pipboy) {
			logger::info("pipboy: the Pip-Boy is not open");
			return false;
		}
		a_root = pipboy->menuObj;
		if (a_root.IsObject()) {
			return true;
		}
		if (pipboy->uiMovie->GetVariable(&a_root, "root") && a_root.IsObject()) {
			return true;
		}
		logger::info("pipboy: the menu has no object to walk");
		return false;
	}

	// Step one of putting the grid into the Pip-Boy, and only step one.
	//
	// The cross the ASSIGN FAVORITE dialog is built around sits at
	//
	//   PipboyMenu.<page>.ModalFadeRect_mc.<dialog>.Cross_mc
	//
	// -- inside the dimmer, which is not where anyone would look. The
	// intermediate names are worthless: the same page was `instance8` in one
	// run and `instance36` in the next. So it is searched for by name from
	// the top, the way the crosshair is in the HUD.
	//
	// This does nothing but turn it invisible and say what it found, because
	// there is exactly one thing worth knowing before anything is drawn:
	// **does the dialog still work when its own cross cannot be seen?** If
	// it stops answering, the grid cannot take that place and would have to
	// stand beside it instead. Everything else waits for that answer.
	//
	// Nothing is remembered between presses. It reads the visibility it
	// finds and writes the opposite, so the key is its own undo, and a
	// reopened dialog brings a fresh, visible cross whatever was done to the
	// last one. A held reference into another movie's heap is what section
	// 42 was about.
	// Whether our panel is standing in the Pip-Boy right now, and the cross
	// it stands in front of.
	//
	// The cross is held on to while it is up, against the rule of section 42
	// -- and deliberately: finding it means walking a thousand-node tree,
	// which is not a thing to do ten times a second. The reference is let go
	// the moment the grid comes down or the Pip-Boy closes, which are the
	// only two ways that movie can go away underneath it.
	bool g_pipboyGridUp = false;
	RE::Scaleform::GFx::Value g_pipboyCross;
	// The inventory list behind the dialog, held while the panel stands so
	// its mouse can be given back. See ShieldPipboyList.
	RE::Scaleform::GFx::Value g_pipboyList;
	// Where the pointer last was, in the dialog's coordinates -- so that a
	// pointer merely resting over a cell when the dialog opens chooses
	// nothing, the same rule the favorites menu keeps.
	double g_pipboyPointerX = std::numeric_limits<double>::lowest();
	double g_pipboyPointerY = std::numeric_limits<double>::lowest();
	std::size_t g_pipboyPage = 0;
	std::uint32_t g_pipboySlot = 0;
	// How many times in a row the dialog has been seen. See the watch.
	int g_pipboySeen = 0;
	// A key chosen while the panel was down, waiting for the next draw --
	// twelve means none. See SelectPipboySpot.
	std::uint32_t g_pipboyPending = 12;

	void ForgetPipboyGrid()
	{
		g_pipboyGridUp = false;
		g_pipboySeen = 0;
		g_pipboyPending = 12;
		g_pipboyCross = RE::Scaleform::GFx::Value();
		g_pipboyList = RE::Scaleform::GFx::Value();
		g_pipboyPointerX = std::numeric_limits<double>::lowest();
		g_pipboyPointerY = std::numeric_limits<double>::lowest();
	}

	// The list behind the dialog takes no mouse while the panel stands.
	//
	// A row change rewrites twelve keys, FallUI rebuilds its list for
	// them, and the entries built under a resting pointer take it for a
	// hover -- the selection jumps to whatever the mouse happens to be
	// over, and Accept then assigns that item. The dialog locks the list's
	// keys (disableInput) but not its mouse. Reached from the cross: cross,
	// dialog, dimmer, page, List_mc.
	void ShieldPipboyList(RE::Scaleform::GFx::Value& a_cross, bool a_on)
	{
		if (a_on) {
			RE::Scaleform::GFx::Value dialog;
			RE::Scaleform::GFx::Value dimmer;
			RE::Scaleform::GFx::Value page;
			if (!a_cross.GetMember("parent", &dialog) || !dialog.IsObject() ||
				!dialog.GetMember("parent", &dimmer) || !dimmer.IsObject() ||
				!dimmer.GetMember("parent", &page) || !page.IsObject() ||
				!page.GetMember("List_mc", &g_pipboyList) ||
				!g_pipboyList.IsDisplayObject()) {
				g_pipboyList = RE::Scaleform::GFx::Value();
				return;
			}
		}
		if (g_pipboyList.IsDisplayObject()) {
			g_pipboyList.SetMember(
				"mouseChildren", RE::Scaleform::GFx::Value(!a_on));
			g_pipboyList.SetMember(
				"mouseEnabled", RE::Scaleform::GFx::Value(!a_on));
			// Not enough on its own. After every data update the list runs
			// its own hit test with mouseX/mouseY -- SetFocusUnderMouse in
			// BSScrollingList.InvalidateData -- and no mouse setting stops
			// that. It runs only while bMouseDrivenNav is set, and the one
			// public way to clear that is SetPlatform: 1 is a controller,
			// and the list does nothing else with the number. The real
			// platform goes back when the panel comes down; a rollover
			// would set the flag again, which is what the shield above
			// is for.
			const std::array platform{
				RE::Scaleform::GFx::Value(
					a_on || HintDevice() == input::Device::kGamepad ? 1.0 : 0.0),
				RE::Scaleform::GFx::Value(false)
			};
			g_pipboyList.Invoke(
				"SetPlatform",
				nullptr,
				platform.data(),
				static_cast<std::uint32_t>(platform.size()));
		}
		if (!a_on) {
			g_pipboyList = RE::Scaleform::GFx::Value();
		}
	}

	// Twelve columns across, and as many rows down as there are pages, into
	// a rectangle that was not built for either.
	//
	// Both directions have to fit, so the cell is the smaller of the two
	// answers. Width alone was enough while there were four pages -- twelve
	// columns into the cross's 418 units give about 33, and four rows of
	// that need 138 of the 419 available. At eight pages it is 278, still
	// inside; at twelve it would not be, and the page count goes to
	// thirty-two.
	//
	// The gap and the row of key numbers are both fractions of the cell, so
	// the cell depends on what depends on it. Three rounds settle it well
	// below anything a screen can show.
	[[nodiscard]] double CellSizeFor(double a_width, double a_height, std::size_t a_rows)
	{
		const auto rows = static_cast<double>(std::max<std::size_t>(a_rows, 1));
		auto cell = std::min(a_width / 12.0, a_height / rows);
		for (int round = 0; round < 3; ++round) {
			const auto gap = std::max(cell * 0.06, 2.0);
			// What stands above the cells and must come out of the height
			// first: the key numbers and the air under them.
			const auto keyRow =
				std::max(cell * 0.36, 8.0) + g_gridWhere.keyRowGap;
			const auto byWidth = (a_width + gap) / 12.0 - gap;
			const auto byHeight = (a_height - keyRow + gap) / rows - gap;
			cell = std::min(byWidth, byHeight);
		}
		return std::clamp(cell, 12.0, 96.0);
	}

	// What the dialog has chosen, shown on our grid.
	//
	// This is the whole trick of step three, and it is a trick of *not*
	// building something. The cross is still there, still listening, still
	// assigning -- it is only invisible. So the player moves its selection
	// with the same keys as always and presses the same Accept, and the game
	// does the assigning it has always done. All that was missing is being
	// able to see it, and a page to do it on.
	//
	// The row is the page the engine is holding, which the page keys already
	// turn while the Pip-Boy is open (they always have -- see the polling
	// loop). So: read the cross's selectedIndex, mark that key on that row,
	// and a favorite lands on whichever page is showing.
	void SelectPipboySpot(std::size_t a_page, std::uint32_t a_slot);

	// Who holds the keyboard focus in the Pip-Boy, by name -- because
	// Accept in the assign dialog is a Keyboard.ENTER to whoever does, and
	// with the grid up it kept using the item instead of assigning it.
	// Said once per change, so the log shows who took it and when.
	[[nodiscard]] std::string PipboyFocusName()
	{
		auto* pipboy = GetMenu("PipboyMenu");
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value stage;
		RE::Scaleform::GFx::Value focus;
		if (!pipboy || !pipboy->uiMovie ||
			!pipboy->uiMovie->GetVariable(&root, "root") || !root.IsObject() ||
			!root.GetMember("stage", &stage) || !stage.IsObject() ||
			!stage.GetMember("focus", &focus)) {
			return "(no stage)";
		}
		if (!focus.IsObject()) {
			return "(nobody)";
		}
		RE::Scaleform::GFx::Value name;
		return focus.GetMember("name", &name) && name.IsString()
			? std::string(name.GetString())
			: "(unnamed)";
	}

	std::string g_lastFocus;

	void SayPipboyFocus(std::string_view a_when)
	{
		auto now = PipboyFocusName();
		if (now != g_lastFocus) {
			logger::info("pipboy: focus is on {} ({})", now, a_when);
			g_lastFocus = std::move(now);
		}
	}

	// Puts the focus on the cross, the way ShowHotkeys does when the
	// dialog opens. Measured first, then set, so the log says whether
	// it was anywhere else.
	// One line per Accept in the Pip-Boy's dialog: who holds the focus at
	// that instant, whether the dimmer (and so the dialog) is up, and
	// whether the list is taking input. Read on the UI thread, a moment
	// after the press -- close enough to say which of the two the
	// dialog did.
	void NoteAcceptInPipboy()
	{
		if (!g_pipboyGridUp) {
			return;
		}
		auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		tasks->AddUITask([]() {
			std::string dimmer = "?";
			std::string listInput = "?";
			std::string selected = "?";
			RE::Scaleform::GFx::Value dialog;
			RE::Scaleform::GFx::Value fade;
			RE::Scaleform::GFx::Value value;
			if (g_pipboyCross.IsDisplayObject() &&
				g_pipboyCross.GetMember("parent", &dialog) && dialog.IsObject() &&
				dialog.GetMember("parent", &fade) && fade.IsObject()) {
				if (fade.GetMember("visible", &value) && value.IsBoolean()) {
					dimmer = value.GetBoolean() ? "up" : "down";
				}
			}
			if (g_pipboyList.IsDisplayObject() &&
				g_pipboyList.GetMember("disableInput", &value) && value.IsBoolean()) {
				listInput = value.GetBoolean() ? "disabled" : "enabled";
			}
			if (g_pipboyCross.IsDisplayObject() &&
				g_pipboyCross.GetMember("selectedIndex", &value)) {
				selected = value.IsNumber() ? std::to_string(static_cast<int>(value.GetNumber()))
						: std::to_string(value.GetUInt());
			}
			logger::info(
				"pipboy: Accept pressed -- focus on {}, dimmer {}, list input {}, "
				"cross selectedIndex {}",
				PipboyFocusName(),
				dimmer,
				listInput,
				selected);
		});
	}

	void FocusPipboyCross()
	{
		SayPipboyFocus("before we set it");
		auto* pipboy = GetMenu("PipboyMenu");
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value stage;
		if (!pipboy || !pipboy->uiMovie || !g_pipboyCross.IsDisplayObject() ||
			!pipboy->uiMovie->GetVariable(&root, "root") || !root.IsObject() ||
			!root.GetMember("stage", &stage) || !stage.IsObject()) {
			return;
		}
		stage.SetMember("focus", g_pipboyCross);
		SayPipboyFocus("after we set it");
	}

	// The cell under the pointer, if it has moved onto one.
	[[nodiscard]] std::optional<grid::Spot> PipboyPointerSpot()
	{
		auto* pipboy = GetMenu("PipboyMenu");
		double x = 0.0;
		double y = 0.0;
		if (!pipboy || !grid::Pointer(pipboy, x, y) ||
			(x == g_pipboyPointerX && y == g_pipboyPointerY)) {
			return std::nullopt;
		}
		g_pipboyPointerX = x;
		g_pipboyPointerY = y;
		return grid::At(x, y);
	}

	// Every tick while the panel stands: is the focus still on the cross?
	//
	// Accept in the dialog is a Keyboard.ENTER to whoever holds the focus,
	// and the instrument of 2026-09-13 17:51 showed it plainly: every
	// press with the focus on Cross_mc assigned, every press with it on
	// List_mc used the item. Who moves it is FallUI: its InvListParser
	// ends every rebuild of the inventory list -- which our page switch
	// causes -- with stage.focus = List_mc, unconditionally. So the focus
	// is put back whenever it is found elsewhere, and the log counts how
	// often. (This was built once before and taken out again the same
	// evening, because Accept happened to work in that one test.)
	unsigned g_focusTaken = 0;

	void KeepPipboyFocus()
	{
		if (!g_pipboyGridUp || !g_pipboyCross.IsDisplayObject()) {
			return;
		}
		auto* pipboy = GetMenu("PipboyMenu");
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value stage;
		RE::Scaleform::GFx::Value focus;
		if (!pipboy || !pipboy->uiMovie ||
			!pipboy->uiMovie->GetVariable(&root, "root") || !root.IsObject() ||
			!root.GetMember("stage", &stage) || !stage.IsObject() ||
			!stage.GetMember("focus", &focus)) {
			return;
		}
		RE::Scaleform::GFx::Value name;
		const auto onCross = focus.IsObject() && focus.GetMember("name", &name) &&
			name.IsString() && std::string_view(name.GetString()) == "Cross_mc";
		if (onCross) {
			return;
		}
		++g_focusTaken;
		if (g_focusTaken <= 5 || g_focusTaken % 100 == 0) {
			logger::info(
				"pipboy: the focus was taken off the cross ({} times so far); "
				"put back",
				g_focusTaken);
		}
		stage.SetMember("focus", g_pipboyCross);
	}

	void RefreshPipboyGrid()
	{
		if (!g_pipboyGridUp || !g_pipboyCross.IsDisplayObject()) {
			return;
		}
		SayPipboyFocus("refresh");

		// The pointer first, because it is what a hand is on. Only when it
		// has moved, and only onto a cell; a row change goes the same way
		// the keys take, panel down and the watch drawing again.
		if (const auto over = PipboyPointerSpot()) {
			if (over->page != g_pipboyPage || over->slot != g_pipboySlot) {
				SelectPipboySpot(
					over->page, static_cast<std::uint32_t>(over->slot));
			}
			return;
		}

		// No pointer here, on purpose. It was let in once (597e70d), and
		// that is when the takeover began to crash on entry: whatever row
		// the mouse happened to rest on turned the page in the first tick,
		// which redrew at once and wrote selectedIndex into a cross the
		// game was still rebuilding for the twelve new keys. The keys make
		// the same turn only when somebody presses them, and the dialog
		// has keys of its own already. So: follow what the dialog chose.
		RE::Scaleform::GFx::Value chosen;
		if (!g_pipboyCross.GetMember("selectedIndex", &chosen)) {
			return;
		}
		const auto slot = chosen.IsNumber()
			? static_cast<std::uint32_t>(chosen.GetNumber())
			: static_cast<std::uint32_t>(chosen.GetUInt());
		EnsurePages();
		if (slot >= 12 || g_currentPage >= g_pages.size()) {
			return;
		}
		if (slot == g_pipboySlot && g_currentPage == g_pipboyPage) {
			return;
		}
		g_pipboySlot = slot;
		g_pipboyPage = g_currentPage;
		grid::Mark(grid::Spot{ g_currentPage, slot });
	}

	// The cross keeps its own keyUp listener. It was taken off for a day
	// (ce1d359) on the guess that its UP handler was what crashed; the
	// guess was wrong, and without the listener Accept stopped working:
	// its ENTER branch is what calls stopPropagation(), and an ENTER that
	// goes on past the page reaches the menu, where it means "use".

	// Takes the panel out of the Pip-Boy and gives the dialog its cross
	// back. Safe while the Pip-Boy is open: the clip our panel hangs on is
	// still there to be taken off. On the way out of the menu itself it is
	// Forget that is wanted instead -- see the close event.
	void TakePipboyGridDown()
	{
		if (!g_pipboyGridUp) {
			return;
		}
		// The panel, not the icon libraries. Those were loaded into the
		// Pip-Boy's own application domain and live as long as its movie
		// does; letting go of them here meant loading the same library
		// into the same domain again on the next draw, and the crash log
		// of 2026-09-12 16:22 ends in the Pip-Boy's own ActionScript,
		// reading a property of an object whose class is null. They go
		// when the movie goes -- see the close event.
		grid::Release();
		if (g_pipboyCross.IsDisplayObject()) {
			g_pipboyCross.SetMember("alpha", RE::Scaleform::GFx::Value(1.0));
			g_pipboyCross.SetMember(
				"mouseChildren", RE::Scaleform::GFx::Value(true));
		}
		ShieldPipboyList(g_pipboyCross, false);
		// The focus goes back to the list, which is where HideHotkeys puts
		// it; left on a cross nobody can see, it kept its rectangle.
		if (g_pipboyList.IsDisplayObject()) {
			if (auto* pipboy = GetMenu("PipboyMenu"); pipboy && pipboy->uiMovie) {
				RE::Scaleform::GFx::Value root;
				RE::Scaleform::GFx::Value stage;
				if (pipboy->uiMovie->GetVariable(&root, "root") && root.IsObject() &&
					root.GetMember("stage", &stage) && stage.IsObject()) {
					stage.SetMember("focus", g_pipboyList);
				}
			}
		}
		input::Listen(false);
		input::ClaimDirectionsOnly(false);
		ForgetPipboyGrid();
	}

	// FallUI's icon library in the Pip-Boy, if FallUI is there.
	//
	//   M8r.Service.IconLibrary.instance.makeTagIcon(keyword, size)
	//
	// is what FallUI's own list and cross use. The class is reached the
	// way FallUI reaches its own: through the application domain the
	// movie was loaded into, by name. Nothing is loaded, nothing is
	// created; a class that is not there answers with nothing, and that
	// is the whole of "no FallUI".
	[[nodiscard]] RE::Scaleform::GFx::Value FindPipboyIconMaker(RE::IMenu* a_pipboy)
	{
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value info;
		RE::Scaleform::GFx::Value domain;
		if (!a_pipboy || !a_pipboy->uiMovie ||
			!a_pipboy->uiMovie->GetVariable(&root, "root") || !root.IsObject() ||
			!root.GetMember("loaderInfo", &info) || !info.IsObject() ||
			!info.GetMember("applicationDomain", &domain) || !domain.IsObject()) {
			return {};
		}

		static bool said = false;
		const auto say = [](std::string_view a_what) {
			if (!said) {
				said = true;
				logger::info("pipboy: {}", a_what);
			}
		};

		const RE::Scaleform::GFx::Value name{ "M8r.Service.IconLibrary" };
		RE::Scaleform::GFx::Value has;
		if (!domain.Invoke("hasDefinition", &has, &name, 1) || !has.IsBoolean() ||
			!has.GetBoolean()) {
			say("no M8r.Service.IconLibrary in this movie -- no FallUI, so the "
				"cells go without symbols");
			return {};
		}
		RE::Scaleform::GFx::Value library;
		RE::Scaleform::GFx::Value instance;
		if (!domain.Invoke("getDefinition", &library, &name, 1) ||
			!library.IsObject() || !library.GetMember("instance", &instance) ||
			!instance.IsObject()) {
			say("M8r.Service.IconLibrary is there but would not give its "
				"instance -- the cells go without symbols");
			return {};
		}
		RE::Scaleform::GFx::Value loaded;
		const auto ready = instance.GetMember("isLoaded", &loaded) &&
			loaded.IsBoolean() && loaded.GetBoolean();
		say(ready ? "the symbols come from FallUI's own IconLibrary"
				  : "FallUI's IconLibrary is there but not loaded yet -- the "
					"cells go without symbols until it is");
		return ready ? instance : RE::Scaleform::GFx::Value();
	}

	// Puts the panel where the dialog's cross is, and takes the four
	// directions while it stands there.
	void DrawPipboyGrid(RE::Scaleform::GFx::Value& a_cross, std::string_view a_path)
	{
		// The rectangle to fill, in the coordinates of whatever holds the
		// cross -- the one frame of reference in there that means anything.
		// The clip between the dimmer and the cross was called instance402
		// in one run and instance389 in the next.
		RE::Scaleform::GFx::Value parent;
		if (!a_cross.GetMember("parent", &parent) || !parent.IsDisplayObject()) {
			logger::info("pipboy: the cross has no parent to draw on");
			return;
		}
		grid::Host host;
		host.parent = &parent;
		host.x = ReadNumber(a_cross, "x", 0.0);
		host.y = ReadNumber(a_cross, "y", 0.0);
		host.width = ReadNumber(a_cross, "width", 0.0);
		host.height = ReadNumber(a_cross, "height", 0.0);
		if (host.width <= 0.0 || host.height <= 0.0) {
			logger::info("pipboy: the cross has no size to fill");
			return;
		}

		auto* pipboy = GetMenu("PipboyMenu");
		if (!pipboy) {
			return;
		}

		// Whose icons. This movie is FallUI's house: its IconLibrary has
		// the same libraries loaded already, in child domains of its own,
		// and makes icons from keywords for its list and its cross. Ours
		// were the same files loaded a second time into the root domain --
		// which shadows a child's, so from then on FallUI's getDefinition
		// was handed our copy of every class, and four crash logs of
		// 2026-09-12 end in FallUI's list redrawing with a class that had
		// nothing behind it. So here the library is asked, not loaded:
		// nothing of ours goes into this movie's domains at all. Without
		// FallUI the cells carry their names and no symbol, the way they
		// do everywhere without a sorter.
		const auto maker = FindPipboyIconMaker(pipboy);

		// The same class as the favorites menu's cross -- EntryHolder_mc,
		// Quickkey_tf and all -- so the font is measured the same way.
		auto font = CrossFont(a_cross);
		if (!g_gridFont.empty()) {
			font = g_gridFont;
		}

		// Will this cross answer at all? The dialog is built when it is
		// asked for, and caught too early it has a clip but no entries yet
		// -- the first attempt drew with an empty font for exactly that
		// reason, GetEntryClip having come back with nothing. Writing a
		// selection into something in that state is not a thing to try.
		RE::Scaleform::GFx::Value chosen;
		if (!a_cross.GetMember("selectedIndex", &chosen) ||
			!(chosen.IsNumber() || chosen.IsUInt() || chosen.IsInt())) {
			static bool said = false;
			if (!said) {
				said = true;
				logger::info(
					"pipboy: the cross has no selectedIndex to write yet -- "
					"the watch asks again");
			}
			return;
		}

		// Transparent, not invisible. Accept in this dialog is not a user
		// event but a Keyboard.ENTER delivered to whatever holds
		// stage.focus, which ShowHotkeys sets to this cross -- and Flash
		// takes the focus off anything that turns invisible. With
		// visible=false the focus fell back to the item list, whose ENTER
		// is "use", and pressing Accept on a cell used the item instead of
		// assigning it. Alpha keeps the focus. The children stop taking
		// the mouse, because entries nobody can see should not choose
		// themselves when the pointer crosses them.
		a_cross.SetMember("alpha", RE::Scaleform::GFx::Value(0.0));
		a_cross.SetMember("mouseChildren", RE::Scaleform::GFx::Value(false));
		// Flash draws a yellow rectangle around whatever holds keyboard
		// focus, and a focus set from here counts as keyboard. The game's
		// own setting of it does not show one; ours did, 418 by 419,
		// exactly the cross, and it stayed after the dialog had gone.
		a_cross.SetMember("focusRect", RE::Scaleform::GFx::Value(false));

		const auto pages = BuildGridPages();

		// A panel drawn again -- icons arriving, a row change settling --
		// keeps its mark, or the mark blinks out for the hundred
		// milliseconds until the next refresh puts it back.
		std::optional<grid::Spot> marked;
		if (g_pipboyGridUp && g_pipboySlot < 12 && g_pipboyPage < pages.size()) {
			marked = grid::Spot{ g_pipboyPage, g_pipboySlot };
		}

		auto where = g_gridWhere;
		where.cellSize = CellSizeFor(host.width, host.height, pages.size());
		where.hint.clear();
		// The two lines the dialog already has of its own; ours would be a
		// second pair saying the same thing.
		where.labelSize = 1.0;
		where.detailSize = 1.0;
		where.labelGap = 0.0;

		host.iconMaker = maker.IsObject() ? &maker : nullptr;
		grid::Draw(
			pipboy,
			pipboy,
			font,
			pages,
			marked,
			g_gridColor <= 0xFFFFFF ? g_gridColor : HUDColor(),
			where,
			&host);
		// And no icons::Want here -- see FindPipboyIconMaker.

		// On a fresh takeover: a page and a key that cannot be the first
		// answer, so the first refresh always draws a mark. On a redraw the
		// mark is already known and was drawn just now.
		if (!g_pipboyGridUp) {
			g_pipboyPage = std::numeric_limits<std::size_t>::max();
			g_pipboySlot = 12;
		}
		g_pipboyGridUp = true;
		g_pipboyCross = a_cross;

		FocusPipboyCross();
		if (!g_pipboyList.IsDisplayObject()) {
			ShieldPipboyList(a_cross, true);
		}

		// The four directions, and only those: Accept belongs to the dialog.
		input::ClaimDirectionsOnly(true);
		input::Listen(true);

		if (!a_path.empty()) {
			logger::info(
				"pipboy: {} is {:.0f},{:.0f} {:.0f}x{:.0f}; the grid went in "
				"at cell {:.1f} for {} pages, written in \"{}\"",
				a_path,
				host.x,
				host.y,
				host.width,
				host.height,
				where.cellSize,
				pages.size(),
				font);
		}
	}

	// Finds the dialog's cross, if it is on screen at all. Shallow on
	// purpose: it lives four levels down --
	// PipboyMenu.<page>.ModalFadeRect_mc.<dialog>.Cross_mc -- and this is
	// asked several times a second, so there is no walking the whole tree
	// for it.
	[[nodiscard]] bool FindPipboyCross(
		RE::Scaleform::GFx::Value& a_cross, std::string& a_path)
	{
		RE::Scaleform::GFx::Value root;
		if (!PipboyRoot(root)) {
			return false;
		}
		a_path = "PipboyMenu";
		return FindByName(root, "Cross_mc", 5, a_cross, a_path) &&
			a_cross.IsDisplayObject();
	}

	// Every few ticks while the Pip-Boy is open: the dialog is built when it
	// is asked for and taken away when it is done, and there is no event for
	// either, so the grid follows what is on screen.
	void WatchPipboyDialog()
	{
		RE::Scaleform::GFx::Value cross;
		std::string path;
		const auto there = FindPipboyCross(cross, path);

		if (!there) {
			g_pipboySeen = 0;
			// A key chosen for a dialog that has since gone belongs to no
			// dialog that comes later.
			g_pipboyPending = 12;
			if (g_pipboyGridUp) {
				// The dialog is gone and our panel with it; the movie is
				// still alive, so this is an ordinary tidy-up.
				TakePipboyGridDown();
			} else if (g_pipboyList.IsDisplayObject()) {
				// The panel was already down -- a row change took it, and
				// the dialog closed before the watch drew again (a click
				// assigns, the mouse wanders on, the game closes the dialog
				// half a second later). The list is still shielded, and a
				// shielded list takes no mouse: that was "no item could be
				// reached until the Pip-Boy was opened again".
				ShieldPipboyList(g_pipboyCross, false);
			}
			return;
		}
		if (g_pipboyGridUp) {
			icons::Poll(GetMenu("PipboyMenu"), []() {
				if (g_pipboyCross.IsDisplayObject()) {
					std::string again;
					DrawPipboyGrid(g_pipboyCross, again);
				}
			});
			return;
		}
		// On sight. The dialog is found the moment it is created, and a
		// clip that exists is not yet a clip that is built -- so
		// DrawPipboyGrid asks the cross whether it answers yet and leaves
		// it alone when it does not, and this asks again on the next tick.
		// Waiting for a second sighting on principle was up to 400 ms of
		// the game's own cross before ours, and the player saw it.
		++g_pipboySeen;
		cross.SetMember("alpha", RE::Scaleform::GFx::Value(0.0));
		DrawPipboyGrid(cross, path);

		// A row change chose a key before it took the panel down; now that
		// the dialog has been found again and drawn on, the key goes in.
		if (g_pipboyGridUp && g_pipboyPending < 12) {
			const auto slot = g_pipboyPending;
			g_pipboyPending = 12;
			SelectPipboySpot(g_currentPage, slot);
		}
	}

	// One step through the grid in the Pip-Boy.
	//
	// Left and right walk the twelve keys of a row and carry on into the
	// next page at either end -- every key of every page in one line, which
	// is what the panel shows. Up and down change the page.
	//
	// The page is not a display matter here: it is *the page the favorite
	// lands on*. So changing rows turns the engine's own page, and the
	// dialog underneath -- which is still the thing doing the assigning --
	// then writes into that page's twelve keys without knowing anything
	// happened.
	//
	// The key within the page is handed to the hidden cross as its
	// selectedIndex, which is what its Accept reads. Between the two,
	// nothing of the assigning is ours.
	// Marks a cell in the Pip-Boy, and makes the dialog underneath agree.
	//
	// Two halves. The **page** is not a display matter here -- it is the
	// page the favorite lands on -- so a different row turns the engine's
	// own page and the twelve keys with it. The **key** is handed to the
	// hidden cross as its selectedIndex, which is what its Accept reads.
	//
	// Both the keys and the pointer come through here, so the two cannot
	// drift apart.
	void SelectPipboySpot(std::size_t a_page, std::uint32_t a_slot)
	{
		// Choosing draws again, drawing marks again, and marking is what
		// asked to choose. One at a time.
		static bool busy = false;
		if (busy) {
			return;
		}
		busy = true;
		const auto done = std::unique_ptr<bool, void (*)(bool*)>{
			&busy, [](bool* a_flag) { *a_flag = false; }
		};

		EnsurePages();
		if (!g_pipboyGridUp || a_page >= g_pages.size() || a_slot >= 12) {
			return;
		}

		if (a_page != g_currentPage) {
			GoToPage(a_page);
			// And nothing more in this breath. The twelve keys are
			// different now, and the game answers by rebuilding the cross's
			// entries for them; drawing again at once and writing
			// selectedIndex into that half-rebuilt cross is where the log
			// of 597e70d ended, and where the log of 2026-09-12 ended too,
			// after two row changes inside one second.
			//
			// So the panel comes down instead, and the watch takes it from
			// here: it finds the dialog again, draws on the second sighting
			// -- the path that held on entry three times that day -- and
			// only then hands the cross the key chosen here. The cross
			// stays hidden meanwhile; the game's own is not what should
			// flash through the gap.
			// The icon libraries stay: same movie, same classes -- see
			// TakePipboyGridDown.
			grid::Release();
			input::Listen(false);
			input::ClaimDirectionsOnly(false);
			g_pipboyGridUp = false;
			g_pipboySeen = 0;
			g_pipboyCross = RE::Scaleform::GFx::Value();
			g_pipboyPending = a_slot;
			return;
		}

		g_pipboyCross.SetMember(
			"selectedIndex", RE::Scaleform::GFx::Value(a_slot));
		g_pipboyPage = a_page;
		g_pipboySlot = a_slot;
		grid::Mark(grid::Spot{ a_page, a_slot });
	}

	void MovePipboyMark(int a_pages, int a_slots)
	{
		EnsurePages();
		if (!g_pipboyGridUp || g_pages.empty() ||
			!g_pipboyCross.IsDisplayObject()) {
			return;
		}

		const auto rows = static_cast<int>(g_pages.size());
		auto page = static_cast<int>(
			g_pipboyPage < g_pages.size() ? g_pipboyPage : g_currentPage);
		auto slot = static_cast<int>(g_pipboySlot < 12 ? g_pipboySlot : 0);

		// The same rule as the favorites menu: a row's ends are doors back
		// into the same row, or walls, as GridWrap says. Running on into
		// the next row was how section 51 built it, and it read as a
		// mistake at the keyboard.
		const auto step = [](int a_from, int a_by, int a_count) {
			const auto to = a_from + a_by;
			if (g_wrapNavigation) {
				return (to % a_count + a_count) % a_count;
			}
			return std::clamp(to, 0, a_count - 1);
		};
		slot = step(slot, a_slots, 12);
		page = step(page, a_pages, rows);

		SelectPipboySpot(
			static_cast<std::size_t>(page), static_cast<std::uint32_t>(slot));
	}

	// A click on the marked cell assigns, through the dialog's own
	// SelectItem -- what its ENTER calls. Only on the cell the pointer is
	// over and only when that cell is on the page being played: a click
	// on another row is a row change still on its way, and assigning
	// into the old page's keys is exactly the bug of section 61.
	void ClickPipboyCell()
	{
		if (!g_pipboyGridUp || !g_pipboyCross.IsDisplayObject()) {
			return;
		}
		auto* pipboy = GetMenu("PipboyMenu");
		double x = 0.0;
		double y = 0.0;
		if (!pipboy || !grid::Pointer(pipboy, x, y)) {
			return;
		}
		const auto over = grid::At(x, y);
		if (!over) {
			return;
		}
		if (over->page != g_currentPage) {
			logger::info(
				"pipboy: click on page {} while page {} is being played -- "
				"not assigned",
				over->page + 1,
				g_currentPage + 1);
			return;
		}
		SelectPipboySpot(over->page, static_cast<std::uint32_t>(over->slot));
		logger::info(
			"pipboy: click on page {} key {} -- the dialog assigns",
			over->page + 1,
			KeyLabel(over->slot));
		g_pipboyCross.Invoke("SelectItem");
	}

	void TogglePipboyCross()
	{
		if (g_pipboyGridUp) {
			TakePipboyGridDown();
			logger::info("pipboy: the grid is down, the cross is back");
			return;
		}
		RE::Scaleform::GFx::Value cross;
		std::string path;
		if (!FindPipboyCross(cross, path)) {
			logger::info(
				"pipboy: no Cross_mc anywhere -- is ASSIGN FAVORITE open?");
			return;
		}
		DrawPipboyGrid(cross, path);
	}

	void SurveyPipboy()
	{
		SurveyMenus();

		RE::Scaleform::GFx::Value root;
		if (!PipboyRoot(root)) {
			return;
		}
		auto* pipboy = GetMenu("PipboyMenu");
		if (!pipboy) {
			return;
		}
		RE::Scaleform::GFx::Value stage;
		if (root.GetMember("stage", &stage) && stage.IsDisplayObject()) {
			logger::info(
				"pipboy: the stage is {:.0f}x{:.0f}",
				ReadNumber(stage, "stageWidth", 0.0),
				ReadNumber(stage, "stageHeight", 0.0));
		}
		SurveyBranch(root, "PipboyMenu", g_surveyDepth > 0 ? g_surveyDepth : 8);
	}

	void ShowGrid()
	{
		// Only into a menu that is open. This is not belt and braces -- it
		// is a crash.
		//
		// Closing queues RestoreDefaultPage as a UI task, which turns a page,
		// and turning a page draws the panel again. That task runs after the
		// close event, while the movie is on its way out but still findable,
		// so the panel was rebuilt inside a menu about to be destroyed. Its
		// display objects then outlived their movie in our globals, and the
		// next opening began with Release() reaching into freed memory to
		// take them off a stage that no longer existed.
		//
		// The close event has already said what is true, so ask it.
		if (!g_favoritesMenuOpen.load()) {
			// Unless the panel is standing in the Pip-Boy, where turning a
			// page comes through here on its way to nowhere. Releasing then
			// would take that panel down under its own feet.
			if (!g_pipboyGridUp) {
				grid::Release();
			}
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

		// The font the cross labels its own keys with, which is one the menu
		// certainly has. Reported once: everything on the panel is written
		// in it, and a font that quietly fell back to the player's default
		// would look like a design decision rather than a miss.
		auto font = CrossFont(cross);
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

		// The keys, so the line under the panel can name them. A drawn
		// button is set larger than the words, so the field is told what the
		// tallest thing in it will be.
		g_gridWhere.hint = BuildHint();
		g_hintDevice = HintDevice();
		g_gridWhere.hintTallest =
			g_hintDevice == input::Device::kGamepad && !g_glyphFont.empty()
			? g_gridWhere.hintSize * g_glyphScale / 100.0
			: g_gridWhere.hintSize;

		// Our own menu, always. Three other canvases were tried and are
		// written up in the handoff; a menu of our own is the only one that
		// draws the whole panel and carries a pointer.
		auto* canvas = GetMenu(menu::kName);
		if (!canvas) {
			logger::warn("grid: {} is not open to draw on", menu::kName);
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

	// The game's own mouse pointer, which is a menu of its own: CursorMenu
	// (`RE::CursorMenu`, and it carries that name). Hiding it is the same
	// grip as the crosshair -- `visible` on the menu's own object.
	//
	// Nothing is kept between frames. An earlier version held the display
	// object in a global and wrote to it every frame; that is a reference
	// into another movie's heap, and the menu it belongs to comes and goes
	// on the game's own schedule, not ours. It is looked up each time
	// instead, which costs a hash lookup and owes nobody anything.
	//
	// Not through MenuCursor::UnregisterCursor either, which would be the
	// obvious way and the dangerous one: `registeredCursors` is a counter
	// shared with every other menu, and lowering it once too often takes the
	// pointer away from the whole game.
	bool g_pointerShown = true;

	void SetPointerVisible(bool a_on)
	{
		if (!g_hidePointer) {
			return;
		}
		auto* cursor = GetMenu("CursorMenu");
		if (!cursor || !cursor->menuObj.IsObject()) {
			static bool said = false;
			if (!said) {
				said = true;
				logger::info("pointer: no CursorMenu to hide -- it stays");
			}
			return;
		}
		auto root = cursor->menuObj;
		if (!root.IsDisplayObject()) {
			static bool said = false;
			if (!said) {
				said = true;
				logger::info(
					"pointer: CursorMenu has no display object -- it stays");
			}
			return;
		}
		root.SetMember("visible", RE::Scaleform::GFx::Value(a_on));
	}

	void ReleasePointerHiding()
	{
		if (!g_pointerShown) {
			SetPointerVisible(true);
		}
		g_pointerShown = true;
	}

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

	// Marking a cell is what turns to its page.
	//
	// Until now the page was turned at the moment of use, and the use
	// followed in the same breath -- one frame later at most. Everything we
	// can read says the twelve keys are right by then, and the engine still
	// equips what was there before, so something on its side is not finished
	// when we ask. What we do know is that a second press is right: time
	// between the switch and the use is what has ever helped.
	//
	// So the switch moves to where the time is. A player marks a cell and
	// then decides; between the two lie hundreds of frames, not one. And it
	// is the better rule anyway: what is marked is what the twelve keys
	// hold, always, so the grid stops being a picture of four pages and
	// becomes the one page you are pointing at.
	//
	// Only on a change of row, so walking along a row costs nothing.
	void SetMark(const std::optional<grid::Spot>& a_spot)
	{
		g_marked = a_spot;

		if (a_spot && a_spot->page != g_currentPage &&
			a_spot->page < g_pages.size()) {
			// GoToPage draws the panel again, which is what puts the row
			// that is now live under the mark.
			GoToPage(a_spot->page);
		}

		grid::Mark(g_marked);
		const auto lines = Describe(g_marked);
		grid::Say(lines.name, lines.what);
	}

	// Every frame the grid is up.
	void TrackPointer()
	{
		// First, and unconditionally: this is what tells the input thread
		// that the panel is really on screen. Everything below may return
		// early; this may not.
		input::Alive();

		KeepCrosshairDown();

		// The library arrives some frames after it was asked for, and the
		// panel was drawn before that. So when it lands, the panel is drawn
		// again -- once -- and this time its cells can carry symbols.
		if (auto* canvas = GetMenu(menu::kName)) {
			icons::Poll(canvas, []() { ShowGrid(); });
		}

		// A hand that left the keyboard for the controller, or the other way
		// round. The whole panel is drawn again for it, which is a lot for
		// one line of text -- but it happens when a player picks a different
		// thing up, not while they are using one.
		if (g_showHint && HintDevice() != g_hintDevice) {
			ShowGrid();
			return;
		}

		// The pointer sleeps while the keys are being used. While it does,
		// it is neither drawn nor asked -- the mark belongs to the keys
		// until the mouse or the right stick says otherwise.
		const auto awake = input::PointerAwake();
		if (awake != g_pointerShown) {
			g_pointerShown = awake;
			SetPointerVisible(awake);
		} else if (!awake) {
			// Every frame, the way the crosshair is kept down: the engine
			// puts its own cursor back whenever it feels like it.
			SetPointerVisible(false);
		}
		if (!awake) {
			return;
		}

		auto* canvas = GetMenu(menu::kName);
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
	// Uses one cell, with the twelve keys already holding its page.
	//
	// Split out of UseMarked for one reason: when a page had to be turned
	// first, this half has to happen a frame later. See there.
	void UseAt(std::size_t a_page, std::size_t a_slot)
	{
		EnsurePages();
		if (a_page >= g_pages.size() || a_slot >= 12) {
			return;
		}
		auto* object = g_pages[a_page][a_slot];
		if (!object || !use::Ready()) {
			return;
		}

		// Whether this press may take something off again rather than put it
		// on a second time. The engine decides what that means -- including
		// what it refuses inside power armour -- because it is the engine's
		// own boolean being turned around, not a second call of ours.
		//
		// But only for things that are worn or held. A stimpak is not put
		// on, it is used up, and asking the engine to take it off again
		// turned a swallowed chem into an equip and an unequip: press once,
		// nothing happens; press again, nothing happens. The boolean means
		// "and off again if it is already on", which is a sentence about
		// weapons and armour and about nothing else.
		//
		// Deliberately by form type rather than by asking whether the thing
		// happens to be equipped right now: an aid item that some mod makes
		// equippable is still an aid item, and the answer should not depend
		// on what the player is carrying at the time.
		const auto worn = object->GetFormType() == RE::ENUM_FORM_ID::kWEAP ||
			object->GetFormType() == RE::ENUM_FORM_ID::kARMO;
		// The engine's own copy of the twelve, brought into agreement in
		// this frame rather than an earlier one. It costs a walk of the
		// inventory; it is what the cross and the HUD read.
		SyncFavoritesCache();

		const auto used = use::Quickkey(
			static_cast<std::uint32_t>(a_slot), g_toggleEquip && worn);

		logger::info(
			"use: [{}] \"{}\" on page {} -- the game {}",
			KeyLabel(a_slot),
			RE::TESFullName::GetFullName(*object),
			a_page + 1,
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

	void UseMarked()
	{
		if (!g_marked) {
			logger::info("use: nothing is marked");
			return;
		}
		// What the panel believed was chosen, before anything is touched.
		// When a report and the log disagree about *which cell*, this is the
		// line that settles it -- everything after it is about the item, not
		// about the choosing.
		logger::info(
			"mark: page {} key {}", g_marked->page + 1, g_marked->slot + 1);
		EnsurePages();

		const auto spot = *g_marked;
		if (spot.page >= g_pages.size() || spot.slot >= 12) {
			return;
		}

		// The page being played is only in the inventory, so it is read back
		// before anything on it is looked up -- and which page that is, is
		// checked before it is written into.
		ReconcileCurrentPage();
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

		// The page should already be this one: marking a cell is what turns
		// to its page, and nothing can be used that was not marked first.
		//
		// This is the way back if that ever stops being true. It turns the
		// page and uses a frame later, which is what the mod did before --
		// and what did not work: the engine equipped the item that had been
		// on the key before the switch, every first press, however clean
		// every reading was in between. So this is a fallback that is known
		// to be poor, kept because doing nothing at all would be worse, and
		// it says so in the log if it is ever reached.
		if (spot.page != g_currentPage) {
			logger::warn(
				"use: page {} is not the one being played -- turning to it "
				"now, which is the timing that used to fail",
				spot.page + 1);
			GoToPage(spot.page);
			if (auto* tasks = F4SE::GetTaskInterface()) {
				const auto page = spot.page;
				const auto slot = spot.slot;
				tasks->AddUITask([page, slot]() { UseAt(page, slot); });
				return;
			}
		}

		UseAt(spot.page, spot.slot);
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
		return g_favoritesMenuOpen.load();
	}

	// From the input thread. Everything the actions do touches Scaleform or
	// the inventory, so none of it happens here.
	void OnAction(input::Action a_action)
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		// The same four directions mean two different things depending on
		// which panel is standing: our own menu, or the one inside the
		// Pip-Boy's assign dialog.
		const auto pipboy = g_pipboyGridUp;

		switch (a_action) {
		case input::Action::kPageUp:
			tasks->AddUITask([pipboy]() {
				pipboy ? MovePipboyMark(-1, 0) : MoveMark(-1, 0);
			});
			break;
		case input::Action::kPageDown:
			tasks->AddUITask([pipboy]() {
				pipboy ? MovePipboyMark(1, 0) : MoveMark(1, 0);
			});
			break;
		case input::Action::kSlotLeft:
			tasks->AddUITask([pipboy]() {
				pipboy ? MovePipboyMark(0, -1) : MoveMark(0, -1);
			});
			break;
		case input::Action::kSlotRight:
			tasks->AddUITask([pipboy]() {
				pipboy ? MovePipboyMark(0, 1) : MoveMark(0, 1);
			});
			break;
		case input::Action::kUse:
			tasks->AddUITask([pipboy]() {
				pipboy ? ClickPipboyCell() : UseMarked();
			});
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
			// A save from before the cap may carry more. They are read --
			// throwing away what a player put there is not ours to do
			// silently -- and EnsurePages trims to the setting afterwards,
			// so the log says it once rather than letting pages vanish
			// without a word.
			if (count == 0 || count > 32) {
				logger::warn("load: {} pages is not a number to trust", count);
				return;
			}
			if (count > static_cast<std::uint32_t>(kMostPages)) {
				logger::warn(
					"load: this save carries {} pages and the mod now offers "
					"{} -- the ones past that are still in the save, but "
					"nothing will show them",
					count,
					kMostPages);
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
		bool previousPeek = false;
		bool previousSurvey = false;
		bool previousCross = false;
		unsigned ticks = 0;
		bool previousNext = false;
		bool previousBack = false;

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
			// Counted here, every time round, and nowhere else. It used to
			// be counted inside the Pip-Boy refresh's own condition, behind
			// an && -- so it only moved while the panel stood, and froze
			// the moment the panel came down. The watch below divides by
			// eight, and a frozen count that is not a multiple of eight is
			// a watch that never runs again: the dialog emptied out on a
			// row change on 2026-09-12 and nobody ever came back for it.
			++ticks;

			if (!IsGameForeground()) {
				previousPeek = false;
				previousNext = false;
				previousBack = false;
				continue;
			}

			const auto peekNow = IsKeyDown(g_peekKey);
			const auto nextPage = IsKeyDown(g_nextPageKey);
			const auto previousPage = IsKeyDown(g_previousPageKey);
			const auto* tasks = F4SE::GetTaskInterface();

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

			const auto surveyNow = g_surveyKey != 0 && IsKeyDown(g_surveyKey);
			if (surveyNow && !previousSurvey) {
				// Scaleform, so not from here.
				if (tasks) {
					tasks->AddUITask([]() { SurveyPipboy(); });
				}
			}
			previousSurvey = surveyNow;

			const auto crossNow = g_crossKey != 0 && IsKeyDown(g_crossKey);
			if (crossNow && !previousCross && tasks) {
				tasks->AddUITask([]() { TogglePipboyCross(); });
			}
			previousCross = crossNow;

			// The default page, once whatever the menu was doing has
			// settled. See the close event.
			// And then, whichever page that leaves live, the word to
			// everyone who draws the favorites somewhere else.
			if (tasks && g_restoreIn > 0 && --g_restoreIn == 0) {
				tasks->AddUITask([]() {
					RestoreDefaultPage();
					AnnounceFavorites();
				});
			}

			// While the grid stands in the Pip-Boy, the mark follows the
			// dialog's own selection. Not every tick: a Scaleform read ten
			// times a second is plenty for a thumb, and forty would be
			// forty.
			if (g_pipboyGridUp && tasks && ticks % 4 == 0) {
				tasks->AddUITask([]() { RefreshPipboyGrid(); });
			}
			// And the focus every tick -- see KeepPipboyFocus.
			if (g_pipboyGridUp && tasks) {
				tasks->AddUITask([]() { KeepPipboyFocus(); });
			}

			// The panel is really there, which is what keeps the claim on
			// the four directions alive -- the same watchdog our own menu
			// stamps every frame (section 43).
			if (g_pipboyGridUp) {
				input::Alive();
			}

			// The assign dialog is built when it is asked for and taken away
			// when it is done, and there is no event for either -- so this
			// looked for it five times a second and took it over on sight.
			//
			// It crashes, and until that is understood it does not run. The
			// grid still goes into the Pip-Boy on PipboyCrossKey, which is
			// where it was when it was working; what is not known is which
			// of the two -- taking over unasked, or taking over that early
			// -- is the one that kills it, and shipping a crash to find out
			// is not a plan.
			// Quick while nothing stands, so the dialog is taken the tick it
			// appears; slow while the panel is up, when the only question is
			// whether the dialog has gone.
			if (g_pipboyOpen && g_pipboyAuto && tasks &&
				ticks % (g_pipboyGridUp ? 8 : 2) == 0) {
				tasks->AddUITask([]() { WatchPipboyDialog(); });
			}

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
			static const RE::BSFixedString pipboyMenu("PipboyMenu");
			if (a_event.menuName == pipboyMenu) {
				g_pipboyOpen = a_event.opening;
			}
			if (a_event.menuName == pipboyMenu && !a_event.opening) {
				// The Pip-Boy turns pages too -- every row change in the
				// assign dialog is one -- so leaving it is a close like the
				// favorites menu's: the default page goes back in a moment,
				// and whoever draws the favorites elsewhere is told.
				g_restoreIn = kRestoreDelayTicks;

				// The movie is going away, and with it every class the icon
				// libraries put into it -- whether or not the panel is
				// standing at this moment. Forgetting them only while it
				// stood left g_asked believing the next Pip-Boy, a new
				// movie, already had them.
				icons::Release();
				// And everything of ours that belongs to that movie -- the
				// panel's objects, the cross, the list -- is forgotten rather
				// than reached into (section 42). Whether or not the panel
				// stands: a row change takes it down and keeps the list, and
				// forgetting only while it stood kept that list into the
				// next Pip-Boy, a new movie, where the first SetMember on it
				// was the crash of 2026-09-13 00:12.
				input::Listen(false);
				input::ClaimDirectionsOnly(false);
				ForgetPipboyGrid();
				grid::Forget();
			}
			if (a_event.menuName == pipboyMenu && a_event.opening &&
				g_surveyDepth > 0) {
				// From a UI task: walking a display tree is Scaleform work,
				// and the event is not the thread for it.
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddUITask([]() { SurveyPipboy(); });
				}
			}

			static const RE::BSFixedString favoritesMenu("FavoritesMenu");
			if (a_event.menuName == favoritesMenu) {
				logger::info(
					"FavoritesMenu {}", a_event.opening ? "opened" : "closed");
				g_favoritesMenuOpen = a_event.opening;
				if (a_event.opening) {
					// A restore still counting down from the last close
					// would fire into an open menu, turn the page under the
					// panel, and RememberCurrentPage would then write those
					// twelve keys into whichever page it believed was
					// current. That is how a stored page ends up holding
					// another page's items.
					//
					// Waiting was right (section 57) and waiting has to be
					// interruptible: the reason to wait is gone the moment
					// the menu is back.
					g_restoreIn = 0;

					// And before anything is drawn: whose twelve keys are
					// these? A save carries them in the game's own file and
					// our pages beside it, and after a load the two can
					// name different pages. The panel draws the live keys as
					// the row it believes is current, so a wrong belief puts
					// the wrong items in the wrong row -- and then uses them
					// from there.
					ReconcileCurrentPage();
				}
				if (a_event.opening && g_logIcons) {
					g_logIconsDue.store(true);
				}

				// The field belongs to the movie that is going away, so it
				// is dropped on close and built again on the next open.
				if (!a_event.opening) {
					input::Listen(false);
					icons::Release();
					ShowCrosshair();
					ReleasePointerHiding();
					g_marked.reset();
					g_held.reset();
					ForgetPointer();
					grid::Release();
					menu::Hide();
					// After the menu is out of the way: the page switch
					// rewrites the twelve keys and refreshes the cross, and
					// there is no reason for either to happen behind a menu
					// that is still on screen.
					// Not now. In a moment.
					//
					// This is the bug that made using another page's cell
					// equip the *default* page's item instead, and it hid
					// behind three correct-looking log lines: the page was
					// switched, the inventory read right, the engine's cache
					// read right, and the game still drew the wrong weapon.
					//
					// Because equipping is not finished when UseQuickkeyItem
					// returns. It is queued -- an animation, a weapon drawn
					// -- and resolves a frame or more later, against
					// whatever the twelve keys hold *then*. With
					// GridCloseAfterUse the menu closes in the same breath,
					// and this restore rewrote all twelve back to the
					// default page before the queued equip ever looked.
					// Hence "always the first row": it was always whatever
					// DefaultPage points at.
					//
					// It also says why section 37 saw this work. DefaultPage
					// was still 0 then, so nothing rewrote anything and the
					// race had no second runner.
					g_restoreIn = kRestoreDelayTicks;
				} else {
					// Our own menu carries the grid. It answers with
					// SetOnReady once its movie is loaded, which is when
					// there is something to draw on.
					menu::Show();
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
		// The last word, whatever took the menu away. Only things that live
		// outside our own movie: it is the movie that is being destroyed.
		menu::SetOnGone([]() {
			input::Listen(false);
			ReleasePointerHiding();
			ShowCrosshair();
		});

		// The one call the grid cannot make up for itself. Looked for once,
		// here, so a failure is in the log before anyone clicks anything.
		use::Find();

		g_gridKeys.clear = g_clearKey;
		g_gridKeys.move = g_moveKey;
		input::SetKeys(g_gridKeys);
		input::SetPad(g_gridPad);
		input::SetRepeat(g_repeatDelay, g_repeatInterval);
		input::SetOnAction(&OnAction);
		input::Install();
		input::SetOnUsePassedThrough(NoteAcceptInPipboy);

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

	// No runtime is refused by its number. Every address this plugin
	// uses comes through the library, which resolves it for the running
	// executable and fails loudly when it cannot -- and the one function
	// found by hand, UseQuickkeyItem, is found by relationship and
	// refused when the relationship does not hold. See use.cpp.
	logger::info("runtime v{}", a_f4se->RuntimeVersion().string());
	return true;
}

// What F4SE 0.7 and later read instead of calling F4SEPlugin_Query: the
// plugin is address-independent (signatures through the library) and
// written against both structure layouts it declares.
namespace
{
	[[nodiscard]] constexpr F4SE::PluginVersionData MakePluginVersionData() noexcept
	{
		F4SE::PluginVersionData data{};
		data.pluginVersion =
			((PLUGIN_VERSION_MAJOR & 0xFF) << 24) |
			((PLUGIN_VERSION_MINOR & 0xFF) << 16) |
			((PLUGIN_VERSION_PATCH & 0xFFF) << 4);
		constexpr std::string_view name{ PLUGIN_LOG_NAME };
		for (std::size_t i = 0; i < name.size() && i + 1 < std::size(data.name); ++i) {
			data.name[i] = name[i];
		}
		data.addressIndependence =
			F4SE::PluginVersionData::kAddressIndependence_Signatures;
		data.structureIndependence =
			F4SE::PluginVersionData::kStructureIndependence_1_10_980Layout |
			F4SE::PluginVersionData::kStructureIndependence_1_11_137Layout;
		return data;
	}
}

extern "C" DLLEXPORT constinit F4SE::PluginVersionData F4SEPlugin_Version =
	MakePluginVersionData();

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	// Room for the one call this plugin redirects: the equip inside
	// UseQuickkeyItem, which is what lets a second press take something off
	// again. See use.cpp for why that is the only way in.
	F4SE::AllocTrampoline(64);

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::critical("could not register the message listener");
		return false;
	}

	// The pages live in the co-save. Registering has to happen here, in
	// Load, not later.
	// The library hands the interface out const and takes SetUniqueID
	// non-const; the call only records a number.
	if (auto* serialization = const_cast<F4SE::SerializationInterface*>(F4SE::GetSerializationInterface())) {
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
