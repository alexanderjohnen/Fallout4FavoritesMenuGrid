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

namespace
{
	// ---- Settings -------------------------------------------------------
	//
	// The keys are in the INI because guessing them cost four rounds: F5 is
	// quicksave, F9 is quickload, and Special K sits on F8 and F9 here.

	int g_inventoryKey = VK_F6;
	int g_swapKey = VK_F8;

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
		read(L"SwapFavoritesKey", g_swapKey);

		logger::info(
			"settings: keys are {:#04x} (inventory) and {:#04x} (swap)",
			g_inventoryKey,
			g_swapKey);
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
						FavoriteSlot{ a_item.object, a_stack.count };
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
	// CommonLibF4 documents StackDataWriteFunctor::WriteDataImpl on vtable
	// slot 1. That is wrong: the vtable of the engine's own
	// ApplyChangesFunctor has the known address of its WriteDataImpl on
	// slot 0, so a plain subclass lines up with what the engine calls.

	class SetQuickkeyFunctor : public RE::BGSInventoryItem::StackDataWriteFunctor
	{
	public:
		explicit SetQuickkeyFunctor(std::int8_t a_index) noexcept :
			index(a_index)
		{
			// The whole stack moves, not a single item split off it.
			shouldSplitStacks = false;
		}

		void WriteDataImpl(
			RE::TESBoundObject&,
			RE::BGSInventoryItem::Stack& a_stack) override
		{
			if (a_stack.extra) {
				if (auto* favorite = a_stack.extra->GetByType<RE::ExtraFavorite>()) {
					favorite->quickkeyIndex = index;
				}
			}
		}

		std::int8_t index;
	};

	class MatchQuickkeyFunctor :
		public RE::BGSInventoryItem::StackDataCompareFunctor
	{
	public:
		explicit MatchQuickkeyFunctor(std::int8_t a_index) noexcept :
			index(a_index)
		{}

		bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
		{
			if (!a_stack.extra) {
				return false;
			}
			const auto* favorite = a_stack.extra->GetByType<RE::ExtraFavorite>();
			return favorite && favorite->quickkeyIndex == index;
		}

		std::int8_t index;
	};

	// Moves the favorite of a_object from one key to another. The engine
	// carries the rest: the display in the cross, storedFavTypes, and the
	// table that made the first key press after a hand-written swap act on
	// the old mapping.
	bool MoveFavorite(RE::TESBoundObject* a_object, int a_from, int a_to)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList || !a_object) {
			return false;
		}

		MatchQuickkeyFunctor compare{ static_cast<std::int8_t>(a_from) };
		SetQuickkeyFunctor set{ static_cast<std::int8_t>(a_to) };
		player->inventoryList->FindAndWriteStackDataForItem(
			a_object, compare, set);

		logger::info(
			"move: \"{}\" {} -> {}",
			RE::TESFullName::GetFullName(*a_object),
			KeyLabel(static_cast<std::size_t>(a_from)),
			KeyLabel(static_cast<std::size_t>(a_to)));
		return true;
	}

	// Exchanges two keys. The detour over a free slot matters: written
	// straight across, the compare functor would match the stack that was
	// just moved.
	void SwapFavorites(int a_first, int a_second)
	{
		if (a_first == a_second || a_first < 0 || a_second < 0 ||
			a_first >= 12 || a_second >= 12) {
			return;
		}

		const auto slots = ReadFavorites();
		auto* firstObject = slots[static_cast<std::size_t>(a_first)].object;
		auto* secondObject = slots[static_cast<std::size_t>(a_second)].object;

		// A free key to park in. With all twelve taken the swap needs a
		// different shape, which is a question for the page switch.
		int park = -1;
		for (std::size_t index = 0; index < slots.size(); ++index) {
			if (!slots[index].object) {
				park = static_cast<int>(index);
				break;
			}
		}

		if (firstObject && secondObject) {
			if (park < 0) {
				logger::warn("swap: all twelve keys are taken, nowhere to park");
				return;
			}
			MoveFavorite(firstObject, a_first, park);
			MoveFavorite(secondObject, a_second, a_first);
			MoveFavorite(firstObject, park, a_second);
		} else if (firstObject) {
			MoveFavorite(firstObject, a_first, a_second);
		} else if (secondObject) {
			MoveFavorite(secondObject, a_second, a_first);
		}

		LogFavorites("after the swap");
	}

	// The test bench for now: exchange the two lowest occupied keys. It is
	// the page switch in miniature and it is its own inverse.
	void SwapTwoLowest()
	{
		const auto slots = ReadFavorites();
		std::vector<int> occupied;
		for (std::size_t index = 0; index < slots.size(); ++index) {
			if (slots[index].object) {
				occupied.push_back(static_cast<int>(index));
			}
		}
		if (occupied.size() < 2) {
			logger::warn("swap: fewer than two favorites");
			return;
		}
		SwapFavorites(occupied[0], occupied[1]);
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
		bool previousSwap = false;

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			if (!IsGameForeground()) {
				previousInventory = false;
				previousSwap = false;
				continue;
			}

			const auto inventory = IsKeyDown(g_inventoryKey);
			const auto swap = IsKeyDown(g_swapKey);
			const auto* tasks = F4SE::GetTaskInterface();

			if (tasks && inventory && !previousInventory) {
				tasks->AddUITask([]() { LogFavorites("probe"); });
			}
			if (tasks && swap && !previousSwap) {
				tasks->AddUITask([]() { SwapTwoLowest(); });
			}

			previousInventory = inventory;
			previousSwap = swap;
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
