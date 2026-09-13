#include "PCH.h"

#include "input.h"

namespace
{
	input::Keys g_keys;
	input::Pad g_pad;
	void (*g_action)(input::Action) = nullptr;
	std::atomic_bool g_listening{ false };

	// When the panel last drew, in steady-clock ticks. Written on the UI
	// thread, read on the input one, so it travels as a plain number.
	std::atomic<std::int64_t> g_alive{ 0 };
	constexpr auto kAliveFor = std::chrono::milliseconds(500);

	[[nodiscard]] bool StillDrawing()
	{
		const auto seen = g_alive.load();
		if (seen == 0) {
			return false;
		}
		const std::chrono::steady_clock::time_point when{
			std::chrono::steady_clock::duration(seen)
		};
		return std::chrono::steady_clock::now() - when < kAliveFor;
	}
	std::atomic<input::Device> g_lastDevice{ input::Device::kNone };

	// The pointer sleeps while the keys are being used. Two ways of choosing
	// a cell in one menu fight each other otherwise: the mark walks with the
	// keys, and a cursor lying over some other cell hands the choice
	// straight back on the next frame.
	std::atomic_bool g_pointerAwake{ true };

	// See input::ClaimDirectionsOnly.
	std::atomic_bool g_directionsOnly{ false };

	[[nodiscard]] bool Directional(input::Action a_action)
	{
		return a_action == input::Action::kPageUp ||
			a_action == input::Action::kPageDown ||
			a_action == input::Action::kSlotLeft ||
			a_action == input::Action::kSlotRight;
	}

	// Which button the game closes this menu with, found in Install.
	//
	// B until the bindings say otherwise, because B is what the vanilla
	// favorites menu answers to on a stock controller and it is on the
	// never-claimed list either way. The first run through a real game's
	// bindings turned up an LB and an 0xffff and no Cancel at all, so a
	// zero here would have left the hint line spelling a button out.
	std::int32_t g_padClose{ 0x2000 };

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

	// The two sticks, as a thumbstick event numbers them. The right one is
	// never claimed: it is what the game moves the cursor with, and taking
	// it would leave a controller with no pointer at all.
	constexpr std::int32_t kLeftThumbstick = 0x0B;
	constexpr std::int32_t kRightThumbstick = 0x0C;

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
			// Claimed in the Pip-Boy too: a click on a cell there is what
			// assigns, and the dialog has nothing else to do with a click.
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

	// The same answer, with everything but the four directions taken out of
	// it. One place rather than a check at every return above.
	[[nodiscard]] std::optional<input::Action> ClaimedNow(
		const RE::ButtonEvent& a_event)
	{
		const auto action = Claimed(a_event);
		if (action && g_directionsOnly.load() && !Directional(*action) &&
			a_event.device.get() != RE::INPUT_DEVICE::kMouse) {
			return std::nullopt;
		}
		return action;
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

			// Both, and the second is the one that cannot get stuck: a
			// switch left on by an event that never came would take the
			// keyboard and the left stick away from the whole game.
			if (!g_listening || !StillDrawing()) {
				return false;
			}
			if (ClaimsStick(*a_event)) {
				return true;
			}
			const auto* button = a_event->As<RE::ButtonEvent>();
			return button && ClaimedNow(*button).has_value();
		}

		void HandleEvent(const RE::ButtonEvent* a_event) override
		{
			if (!a_event || !g_listening || !g_action) {
				return;
			}
			const auto action = ClaimedNow(*a_event);
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

		void HandleEvent(const RE::ThumbstickEvent* a_event) override
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
				// A press is a hand on the keys, and the pointer stands
				// down -- except a mouse button, which is the pointer
				// speaking for itself.
				if (device == input::Device::kGamepad ||
					a_event.device.get() == RE::INPUT_DEVICE::kKeyboard) {
					g_pointerAwake.store(false);
				}
			} else if (const auto* stick = a_event.As<RE::ThumbstickEvent>()) {
				if (std::abs(stick->xValue) < kStickOn &&
					std::abs(stick->yValue) < kStickOn) {
					return;
				}
				// The right stick is the cursor's own; the left one walks
				// the mark and puts the cursor to sleep with the keys.
				g_pointerAwake.store(stick->idCode == kRightThumbstick);
			} else if (a_event.As<RE::MouseMoveEvent>()) {
				g_pointerAwake.store(true);
			} else {
				return;
			}

			g_lastDevice.store(device);
		}
	};

	Handler g_handler;

	// Whether a number out of the bindings is a button at all. The tables
	// are full of placeholders for "not bound", and they are not all the
	// same placeholder.
	[[nodiscard]] bool Buttonish(std::int32_t a_code)
	{
		switch (a_code) {
		case kPadDPadUp:
		case kPadDPadDown:
		case kPadDPadLeft:
		case kPadDPadRight:
		case kPadStart:
		case kPadBack:
		case kPadLStick:
		case kPadRStick:
		case kPadLShoulder:
		case kPadRShoulder:
		case kPadLTrigger:
		case kPadRTrigger:
		case kPadA:
		case kPadB:
		case kPadX:
		case kPadY:
			return true;
		default:
			return false;
		}
	}

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
				if (!Buttonish(mapping.inputKey)) {
					// An unbound entry, and the bindings are full of them:
					// 0xffff is what "nothing" looks like here, and -1 and 0
					// mean the same. Guarding against a button that does not
					// exist costs nothing; naming it in the hint line would
					// have cost the line.
					continue;
				}
				g_padForbidden.insert(mapping.inputKey);
				// Cancel is the one to name in the hint line: Quickkeys is
				// what opened the menu, Cancel is what a player reaches for
				// to leave one.
				if (mapping.eventID == cancel) {
					g_padClose = mapping.inputKey;
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
		line += std::format(
			"; {} stay with the game, and {} is the way out",
			guarded,
			input::PadName(g_padClose));
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

void input::Alive()
{
	g_alive.store(std::chrono::steady_clock::now().time_since_epoch().count());
}

void input::ClaimDirectionsOnly(bool a_on)
{
	g_directionsOnly.store(a_on);
}

void input::Listen(bool a_on)
{
	g_listening = a_on;
	if (!a_on) {
		g_alive.store(0);
	}
	if (a_on) {
		// A player who came in on a controller should not be handed a mouse
		// pointer they did not ask for.
		g_pointerAwake.store(g_lastDevice.load() != Device::kGamepad);
	} else {
		g_stickHeld.reset();
		g_stepped.fill(0.0F);
	}
}

int input::PadCloseButton()
{
	return g_padClose;
}

bool input::PointerAwake()
{
	return g_pointerAwake.load();
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

// The button art the game already owns.
//
// Fallout 4 keeps its controller symbols in a font, not in pictures:
// `$Controller_Buttons` in `Interface\fonts_en.swf`, mapped by
// `Interface\FontConfig.txt` to the face "Controller  Buttons" -- with two
// spaces, which is not a typo. A text field set to that font and given the
// letter "A" draws the A button. The vanilla menus do exactly this; the "A"
// and "X" sitting in ContainerMenu's fields are authoring placeholders, not
// the mapping.
//
// The mapping was read out of the font itself. It has 57 glyphs: a space,
// A-Z, a-z, three brackets and a non-breaking space. **A-Z is the Xbox set
// and a-z the PlayStation one**, in the same order for the parts that
// matter -- T/t is left, U/u right, V/v down, W/w up. Which set to use is
// the game's own answer: `ControlMap::pcGamePadMapType`.
//
// Nothing here is a guess. The glyph shapes were pulled out of the SWF and
// drawn, so what stands next to each letter is what the font draws.
std::string input::PadGlyph(int a_button, bool a_orbis)
{
	switch (a_button) {
	case kPadA:
		return a_orbis ? "a" : "A";  // cross / A
	case kPadB:
		return a_orbis ? "d" : "B";  // circle / B
	case kPadX:
		return a_orbis ? "c" : "C";  // square / X
	case kPadY:
		return a_orbis ? "b" : "D";  // triangle / Y
	case kPadDPadUp:
		return a_orbis ? "w" : "W";
	case kPadDPadDown:
		return a_orbis ? "v" : "V";
	case kPadDPadLeft:
		return a_orbis ? "t" : "T";
	case kPadDPadRight:
		return a_orbis ? "u" : "U";
	case kPadLShoulder:
		return a_orbis ? "g" : "G";  // L1 / LB
	case kPadRShoulder:
		return a_orbis ? "m" : "L";  // R1 / RB
	case kPadLTrigger:
		return a_orbis ? "j" : "I";  // L2 / LT
	case kPadRTrigger:
		return a_orbis ? "o" : "N";  // R2 / RT
	case kPadLStick:
		return a_orbis ? "f" : "F";  // L3 / left stick
	case kPadRStick:
		return a_orbis ? "l" : "K";  // R3 / right stick
	case kPadStart:
		return a_orbis ? "p" : "O";  // Options / Menu
	case kPadBack:
		return a_orbis ? "e" : "E";  // Share / View
	default:
		return {};
	}
}

std::string input::PadGlyphDPad(bool a_orbis)
{
	return a_orbis ? "s" : "P";
}

std::string input::PadGlyphStick(bool a_orbis)
{
	return a_orbis ? "i" : "F";
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
