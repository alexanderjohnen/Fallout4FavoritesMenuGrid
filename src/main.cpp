// Milestone 0 of the Fallout 4 port: it changes nothing in the game and
// answers the three questions the headers cannot.
//
//   1. Does REL::ID(198281) resolve to FavoritesManager on 1.10.163, and
//      does the documented layout of storedFavTypes[12] hold?
//   2. Which menu hosts the favorites cross, and can a plugin reach that
//      menu's stage the same way the Starfield grid reaches the wheel's?
//   3. Does the stage accept a sprite of ours -- that is, can the grid be
//      drawn without replacing an interface file?
//
// Everything it does is a read or an addition. Nothing is written to the
// favorites, nothing existing in the menu is touched.

#include "PCH.h"

namespace
{
	// HUDMenu was the first guess and it worked, but Fallout 4 has a
	// FavoritesMenu of its own, and that is where the grid belongs: it
	// lives and dies with the favorites cross instead of sitting on the HUD
	// forever. Probing HUDMenu as well left a rectangle on screen for the
	// whole session, which is exactly the wrong place to leave one.
	constexpr auto kProbeMenu = "FavoritesMenu"sv;
	constexpr auto kProbeSprite = "FavoritesMenuGridProbe";

	// Scaleform hands the same number back as a double, an int or an
	// unsigned int depending on the property. Asking only for IsNumber is
	// how the first probe read the stage as -1 x -1.
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

	void DumpFavorites(std::string_view a_reason)
	{
		const auto* manager = RE::FavoritesManager::GetSingleton();
		if (!manager) {
			logger::warn("favorites: singleton is null ({})", a_reason);
			return;
		}

		logger::info(
			"favorites: singleton at {:p} ({})",
			static_cast<const void*>(manager),
			a_reason);

		for (std::size_t index = 0; index < 12; ++index) {
			const auto* form = manager->storedFavTypes[index];
			if (!form) {
				logger::info("favorites: slot {:2} empty", index);
				continue;
			}

			// GetFullName takes the form, not the component, and copes with
			// forms that carry no name at all.
			const auto name = RE::TESFullName::GetFullName(*form);
			logger::info(
				"favorites: slot {:2} form {:08X} type {:3} \"{}\"",
				index,
				form->formID,
				static_cast<int>(*form->formType),
				name);
		}

		// The ammo array sits right behind the slots. If its values look
		// like magazine counts, the offsets in the header hold; if they
		// look like garbage, something in the layout is off and nothing
		// above should be trusted either.
		std::string ammo;
		for (const auto value : manager->weaponLoadedAmmo) {
			ammo += std::format("{} ", value);
		}
		logger::info("favorites: loaded ammo [ {}]", ammo);

		// Two more fields from the same struct, as a check on the layout
		// itself. Twelve empty slots are equally consistent with a correct
		// read of an empty set and with a read of the wrong address; a
		// plausible component count and a boolean that is 0 or 1 make the
		// first reading much more likely.
		logger::info(
			"favorites: {} favorited components, allowStimpakUse={}",
			manager->favoritedComponents.size(),
			static_cast<int>(manager->allowStimpakUse));
	}

	// Adds an empty sprite to the menu's stage and fills a rectangle in it.
	// This is exactly what the Starfield grid does before it draws anything
	// real, so if it works here, the interface half of the port works.
	void ProbeStage(std::string_view a_menuName)
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			logger::warn("probe: no UI singleton");
			return;
		}

		const RE::BSFixedString menuName{ a_menuName };
		const auto menu = ui->GetMenu(menuName);
		if (!menu) {
			logger::info("probe: {} is not open", a_menuName);
			return;
		}
		if (!menu->uiMovie) {
			logger::warn("probe: {} has no movie", a_menuName);
			return;
		}
		if (!menu->menuObj.IsObject()) {
			logger::warn("probe: {} has no menu object", a_menuName);
			return;
		}

		RE::Scaleform::GFx::Value stage;
		if (!menu->menuObj.GetMember("stage", &stage) ||
			!stage.IsDisplayObject()) {
			// An AS2 movie has no stage member. That result is as
			// informative as a success: it decides how the grid is drawn.
			logger::warn(
				"probe: {} exposes no stage -- the movie is probably not AS3",
				a_menuName);
			return;
		}

		logger::info(
			"probe: {} stage is {} x {}",
			a_menuName,
			ReadNumber(stage, "stageWidth", -1.0),
			ReadNumber(stage, "stageHeight", -1.0));

		// What the menu object offers decides how a selection is fired
		// later: through the menu's own path, the way the Starfield grid
		// does it, or by equipping ourselves.
		logger::info(
			"probe: {} menuObj has ProcessUserEvent={} root={}",
			a_menuName,
			menu->menuObj.HasMember("ProcessUserEvent"),
			menu->menuObj.HasMember("root"));

		// Reuse the sprite across probes; a second one per menu opening
		// would pile up.
		RE::Scaleform::GFx::Value overlay;
		const RE::Scaleform::GFx::Value spriteName{ kProbeSprite };
		if (stage.Invoke("getChildByName", &overlay, &spriteName, 1) &&
			overlay.IsDisplayObject()) {
			logger::info("probe: the sprite from a previous probe is still there");
		} else {
			menu->uiMovie->CreateObject(&overlay, "flash.display.Sprite");
			if (!overlay.IsDisplayObject()) {
				logger::warn("probe: the movie refused to create a sprite");
				return;
			}
			overlay.SetMember("name", spriteName);
			if (!stage.Invoke("addChild", nullptr, &overlay, 1)) {
				logger::warn("probe: the stage refused the sprite");
				return;
			}
			logger::info("probe: sprite added to the stage of {}", a_menuName);
		}

		RE::Scaleform::GFx::Value graphics;
		if (!overlay.GetMember("graphics", &graphics) ||
			!graphics.IsObject()) {
			logger::warn("probe: the sprite has no graphics object");
			return;
		}

		graphics.Invoke("clear");

		const std::array fill{
			RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(0x1D2A36)),
			RE::Scaleform::GFx::Value(0.85)
		};
		graphics.Invoke("beginFill", nullptr, fill.data(), fill.size());

		const std::array rect{
			RE::Scaleform::GFx::Value(80.0),
			RE::Scaleform::GFx::Value(80.0),
			RE::Scaleform::GFx::Value(240.0),
			RE::Scaleform::GFx::Value(120.0)
		};
		if (!graphics.Invoke("drawRect", nullptr, rect.data(), rect.size())) {
			logger::warn("probe: drawRect was refused");
		}
		graphics.Invoke("endFill");

		logger::info("probe: rectangle drawn into {}", a_menuName);
	}

	// ---- The write test -------------------------------------------------
	//
	// Reading the twelve slots is settled. What is not is whether writing
	// them reaches the menu on screen, or whether the menu keeps its own
	// copy and has to be told. That decides the shape of the page switch,
	// so it is worth a test of its own before any of it is designed.
	//
	// The test reverses the twelve entries. Reversing is its own inverse:
	// pressing the key twice restores the original order exactly, and even
	// an interruption leaves a set of the player's own favorites rather
	// than something invented. No pointer is created or destroyed, so
	// nothing can dangle.
	//
	// F9  writes and does nothing else.
	// F10 writes and then asks the menu to update.
	//
	// If F9 alone moves the icons, a page switch is a plain write. If only
	// F10 does, the message belongs in the switch. If neither does while
	// the log shows the array reversed, the menu holds its own copy and
	// gets it from somewhere else -- and that is the next thing to find.
	// The keys come from the INI. Guessing them has cost four rounds now:
	// F5 is quicksave, F9 is quickload, and Steam's screenshot key sits
	// wherever the player has put it -- on this machine on F8 and F9. The
	// defaults below are only what applies when the file says nothing.
	int g_inventoryKey = VK_F6;
	int g_displayListKey = VK_F7;
	int g_swapKey = VK_F8;
	int g_refreshKey = VK_F10;
	int g_notifyKey = VK_NUMPAD3;

	[[nodiscard]] std::filesystem::path GetSettingsPath()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		const auto length = GetModuleFileNameW(
			nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path() /
			L"Data" / L"F4SE" / L"Plugins" / L"FavoritesMenuGrid.ini";
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
			logger::info("settings: no {}, using the defaults", path.string());
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
				logger::warn(
					"settings: could not read a key from \"{}\"",
					std::filesystem::path(value).string());
			}
		};

		read(L"InventoryProbeKey", g_inventoryKey);
		read(L"DisplayListKey", g_displayListKey);
		read(L"SwapFavoritesKey", g_swapKey);
		read(L"RefreshMenuKey", g_refreshKey);
		read(L"NotifyFavoriteKey", g_notifyKey);

		logger::info(
			"settings: keys are {:#04x} (inventory), {:#04x} (display list), {:#04x} (swap)",
			g_inventoryKey,
			g_displayListKey,
			g_swapKey);
		logger::info(
			"settings: refresh key is {:#04x}, notify key is {:#04x}",
			g_refreshKey,
			g_notifyKey);
	}


	// Saving and reloading picks up a swap, so the writes are right and they
	// last -- the game simply never looks again while it is running. What is
	// missing is the nudge. Rather than guess which message that is, the key
	// walks through the candidates one press at a time and says which one it
	// just sent, so the one that works can be recognised by its effect.
	// The menu's own functions, read out of FavoritesMenu.swf and then
	// confirmed present on Cross_mc: SetIsDirty, UpdateBrackets,
	// GetEntryClip, SelectItem. Calling one of those is a far better bet
	// than another UI message, so they go first; the messages stay at the
	// end as the fallback they have earned.
	[[nodiscard]] bool GetCross(RE::Scaleform::GFx::Value& a_cross)
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return false;
		}
		static const RE::BSFixedString menuName{ kProbeMenu };
		const auto menu = ui->GetMenu(menuName);
		if (!menu || !menu->menuObj.IsObject()) {
			logger::info("refresh: {} is not open", kProbeMenu);
			return false;
		}
		return menu->menuObj.GetMember("Cross_mc", &a_cross) &&
			a_cross.IsObject();
	}

	void InvokeOnCross(std::string_view a_name, bool a_withTrue)
	{
		RE::Scaleform::GFx::Value cross;
		if (!GetCross(cross)) {
			return;
		}
		const RE::Scaleform::GFx::Value argument{ true };
		const auto ok = a_withTrue
			? cross.Invoke(a_name.data(), nullptr, &argument, 1)
			: cross.Invoke(a_name.data(), nullptr);
		logger::info(
			"refresh: Cross_mc.{}({}) {}",
			a_name,
			a_withTrue ? "true" : "",
			ok ? "returned" : "was refused");
	}

	void SendMessage(RE::UI_MESSAGE_TYPE a_type, std::string_view a_name)
	{
		auto* queue = RE::UIMessageQueue::GetSingleton();
		if (!queue) {
			logger::warn("refresh: no UI message queue");
			return;
		}
		static const RE::BSFixedString menuName{ kProbeMenu };
		queue->AddMessage(menuName, a_type);
		logger::info("refresh: sent {} to {}", a_name, kProbeMenu);
	}

	// Every attempt to call one of the movie's functions found the menu
	// closed, even with the cross on screen -- most likely because the
	// cross reacts to the key press and closes, while our call runs a tick
	// later as a UI task. So the key no longer calls anything: it arms the
	// next step, and the step runs the moment the menu opens.
	std::atomic<void (*)()> g_pendingStep{ nullptr };
	std::atomic_bool g_pendingIsScaleform{ false };

	void RunPendingStep()
	{
		if (const auto step = g_pendingStep.exchange(nullptr)) {
			logger::info("refresh: running the armed step now that the cross is open");
			step();
		}
	}

	void SendNextRefreshMessage()
	{
		using Action = void (*)();
		static constexpr std::array<std::pair<Action, std::string_view>, 7> kSteps{
			std::pair{ +[]() { InvokeOnCross("SetIsDirty", false); }, "SetIsDirty()"sv },
			std::pair{ +[]() { InvokeOnCross("SetIsDirty", true); }, "SetIsDirty(true)"sv },
			std::pair{ +[]() { InvokeOnCross("UpdateBrackets", false); }, "UpdateBrackets()"sv },
			std::pair{ +[]() { InvokeOnCross("ClearIsDirty", false); }, "ClearIsDirty()"sv },
			std::pair{ +[]() { SendMessage(RE::UI_MESSAGE_TYPE::kShow, "kShow"sv); }, "kShow"sv },
			std::pair{ +[]() { SendMessage(RE::UI_MESSAGE_TYPE::kInventoryUpdate, "kInventoryUpdate"sv); }, "kInventoryUpdate"sv },
			std::pair{ +[]() { SendMessage(RE::UI_MESSAGE_TYPE::kUpdate, "kUpdate"sv); }, "kUpdate"sv }
		};

		static std::size_t next = 0;
		const auto [action, name] = kSteps[next % kSteps.size()];
		++next;

		// The first four talk to the movie and need the menu; the rest are
		// messages and can go out at once.
		if (next % kSteps.size() <= 4 && next % kSteps.size() != 0) {
			// kShow opens the cross, so the step does not have to wait for
			// the player: arm it, then ask the menu to appear, and it runs
			// the moment it does. One press, one complete attempt.
			logger::info("refresh: step {} -- {} armed, opening the cross", next, name);
			g_pendingStep.store(action);
			SendMessage(RE::UI_MESSAGE_TYPE::kShow, "kShow (to run the armed step)"sv);
		} else {
			logger::info("refresh: step {} -- {}", next, name);
			action();
		}
	}

	// Which slot the write functor's method sits on, read rather than
	// assumed. CommonLibF4 documents StackDataWriteFunctor::WriteDataImpl
	// as slot 1 and StackDataCompareFunctor::CompareData as slot 0, and
	// both classes have exactly one virtual method and no destructor -- so
	// one of the two is wrong, and passing a functor of our own to the
	// engine on the wrong assumption means it calls into nothing.
	//
	// ApplyChangesFunctor is a write functor of the engine's own, and its
	// WriteDataImpl has a known address. Finding that address in its vtable
	// says what the layout really is. This only reads memory.
	void DumpFunctorVTable()
	{
		const REL::Relocation<std::uintptr_t> vtable{
			RE::VTABLE::__ApplyChangesFunctor[0]
		};
		const REL::Relocation<std::uintptr_t> writeData{ REL::ID(1291190) };
		const auto base = REL::Module::get().base();

		logger::info(
			"vtable: ApplyChangesFunctor at {:X}, WriteDataImpl at {:X} (offsets from the module base)",
			vtable.address() - base,
			writeData.address() - base);

		const auto* slots = reinterpret_cast<const std::uintptr_t*>(vtable.address());
		for (std::size_t index = 0; index < 4; ++index) {
			const auto entry = slots[index];
			logger::info(
				"vtable: slot {} -> {:X}{}",
				index,
				entry > base ? entry - base : entry,
				entry == writeData.address() ? "   <-- WriteDataImpl" : "");
		}
	}

	void NotifyFavoriteChanged()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* manager = RE::FavoritesManager::GetSingleton();
		if (!player || !player->inventoryList || !manager) {
			logger::warn("notify: no inventory or no singleton");
			return;
		}

		std::vector<RE::BGSInventoryItem*> favorited;
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (a_stack.extra &&
					a_stack.extra->GetByType<RE::ExtraFavorite>()) {
					favorited.push_back(&a_item);
				}
				return true;
			});

		if (favorited.empty()) {
			logger::warn("notify: nothing is favorited");
			return;
		}

		auto* sink = static_cast<
			RE::BSTEventSink<RE::InventoryInterface::FavoriteChangedEvent>*>(
			manager);

		for (auto* item : favorited) {
			logger::info(
				"notify: telling the manager about \"{}\"",
				item->object ? RE::TESFullName::GetFullName(*item->object)
							 : "?"sv);
			RE::InventoryInterface::FavoriteChangedEvent event{ item };
			static_cast<void>(sink->ProcessEvent(event, nullptr));
		}

		logger::info("notify: done -- look at the cross");
		DumpFavorites("after notify");
	}

	// Names and key labels of the twelve slots, for a log that can be read
	// without holding the state of the game in your head.
	[[nodiscard]] std::string DescribeSlots(const RE::FavoritesManager& a_manager)
	{
		std::string line;
		for (std::size_t index = 0; index < 12; ++index) {
			const auto* form = a_manager.storedFavTypes[index];
			// The digit keys run 1..9, then 0, then - and =.
			const auto key = index < 9 ? std::string(1, static_cast<char>('1' + index))
				: index == 9           ? std::string("0")
				: index == 10          ? std::string("-")
									   : std::string("=");
			line += std::format(
				"[{}]{} ",
				key,
				form ? RE::TESFullName::GetFullName(*form) : "-"sv);
		}
		return line;
	}

	// Defined further down, with the rest of the inventory reading.
	void DumpInventoryFavorites();

	// Exchanges the two favorites with the lowest keys, by writing the
	// binding on the inventory stacks. The cache is deliberately left
	// alone: with both halves written, a notification test has nothing it
	// could be seen to fix.
	void RunFavoriteSwap()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* manager = RE::FavoritesManager::GetSingleton();
		if (!player || !player->inventoryList || !manager) {
			logger::warn("swap: no inventory or no singleton");
			return;
		}

		struct Bound
		{
			RE::ExtraFavorite* favorite;
			RE::TESBoundObject* object;
			int index;
		};
		std::vector<Bound> bound;

		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (!a_stack.extra) {
					return true;
				}
				if (auto* favorite = a_stack.extra->GetByType<RE::ExtraFavorite>()) {
					bound.push_back(
						{ favorite,
						  a_item.object,
						  static_cast<int>(favorite->quickkeyIndex) });
				}
				return true;
			});

		if (bound.size() < 2) {
			logger::warn("swap: fewer than two favorites to swap");
			return;
		}

		std::ranges::sort(bound, {}, &Bound::index);
		auto& first = bound[0];
		auto& second = bound[1];

		logger::info(
			"swap: \"{}\" is on key index {}, \"{}\" on {} -- exchanging them",
			first.object ? RE::TESFullName::GetFullName(*first.object) : "?"sv,
			first.index,
			second.object ? RE::TESFullName::GetFullName(*second.object) : "?"sv,
			second.index);

		first.favorite->quickkeyIndex = static_cast<std::int8_t>(second.index);
		second.favorite->quickkeyIndex = static_cast<std::int8_t>(first.index);

		logger::info("swap: bindings changed, cache deliberately left stale");
		logger::info("swap: cache still says [ {}]", DescribeSlots(*manager));
		DumpInventoryFavorites();
	}

	// Walks the player's inventory and reports every stack that carries an
	// ExtraFavorite. Writing storedFavTypes changed nothing, so this is the
	// other candidate for where a favorite really lives -- and unlike the
	// manager's array, it says which *stack* is bound, not just which kind
	// of item. That distinction decides how a page has to remember a
	// favorite it is not currently showing.
	// The few extra-data types worth naming in the log. Everything else is
	// printed as its raw number; the enum in BSExtraData.h translates it.
	[[nodiscard]] std::string_view NameExtraType(RE::EXTRA_DATA_TYPE a_type)
	{
		switch (a_type) {
		case RE::EXTRA_DATA_TYPE::kFavorite:
			return "Favorite"sv;
		case RE::EXTRA_DATA_TYPE::kHealth:
			return "Health"sv;
		case RE::EXTRA_DATA_TYPE::kCount:
			return "Count"sv;
		case RE::EXTRA_DATA_TYPE::kOwnership:
			return "Ownership"sv;
		case RE::EXTRA_DATA_TYPE::kCharge:
			return "Charge"sv;
		case RE::EXTRA_DATA_TYPE::kRank:
			return "Rank"sv;
		case RE::EXTRA_DATA_TYPE::kTimeLeft:
			return "TimeLeft"sv;
		case RE::EXTRA_DATA_TYPE::kPoison:
			return "Poison"sv;
		case RE::EXTRA_DATA_TYPE::kLock:
			return "Lock"sv;
		case RE::EXTRA_DATA_TYPE::kReferenceHandle:
			return "ReferenceHandle"sv;
		default:
			return {};
		}
	}

	// Everything hanging off a stack, by type. This is the whole question of
	// identity in one line: what a page has to write down about a favorite
	// so that it can find the same stack again later.
	[[nodiscard]] std::string DescribeExtras(const RE::ExtraDataList& a_extra)
	{
		std::string types;
		for (std::uint32_t raw = 0;
			 raw < static_cast<std::uint32_t>(RE::EXTRA_DATA_TYPE::kTotal);
			 ++raw) {
			const auto type = static_cast<RE::EXTRA_DATA_TYPE>(raw);
			if (!a_extra.HasType(type)) {
				continue;
			}
			const auto name = NameExtraType(type);
			types += name.empty() ? std::format("#{} ", raw)
								  : std::format("{} ", name);
		}
		return types.empty() ? std::string{ "none" } : types;
	}

	void DumpInventoryFavorites()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList) {
			logger::warn("inventory: no player inventory");
			return;
		}

		logger::info("inventory: --- stacks carrying ExtraFavorite ---");

		int stacksSeen = 0;
		int favoritesSeen = 0;
		// Which kinds of item hold a favorite. Their other stacks are worth
		// a second pass: a page has to tell the favorited 10mm from the
		// three others in the pocket, and this says what there is to tell
		// them apart by.
		std::vector<RE::TESBoundObject*> favoritedObjects;

		// ForEachStack does not lock; the whole probe runs as a UI task, so
		// nothing else is walking the list at the same time.
		player->inventoryList->ForEachStack(
			[](RE::BGSInventoryItem&) { return true; },
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				++stacksSeen;
				if (!a_stack.extra) {
					return true;
				}
				const auto* favorite =
					a_stack.extra->GetByType<RE::ExtraFavorite>();
				if (!favorite) {
					return true;
				}

				++favoritesSeen;
				if (a_item.object) {
					favoritedObjects.push_back(a_item.object);
				}
				const auto* object = a_item.object;
				// The digit keys run 1..9 then 0, so index 9 is key 0.
				const auto index = static_cast<int>(favorite->quickkeyIndex);
				logger::info(
					"inventory: quickkey {:2} (key {}) count {:3} form {:08X} \"{}\"",
					index,
					(index >= 0 && index <= 8) ? static_cast<char>('1' + index)
											   : (index == 9 ? '0' : '?'),
					a_stack.count,
					object ? object->formID : 0,
					object ? RE::TESFullName::GetFullName(*object) : "?"sv);
				return true;
			});

		logger::info(
			"inventory: {} stacks, {} of them favorited", stacksSeen, favoritesSeen);

		// Second pass: every stack of every favorited kind, with what makes
		// it distinguishable.
		logger::info("inventory: --- the stacks of those items in detail ---");
		player->inventoryList->ForEachStack(
			[&](RE::BGSInventoryItem& a_item) {
				return std::ranges::find(favoritedObjects, a_item.object) !=
					favoritedObjects.end();
			},
			[&](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				// The ordinal is the position in this item's chain of
				// stacks -- the closest thing to a handle the engine offers
				// here, and the very thing that moves when the inventory
				// changes.
				std::uint32_t ordinal = 0;
				for (auto* walk = a_item.stackData.get(); walk;
					 walk = walk->nextStack.get(), ++ordinal) {
					if (walk == &a_stack) {
						break;
					}
				}

				const auto* favorite =
					a_stack.extra ? a_stack.extra->GetByType<RE::ExtraFavorite>()
								  : nullptr;

				logger::info(
					"inventory:   \"{}\" stack {} at {:p} count {:3} flags {:#06x} quickkey {} extras [ {}]",
					a_item.object ? RE::TESFullName::GetFullName(*a_item.object)
								  : "?"sv,
					ordinal,
					static_cast<const void*>(&a_stack),
					a_stack.count,
					static_cast<std::uint16_t>(*a_stack.flags),
					favorite ? std::to_string(favorite->quickkeyIndex) : "-",
					a_stack.extra ? DescribeExtras(*a_stack.extra) : "none");
				return true;
			});

		// Side by side with the manager's array, so the log shows in one
		// place whether the two agree.
		DumpFavorites("inventory probe");
	}

	// Walks the display list of a menu and writes it to the log. The write
	// test showed the menu ignores the array while it is open, so the next
	// question is where it keeps what it does show. CommonLibF4 has no way
	// to enumerate the members of a Scaleform object, but the display list
	// can be walked by index, and the names in it are usually enough to
	// recognise the clips that hold the twelve slots.
	void DumpDisplayList(
		RE::Scaleform::GFx::Value& a_node,
		const std::string& a_path,
		int a_depth)
	{
		if (a_depth <= 0 || !a_node.IsObject()) {
			return;
		}

		const auto count = static_cast<int>(ReadNumber(a_node, "numChildren", 0.0));
		for (int index = 0; index < count; ++index) {
			const RE::Scaleform::GFx::Value argument{ index };
			RE::Scaleform::GFx::Value child;
			if (!a_node.Invoke("getChildAt", &child, &argument, 1) ||
				!child.IsDisplayObject()) {
				continue;
			}

			RE::Scaleform::GFx::Value name;
			const auto named =
				child.GetMember("name", &name) && name.IsString();
			const auto childPath = std::format(
				"{}.{}", a_path, named ? name.GetString() : "?");

			// Text and item id where there is any: that is what tells one
			// entry of the cross from another, and it is the first thing a
			// grid drawing its own cells would have to read.
			std::string detail;
			for (const auto* member : { "text", "htmlText", "itemId", "formId" }) {
				RE::Scaleform::GFx::Value value;
				if (child.GetMember(member, &value) && value.IsString()) {
					detail += std::format(" {}=\"{}\"", member, value.GetString());
				} else if (value.IsNumber() || value.IsInt() || value.IsUInt()) {
					detail += std::format(
						" {}={}", member, ReadNumber(child, member, 0.0));
				}
			}

			logger::info(
				"display: {} ({} children){}",
				childPath,
				static_cast<int>(ReadNumber(child, "numChildren", 0.0)),
				detail);

			DumpDisplayList(child, childPath, a_depth - 1);
		}
	}

	// Which functions the menu offers. The game fills the twelve cells at
	// load, so something can set one -- and if that something is reachable
	// as a function on the menu object, calling it is far safer than
	// writing past the engine and hoping it notices.
	//
	// HasMember only asks; nothing is invoked here.
	void ProbeMenuFunctions()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		static const RE::BSFixedString menuName{ kProbeMenu };
		const auto menu = ui->GetMenu(menuName);
		if (!menu || !menu->menuObj.IsObject()) {
			logger::info("functions: {} is not open", kProbeMenu);
			return;
		}

		// Not guesses any more. These are the identifiers inside
		// FavoritesMenu.swf itself, read out of the file after extracting
		// it from Fallout4 - Interface.ba2 with Archive2. Thirty invented
		// names had found nothing; the first look at the real ones turned
		// up SetIsDirty and _FavoritesInfoA.
		constexpr std::array kCandidates{
			"ProcessUserEvent"sv, "selectedIndex"sv,
			"SetIsDirty"sv, "ClearIsDirty"sv, "UpdateBrackets"sv,
			"GetEntryClip"sv, "SelectItem"sv, "ShouldHideSlot"sv,
			"_FavoritesInfoA"sv, "_EntryIndex"sv, "_HideEmptySlots"sv,
			"selectedEntry"sv, "hideEmptySlots"sv, "entryIndex"sv,
			"Quickkeys"sv, "FavIconType"sv, "useQuickkeyP"sv,
			"onFavEntryClick"sv, "itemPress"sv, "selectionUpdate"sv,
			"Cross_mc"sv, "EntryHolder_mc"sv, "Icon_mc"sv,
			"ItemName_tf"sv, "ItemAmmo_tf"sv, "Quickkey_tf"sv
		};

		const auto report = [&](const char* a_where,
								RE::Scaleform::GFx::Value& a_object) {
			if (!a_object.IsObject()) {
				return;
			}
			std::string found;
			for (const auto candidate : kCandidates) {
				if (a_object.HasMember(candidate.data())) {
					found += std::format("{} ", candidate);
				}
			}
			logger::info(
				"functions: {} has [ {}]",
				a_where,
				found.empty() ? "nothing of the list" : found);
		};

		report("menuObj", menu->menuObj);

		RE::Scaleform::GFx::Value cross;
		if (menu->menuObj.GetMember("Cross_mc", &cross)) {
			report("Cross_mc", cross);

			RE::Scaleform::GFx::Value holder;
			if (cross.GetMember("EntryHolder_mc", &holder)) {
				report("EntryHolder_mc", holder);

				RE::Scaleform::GFx::Value entry;
				const RE::Scaleform::GFx::Value entryName{ "Entry_2" };
				if (holder.Invoke("getChildByName", &entry, &entryName, 1)) {
					report("Entry_2", entry);
				}
			}
		}
	}

	void DumpMenuStructure()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		static const RE::BSFixedString menuName{ kProbeMenu };
		const auto menu = ui->GetMenu(menuName);
		if (!menu || !menu->menuObj.IsObject()) {
			logger::info("display: {} is not open", kProbeMenu);
			return;
		}

		logger::info("display: --- {} ---", kProbeMenu);
		DumpDisplayList(menu->menuObj, "menuObj", 4);

		RE::Scaleform::GFx::Value stage;
		if (menu->menuObj.GetMember("stage", &stage) &&
			stage.IsDisplayObject()) {
			DumpDisplayList(stage, "stage", 2);
		}
	}

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
		return (GetAsyncKeyState(a_key) & 0x8000) != 0;
	}

	// The thread only ever reads the keyboard and asks whether the menu is
	// open. Everything that touches the game runs as a UI task, on the
	// thread the engine expects.
	void KeyboardPollingLoop()
	{
		bool previousPlain = false;
		bool previousDisplay = false;
		bool previousInventory = false;
		bool previousRefresh = false;
		bool previousNotify = false;

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			// The menu no longer has to be open: writing while it is closed
			// is the more interesting half of the test.
			if (!IsGameForeground()) {
				previousPlain = false;
				previousDisplay = false;
				previousInventory = false;
				previousRefresh = false;
				previousNotify = false;
				continue;
			}

			const auto plain = IsKeyDown(g_swapKey);
			const auto display = IsKeyDown(g_displayListKey);
			const auto inventory = IsKeyDown(g_inventoryKey);
			const auto refresh = IsKeyDown(g_refreshKey);
			const auto notify = IsKeyDown(g_notifyKey);

			const auto* tasks = F4SE::GetTaskInterface();
			if (tasks && plain && !previousPlain) {
				// Logged on the way in, not only on the way out: a press
				// that never arrives looks exactly like one that arrived
				// and did nothing, and telling those two apart by
				// timestamps costs more than this line.
				logger::info("key: swap requested");
				tasks->AddUITask([]() { RunFavoriteSwap(); });
			}
			if (tasks && display && !previousDisplay) {
				logger::info("key: display list requested");
				tasks->AddUITask([]() {
					DumpMenuStructure();
					ProbeMenuFunctions();
				});
			}
			if (tasks && inventory && !previousInventory) {
				logger::info("key: inventory probe requested");
				tasks->AddUITask([]() { DumpInventoryFavorites(); });
			}
			if (tasks && refresh && !previousRefresh) {
				logger::info("key: refresh requested");
				tasks->AddUITask([]() { SendNextRefreshMessage(); });
			}
			if (tasks && notify && !previousNotify) {
				logger::info("key: notify requested");
				tasks->AddUITask([]() {
					DumpFunctorVTable();
					NotifyFavoriteChanged();
				});
			}

			previousPlain = plain;
			previousDisplay = display;
			previousInventory = inventory;
			previousRefresh = refresh;
			previousNotify = notify;
		}
	}

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
			const std::string_view name{ a_event.menuName.c_str() };
			logger::info("menu: {} {}", name, a_event.opening ? "opened" : "closed");

			// Every menu event is a moment on the UI thread with the game
			// in a known state -- the cheapest safe place to look at both
			// halves at once.
			DumpFavorites(a_event.opening ? "menu opened" : "menu closed");

			// Only on the way in, and only for our own menu. Drawing into
			// a menu that is closing is what crashed the game at 01:31:58
			// on 2026-09-05; a menu being torn down is not a canvas.
			if (a_event.opening && name == kProbeMenu) {
				ProbeStage(kProbeMenu);
				RunPendingStep();
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
		logger::info("menu watch registered");

		std::thread(KeyboardPollingLoop).detach();
		logger::info(
			"armed -- the keys are in FavoritesMenuGrid.ini; {} is the menu being probed",
			kProbeMenu);

		DumpFavorites("game data ready");
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
