#include "PCH.h"

#include "input.h"

namespace
{
	input::Keys g_keys;
	void (*g_action)(input::Action) = nullptr;
	std::atomic_bool g_listening{ false };

	// A key held down arrives as an event per frame, and acting on every one
	// of them walked the mark across the panel faster than anyone could read
	// it. The first answer was to act only on the press, which is worse in
	// the other direction: holding a direction is what a hand does, and
	// nothing happening reads as a broken key.
	//
	// So: the press, then a pause, then a steady walk -- what every list in
	// every menu has always done.
	double g_repeatDelay = 0.4;
	double g_repeatInterval = 0.09;

	// When each action last stepped, measured in the same held-down seconds
	// the event carries. One per action, so two directions held at once do
	// not take each other's turn.
	std::array<float, 5> g_stepped{};

	[[nodiscard]] bool Walks(input::Action a_action)
	{
		return a_action == input::Action::kPageUp ||
			a_action == input::Action::kPageDown ||
			a_action == input::Action::kSlotLeft ||
			a_action == input::Action::kSlotRight;
	}

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
		if (code == g_keys.clear) {
			return input::Action::kClear;
		}
		if (code == g_keys.move) {
			return input::Action::kMove;
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
			const auto action = Claimed(*a_event);
			if (!action) {
				return;
			}
			const auto which = static_cast<std::size_t>(*action);

			// Letting go.
			if (a_event->value == 0.0F) {
				g_stepped[which] = 0.0F;
				return;
			}

			if (a_event->QJustPressed()) {
				g_stepped[which] = 0.0F;
				g_action(*action);
				return;
			}

			if (!Walks(*action)) {
				return;
			}
			const auto held = a_event->QHeldDownSecs();
			if (held < static_cast<float>(g_repeatDelay) ||
				held - g_stepped[which] < static_cast<float>(g_repeatInterval)) {
				return;
			}
			g_stepped[which] = held;
			g_action(*action);
		}
	};

	Handler g_handler;
}

void input::SetKeys(const Keys& a_keys)
{
	g_keys = a_keys;
}

void input::SetRepeat(double a_delay, double a_interval)
{
	g_repeatDelay = a_delay;
	g_repeatInterval = a_interval;
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
