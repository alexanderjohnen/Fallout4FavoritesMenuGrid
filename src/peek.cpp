#include "PCH.h"

#include "peek.h"

#include <fstream>

namespace
{
	// A function longer than this is either not a function or not worth
	// carrying through a text file.
	constexpr std::size_t kFunctionLimit = 0x2000;

	// What to copy when the exception directory knows no function.
	constexpr std::size_t kPlainLength = 0x100;

	struct Range
	{
		std::uintptr_t begin{ 0 };
		std::uintptr_t end{ 0 };
	};

	[[nodiscard]] bool InSegment(std::uintptr_t a_address, REL::Segment::Name a_name)
	{
		const auto segment = REL::Module::get().segment(a_name);
		return a_address >= segment.address() &&
			a_address < segment.address() + segment.size();
	}

	// The exception directory lists where every function begins and ends. It
	// is plain data, so it survives what the packer does to the code, and it
	// turns an address in the middle of a function into the whole of it.
	[[nodiscard]] std::optional<Range> FunctionAround(std::uintptr_t a_address)
	{
		const auto base = REL::Module::get().base();
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto* headers =
			reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		const auto& directory =
			headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
		if (!directory.VirtualAddress || !directory.Size) {
			return std::nullopt;
		}

		const auto* functions = reinterpret_cast<const RUNTIME_FUNCTION*>(
			base + directory.VirtualAddress);
		const auto count = directory.Size / sizeof(RUNTIME_FUNCTION);
		const auto rva = static_cast<std::uint32_t>(a_address - base);

		std::size_t low = 0;
		std::size_t high = count;
		while (low < high) {
			const auto middle = low + (high - low) / 2;
			if (rva < functions[middle].BeginAddress) {
				high = middle;
			} else if (rva >= functions[middle].EndAddress) {
				low = middle + 1;
			} else {
				return Range{ base + functions[middle].BeginAddress,
					base + functions[middle].EndAddress };
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::vector<std::uint64_t> ParseList(const std::wstring& a_raw)
	{
		std::vector<std::uint64_t> values;
		std::size_t at = 0;
		while (at < a_raw.size()) {
			const auto comma = a_raw.find(L',', at);
			auto piece = a_raw.substr(
				at, comma == std::wstring::npos ? std::wstring::npos : comma - at);
			wchar_t* end = nullptr;
			const auto value = std::wcstoull(piece.c_str(), &end, 10);
			if (value != 0) {
				values.push_back(value);
			}
			if (comma == std::wstring::npos) {
				break;
			}
			at = comma + 1;
		}
		return values;
	}

	// The address library answers a wrong ID with a neighbour's offset
	// instead of an error, so every address is checked against the section
	// it should be in before anything is read through it.
	[[nodiscard]] std::optional<std::uintptr_t> Resolve(
		std::uint64_t a_id,
		REL::Segment::Name a_expected)
	{
		const auto address = REL::Module::get().base() +
			REL::IDDatabase::get().id2offset(a_id);
		if (!InSegment(address, a_expected)) {
			logger::warn(
				"peek: ID {} lands outside the section it should be in", a_id);
			return std::nullopt;
		}
		return address;
	}

	void WriteBlock(
		std::ostream& a_out,
		std::string_view a_label,
		std::uintptr_t a_address,
		std::size_t a_length)
	{
		const auto base = REL::Module::get().base();
		a_out << std::format(
			"\n# {} rva {:#x} length {:#x}\n", a_label, a_address - base, a_length);

		const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_address);
		std::string line;
		for (std::size_t index = 0; index < a_length; ++index) {
			line += std::format("{:02x}", bytes[index]);
			if (line.size() >= 64) {
				a_out << line << '\n';
				line.clear();
			}
		}
		if (!line.empty()) {
			a_out << line << '\n';
		}
	}

	// One address, written out as the whole function it sits in when that
	// can be found.
	void WriteFunction(
		std::ostream& a_out,
		std::string_view a_label,
		std::uintptr_t a_address)
	{
		if (const auto range = FunctionAround(a_address)) {
			const auto length = range->end - range->begin;
			if (length <= kFunctionLimit) {
				WriteBlock(a_out, a_label, range->begin, length);
				return;
			}
			logger::info(
				"peek: {} sits in a function of {:#x} bytes -- only the start",
				a_label,
				length);
		}
		WriteBlock(a_out, a_label, a_address, kPlainLength);
	}

	// Every `lea reg, [rip + x]` in the code section that points at the
	// address. That is how a vtable pointer reaches an object, so this finds
	// the places where the engine builds one for itself.
	[[nodiscard]] std::vector<std::uintptr_t> FindReferences(std::uintptr_t a_target)
	{
		std::vector<std::uintptr_t> hits;

		const auto text = REL::Module::get().segment(REL::Segment::text);
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.address());
		const auto size = text.size();

		for (std::size_t index = 0; index + 7 <= size; ++index) {
			if ((bytes[index] & 0xF8) != 0x48 || bytes[index + 1] != 0x8D) {
				continue;
			}
			if ((bytes[index + 2] & 0xC7) != 0x05) {
				continue;
			}
			std::int32_t displacement = 0;
			std::memcpy(&displacement, bytes + index + 3, sizeof(displacement));
			const auto here = text.address() + index;
			if (here + 7 + displacement == a_target) {
				hits.push_back(here);
			}
		}
		return hits;
	}
}

void peek::Run(const std::wstring& a_ids, const std::wstring& a_vtableRefs)
{
	const auto ids = ParseList(a_ids);
	const auto vtables = ParseList(a_vtableRefs);
	if (ids.empty() && vtables.empty()) {
		return;
	}

	auto directory = logger::log_directory();
	if (!directory) {
		return;
	}
	const auto path = *directory / std::format("{}.peek.txt", PLUGIN_LOG_NAME);

	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		logger::error("peek: could not write next to the log");
		return;
	}
	out << std::format("# base {:#x}\n", REL::Module::get().base());

	for (const auto id : ids) {
		if (const auto address = Resolve(id, REL::Segment::text)) {
			WriteFunction(out, std::format("id {}", id), *address);
		}
	}

	for (const auto id : vtables) {
		const auto address = Resolve(id, REL::Segment::rdata);
		if (!address) {
			continue;
		}
		const auto hits = FindReferences(*address);
		logger::info(
			"peek: vtable {} is mentioned {} time(s) in the code", id, hits.size());
		for (const auto hit : hits) {
			WriteFunction(out, std::format("vtable {} used at", id), hit);
		}
	}

	logger::info("peek: wrote {}", path.string());
}
