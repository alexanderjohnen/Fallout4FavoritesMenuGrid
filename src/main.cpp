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
	// Every menu open and close is logged once, so the log itself answers
	// which menu the favorites cross lives in -- a question no header can.
	// The probe draws into the menu named here.
	constexpr auto kProbeMenu = "HUDMenu"sv;
	constexpr auto kProbeSprite = "FavoritesMenuGridProbe";

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

		RE::Scaleform::GFx::Value width;
		RE::Scaleform::GFx::Value height;
		if (stage.GetMember("stageWidth", &width) &&
			stage.GetMember("stageHeight", &height)) {
			logger::info(
				"probe: {} stage is {} x {}",
				a_menuName,
				width.IsNumber() ? width.GetNumber() : -1.0,
				height.IsNumber() ? height.GetNumber() : -1.0);
		}

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
