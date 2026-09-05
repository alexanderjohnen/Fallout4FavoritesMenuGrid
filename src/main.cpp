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

#include "grid.h"
#include "peek.h"

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

		const std::array<std::pair<std::wstring_view, int>, 20> names{
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

		read(L"Debug", L"InventoryProbeKey", g_inventoryKey);
		read(L"Debug", L"FavoriteRoundTripKey", g_roundTripKey);
		read(L"Debug", L"RotateFavoritesKey", g_rotateKey);
		read(L"Debug", L"PeekKey", g_peekKey);

		// What the cross shows.
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
		g_gridWhere.backdrop =
			GetPrivateProfileIntW(L"Display", L"GridBackdrop", 0, path.c_str()) != 0;
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
		if (!menu || !menu->uiMovie || !menu->menuObj.IsObject()) {
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

	// Defined below, with the rest of the page marker: they need the pages,
	// and the pages need to show themselves.
	void ShowPageIndicator();
	void ShowGrid();

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
		if (g_pageMessage.empty()) {
			return;
		}
		const auto line = std::format(
			"{} {} / {}", g_pageMessage, g_currentPage + 1, g_pages.size());
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
	[[nodiscard]] std::vector<grid::Page> BuildGridPages()
	{
		EnsurePages();
		const auto live = ReadFavorites();

		std::vector<grid::Page> rows(g_pages.size());
		for (std::size_t row = 0; row < g_pages.size(); ++row) {
			for (std::size_t slot = 0; slot < 12; ++slot) {
				auto* object = row == g_currentPage ? live[slot].object
													: g_pages[row][slot];
				rows[row][slot].label = KeyLabel(slot);
				rows[row][slot].name = object
					? std::string(WithoutTag(RE::TESFullName::GetFullName(*object)))
					: std::string{};
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
		// marker is switched off and nothing has been measured yet.
		auto font = g_indicatorFontInUse;
		if (font == "?") {
			font = CrossFont(cross);
		}

		// Drawn on the HUD by default: the favorites menu only paints a
		// strip around its own cross, so anything of ours outside that never
		// reaches the screen.
		auto* canvas = GetMenu(g_gridWhere.canvas);
		if (!canvas) {
			logger::warn("grid: {} is not open to draw on", g_gridWhere.canvas);
			return;
		}

		grid::Draw(
			canvas,
			menu,
			font,
			PageWording(g_indicatorText),
			BuildGridPages(),
			g_currentPage,
			g_indicatorColor <= 0xFFFFFF ? g_indicatorColor : HUDColor(),
			g_gridWhere);
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
			if (tasks && nextPage && !previousNext) {
				tasks->AddUITask([]() { TurnPage(1); });
			}
			if (tasks && previousPage && !previousBack) {
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

				// The field belongs to the movie that is going away, so it
				// is dropped on close and built again on the next open.
				if (!a_event.opening) {
					ReleaseIndicator();
					grid::Release();
				} else if (const auto* tasks = F4SE::GetTaskInterface()) {
					// Not straight away: the menu is still being put
					// together while this event runs.
					tasks->AddUITask([]() {
						ShowPageIndicator();
						ShowGrid();
					});
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
