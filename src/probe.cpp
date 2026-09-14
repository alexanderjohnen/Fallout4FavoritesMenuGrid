#include "probe.h"

namespace
{
	bool (*g_active)() = nullptr;

	using ShouldFn = bool(RE::BSInputEventUser*, const RE::InputEvent*);
	using ButtonFn = void(RE::BSInputEventUser*, const RE::ButtonEvent*);
	using StickFn = void(RE::BSInputEventUser*, const RE::ThumbstickEvent*);
	ShouldFn* g_should = nullptr;
	ButtonFn* g_button = nullptr;
	StickFn* g_stick = nullptr;

	// The last direction the stick was reported at, so a held stick is one
	// line and not one per frame.
	int g_stickX = 0;
	int g_stickY = 0;

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

	[[nodiscard]] bool Speaking()
	{
		return g_active && g_active();
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

	// What the menu is asked, and what it answers. Only presses and
	// releases: a held key asks every frame.
	bool ShouldHandle(RE::BSInputEventUser* a_self, const RE::InputEvent* a_event)
	{
		const auto answer = g_should(a_self, a_event);
		if (a_event && Speaking()) {
			if (const auto* button = a_event->As<RE::ButtonEvent>()) {
				if (button->QJustPressed() || button->value == 0.0F) {
					logger::info(
						"probe: menu asked about {} -- {}",
						Describe(*button),
						answer ? "takes it" : "declines");
				}
			}
		}
		return answer;
	}

	// What the menu is handed. This is the slot that turns a press into a
	// Scaleform key event for the focused clip.
	void OnButton(RE::BSInputEventUser* a_self, const RE::ButtonEvent* a_event)
	{
		if (a_event && Speaking() &&
			(a_event->QJustPressed() || a_event->value == 0.0F)) {
			logger::info("probe: menu handles {}", Describe(*a_event));
		}
		g_button(a_self, a_event);
	}

	void OnStick(RE::BSInputEventUser* a_self, const RE::ThumbstickEvent* a_event)
	{
		if (a_event && Speaking()) {
			const auto x = a_event->xValue > 0.5F ? 1 : a_event->xValue < -0.5F ? -1 : 0;
			const auto y = a_event->yValue > 0.5F ? 1 : a_event->yValue < -0.5F ? -1 : 0;
			if (x != g_stickX || y != g_stickY) {
				g_stickX = x;
				g_stickY = y;
				logger::info(
					"probe: menu handles stick {:#x} x {:+} y {:+}",
					static_cast<std::uint32_t>(a_event->idCode),
					x,
					y);
			}
		}
		g_stick(a_self, a_event);
	}
}

void probe::WatchPipboyMenu(bool (*a_active)())
{
	g_active = a_active;

	// PipboyMenu carries four vtables; the second is the input handler's
	// (BSInputEventUser at 0x10 of IMenu), the same layout that use.cpp
	// found on FavoritesManager. Its slots: 1 ShouldHandleEvent, 4 the
	// thumbstick, 8 the button.
	REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PipboyMenu[1] };
	g_should = reinterpret_cast<ShouldFn*>(vtable.write_vfunc(1, &ShouldHandle));
	g_stick = reinterpret_cast<StickFn*>(vtable.write_vfunc(4, &OnStick));
	g_button = reinterpret_cast<ButtonFn*>(vtable.write_vfunc(8, &OnButton));
	logger::info(
		"probe: watching PipboyMenu input (should {:#x}, stick {:#x}, button {:#x})",
		reinterpret_cast<std::uintptr_t>(g_should) - REL::Module::get().base(),
		reinterpret_cast<std::uintptr_t>(g_stick) - REL::Module::get().base(),
		reinterpret_cast<std::uintptr_t>(g_button) - REL::Module::get().base());
}
