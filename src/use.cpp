#include "PCH.h"

#include "use.h"

namespace
{
	// Read out of FavoritesManager::OnButtonEvent, which is the one place the
	// game itself uses a favorite:
	//
	//     mov  rcx, rbx        ; the manager -- rcx - 0x10, the object behind
	//                          ; the input handler this was called on
	//     mov  edx, eax        ; the key index, 0 to 11
	//     call 0x126fcb0       ; [ID 303130]
	//     test al, al          ; false, and the game says so out loud
	//
	// So: bool UseQuickkeyItem(FavoritesManager*, std::uint32_t). The index
	// comes out of GetQuickkeyIndexFromString (0x1271480, ID 1330478), which
	// walks the user event name against twelve interned strings -- that is
	// the function the old signature landed on, and handing it a number
	// instead of a string is what took the game down.
	constexpr std::uint64_t kUseQuickkeyItem = 303130;

	// How far into it the equip call sits. Read out of the disassembly, and
	// out of ToggleEquip, which adds the same 0x1b3 to the same ID.
	constexpr std::size_t kEquipCall = 0x1b3;

	bool (*g_useQuickkey)(RE::FavoritesManager*, std::uint32_t) = nullptr;

	// ---- Taking something off again --------------------------------------
	//
	// Putting a favorite on is one call inside UseQuickkeyItem, and that call
	// carries a boolean the game always passes as false:
	//
	//     mov  rdx, [rip + ...]          ; the player
	//     mov  rcx, [rip + ...]          ; the ActorEquipManager
	//     mov  byte ptr [rsp+0x30], 0    ; the second boolean
	//     lea  r8,  [rsp+0x40]           ; the inventory handle, built above
	//     mov  r9d, ebp                  ; the index
	//     mov  byte ptr [rsp+0x28], 0    ; the first -- this one
	//     mov  qword ptr [rsp+0x20], 0   ; no equip slot
	//     call 0xe1c750                  ; [ID 332489]
	//
	// True there means "and take it off again if it is already on". That is
	// how the ToggleEquip mod does it: it hooks this very call -- its own
	// code resolves ID 303130, adds 0x1b3, and turns that boolean around --
	// and it is the only way in, because the handle the call needs is built
	// two instructions earlier and released two instructions later.
	//
	// Calling the equip manager separately was the obvious alternative and it
	// simply refuses: four different ways of asking, all refused, which is
	// what sent us looking here.
	//
	// So the call is hooked, and the boolean is turned around only when this
	// plugin asked for it. The game's own digit keys go through the same call
	// and are left exactly as they were.

	// The engine's own answer, kept so it can still be given.
	bool (*g_equip)(
		RE::ActorEquipManager*,
		RE::Actor*,
		RE::InventoryInterface::Handle&,
		std::uint32_t,
		const RE::BGSEquipSlot*,
		bool,
		bool) = nullptr;

	// Set for the length of one call. The UI thread is the only one that ever
	// gets here, but the flag is atomic because the engine is entitled to
	// call this from wherever it likes.
	std::atomic_bool g_toggleThisTime{ false };

	bool EquipThunk(
		RE::ActorEquipManager* a_manager,
		RE::Actor* a_actor,
		RE::InventoryInterface::Handle& a_handle,
		std::uint32_t a_index,
		const RE::BGSEquipSlot* a_slot,
		bool a_toggle,
		bool a_second)
	{
		return g_equip(
			a_manager,
			a_actor,
			a_handle,
			a_index,
			a_slot,
			a_toggle || g_toggleThisTime.load(),
			a_second);
	}


	[[nodiscard]] bool InText(std::uintptr_t a_address)
	{
		const auto text = REL::Module::get().segment(REL::Segment::text);
		return a_address >= text.address() && a_address < text.address() + text.size();
	}

	// The Address Library the other way round, so a finding can be named
	// rather than only pointed at.
	[[nodiscard]] std::uint64_t IdentityOf(std::uintptr_t a_address)
	{
		static const REL::IDDatabase::Offset2ID map;
		const auto offset =
			static_cast<std::uint64_t>(a_address - REL::Module::get().base());
		const auto found = std::lower_bound(
			map.begin(),
			map.end(),
			offset,
			[](const auto& a_entry, std::uint64_t a_wanted) {
				return a_entry.offset < a_wanted;
			});
		// Only an exact hit. The database answers a near miss with the
		// neighbour, and a neighbour's ID written into a handoff is worse
		// than no ID at all.
		return found != map.end() && found->offset == offset ? found->id : 0;
	}

}

void use::Find()
{
	// The address is the answer to a long question, and the question is
	// closed: FavoritesMenuEx's signature led to the wrong function, the
	// right one was read out of FavoritesManager's own OnButtonEvent, and it
	// has been used ever since. The whole trail is in the handoff; what
	// belongs here is the number and a check that it is code.
	const auto address = REL::Module::get().base() +
		REL::IDDatabase::get().id2offset(kUseQuickkeyItem);
	if (!InText(address)) {
		logger::warn(
			"use: ID {} does not land in the code -- nothing will be used",
			kUseQuickkeyItem);
		return;
	}
	g_useQuickkey =
		reinterpret_cast<bool (*)(RE::FavoritesManager*, std::uint32_t)>(address);

	// And the one call inside it that can take something off again.
	auto& trampoline = F4SE::GetTrampoline();
	const auto site = address + kEquipCall;
	if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
		logger::warn(
			"use: {:#x} is not a call -- nothing will be taken off",
			site - REL::Module::get().base());
		return;
	}
	g_equip =
		reinterpret_cast<decltype(g_equip)>(trampoline.write_call<5>(site, &EquipThunk));
	logger::info(
		"use: UseQuickkeyItem is {:#x}, and its equip call comes through here",
		address - REL::Module::get().base());
}

bool use::Ready()
{
	return g_useQuickkey != nullptr;
}

bool use::Quickkey(std::uint32_t a_index, bool a_toggle)
{
	auto* manager = RE::FavoritesManager::GetSingleton();
	if (!manager || !g_useQuickkey) {
		return false;
	}

	// Only for the length of this one call, and only when the hook took.
	// Everything else that reaches that call -- the game's own digit keys
	// among them -- is left as it was.
	g_toggleThisTime = a_toggle && g_equip != nullptr;
	// The game answers false when it will not use the key -- a weapon with no
	// ammo, an item that has gone -- and says so with a sound of its own. We
	// only pass the answer on.
	const auto used = g_useQuickkey(manager, a_index);
	g_toggleThisTime = false;
	return used;
}
