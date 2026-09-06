#include "PCH.h"

#include "use.h"

#include "peek.h"

namespace
{
	// FavoritesManager carries four vtables and the one with the input
	// handlers sits at object offset 0x10 -- measured, not assumed: what is
	// at offset 0 has a single entry. Of its nine slots, the eighth is
	// OnButtonEvent, which is where the game uses a favorite when a digit is
	// pressed.
	constexpr std::size_t kInputVTableOffset = 0x10;
	constexpr std::size_t kShouldHandleEvent = 1;
	constexpr std::size_t kOnButtonEvent = 8;

	// Enough to run past the end of either handler: the next function the
	// vtable names sits 0x150 bytes after OnButtonEvent begins.
	constexpr std::size_t kSample = 0x280;

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

	// Writes out the two handlers the manager brings to the input, so the
	// one call we still lack can be read out of the one that already makes
	// it. Reading only.
	void WriteOutTheHandlers()
	{
		const auto* manager = RE::FavoritesManager::GetSingleton();
		if (!manager) {
			return;
		}

		const auto* object = reinterpret_cast<const std::uint8_t*>(manager);
		std::uintptr_t vtable = 0;
		std::memcpy(&vtable, object + kInputVTableOffset, sizeof(vtable));
		const auto rdata = REL::Module::get().segment(REL::Segment::rdata);
		if (vtable < rdata.address() || vtable >= rdata.address() + rdata.size()) {
			logger::warn("use: the manager has no vtable where one should be");
			return;
		}

		const auto* slots = reinterpret_cast<const std::uintptr_t*>(vtable);
		const auto base = REL::Module::get().base();
		for (const auto [slot, name] :
			 { std::pair{ kOnButtonEvent, "FavoritesManager::OnButtonEvent" },
				 std::pair{ kShouldHandleEvent,
					 "FavoritesManager::ShouldHandleEvent" } }) {
			const auto address = slots[slot];
			if (!InText(address)) {
				continue;
			}
			logger::info(
				"use: {} is {:#x} (ID {})",
				name,
				address - base,
				IdentityOf(address));
			// A fixed length rather than "the whole function". The
			// exception directory lists a function built from separate
			// chunks once per chunk, and OnButtonEvent's first chunk is 35
			// bytes -- it ended in the middle of the answer. This much
			// reaches the next entry of the vtable either way.
			peek::Note(name, address, kSample);
		}
	}

	[[nodiscard]] std::optional<std::uintptr_t> FromTheSettings(
		const std::filesystem::path& a_settings)
	{
		const auto base = REL::Module::get().base();

		const auto identifier = static_cast<std::uint64_t>(GetPrivateProfileIntW(
			L"Debug", L"UseQuickkeyID", 0, a_settings.c_str()));
		if (identifier != 0) {
			return base + REL::IDDatabase::get().id2offset(identifier);
		}

		std::wstring raw(64, L'\0');
		raw.resize(GetPrivateProfileStringW(
			L"Debug",
			L"UseQuickkeyRVA",
			L"",
			raw.data(),
			static_cast<DWORD>(raw.size()),
			a_settings.c_str()));
		if (raw.empty()) {
			return std::nullopt;
		}
		const auto offset = std::wcstoull(raw.c_str(), nullptr, 0);
		return offset != 0 ? std::optional{ base + offset } : std::nullopt;
	}
}

void use::Find(const std::filesystem::path& a_settings)
{
	WriteOutTheHandlers();

	// The first answer here was found by the bytes around a call --
	// FavoritesMenuEx names the signature `E8 ? ? ? ? 83 F8 0C 74 04` -- and
	// it was the wrong function: the call it lands on is a classifier that
	// walks a form's vtable against a dozen known ones and answers with a
	// number, twelve meaning "none of them". Handed a key index instead of a
	// pointer it read from address 1 and took the game with it. Hence the
	// dump above, and hence nothing at all is called until an address is
	// named here by someone who has read that dump.
	auto address = FromTheSettings(a_settings);
	if (!address) {
		address = REL::Module::get().base() +
			REL::IDDatabase::get().id2offset(kUseQuickkeyItem);
	}
	if (!InText(*address)) {
		logger::warn(
			"use: {:#x} is not in the code section -- nothing will be called",
			*address - REL::Module::get().base());
		return;
	}

	logger::info(
		"use: UseQuickkeyItem is {:#x} (ID {})",
		*address - REL::Module::get().base(),
		IdentityOf(*address));
	g_useQuickkey =
		reinterpret_cast<bool (*)(RE::FavoritesManager*, std::uint32_t)>(*address);

	// And the function itself, which is where the interesting call is.
	//
	// The ToggleEquip mod hooks a call inside it -- its own code resolves ID
	// 303130, which is this function, and adds 0x1b3 -- and turns one boolean
	// argument of that call from false to true. That boolean is what the
	// engine's own equip path uses to mean "and take it off if it is already
	// on". So the answer to toggling is not a second call to the equip
	// manager at all; it is this one call, asked differently.
	if (g_useQuickkey) {
		peek::Note(
			"FavoritesManager::UseQuickkeyItem",
			reinterpret_cast<std::uintptr_t>(g_useQuickkey),
			kSample);
	}

}

bool use::Ready()
{
	return g_useQuickkey != nullptr;
}

bool use::Quickkey(std::uint32_t a_index)
{
	auto* manager = RE::FavoritesManager::GetSingleton();
	if (!manager || !g_useQuickkey) {
		return false;
	}
	// The game answers false when it will not use the key -- a weapon with no
	// ammo, an item that has gone -- and says so with a sound of its own. We
	// only pass the answer on.
	return g_useQuickkey(manager, a_index);
}
