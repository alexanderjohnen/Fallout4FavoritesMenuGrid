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

	// A place to look at: a number, and how much of it to copy. Zero length
	// means "the function around it".
	struct Request
	{
		std::uint64_t value{ 0 };
		std::size_t length{ 0 };
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
	// Beware: a function built from separate chunks has one entry per chunk,
	// so the answer can be a piece rather than the whole thing.
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

	// "534268" or "0x1a7210:0x200" -- the number, and after a colon how much
	// to copy.
	[[nodiscard]] std::vector<Request> ParseList(const std::wstring& a_raw)
	{
		std::vector<Request> requests;
		std::size_t at = 0;
		while (at <= a_raw.size()) {
			const auto comma = a_raw.find(L',', at);
			const auto piece = a_raw.substr(
				at, comma == std::wstring::npos ? std::wstring::npos : comma - at);

			const auto colon = piece.find(L':');
			const auto number = std::wcstoull(
				piece.substr(0, colon).c_str(), nullptr, 0);
			if (number != 0) {
				Request request{ number, 0 };
				if (colon != std::wstring::npos) {
					request.length = static_cast<std::size_t>(
						std::wcstoull(piece.c_str() + colon + 1, nullptr, 0));
				}
				requests.push_back(request);
			}

			if (comma == std::wstring::npos) {
				break;
			}
			at = comma + 1;
		}
		return requests;
	}

	[[nodiscard]] std::wstring ReadSetting(
		const std::filesystem::path& a_settings,
		const wchar_t* a_key)
	{
		std::wstring value(1024, L'\0');
		const auto length = GetPrivateProfileStringW(
			L"Debug",
			a_key,
			L"",
			value.data(),
			static_cast<DWORD>(value.size()),
			a_settings.c_str());
		value.resize(length);
		return value;
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

	// One address, written out as the whole function it sits in unless a
	// length was asked for.
	void WritePlace(
		std::ostream& a_out,
		std::string_view a_label,
		std::uintptr_t a_address,
		std::size_t a_length)
	{
		if (a_length != 0) {
			WriteBlock(a_out, a_label, a_address, a_length);
			return;
		}
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

void peek::Run(const std::filesystem::path& a_settings)
{
	const auto ids = ParseList(ReadSetting(a_settings, L"PeekIDs"));
	const auto offsets = ParseList(ReadSetting(a_settings, L"PeekRVAs"));
	const auto vtables = ParseList(ReadSetting(a_settings, L"PeekVtableRefs"));
	if (ids.empty() && offsets.empty() && vtables.empty()) {
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

	const auto base = REL::Module::get().base();
	out << std::format("# base {:#x}\n", base);

	for (const auto& request : ids) {
		if (const auto address = Resolve(request.value, REL::Segment::text)) {
			WritePlace(
				out, std::format("id {}", request.value), *address, request.length);
		}
	}

	for (const auto& request : offsets) {
		const auto address = base + request.value;
		if (!InSegment(address, REL::Segment::text)) {
			logger::warn("peek: {:#x} is not in the code section", request.value);
			continue;
		}
		WritePlace(
			out, std::format("rva {:#x}", request.value), address, request.length);
	}

	for (const auto& request : vtables) {
		const auto address = Resolve(request.value, REL::Segment::rdata);
		if (!address) {
			continue;
		}
		const auto hits = FindReferences(*address);
		logger::info(
			"peek: vtable {} is mentioned {} time(s) in the code",
			request.value,
			hits.size());
		for (const auto hit : hits) {
			WritePlace(
				out,
				std::format("vtable {} used at", request.value),
				hit,
				request.length);
		}
	}

	logger::info("peek: wrote {}", path.string());
}
