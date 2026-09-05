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
	int g_roundTripKey = VK_F7;
	int g_rotateKey = VK_F8;

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
				MoveFavorite(move->object, move->from, move->to);
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
				MoveFavorite(move.object, move.from, park);
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
			MoveFavorite(move.object, move.from, move.to);
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
	// A page holding more than twelve items needs this: items of the outgoing
	// page lose their key, items of the incoming page get one. In Fallout 4 a
	// favorite is nothing but the ExtraFavorite on its inventory stack, so the
	// question is whether the engine follows when that one comes and goes --
	// through the same write path that carries a move.
	//
	// The probe is a round trip on one key: the first press takes the lowest
	// favorite away, the second gives it back.

	class ClearFavoriteFunctor : public RE::BGSInventoryItem::StackDataWriteFunctor
	{
	public:
		ClearFavoriteFunctor() noexcept { shouldSplitStacks = false; }

		void WriteDataImpl(
			RE::TESBoundObject&,
			RE::BGSInventoryItem::Stack& a_stack) override
		{
			// ExtraDataList::ClearFavorite (REL::ID(254434)) does the same
			// from the engine's side, if plain removal turns out to leave
			// something behind.
			if (a_stack.extra) {
				a_stack.extra->RemoveExtra<RE::ExtraFavorite>();
			}
		}
	};

	class AddFavoriteFunctor : public RE::BGSInventoryItem::StackDataWriteFunctor
	{
	public:
		explicit AddFavoriteFunctor(std::int8_t a_index) noexcept :
			index(a_index)
		{
			shouldSplitStacks = false;
		}

		void WriteDataImpl(
			RE::TESBoundObject&,
			RE::BGSInventoryItem::Stack& a_stack) override
		{
			if (!a_stack.extra || a_stack.extra->GetByType<RE::ExtraFavorite>()) {
				return;
			}

			// ExtraFavorite carries no constructor of its own, so type and
			// vtable are set the way CommonLibF4 sets them for every other
			// piece of extra data.
			auto* favorite = new RE::ExtraFavorite();
			RE::stl::emplace_vtable(favorite);
			favorite->type = RE::EXTRA_DATA_TYPE::kFavorite;
			favorite->quickkeyIndex = index;
			a_stack.extra->AddExtra(favorite);
		}

		std::int8_t index;
	};

	// A stack of the item that carries no favorite -- where a key can go.
	class MatchPlainStackFunctor :
		public RE::BGSInventoryItem::StackDataCompareFunctor
	{
	public:
		bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
		{
			return a_stack.extra &&
				!a_stack.extra->GetByType<RE::ExtraFavorite>();
		}
	};

	RE::TESBoundObject* g_probeObject = nullptr;
	int g_probeIndex = -1;

	void FavoriteRoundTrip()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList) {
			return;
		}

		if (g_probeIndex < 0) {
			const auto current = ReadFavorites();
			for (std::size_t key = 0; key < current.size(); ++key) {
				if (current[key].object) {
					g_probeObject = current[key].object;
					g_probeIndex = static_cast<int>(key);
					break;
				}
			}
			if (g_probeIndex < 0) {
				logger::warn("round trip: there is no favorite to take away");
				return;
			}

			MatchQuickkeyFunctor compare{ static_cast<std::int8_t>(g_probeIndex) };
			ClearFavoriteFunctor clear;
			player->inventoryList->FindAndWriteStackDataForItem(
				g_probeObject, compare, clear);

			logger::info(
				"round trip: took \"{}\" off [{}] -- press again to give it back",
				RE::TESFullName::GetFullName(*g_probeObject),
				KeyLabel(static_cast<std::size_t>(g_probeIndex)));
			LogFavorites("after taking a key away");
			return;
		}

		MatchPlainStackFunctor compare;
		AddFavoriteFunctor add{ static_cast<std::int8_t>(g_probeIndex) };
		player->inventoryList->FindAndWriteStackDataForItem(
			g_probeObject, compare, add);

		logger::info(
			"round trip: gave \"{}\" back to [{}]",
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

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			if (!IsGameForeground()) {
				previousInventory = false;
				previousRoundTrip = false;
				previousRotate = false;
				continue;
			}

			const auto inventory = IsKeyDown(g_inventoryKey);
			const auto roundTrip = IsKeyDown(g_roundTripKey);
			const auto rotate = IsKeyDown(g_rotateKey);
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

			previousInventory = inventory;
			previousRoundTrip = roundTrip;
			previousRotate = rotate;
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
