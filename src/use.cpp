#include "PCH.h"

#include "use.h"

#include "peek.h"

namespace
{
	// The bytes FavoritesMenuEx keys on. Only the tail is compared: the call
	// itself carries an address that differs at every site, and the four
	// bytes of it are what we are after.
	//
	//     E8 ? ? ? ?      call    ...
	//     83 F8 0C        cmp     eax, 0Ch
	//     74 04           je      short ...
	constexpr std::array<std::uint8_t, 5> kAfterTheCall{ 0x83, 0xF8, 0x0C, 0x74, 0x04 };
	constexpr std::size_t kCallLength = 5;

	// Enough of the function to recognise it in a disassembly, and enough of
	// the call site to see what it is handed.
	constexpr std::size_t kFunctionSample = 0x140;
	constexpr std::size_t kCallSiteSample = 0x40;
	constexpr std::size_t kCallSiteLead = 0x20;

	void (*g_useQuickkey)(RE::FavoritesManager*, std::uint32_t) = nullptr;

	[[nodiscard]] bool InText(std::uintptr_t a_address)
	{
		const auto text = REL::Module::get().segment(REL::Segment::text);
		return a_address >= text.address() && a_address < text.address() + text.size();
	}

	// Every call in the code whose answer is compared with twelve.
	[[nodiscard]] std::vector<std::uintptr_t> FindCallSites()
	{
		const auto text = REL::Module::get().segment(REL::Segment::text);
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.address());
		const auto size = static_cast<std::size_t>(text.size());

		std::vector<std::uintptr_t> sites;
		for (std::size_t index = 0;
			 index + kCallLength + kAfterTheCall.size() <= size;
			 ++index) {
			if (bytes[index] != 0xE8) {
				continue;
			}
			if (std::equal(
					kAfterTheCall.begin(),
					kAfterTheCall.end(),
					bytes + index + kCallLength)) {
				sites.push_back(text.address() + index);
			}
		}
		return sites;
	}

	[[nodiscard]] std::uintptr_t TargetOf(std::uintptr_t a_site)
	{
		std::int32_t displacement = 0;
		std::memcpy(
			&displacement,
			reinterpret_cast<const void*>(a_site + 1),
			sizeof(displacement));
		return a_site + kCallLength + displacement;
	}

	// The Address Library the other way round, so the finding can be named
	// rather than only pointed at. Nothing is built until there is something
	// to look up.
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
	const auto sites = FindCallSites();
	if (sites.empty()) {
		logger::warn(
			"use: no call of that shape in the code -- nothing will be used");
		return;
	}

	std::vector<std::uintptr_t> targets;
	for (const auto site : sites) {
		const auto target = TargetOf(site);
		if (InText(target) &&
			std::find(targets.begin(), targets.end(), target) == targets.end()) {
			targets.push_back(target);
		}
	}

	const auto base = REL::Module::get().base();
	if (targets.size() != 1) {
		std::string all;
		for (const auto target : targets) {
			all += std::format("{:#x} ", target - base);
		}
		logger::warn(
			"use: {} call site(s) point at {} different functions ({}) -- too "
			"unclear to call any of them",
			sites.size(),
			targets.size(),
			all);
		return;
	}

	const auto target = targets.front();
	logger::info(
		"use: UseQuickkeyItem looks to be {:#x} (ID {}), called from {:#x} in "
		"{} place(s)",
		target - base,
		IdentityOf(target),
		sites.front() - base,
		sites.size());

	// Written down before it is ever called. If the first call turns out to
	// be the wrong function and takes the game with it, this file still says
	// what was called.
	peek::Note("FavoritesManager::UseQuickkeyItem", target, kFunctionSample);
	peek::Note(
		"the call that named it",
		sites.front() - kCallSiteLead,
		kCallSiteSample);

	g_useQuickkey =
		reinterpret_cast<void (*)(RE::FavoritesManager*, std::uint32_t)>(target);
}

bool use::Ready()
{
	return g_useQuickkey != nullptr;
}

void use::Quickkey(std::uint32_t a_index)
{
	auto* manager = RE::FavoritesManager::GetSingleton();
	if (!manager || !g_useQuickkey) {
		return;
	}
	g_useQuickkey(manager, a_index);
}
