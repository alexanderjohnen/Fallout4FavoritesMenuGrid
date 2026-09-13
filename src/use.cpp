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

namespace
{
	// Every direct call inside a stretch of code: where it stands and where
	// it goes. Bounded by length, not by a return, because a function's end
	// is not something to guess at from bytes; a target outside the code
	// section is data that happened to look like E8 and is dropped.
	struct Call
	{
		std::uintptr_t site;
		std::uintptr_t target;
	};

	[[nodiscard]] std::vector<Call> CallsIn(std::uintptr_t a_from, std::size_t a_length)
	{
		std::vector<Call> calls;
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_from);
		for (std::size_t at = 0; at + 5 <= a_length; ++at) {
			if (bytes[at] != 0xE8) {
				continue;
			}
			std::int32_t rel = 0;
			std::memcpy(&rel, bytes + at + 1, sizeof(rel));
			const auto target = a_from + at + 5 + rel;
			if (InText(target)) {
				calls.push_back({ a_from + at, target });
			}
		}
		return calls;
	}

	// How far into a handler and into a candidate the search reaches. Both
	// are generous: OnButtonEvent is a few hundred bytes, UseQuickkeyItem
	// about five hundred, and the relationship being looked for is specific
	// enough that a wider window finds nothing extra.
	constexpr std::size_t kHandlerWindow = 0x800;
	constexpr std::size_t kCandidateWindow = 0x600;
}

void use::Find()
{
	// The address is found the way it was first read, every start, on
	// whatever runtime this is.
	//
	// FavoritesManager is an input handler, and the eighth slot of that
	// vtable is OnButtonEvent -- the one place the game itself uses a
	// favorite when a digit is pressed. Somewhere in it is a call to
	// UseQuickkeyItem, and UseQuickkeyItem is the function that, in turn,
	// calls ActorEquipManager::EquipObject to put the thing on. Both ends
	// of that chain the library knows on every runtime: the vtable and
	// EquipObject. Neither UseQuickkeyItem nor the call inside it has to be
	// named by number any more, which was the one number in this plugin
	// that a game update could quietly move.
	// FavoritesManager carries two vtables in front: the event receiver's
	// at offset 0, the input handler's at offset 0x10 -- and OnButtonEvent
	// is in the second. The first was tried first; its eighth slot is an
	// empty default (0x1e0770 on 1.10.163), which found nothing.
	const auto base = REL::Module::get().base();
	REL::Relocation<std::uintptr_t*> vtable{ RE::VTABLE::FavoritesManager[1] };
	const auto onButtonEvent = vtable.get()[8];
	REL::Relocation<std::uintptr_t> equipObject{ REL::ID(332489) };
	if (!InText(onButtonEvent)) {
		logger::warn("use: OnButtonEvent is not in the code -- nothing will be used");
		return;
	}

	// The candidates are the functions OnButtonEvent calls; the one that
	// calls EquipObject is UseQuickkeyItem, and that call is the site.
	std::uintptr_t found = 0;
	std::uintptr_t site = 0;
	int matches = 0;
	const auto outer = CallsIn(onButtonEvent, kHandlerWindow);
	{
		// What the search sees, said once, so a miss can be read rather
		// than guessed at.
		std::string list;
		for (const auto& call : outer) {
			list += std::format(" +{:#x}->{:#x}", call.site - onButtonEvent, call.target - base);
		}
		logger::info(
			"use: OnButtonEvent is {:#x} (vtable {:#x}), EquipObject {:#x}; {} calls:{}",
			onButtonEvent - base,
			vtable.address() - base,
			equipObject.address() - base,
			outer.size(),
			list);
	}
	for (const auto& call : outer) {
		for (const auto& inner : CallsIn(call.target, kCandidateWindow)) {
			if (inner.target == equipObject.address()) {
				if (found != call.target) {
					++matches;
				}
				found = call.target;
				site = inner.site;
				break;
			}
		}
	}
	if (matches != 1 || !found) {
		logger::warn(
			"use: {} functions called from OnButtonEvent call EquipObject -- "
			"nothing will be used",
			matches);
		return;
	}

	// What the old number said, on the one runtime where it is known, so
	// that the two answers can be held against each other there. (id2offset
	// answers with a neighbour rather than failing for an unknown ID, so it
	// is only asked where the ID is real.)
	if (const auto legacy = REL::Module::get().version() == F4SE::RUNTIME_1_10_163
			? REL::IDDatabase::get().id2offset(kUseQuickkeyItem)
			: std::size_t{ 0 };
		legacy != 0 && base + legacy != found) {
		logger::warn(
			"use: found UseQuickkeyItem at {:#x}, but ID {} says {:#x} -- the "
			"search is wrong, and nothing will be used",
			found - base,
			kUseQuickkeyItem,
			legacy);
		return;
	}

	g_useQuickkey =
		reinterpret_cast<bool (*)(RE::FavoritesManager*, std::uint32_t)>(found);

	// And the one call inside it that can take something off again.
	auto& trampoline = F4SE::GetTrampoline();
	g_equip =
		reinterpret_cast<decltype(g_equip)>(trampoline.write_call<5>(site, &EquipThunk));
	logger::info(
		"use: UseQuickkeyItem is {:#x}, found through OnButtonEvent; its equip "
		"call at +{:#x} comes through here",
		found - base,
		site - found);
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
