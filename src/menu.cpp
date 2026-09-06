#include "PCH.h"

#include "menu.h"

namespace
{
	// The movie tools/build_swf.py writes, relative to Data.
	constexpr auto kMoviePath = "Interface/FavoritesMenuGrid.swf";

	void (*g_ready)() = nullptr;
	void (*g_advance)() = nullptr;

	class GridMenu : public RE::IMenu
	{
	public:
		GridMenu()
		{
			auto* scaleform = RE::BSScaleformManager::GetSingleton();
			if (!scaleform) {
				logger::error("menu: no scaleform manager");
				return;
			}

			// The stage is the movie's own, so nothing here is clipped to
			// anyone else's business, and the background stays transparent:
			// a favorites menu that dims the fight is no use to anybody.
			if (!scaleform->LoadMovieEx(
					*this,
					kMoviePath,
					""sv,
					RE::Scaleform::GFx::Movie::ScaleModeType::kShowAll,
					0.0F)) {
				logger::error("menu: {} would not load", kMoviePath);
				return;
			}

			// Cursor and input, without pausing: the point of a favorites
			// menu is to be quick, and a paused game behind it would be a
			// different thing entirely. kUsesMenuContext is what takes the
			// mouse off the camera and gives it to the menu.
			// No kCustomRendering. It says the menu paints itself, and the
			// game then leaves it out of its own pass -- the grid was drawn,
			// forty children and all, onto a movie nobody rendered.
			menuFlags.set(
				RE::UI_MENU_FLAGS::kUsesCursor,
				RE::UI_MENU_FLAGS::kUsesMenuContext,
				RE::UI_MENU_FLAGS::kUpdateUsesCursor,
				RE::UI_MENU_FLAGS::kRequiresUpdate);

			// Above the HUD, below anything the player opens on purpose.
			depthPriority = RE::UI_DEPTH_PRIORITY::kStandard;

			logger::info("menu: {} is up", menu::kName);

			// Not from in here: the menu only joins the game's list once
			// this constructor has returned, and whatever draws on it has
			// to be able to find it.
			if (g_ready) {
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddUITask([]() {
						if (g_ready) {
							g_ready();
						}
					});
				}
			}
		}

		// Every frame the menu is up. The base class does the drawing; what
		// is added is the one question that has to be asked again and again
		// -- where the pointer is now.
		void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override
		{
			RE::IMenu::AdvanceMovie(a_timeDelta, a_time);
			if (g_advance) {
				g_advance();
			}
		}

		static RE::IMenu* Create(const RE::UIMessage&)
		{
			return new GridMenu();
		}
	};
}

void menu::Register()
{
	auto* ui = RE::UI::GetSingleton();
	if (!ui) {
		logger::error("menu: no UI to register with");
		return;
	}
	ui->RegisterMenu(kName, GridMenu::Create);
	logger::info("menu: {} is registered", kName);
}

void menu::SetOnReady(void (*a_ready)())
{
	g_ready = a_ready;
}

void menu::SetOnAdvance(void (*a_advance)())
{
	g_advance = a_advance;
}

void menu::Show()
{
	if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
		queue->AddMessage(kName, RE::UI_MESSAGE_TYPE::kShow);
	}
}

void menu::Hide()
{
	if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
		queue->AddMessage(kName, RE::UI_MESSAGE_TYPE::kHide);
	}
}

bool menu::IsOpen()
{
	auto* ui = RE::UI::GetSingleton();
	return ui && ui->GetMenuOpen(kName);
}
