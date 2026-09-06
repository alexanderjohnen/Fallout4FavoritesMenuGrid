#pragma once

// What an item's name says about its icon.
//
// An item sorter renames things: "[Aid] Antibiotics", "[Stimpak] Stimpak".
// The tag in front is a keyword, and a tag configuration maps that keyword to
// a symbol in an icon library:
//
//     <tags iconLibraryFile="FallUI_IconLib.swf">
//       <tag keyword="Stimpak" icon="M8r.Repo.MedSyringe" colorname="MedicLightRed" />
//
// Three things make this less tidy than it looks, and all three are real on a
// machine with a few hundred mods:
//
//   * There is more than one library. Every `<tags>` block names its own, and
//     the addon folder holds a dozen of them -- one per mod that brings its
//     own artwork.
//   * There is more than one configuration. FIS and DEF_UI both live under
//     Interface\ItemSorter, and which one is in use is an MCM setting we
//     cannot read. So all of them are read and merged: a keyword is looked up
//     by what the item is actually called, and whichever sorter wrote that
//     name, its keyword is in the map.
//   * A colour comes with the keyword, by name, resolved through <color>
//     entries that may themselves be aliases of other colours.
//
// None of this is shipped with the mod. It is read where the player already
// has it, which is what FallUI's own auto-detect does, and when none of it is
// there the map is simply empty.
namespace tags
{
	// Above white, the same way the rest of this plugin says "no colour was
	// asked for".
	inline constexpr std::uint32_t kNoColor = 0x1000000;

	struct Icon
	{
		std::string symbol;   // the class name, without the "m_" the SWF adds
		std::string library;  // the movie it lives in, as a Scaleform path
		std::uint32_t color{ kNoColor };
	};

	// Reads every tag configuration under Interface\ItemSorter and merges
	// them. Safe to call again; it starts over.
	void Load(const std::filesystem::path& a_interface);

	// The keyword in front of an item's name, or nothing. "[Aid] Antibiotics"
	// gives "Aid"; DEF_UI's "[Aid|Chem]" gives "Aid", because the part after
	// the bar is a subtitle rather than a second keyword.
	[[nodiscard]] std::string_view KeywordOf(std::string_view a_name);

	// What that keyword draws, or nothing.
	[[nodiscard]] const Icon* Find(std::string_view a_keyword);

	[[nodiscard]] std::size_t Count();
}
