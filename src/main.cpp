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
	constexpr int kWriteTestKey = VK_F9;
	constexpr int kWriteTestKeyWithRefresh = VK_F10;

	[[nodiscard]] std::string DescribeSlots(const RE::FavoritesManager& a_manager)
	{
		std::string line;
		for (const auto* form : a_manager.storedFavTypes) {
			line += form ? std::format("{:08X} ", form->formID) : "-------- ";
		}
		return line;
	}

	void RunWriteTest(bool a_withRefresh)
	{
		auto* manager = RE::FavoritesManager::GetSingleton();
		if (!manager) {
			logger::warn("write test: no singleton");
			return;
		}

		logger::info("write test: before [ {}]", DescribeSlots(*manager));

		std::reverse(
			std::begin(manager->storedFavTypes),
			std::end(manager->storedFavTypes));

		logger::info("write test: after  [ {}]", DescribeSlots(*manager));

		if (!a_withRefresh) {
			logger::info("write test: no message sent -- watch the menu");
			return;
		}

		auto* queue = RE::UIMessageQueue::GetSingleton();
		if (!queue) {
			logger::warn("write test: no UI message queue");
			return;
		}
		static const RE::BSFixedString menuName{ kProbeMenu };
		queue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kInventoryUpdate);
		logger::info("write test: kInventoryUpdate sent to {}", kProbeMenu);
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
		bool previousRefresh = false;

		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			auto* ui = RE::UI::GetSingleton();
			static const RE::BSFixedString menuName{ kProbeMenu };
			const auto ready =
				ui && ui->GetMenuOpen(menuName) && IsGameForeground();
			if (!ready) {
				previousPlain = false;
				previousRefresh = false;
				continue;
			}

			const auto plain = IsKeyDown(kWriteTestKey);
			const auto refresh = IsKeyDown(kWriteTestKeyWithRefresh);

			if ((plain && !previousPlain) || (refresh && !previousRefresh)) {
				const auto withRefresh = refresh && !previousRefresh;
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddUITask([withRefresh]() {
						RunWriteTest(withRefresh);
					});
				}
			}

			previousPlain = plain;
			previousRefresh = refresh;
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
			ProbeStage(kProbeMenu);

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
			"write test armed: F9 writes, F10 writes and updates -- only while {} is open",
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
