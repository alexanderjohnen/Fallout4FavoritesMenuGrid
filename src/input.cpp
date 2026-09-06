#include "PCH.h"

#include "input.h"

namespace
{
	input::Keys g_keys;
	input::Pad g_pad;
	void (*g_action)(input::Action) = nullptr;
	std::atomic_bool g_listening{ false };
	std::atomic<input::Device> g_lastDevice{ input::Device::kNone };

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
	std::array<float, static_cast<std::size_t>(input::Action::kMove) + 1>
		g_stepped{};

	[[nodiscard]] bool Walks(input::Action a_action)
	{
		return a_action == input::Action::kPageUp ||
			a_action == input::Action::kPageDown ||
			a_action == input::Action::kSlotLeft ||
			a_action == input::Action::kSlotRight;
	}

	// The mouse numbers its own buttons from zero, so the left one is zero.
	constexpr std::int32_t kLeftMouseButton = 0;

	// XInput's own bits, which is what a gamepad button event carries in its
	// idCode. Only the ones that are named somewhere in this file.
	constexpr std::int32_t kPadDPadUp = 0x0001;
	constexpr std::int32_t kPadDPadDown = 0x0002;
	constexpr std::int32_t kPadDPadLeft = 0x0004;
	constexpr std::int32_t kPadDPadRight = 0x0008;
	constexpr std::int32_t kPadStart = 0x0010;
	constexpr std::int32_t kPadBack = 0x0020;
	constexpr std::int32_t kPadLStick = 0x0040;
	constexpr std::int32_t kPadRStick = 0x0080;
	constexpr std::int32_t kPadLShoulder = 0x0100;
	constexpr std::int32_t kPadRShoulder = 0x0200;
	constexpr std::int32_t kPadLTrigger = 0x0009;
	constexpr std::int32_t kPadRTrigger = 0x000A;
	constexpr std::int32_t kPadA = 0x1000;
	constexpr std::int32_t kPadB = 0x2000;
	constexpr std::int32_t kPadX = 0x4000;
	constexpr std::int32_t kPadY = 0x8000;

	// The left stick, as the thumbstick event numbers it.
	constexpr std::int32_t kLeftThumbstick = 0x0B;

	// Buttons this handler will not take, whatever the INI says. B, Start
	// and Back are in it from the start, and Install adds whatever the game
	// has bound "Quickkeys" and "Cancel" to -- the two user events the
	// vanilla favorites menu closes itself on. Swallowing one of those would
	// leave a controller inside a menu with no way out.
	std::set<std::int32_t> g_padForbidden{ kPadB, kPadStart, kPadBack };

	// Which of the grid's actions a press means, if any. Keyboard codes are
	// virtual key codes -- BS_BUTTON_CODE spells the letters the same way --
	// so what the INI reads and what the event carries are one number.
	[[nodiscard]] std::optional<input::Action> Claimed(const RE::ButtonEvent& a_event)
	{
		const auto code = static_cast<std::int32_t>(a_event.idCode);

		switch (a_event.device.get()) {
		case RE::INPUT_DEVICE::kMouse:
			if (g_keys.useOnClick && code == kLeftMouseButton) {
				return input::Action::kUse;
			}
			return std::nullopt;

		case RE::INPUT_DEVICE::kGamepad:
			if (!g_pad.enabled || g_padForbidden.contains(code)) {
				return std::nullopt;
			}
			if (code == g_pad.pageUp) {
				return input::Action::kPageUp;
			}
			if (code == g_pad.pageDown) {
				return input::Action::kPageDown;
			}
			if (code == g_pad.slotLeft) {
				return input::Action::kSlotLeft;
			}
			if (code == g_pad.slotRight) {
				return input::Action::kSlotRight;
			}
			if (code == g_pad.use) {
				return input::Action::kUse;
			}
			if (code == g_pad.clear) {
				return input::Action::kClear;
			}
			if (code == g_pad.move) {
				return input::Action::kMove;
			}
			return std::nullopt;

		case RE::INPUT_DEVICE::kKeyboard:
			break;

		default:
			return std::nullopt;
		}

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

	// Whether the left stick is ours while the grid is up. All of its events
	// or none of them: claiming only the deflected ones would mean never
	// seeing the stick come back to the middle, and the mark would walk on
	// after the thumb let go.
	[[nodiscard]] bool ClaimsStick(const RE::InputEvent& a_event)
	{
		if (!g_pad.enabled || !g_pad.stick) {
			return false;
		}
		const auto* stick = a_event.As<RE::ThumbstickEvent>();
		return stick && stick->idCode == kLeftThumbstick;
	}

	// The stick reports where it is, not that something happened, so the
	// stepping the buttons get from the event has to be built here.
	//
	// A dead zone with two edges: past 0.55 the stick has chosen a
	// direction, and it keeps it until it falls back under 0.35. One number
	// would have the mark twitching between two cells at the very edge of
	// it.
	constexpr float kStickOn = 0.55F;
	constexpr float kStickOff = 0.35F;

	std::optional<input::Action> g_stickHeld;
	std::chrono::steady_clock::time_point g_stickSince;
	std::chrono::steady_clock::time_point g_stickStepped;

	[[nodiscard]] std::optional<input::Action> StickDirection(float a_x, float a_y)
	{
		const auto edge = g_stickHeld ? kStickOff : kStickOn;
		const auto x = std::abs(a_x);
		const auto y = std::abs(a_y);
		if (x < edge && y < edge) {
			return std::nullopt;
		}
		// The bigger deflection wins, so a thumb halfway between two cells
		// picks one rather than both.
		if (y >= x) {
			return a_y > 0.0F ? input::Action::kPageUp : input::Action::kPageDown;
		}
		return a_x > 0.0F ? input::Action::kSlotRight : input::Action::kSlotLeft;
	}

	class Handler : public RE::BSInputEventUser
	{
	public:
		// Says whether this handler owns the event. Answering yes to
		// everything would swallow the digits and the key that closes the
		// menu, so it is asked of each event on its own.
		bool ShouldHandleEvent(const RE::InputEvent* a_event) override
		{
			if (!a_event) {
				return false;
			}

			// Asked of every event, claimed or not, which makes this the one
			// place that sees the whole stream -- and therefore the cheapest
			// place to notice that the player has put the keyboard down and
			// picked a controller up.
			Remember(*a_event);

			if (!g_listening) {
				return false;
			}
			if (ClaimsStick(*a_event)) {
				return true;
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

		void OnThumbstickEvent(const RE::ThumbstickEvent* a_event) override
		{
			if (!a_event || !g_listening || !g_action || !ClaimsStick(*a_event)) {
				return;
			}

			const auto now = std::chrono::steady_clock::now();
			const auto direction = StickDirection(a_event->xValue, a_event->yValue);
			if (!direction) {
				g_stickHeld.reset();
				return;
			}

			if (g_stickHeld != direction) {
				g_stickHeld = direction;
				g_stickSince = now;
				g_stickStepped = now;
				g_action(*direction);
				return;
			}

			const std::chrono::duration<double> since = now - g_stickSince;
			const std::chrono::duration<double> step = now - g_stickStepped;
			if (since.count() < g_repeatDelay || step.count() < g_repeatInterval) {
				return;
			}
			g_stickStepped = now;
			g_action(*direction);
		}

	private:
		// A press, not a wobble: a stick that drifts in its rest position
		// would otherwise claim to be the device in use forever.
		static void Remember(const RE::InputEvent& a_event)
		{
			auto device = input::Device::kNone;
			switch (a_event.device.get()) {
			case RE::INPUT_DEVICE::kKeyboard:
			case RE::INPUT_DEVICE::kMouse:
				device = input::Device::kKeyboard;
				break;
			case RE::INPUT_DEVICE::kGamepad:
				device = input::Device::kGamepad;
				break;
			default:
				return;
			}

			if (const auto* button = a_event.As<RE::ButtonEvent>()) {
				if (button->value == 0.0F) {
					return;
				}
			} else if (const auto* stick = a_event.As<RE::ThumbstickEvent>()) {
				if (std::abs(stick->xValue) < kStickOn &&
					std::abs(stick->yValue) < kStickOn) {
					return;
				}
			} else if (!a_event.As<RE::MouseMoveEvent>()) {
				return;
			}

			g_lastDevice.store(device);
		}
	};

	Handler g_handler;

	// What the game has bound "Quickkeys" and "Cancel" to on the controller.
	// Read rather than assumed: the player may have remapped either, and the
	// one thing that must never break is the way out of this menu.
	void ForbidTheWayOut()
	{
		auto* controls = RE::ControlMap::GetSingleton();
		if (!controls) {
			// Without the bindings there is no safe answer, so the buttons
			// the vanilla menu is known to close on stay out of our reach
			// and the rest goes on as usual.
			logger::warn(
				"input: no control map -- the gamepad keeps only its fixed "
				"guards");
			return;
		}

		static const RE::BSFixedString quickkeys("Quickkeys");
		static const RE::BSFixedString cancel("Cancel");
		constexpr auto pad = static_cast<std::size_t>(RE::INPUT_DEVICE::kGamepad);

		for (const auto* context : controls->controlMaps) {
			if (!context) {
				continue;
			}
			for (const auto& mapping : context->deviceMappings[pad]) {
				if (mapping.eventID != quickkeys && mapping.eventID != cancel) {
					continue;
				}
				if (mapping.inputKey > 0 &&
					mapping.inputKey != static_cast<std::int32_t>(-1)) {
					g_padForbidden.insert(mapping.inputKey);
				}
			}
		}
	}

	// The map as it ended up, in one line, so a controller that does nothing
	// can be told apart from a controller whose buttons were all taken away.
	[[nodiscard]] std::string PadSummary()
	{
		if (!g_pad.enabled) {
			return "the gamepad is off";
		}

		std::string line = std::format(
			"the gamepad walks with {}{}{}{}{}, uses with {}, picks up with "
			"{}, clears with {}",
			input::PadName(g_pad.pageUp),
			input::PadName(g_pad.slotLeft),
			input::PadName(g_pad.pageDown),
			input::PadName(g_pad.slotRight),
			g_pad.stick ? " and the left stick" : "",
			input::PadName(g_pad.use),
			input::PadName(g_pad.move),
			input::PadName(g_pad.clear));

		std::string guarded;
		for (const auto code : g_padForbidden) {
			auto name = input::PadName(code);
			if (name.empty()) {
				name = std::format("{:#06x}", code);
			}
			if (!guarded.empty()) {
				guarded += " ";
			}
			guarded += name;
		}
		line += std::format("; {} stay with the game", guarded);
		return line;
	}
}

void input::SetKeys(const Keys& a_keys)
{
	g_keys = a_keys;
}

void input::SetPad(const Pad& a_pad)
{
	g_pad = a_pad;
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
	if (!a_on) {
		g_stickHeld.reset();
		g_stepped.fill(0.0F);
	}
}

input::Device input::LastDevice()
{
	return g_lastDevice.load();
}

std::string input::PadName(int a_button)
{
	switch (a_button) {
	case kPadDPadUp:
		return "^";
	case kPadDPadDown:
		return "v";
	case kPadDPadLeft:
		return "<";
	case kPadDPadRight:
		return ">";
	case kPadA:
		return "A";
	case kPadB:
		return "B";
	case kPadX:
		return "X";
	case kPadY:
		return "Y";
	case kPadLShoulder:
		return "LB";
	case kPadRShoulder:
		return "RB";
	case kPadLTrigger:
		return "LT";
	case kPadRTrigger:
		return "RT";
	case kPadLStick:
		return "LS";
	case kPadRStick:
		return "RS";
	case kPadStart:
		return "START";
	case kPadBack:
		return "BACK";
	default:
		return {};
	}
}

void input::Install()
{
	auto* controls = RE::MenuControls::GetSingleton();
	if (!controls) {
		logger::error("input: there are no menu controls to join");
		return;
	}

	if (g_pad.enabled) {
		ForbidTheWayOut();
	}

	// At the front. The array is walked in order and the first handler that
	// owns an event ends the walk, so what the grid claims never reaches
	// anyone behind it.
	controls->handlers.emplace(controls->handlers.begin(), &g_handler);

	logger::info(
		"input: the grid listens first of {} handlers; w/a/s/d are {:#04x} "
		"{:#04x} {:#04x} {:#04x}, use is {:#04x} or {:#04x}{}; {}",
		controls->handlers.size(),
		g_keys.pageUp,
		g_keys.slotLeft,
		g_keys.pageDown,
		g_keys.slotRight,
		g_keys.use,
		g_keys.useAlt,
		g_keys.useOnClick ? " and the left mouse button" : "",
		PadSummary());
}
