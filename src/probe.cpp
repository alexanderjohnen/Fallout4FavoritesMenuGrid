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

	constexpr std::int32_t kLeftThumbstick = 0x0B;

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

	// What the grid keeps from the menu: the four directions off the pad,
	// by the name the game gives them, and the left stick. Not the
	// keyboard's arrows -- those are not what jumped -- and not Accept,
	// which is how the dialog assigns.
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

	bool ShouldHandle(RE::BSInputEventUser* a_self, const RE::InputEvent* a_event)
	{
		if (a_event && Kept(*a_event)) {
			// Said once per press, not per frame of a held stick.
			if (const auto* button = a_event->As<RE::ButtonEvent>();
				button && button->QJustPressed()) {
				logger::info("pipboy: {} kept from the menu", Describe(*button));
			}
			return false;
		}
		const auto answer = g_should(a_self, a_event);
		if (a_event && Active()) {
			if (const auto* button = a_event->As<RE::ButtonEvent>();
				button && button->QJustPressed() &&
				button->device.get() == RE::INPUT_DEVICE::kGamepad) {
				logger::info(
					"pipboy: menu asked about {} -- {}",
					Describe(*button),
					answer ? "takes it" : "declines");
			}
		}
		return answer;
	}

	// The same gate on the slots themselves, in case anything hands the
	// menu an event without asking first.
	void OnButton(RE::BSInputEventUser* a_self, const RE::ButtonEvent* a_event)
	{
		if (a_event && Kept(*a_event)) {
			return;
		}
		g_button(a_self, a_event);
	}

	void OnStick(RE::BSInputEventUser* a_self, const RE::ThumbstickEvent* a_event)
	{
		if (a_event && Kept(*a_event)) {
			return;
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
	// thumbstick, 8 the button. What was in the slots before is kept and
	// called -- on 2026-09-14 two of the three already pointed outside
	// the game, so some other plugin sits here too.
	REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PipboyMenu[1] };
	g_should = reinterpret_cast<ShouldFn*>(vtable.write_vfunc(1, &ShouldHandle));
	g_stick = reinterpret_cast<StickFn*>(vtable.write_vfunc(4, &OnStick));
	g_button = reinterpret_cast<ButtonFn*>(vtable.write_vfunc(8, &OnButton));
	logger::info(
		"pipboy: standing before PipboyMenu's input (should {:#x}, stick {:#x}, button {:#x})",
		reinterpret_cast<std::uintptr_t>(g_should) - REL::Module::get().base(),
		reinterpret_cast<std::uintptr_t>(g_stick) - REL::Module::get().base(),
		reinterpret_cast<std::uintptr_t>(g_button) - REL::Module::get().base());
}
