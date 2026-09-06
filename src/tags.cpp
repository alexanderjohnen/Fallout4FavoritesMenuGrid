#include "PCH.h"

#include "tags.h"

#include <fstream>
#include <functional>

namespace
{
	std::unordered_map<std::string, tags::Icon> g_icons;
	// The colour a keyword asks for, by name. Kept beside the icons rather
	// than in them because a name can only be resolved once every file has
	// been read: an alias may point at a colour another file defines.
	std::unordered_map<std::string, std::string> g_wanted;
	std::unordered_map<std::string, std::string> g_colorHex;
	std::unordered_map<std::string, std::string> g_colorAlias;

	[[nodiscard]] std::string Lowered(std::string_view a_text)
	{
		std::string lowered(a_text);
		std::transform(
			lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
		return lowered;
	}

	// One attribute out of one element. These files are hand-written XML,
	// with comments, line breaks inside elements and the odd stray
	// character; a reader that only ever looks for `name="value"` in an
	// element it already found is less to go wrong than one that tries to
	// understand the document.
	[[nodiscard]] std::string Attribute(
		std::string_view a_element,
		std::string_view a_name)
	{
		const auto key = std::string(a_name) + "=";
		auto at = a_element.find(key);
		while (at != std::string_view::npos) {
			// Whitespace in front, so `name=` cannot answer for `colorname=`.
			if (at > 0 && std::isspace(static_cast<unsigned char>(a_element[at - 1]))) {
				break;
			}
			at = a_element.find(key, at + 1);
		}
		if (at == std::string_view::npos) {
			return {};
		}

		auto value = a_element.substr(at + key.size());
		if (value.empty()) {
			return {};
		}
		const auto quote = value.front();
		if (quote != '"' && quote != '\'') {
			return {};
		}
		value.remove_prefix(1);
		const auto close = value.find(quote);
		return close == std::string_view::npos
			? std::string{}
			: std::string(value.substr(0, close));
	}

	// Every `<name ...>` in the text, handed over whole.
	void ForEachElement(
		std::string_view a_text,
		std::string_view a_name,
		const std::function<void(std::string_view)>& a_visit)
	{
		const auto open = std::string("<") + std::string(a_name);
		std::size_t at = 0;
		while ((at = a_text.find(open, at)) != std::string_view::npos) {
			const auto after = at + open.size();
			// `<tag` must not answer for `<tags`.
			if (after < a_text.size() &&
				!std::isspace(static_cast<unsigned char>(a_text[after])) &&
				a_text[after] != '>' && a_text[after] != '/') {
				at = after;
				continue;
			}
			const auto close = a_text.find('>', after);
			if (close == std::string_view::npos) {
				return;
			}
			a_visit(a_text.substr(at, close - at));
			at = close;
		}
	}

	[[nodiscard]] std::string ReadFile(const std::filesystem::path& a_path)
	{
		std::ifstream in(a_path, std::ios::binary);
		if (!in) {
			return {};
		}
		return std::string(
			std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	void ReadColors(std::string_view a_text)
	{
		ForEachElement(a_text, "color", [](std::string_view a_element) {
			const auto name = Attribute(a_element, "name");
			if (name.empty()) {
				return;
			}
			const auto hex = Attribute(a_element, "hex");
			if (!hex.empty()) {
				g_colorHex[Lowered(name)] = hex;
				return;
			}
			const auto alias = Attribute(a_element, "alias");
			if (!alias.empty()) {
				g_colorAlias[Lowered(name)] = Lowered(alias);
			}
		});
	}

	// The tags of one file. Each `<tags>` block names the library its icons
	// live in, and the `<tag>` elements that follow belong to it until the
	// next block begins.
	void ReadTags(std::string_view a_text)
	{
		struct Block
		{
			std::size_t at{ 0 };
			std::string library;
		};

		std::vector<Block> blocks;
		std::size_t at = 0;
		while ((at = a_text.find("<tags", at)) != std::string_view::npos) {
			const auto close = a_text.find('>', at);
			if (close == std::string_view::npos) {
				break;
			}
			blocks.push_back(Block{
				at, Attribute(a_text.substr(at, close - at), "iconLibraryFile") });
			at = close;
		}
		if (blocks.empty()) {
			return;
		}

		std::size_t block = 0;
		ForEachElement(a_text, "tag", [&](std::string_view a_element) {
			const auto keyword = Attribute(a_element, "keyword");
			const auto icon = Attribute(a_element, "icon");
			if (keyword.empty() || icon.empty()) {
				return;
			}

			// Which block this tag sits in: the last one that begins before
			// it. Elements come in document order, so this only walks
			// forward.
			const auto here =
				static_cast<std::size_t>(a_element.data() - a_text.data());
			while (block + 1 < blocks.size() && blocks[block + 1].at < here) {
				++block;
			}

			const auto key = Lowered(keyword);
			tags::Icon entry;
			entry.symbol = icon;
			entry.library = blocks[block].library;
			g_icons.insert_or_assign(key, std::move(entry));

			auto color = Attribute(a_element, "colorname");
			if (!color.empty()) {
				// A symbol built from several shapes names a colour for each,
				// separated by commas -- RadAway is brown and silver. We paint
				// one flat colour, so the first is the one that counts.
				const auto comma = color.find(',');
				if (comma != std::string::npos) {
					color.resize(comma);
				}
				g_wanted[key] = Lowered(color);
			}
		});
	}

	[[nodiscard]] std::optional<std::uint32_t> ParseHex(const std::string& a_hex)
	{
		const auto* begin = a_hex.c_str() + (a_hex.starts_with("#") ? 1 : 0);
		char* end = nullptr;
		const auto value = std::strtoul(begin, &end, 16);
		if (end == begin) {
			return std::nullopt;
		}
		return static_cast<std::uint32_t>(value & 0xFFFFFF);
	}

	// A colour name, through however many aliases it takes. Aliases are
	// written by hand and could in principle point at one another, so the
	// walk is bounded rather than trusted.
	[[nodiscard]] std::uint32_t ResolveColor(std::string a_name)
	{
		for (int step = 0; step < 8; ++step) {
			if (const auto hex = g_colorHex.find(a_name); hex != g_colorHex.end()) {
				if (const auto parsed = ParseHex(hex->second)) {
					return *parsed;
				}
				return tags::kNoColor;
			}
			const auto alias = g_colorAlias.find(a_name);
			if (alias == g_colorAlias.end()) {
				return tags::kNoColor;
			}
			a_name = alias->second;
		}
		return tags::kNoColor;
	}

	// A variation is an alternative icon set the player picks in MCM, and the
	// files for all of them sit side by side in a folder. Reading them all
	// means the last one read wins, which is how "Stimpak" came out as a med
	// kit on a machine whose owner had chosen no variation at all. So the
	// folder is skipped, and afterwards exactly the chosen ones are read.
	struct Variation
	{
		std::string modName;
		std::string section;
		std::string basePath;
		std::string key;
	};

	std::vector<Variation> g_variations;

	void ReadVariations(std::string_view a_text)
	{
		struct Block
		{
			std::size_t at{ 0 };
			Variation what;
		};

		std::vector<Block> blocks;
		std::size_t at = 0;
		while ((at = a_text.find("<variations", at)) != std::string_view::npos) {
			const auto close = a_text.find('>', at);
			if (close == std::string_view::npos) {
				break;
			}
			const auto element = a_text.substr(at, close - at);
			blocks.push_back(Block{ at,
				Variation{ Attribute(element, "modName"),
					Attribute(element, "iniSection"),
					Attribute(element, "baseXmlPath"),
					{} } });
			at = close;
		}
		if (blocks.empty()) {
			return;
		}

		std::size_t block = 0;
		ForEachElement(a_text, "variation", [&](std::string_view a_element) {
			const auto key = Attribute(a_element, "iniKey");
			if (key.empty()) {
				return;
			}
			const auto here =
				static_cast<std::size_t>(a_element.data() - a_text.data());
			while (block + 1 < blocks.size() && blocks[block + 1].at < here) {
				++block;
			}
			auto what = blocks[block].what;
			what.key = key;
			g_variations.push_back(std::move(what));
		});
	}

	// What MCM has the player set. The settings a player changed live under
	// MCM\Settings; what the mod shipped lives under MCM\Config. The first
	// wins, and neither having it means the default, which is none.
	[[nodiscard]] std::string McmValue(
		const std::filesystem::path& a_data,
		const Variation& a_variation)
	{
		const std::array<std::filesystem::path, 2> places{
			a_data / "MCM" / "Settings" /
				(std::filesystem::path(a_variation.modName).wstring() + L".ini"),
			a_data / "MCM" / "Config" / a_variation.modName / "settings.ini"
		};

		for (const auto& place : places) {
			std::error_code error;
			if (!std::filesystem::exists(place, error)) {
				continue;
			}
			const std::wstring section(
				a_variation.section.begin(), a_variation.section.end());
			const std::wstring key(a_variation.key.begin(), a_variation.key.end());
			std::wstring value(128, L'\0');
			value.resize(GetPrivateProfileStringW(
				section.c_str(),
				key.c_str(),
				L"",
				value.data(),
				static_cast<DWORD>(value.size()),
				place.c_str()));
			while (!value.empty() && (value.back() == L' ' || value.back() == L'\t')) {
				value.pop_back();
			}
			if (!value.empty()) {
				// A variation name is a file name and plainly ASCII, but the
				// conversion is written out rather than left to a narrowing
				// copy the compiler is right to complain about.
				std::string narrow;
				narrow.reserve(value.size());
				for (const auto character : value) {
					narrow.push_back(static_cast<char>(character));
				}
				return narrow;
			}
		}
		return {};
	}

	void ReadEvery(const std::filesystem::path& a_directory, int a_depth)
	{
		std::error_code error;
		if (a_depth < 0 || !std::filesystem::is_directory(a_directory, error)) {
			return;
		}
		for (const auto& entry :
			std::filesystem::directory_iterator(a_directory, error)) {
			if (entry.is_directory(error)) {
				if (Lowered(entry.path().filename().string()) != "variations") {
					ReadEvery(entry.path(), a_depth - 1);
				}
				continue;
			}
			if (Lowered(entry.path().extension().string()) != ".xml") {
				continue;
			}
			const auto text = ReadFile(entry.path());
			if (text.empty()) {
				continue;
			}
			ReadColors(text);
			ReadTags(text);
			ReadVariations(text);
		}
	}

	// The variations the player actually chose, read last so they win.
	void ReadChosenVariations(const std::filesystem::path& a_interface)
	{
		const auto data = a_interface.parent_path();
		for (const auto& variation : g_variations) {
			if (variation.modName.empty() || variation.basePath.empty()) {
				continue;
			}
			const auto value = McmValue(data, variation);
			if (value.empty() || Lowered(value) == "none") {
				continue;
			}

			const auto path = a_interface /
				std::filesystem::path(variation.basePath + value + ".xml");
			const auto text = ReadFile(path);
			if (text.empty()) {
				logger::info(
					"tags: {} is set to \"{}\", but there is no such file",
					variation.key,
					value);
				continue;
			}
			logger::info("tags: {} is \"{}\"", variation.key, value);
			ReadColors(text);
			ReadTags(text);
		}
	}
}

void tags::Load(const std::filesystem::path& a_interface)
{
	g_icons.clear();
	g_wanted.clear();
	g_colorHex.clear();
	g_colorAlias.clear();

	// Three levels reach Interface\ItemSorter itself, a sorter's own folder
	// beneath it, and the addon folder inside that.
	g_variations.clear();
	const auto root = a_interface / "ItemSorter";
	ReadEvery(root, 3);
	ReadChosenVariations(a_interface);

	for (auto& [keyword, icon] : g_icons) {
		const auto wanted = g_wanted.find(keyword);
		if (wanted != g_wanted.end()) {
			icon.color = ResolveColor(wanted->second);
		}
	}

	std::size_t libraries = 0;
	std::set<std::string> seen;
	for (const auto& [keyword, icon] : g_icons) {
		if (!icon.library.empty() && seen.insert(icon.library).second) {
			++libraries;
		}
	}

	logger::info(
		"tags: {} keywords out of {} libraries, read from {}",
		g_icons.size(),
		libraries,
		root.string());
}

std::string_view tags::KeywordOf(std::string_view a_name)
{
	if (a_name.empty() || a_name.front() != '[') {
		return {};
	}
	const auto close = a_name.find(']');
	if (close == std::string_view::npos) {
		return {};
	}
	auto keyword = a_name.substr(1, close - 1);

	// DEF_UI writes "[Aid|Chem]": what follows the bar is a subtitle, not a
	// second keyword.
	const auto bar = keyword.find('|');
	if (bar != std::string_view::npos) {
		keyword = keyword.substr(0, bar);
	}
	while (!keyword.empty() && keyword.back() == ' ') {
		keyword.remove_suffix(1);
	}
	return keyword;
}

const tags::Icon* tags::Find(std::string_view a_keyword)
{
	if (a_keyword.empty()) {
		return nullptr;
	}
	const auto found = g_icons.find(Lowered(a_keyword));
	return found == g_icons.end() ? nullptr : &found->second;
}

std::size_t tags::Count()
{
	return g_icons.size();
}
