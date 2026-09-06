#include "PCH.h"

#include "input.h"

namespace
{
	input::Keys g_keys;
	void (*g_action)(input::Action) = nullptr;
	std::atomic_bool g_listening{ false };

	// The mouse numbers its own buttons from zero, so the left one is zero.
	constexpr std::int32_t kLeftMouseButton = 0;

	// Which of the grid's actions a press means, if any. Keyboard codes are
	// virtual key codes -- BS_BUTTON_CODE spells the letters the same way --
	// so what the INI reads and what the event carries are one number.
	[[nodiscard]] std::optional<input::Action> Claimed(const RE::ButtonEvent& a_event)
	{
		switch (a_event.device.get()) {
		case RE::INPUT_DEVICE::kMouse:
			if (g_keys.useOnClick && a_event.idCode == kLeftMouseButton) {
				return input::Action::kUse;
			}
			return std::nullopt;

		case RE::INPUT_DEVICE::kKeyboard:
			break;

		default:
			// The gamepad still goes to the cross. Its own way through the
			// grid is a question of its own, and one wrong answer there
			// would take the menu away from a controller entirely.
			return std::nullopt;
		}

		const auto code = a_event.idCode;
		if (code == g_keys.pageUp) {
			return input::Action::kPageUp;
		}
		if (code == g_keys.pageDown) {
			return input::Action::kPageDown;
		}
		if (code == g_keys.slotLeft) {
			return input::Action::kSlotLeft;
		}
		if (code == g_keys.slotRight) {
			return input::Action::kSlotRight;
		}
		if (code == g_keys.use || code == g_keys.useAlt) {
			return input::Action::kUse;
		}
		return std::nullopt;
	}

	class Handler : public RE::BSInputEventUser
	{
	public:
		// Says whether this handler owns the event. Answering yes to
		// everything would swallow the digits and the key that closes the
		// menu, so it is asked of each event on its own.
		bool ShouldHandleEvent(const RE::InputEvent* a_event) override
		{
			if (!g_listening || !a_event) {
				return false;
			}
			const auto* button = a_event->As<RE::ButtonEvent>();
			return button && Claimed(*button).has_value();
		}

		void OnButtonEvent(const RE::ButtonEvent* a_event) override
		{
			if (!a_event || !g_listening || !g_action) {
				return;
			}
			// The press, not the holding of it: a key held down repeats as
			// events, and a grid that walked a cell per frame would be
			// unusable.
			if (!a_event->QJustPressed()) {
				return;
			}
			if (const auto action = Claimed(*a_event)) {
				g_action(*action);
			}
		}
	};

	Handler g_handler;
}

void input::SetKeys(const Keys& a_keys)
{
	g_keys = a_keys;
}

void input::SetOnAction(void (*a_action)(Action))
{
	g_action = a_action;
}

void input::Listen(bool a_on)
{
	g_listening = a_on;
}

void input::Install()
{
	auto* controls = RE::MenuControls::GetSingleton();
	if (!controls) {
		logger::error("input: there are no menu controls to join");
		return;
	}

	// At the front. The array is walked in order and the first handler that
	// owns an event ends the walk, so what the grid claims never reaches
	// anyone behind it.
	controls->handlers.emplace(controls->handlers.begin(), &g_handler);

	logger::info(
		"input: the grid listens first of {} handlers; w/a/s/d are {:#04x} "
		"{:#04x} {:#04x} {:#04x}, use is {:#04x} or {:#04x}{}",
		controls->handlers.size(),
		g_keys.pageUp,
		g_keys.slotLeft,
		g_keys.pageDown,
		g_keys.slotRight,
		g_keys.use,
		g_keys.useAlt,
		g_keys.useOnClick ? " and the left mouse button" : "");
}
