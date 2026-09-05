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

	void LoadSettings()
	{
		const auto path = GetSettingsPath();
		std::error_code error;
		if (!std::filesystem::exists(path, error)) {
			logger::info("settings: no INI, using the defaults");
			return;
		}

		const auto read = [&](const wchar_t* a_key, int& a_target) {
			std::wstring value(64, L'\0');
			const auto length = GetPrivateProfileStringW(
				L"Debug",
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

		read(L"InventoryProbeKey", g_inventoryKey);
		read(L"FavoriteRoundTripKey", g_roundTripKey);
		read(L"RotateFavoritesKey", g_rotateKey);
		read(L"PeekKey", g_peekKey);

		logger::info(
			"settings: keys are {:#04x} (inventory), {:#04x} (round trip) and "
			"{:#04x} (rotate)",
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

	struct FavoriteSlot
	{
		RE::TESBoundObject* object{ nullptr };

		// Where the favorite sits in the inventory. The engine addresses a
		// stack as "the nth one of this item", so both halves are needed to
		// write to it.
		RE::BGSInventoryItem* item{ nullptr };
		std::int32_t stackIndex{ 0 };

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

		RE::BGSInventoryItem* walked = nullptr;
		std::int32_t stackIndex = 0;

		// ForEachStack does not lock, so every caller runs as a UI task.
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				// The stacks of an item come in order, so counting them
				// gives the number the engine wants.
				if (&a_item != walked) {
					walked = &a_item;
					stackIndex = 0;
				} else {
					++stackIndex;
				}

				if (!a_stack.extra) {
					return true;
				}
				const auto* favorite = a_stack.extra->GetByType<RE::ExtraFavorite>();
				if (!favorite) {
					return true;
				}
				const auto index = static_cast<int>(favorite->quickkeyIndex);
				if (index >= 0 && index < 12) {
					slots[static_cast<std::size_t>(index)] =
						FavoriteSlot{ a_item.object, &a_item, stackIndex, a_stack.count };
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

	// ---- Writing a favorite, the way the game does -----------------------
	//
	// Read out of the running game with tools/f4dis.py, after two attempts
	// that wrote past the engine and cost an item each. What the engine does
	// when it puts an item on a key is one function:
	//
	//     BGSInventoryItem::SetFavoriteIndex(stackIndex, favoriteIndex)
	//
	// and it does five things in a row -- find the stack, run the write
	// under the inventory lock, merge the stacks again, and, if anything
	// changed, send a notification. That last step is what every
	// hand-written version left out, and why an item that lost its key
	// could not be favorited again.
	//
	// Underneath sits ExtraDataList::SetFavorite, which reads:
	//
	//     0xFE          take the ExtraFavorite away -- not a favorite at all
	//     anything else set quickkeyIndex, creating the ExtraFavorite if the
	//                   stack has none
	//
	// So 0xFF is not "no favorite" but "a favorite without a key" -- and
	// that is the state the game itself writes when you favorite something
	// that has no key yet. A page switch therefore never has to delete
	// anything: the outgoing page goes to -1 and stays favorited.

	inline constexpr std::uint8_t kNoKey = 0xFF;
	inline constexpr std::uint8_t kNotAFavorite = 0xFE;

	void SetFavoriteIndex(
		RE::BGSInventoryItem* a_item,
		std::int32_t a_stackIndex,
		std::uint8_t a_favoriteIndex)
	{
		using func_t = void (*)(RE::BGSInventoryItem*, std::int32_t, std::uint8_t);
		static REL::Relocation<func_t> func{ REL::ID(1349090) };
		func(a_item, a_stackIndex, a_favoriteIndex);
	}

	// Moves the favorite on one key to another, or off the keys entirely
	// when a_to is negative. The inventory is read again for every move,
	// because merging stacks after a write can renumber them.
	bool MoveFavorite(int a_from, int a_to)
	{
		if (a_from < 0 || a_from >= 12) {
			return false;
		}

		const auto slots = ReadFavorites();
		const auto& slot = slots[static_cast<std::size_t>(a_from)];
		if (!slot.item) {
			logger::warn("move: [{}] holds nothing", KeyLabel(static_cast<std::size_t>(a_from)));
			return false;
		}

		SetFavoriteIndex(
			slot.item,
			slot.stackIndex,
			a_to < 0 ? kNoKey : static_cast<std::uint8_t>(a_to));

		logger::info(
			"move: \"{}\" {} -> {}",
			RE::TESFullName::GetFullName(*slot.object),
			KeyLabel(static_cast<std::size_t>(a_from)),
			a_to < 0 ? std::string("no key") : KeyLabel(static_cast<std::size_t>(a_to)));
		return true;
	}

	// ---- Setting all twelve keys at once ---------------------------------

	using PageLayout = std::array<RE::TESBoundObject*, 12>;

	// Applies a whole arrangement: afterwards the item named in slot i holds
	// key i.
	//
	// The target may only name items that hold a key right now. Giving a key
	// to an item that has none is a different operation -- it has to create
	// the ExtraFavorite -- and that is what the round trip further down
	// measures. Favorites the target does not mention keep a key: they stay
	// where they are if the target leaves that slot open, otherwise they take
	// the next free one. So the same items are favorited afterwards, only
	// arranged differently.
	void ApplyPage(const PageLayout& a_target)
	{
		const auto current = ReadFavorites();

		// Which key each slot of the target draws its item from. An item can
		// hold two keys with two stacks; the first match wins and the second
		// stack keeps its own key.
		std::array<int, 12> source;
		source.fill(-1);
		std::array<bool, 12> claimed{};

		for (std::size_t slot = 0; slot < 12; ++slot) {
			auto* wanted = a_target[slot];
			if (!wanted) {
				continue;
			}
			for (std::size_t key = 0; key < 12; ++key) {
				if (!claimed[key] && current[key].object == wanted) {
					claimed[key] = true;
					source[slot] = static_cast<int>(key);
					break;
				}
			}
			if (source[slot] < 0) {
				logger::warn(
					"page: \"{}\" holds no key -- [{}] takes whatever is left",
					RE::TESFullName::GetFullName(*wanted),
					KeyLabel(slot));
			}
		}

		// Everything the target does not mention keeps a key. Standing still
		// is the cheapest move, so that pass comes first.
		for (std::size_t key = 0; key < 12; ++key) {
			if (current[key].object && !claimed[key] && source[key] < 0) {
				claimed[key] = true;
				source[key] = static_cast<int>(key);
			}
		}
		for (std::size_t key = 0; key < 12; ++key) {
			if (!current[key].object || claimed[key]) {
				continue;
			}
			for (std::size_t slot = 0; slot < 12; ++slot) {
				if (source[slot] < 0) {
					claimed[key] = true;
					source[slot] = static_cast<int>(key);
					break;
				}
			}
		}

		// Turned around: where each key's item is headed.
		struct Move
		{
			int from;
			int to;
			RE::TESBoundObject* object;
		};

		std::vector<Move> pending;
		for (std::size_t slot = 0; slot < 12; ++slot) {
			const auto key = source[slot];
			if (key >= 0 && key != static_cast<int>(slot)) {
				pending.push_back(
					Move{ key,
						static_cast<int>(slot),
						current[static_cast<std::size_t>(key)].object });
			}
		}
		if (pending.empty()) {
			logger::info("page: everything is already where it belongs");
			return;
		}

		// The rule the loop follows: only write into a key that nobody needs
		// any more. `occupant` is who sits where while the moves run, and
		// `settled` marks the keys that already hold their final item.
		std::array<RE::TESBoundObject*, 12> occupant{};
		std::array<bool, 12> settled{};
		for (std::size_t key = 0; key < 12; ++key) {
			occupant[key] = current[key].object;
		}

		const auto leave = [&](int a_key) {
			if (!settled[static_cast<std::size_t>(a_key)]) {
				occupant[static_cast<std::size_t>(a_key)] = nullptr;
			}
		};

		while (!pending.empty()) {
			bool moved = false;
			for (auto move = pending.begin(); move != pending.end();) {
				if (occupant[static_cast<std::size_t>(move->to)]) {
					++move;
					continue;
				}
				MoveFavorite(move->from, move->to);
				leave(move->from);
				occupant[static_cast<std::size_t>(move->to)] = move->object;
				settled[static_cast<std::size_t>(move->to)] = true;
				move = pending.erase(move);
				moved = true;
			}
			if (moved) {
				continue;
			}

			// Nothing could move, so what is left is a ring: every key in it
			// waits for the next one. Breaking it takes a free key to park
			// on -- the same detour the two-key swap needed.
			int park = -1;
			for (std::size_t key = 0; key < 12; ++key) {
				if (!occupant[key] && !settled[key]) {
					park = static_cast<int>(key);
					break;
				}
			}

			auto& move = pending.front();
			if (park >= 0) {
				MoveFavorite(move.from, park);
				leave(move.from);
				occupant[static_cast<std::size_t>(park)] = move.object;
				move.from = park;
				continue;
			}

			// All twelve keys are taken, so the ring has to be broken on an
			// occupied one: for a moment two items carry the same index. Our
			// own compare functor is not confused by that -- it matches item
			// and index together -- but whether the engine's own copy
			// survives it has not been measured. If the log below is followed
			// by a cache that disagrees with the inventory, this is the spot.
			logger::warn(
				"page: all twelve keys are taken -- breaking the ring on an "
				"occupied key, which is the untested path");
			MoveFavorite(move.from, move.to);
			leave(move.from);
			settled[static_cast<std::size_t>(move.to)] = true;
			pending.erase(pending.begin());
		}

		LogFavorites("after the page");
	}

	// The test bench: every favorite moves up one key and the topmost one
	// wraps around. That is a single long ring, so it puts all of ApplyPage
	// to work, and enough presses bring everything back.
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

		PageLayout target{};
		for (std::size_t index = 0; index < occupied.size(); ++index) {
			target[occupied[(index + 1) % occupied.size()]] =
				current[occupied[index]].object;
		}
		ApplyPage(target);
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

	struct StackAddress
	{
		RE::BGSInventoryItem* item{ nullptr };
		std::int32_t index{ -1 };
	};

	// The stack of an item that carries a given favorite index. kNoKey finds
	// one that is favorited but on no key -- a parked favorite -- and
	// kNotAFavorite one that carries no favorite at all.
	[[nodiscard]] StackAddress FindStack(
		RE::TESBoundObject* a_object,
		std::uint8_t a_favoriteIndex)
	{
		StackAddress found;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList || !a_object) {
			return found;
		}

		RE::BGSInventoryItem* walked = nullptr;
		std::int32_t stackIndex = 0;

		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (&a_item != walked) {
					walked = &a_item;
					stackIndex = 0;
				} else {
					++stackIndex;
				}
				if (a_item.object != a_object) {
					return true;
				}

				auto carried = kNotAFavorite;
				if (a_stack.extra) {
					if (const auto* favorite =
							a_stack.extra->GetByType<RE::ExtraFavorite>()) {
						carried = static_cast<std::uint8_t>(favorite->quickkeyIndex);
					}
				}
				if (carried != a_favoriteIndex) {
					return true;
				}

				found = StackAddress{ &a_item, stackIndex };
				return false;
			});
		return found;
	}

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

			MoveFavorite(g_probeIndex, -1);
			logger::info(
				"round trip: \"{}\" is parked -- press again to give the key back",
				RE::TESFullName::GetFullName(*g_probeObject));
			LogFavorites("after parking one favorite");
			return;
		}

		const auto parked = FindStack(g_probeObject, kNoKey);
		if (!parked.item) {
			logger::warn(
				"round trip: \"{}\" is on no key and not parked either",
				RE::TESFullName::GetFullName(*g_probeObject));
			g_probeObject = nullptr;
			g_probeIndex = -1;
			return;
		}

		SetFavoriteIndex(
			parked.item, parked.index, static_cast<std::uint8_t>(g_probeIndex));
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

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			if (!IsGameForeground()) {
				previousInventory = false;
				previousRoundTrip = false;
				previousRotate = false;
				previousPeek = false;
				continue;
			}

			const auto inventory = IsKeyDown(g_inventoryKey);
			const auto roundTrip = IsKeyDown(g_roundTripKey);
			const auto rotate = IsKeyDown(g_rotateKey);
			const auto peekNow = IsKeyDown(g_peekKey);
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
			// Reading memory only, so this one needs no UI task.
			if (peekNow && !previousPeek) {
				peek::Run(GetSettingsPath());
			}

			previousInventory = inventory;
			previousRoundTrip = roundTrip;
			previousRotate = rotate;
			previousPeek = peekNow;
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

	logger::info("loaded");
	return true;
}
