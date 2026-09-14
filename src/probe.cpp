#include "probe.h"

namespace
{
	bool (*g_active)() = nullptr;

	using ShouldFn = bool(RE::BSInputEventUser*, const RE::InputEvent*);
	using ButtonFn = void(RE::BSInputEventUser*, const RE::ButtonEvent*);
	using StickFn = void(RE::BSInputEventUser*, const RE::ThumbstickEvent*);

	constexpr std::int32_t kLeftThumbstick = 0x0B;

	// The module an address belongs to, by file name, or the offset alone.
	[[nodiscard]] std::string Whose(std::uintptr_t a_address)
	{
		HMODULE module = nullptr;
		if (::GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(a_address),
				&module) &&
			module) {
			wchar_t path[MAX_PATH]{};
			if (::GetModuleFileNameW(module, path, MAX_PATH)) {
				const auto name = std::filesystem::path(path).filename().string();
				return std::format("{}+{:#x}", name, a_address - reinterpret_cast<std::uintptr_t>(module));
			}
		}
		return std::format("{:#x}", a_address);
	}

	[[nodiscard]] const char* DeviceName(RE::INPUT_DEVICE a_device)
	{
		switch (a_device) {
		case RE::INPUT_DEVICE::kKeyboard:
			return "keyboard";
		case RE::INPUT_DEVICE::kMouse:
			return "mouse";
		case RE::INPUT_DEVICE::kGamepad:
			return "gamepad";
		default:
			return "other";
		}
	}

	[[nodiscard]] bool Active()
	{
		return g_active && g_active();
	}

	// What the grid keeps to itself: the four directions off the pad, by
	// the name the game gives them, and the left stick. Not the keyboard's
	// arrows -- those are not what jumped -- and not Accept, which is how
	// the dialog assigns.
	[[nodiscard]] bool Kept(const RE::InputEvent& a_event)
	{
		if (!Active()) {
			return false;
		}
		if (const auto* button = a_event.As<RE::ButtonEvent>()) {
			if (button->device.get() != RE::INPUT_DEVICE::kGamepad) {
				return false;
			}
			const std::string_view name{ button->QUserEvent() };
			return name == "Up" || name == "Down" || name == "Left" || name == "Right";
		}
		if (const auto* stick = a_event.As<RE::ThumbstickEvent>()) {
			return stick->idCode == kLeftThumbstick;
		}
		return false;
	}

	[[nodiscard]] std::string Describe(const RE::ButtonEvent& a_event)
	{
		const std::string_view name{ a_event.QUserEvent() };
		return std::format(
			"{} code {:#x} event '{}' {}",
			DeviceName(a_event.device.get()),
			static_cast<std::uint32_t>(a_event.idCode),
			name,
			a_event.QJustPressed()   ? "pressed" :
			a_event.value == 0.0F    ? "released" :
									   std::format("held {:.2f}s", a_event.QHeldDownSecs()));
	}

	// One door: three slots of one BSInputEventUser vtable, with what was
	// in them before, which is called for everything not kept.
	struct Door
	{
		const char* name = "";
		std::uintptr_t slotShould = 0;
		std::uintptr_t slotStick = 0;
		std::uintptr_t slotButton = 0;
		ShouldFn* should = nullptr;
		StickFn* stick = nullptr;
		ButtonFn* button = nullptr;
		ShouldFn* ourShould = nullptr;
		StickFn* ourStick = nullptr;
		ButtonFn* ourButton = nullptr;

		void Install(const REL::ID& a_vtable, ShouldFn* a_should, StickFn* a_stick, ButtonFn* a_button)
		{
			REL::Relocation<std::uintptr_t> vtable{ a_vtable };
			slotShould = vtable.address() + sizeof(void*) * 1;
			slotStick = vtable.address() + sizeof(void*) * 4;
			slotButton = vtable.address() + sizeof(void*) * 8;
			ourShould = a_should;
			ourStick = a_stick;
			ourButton = a_button;
			should = reinterpret_cast<ShouldFn*>(vtable.write_vfunc(1, a_should));
			stick = reinterpret_cast<StickFn*>(vtable.write_vfunc(4, a_stick));
			button = reinterpret_cast<ButtonFn*>(vtable.write_vfunc(8, a_button));
			logger::info(
				"pipboy: standing before {} at {:#x} -- before us: should {}, stick {}, button {}",
				name,
				vtable.address() - REL::Module::get().base(),
				Whose(reinterpret_cast<std::uintptr_t>(should)),
				Whose(reinterpret_cast<std::uintptr_t>(stick)),
				Whose(reinterpret_cast<std::uintptr_t>(button)));
		}

		void Check() const
		{
			if (!slotShould) {
				return;
			}
			const auto read = [](std::uintptr_t a_slot) {
				return *reinterpret_cast<std::uintptr_t*>(a_slot);
			};
			const auto s = read(slotShould);
			const auto t = read(slotStick);
			const auto b = read(slotButton);
			const bool ours =
				s == reinterpret_cast<std::uintptr_t>(ourShould) &&
				t == reinterpret_cast<std::uintptr_t>(ourStick) &&
				b == reinterpret_cast<std::uintptr_t>(ourButton);
			logger::info(
				"pipboy: the input slots of {} are {} -- should {}, stick {}, button {}",
				name,
				ours ? "still ours" : "NOT ours any more",
				Whose(s),
				Whose(t),
				Whose(b));
		}

		bool ShouldHandle(RE::BSInputEventUser* a_self, const RE::InputEvent* a_event) const
		{
			if (a_event && Kept(*a_event)) {
				// Said once per press, not per frame of a held stick.
				if (const auto* b = a_event->As<RE::ButtonEvent>(); b && b->QJustPressed()) {
					logger::info("pipboy: {} kept from {}", Describe(*b), name);
				}
				return false;
			}
			const auto answer = should(a_self, a_event);
			if (a_event && Active()) {
				if (const auto* b = a_event->As<RE::ButtonEvent>();
					b && b->QJustPressed() && b->device.get() == RE::INPUT_DEVICE::kGamepad) {
					logger::info(
						"pipboy: {} asked about {} -- {}",
						name,
						Describe(*b),
						answer ? "takes it" : "declines");
				}
			}
			return answer;
		}

		// The same gate on the slots themselves, in case anything hands
		// an event over without asking first.
		void OnButton(RE::BSInputEventUser* a_self, const RE::ButtonEvent* a_event) const
		{
			if (a_event && Kept(*a_event)) {
				return;
			}
			button(a_self, a_event);
		}

		void OnStick(RE::BSInputEventUser* a_self, const RE::ThumbstickEvent* a_event) const
		{
			if (a_event && Kept(*a_event)) {
				return;
			}
			stick(a_self, a_event);
		}
	};

	Door g_menu{ "PipboyMenu" };
	Door g_convert{ "GFxConvertHandler" };

	bool MenuShould(RE::BSInputEventUser* a_self, const RE::InputEvent* a_event) { return g_menu.ShouldHandle(a_self, a_event); }
	void MenuButton(RE::BSInputEventUser* a_self, const RE::ButtonEvent* a_event) { g_menu.OnButton(a_self, a_event); }
	void MenuStick(RE::BSInputEventUser* a_self, const RE::ThumbstickEvent* a_event) { g_menu.OnStick(a_self, a_event); }
	bool ConvertShould(RE::BSInputEventUser* a_self, const RE::InputEvent* a_event) { return g_convert.ShouldHandle(a_self, a_event); }
	void ConvertButton(RE::BSInputEventUser* a_self, const RE::ButtonEvent* a_event) { g_convert.OnButton(a_self, a_event); }
	void ConvertStick(RE::BSInputEventUser* a_self, const RE::ThumbstickEvent* a_event) { g_convert.OnStick(a_self, a_event); }
}

void probe::WatchPipboyMenu(bool (*a_active)())
{
	g_active = a_active;

	// PipboyMenu carries four vtables; the second is the input handler's
	// (BSInputEventUser at 0x10 of IMenu), the same layout that use.cpp
	// found on FavoritesManager. GFxConvertHandler is a BSInputEventUser
	// and nothing else. Slots: 1 ShouldHandleEvent, 4 the thumbstick, 8
	// the button. What was in them before is kept and called -- on
	// 2026-09-14 BakaFullscreenPipboy was already in two of the menu's.
	g_menu.Install(RE::VTABLE::PipboyMenu[1], &MenuShould, &MenuStick, &MenuButton);
	g_convert.Install(RE::VTABLE::GFxConvertHandler[0], &ConvertShould, &ConvertStick, &ConvertButton);
}

void probe::CheckPipboyMenu()
{
	g_menu.Check();
	g_convert.Check();
}
