#include "PCH.h"

#include "input.h"

namespace
{
	// The second of the four vtables CommonLibF4 lists for FavoritesManager
	// is the one with the input handlers: nine entries, of which only two --
	// ShouldHandleEvent and OnButtonEvent -- are the manager's own, the rest
	// being the empty defaults that sit together in memory. Measured with
	// tools/f4dis.py rather than taken on trust, because the library's slot
	// numbers have been wrong twice in this project already.
	constexpr std::uint64_t kInputVTable = 892289;

	constexpr std::size_t kShouldHandleEvent = 1;
	constexpr std::size_t kOnMouseMoveEvent = 6;
	constexpr std::size_t kOnButtonEvent = 8;

	input::Hooks g_hooks;

	REL::Relocation<bool (*)(RE::BSInputEventUser*, const RE::InputEvent*)>
		g_shouldHandle;
	REL::Relocation<void (*)(RE::BSInputEventUser*, const RE::MouseMoveEvent*)>
		g_onMouseMove;
	REL::Relocation<void (*)(RE::BSInputEventUser*, const RE::ButtonEvent*)>
		g_onButton;

	bool ShouldHandleEvent(RE::BSInputEventUser* a_this, const RE::InputEvent* a_event)
	{
		return g_shouldHandle(a_this, a_event);
	}

	void OnMouseMoveEvent(
		RE::BSInputEventUser* a_this,
		const RE::MouseMoveEvent* a_event)
	{
		if (a_event && g_hooks.mouse) {
			g_hooks.mouse(a_event->mouseInputX, a_event->mouseInputY);
		}
		g_onMouseMove(a_this, a_event);
	}

	void OnButtonEvent(RE::BSInputEventUser* a_this, const RE::ButtonEvent* a_event)
	{
		if (a_event && g_hooks.button && g_hooks.button(*a_event)) {
			return;
		}
		g_onButton(a_this, a_event);
	}
}

void input::Install(const Hooks& a_hooks)
{
	g_hooks = a_hooks;

	// Which vtable the living object actually uses. Patching one it does not
	// carry changes nothing at all, which is exactly what the first attempt
	// achieved: the hook reported itself installed and not one event ever
	// arrived. The four the library lists are printed beside it, so the
	// answer is a comparison rather than another guess.
	if (const auto* manager = RE::FavoritesManager::GetSingleton()) {
		const auto base = REL::Module::get().base();
		const auto live = *reinterpret_cast<const std::uintptr_t*>(manager);
		std::string known;
		for (const auto& candidate : RE::VTABLE::FavoritesManager) {
			known += std::format("{:#x} ", candidate.address() - base);
		}
		logger::info(
			"input: the manager carries vtable {:#x}; the library lists {}",
			live - base,
			known);
		logger::info(
			"input: its input handling is {}",
			manager->inputEventHandlingEnabled ? "on" : "off");
	}

	REL::Relocation<std::uintptr_t> vtable{ REL::ID(kInputVTable) };
	g_shouldHandle = vtable.write_vfunc(kShouldHandleEvent, &ShouldHandleEvent);
	g_onMouseMove = vtable.write_vfunc(kOnMouseMoveEvent, &OnMouseMoveEvent);
	g_onButton = vtable.write_vfunc(kOnButtonEvent, &OnButtonEvent);

	logger::info("input: the favorites keys come through here now");
}
