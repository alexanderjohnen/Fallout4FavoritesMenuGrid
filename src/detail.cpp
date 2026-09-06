#include "PCH.h"

#include "detail.h"

namespace
{
	// What one stack of a thing carries with it, and how many of the thing
	// the player has in total.
	struct Carried
	{
		std::uint32_t count{ 0 };
		const RE::TBO_InstanceData* instance{ nullptr };
	};

	[[nodiscard]] Carried Find(RE::TESBoundObject* a_object)
	{
		Carried carried;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!a_object || !player || !player->inventoryList) {
			return carried;
		}

		// ForEachStack does not lock, so every caller runs as a UI task.
		player->inventoryList->ForEachStack(
			[&](RE::BGSInventoryItem& a_item) { return a_item.object == a_object; },
			[&](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack& a_stack) {
				carried.count += a_stack.count;
				if (!carried.instance && a_stack.extra) {
					if (const auto* data =
							a_stack.extra->GetByType<RE::ExtraInstanceData>()) {
						carried.instance = data->data.get();
					}
				}
				return true;
			});
		return carried;
	}

	[[nodiscard]] std::string_view WithoutTag(std::string_view a_name, bool a_strip)
	{
		if (!a_strip || a_name.empty() || a_name.front() != '[') {
			return a_name;
		}
		const auto close = a_name.find(']');
		if (close == std::string_view::npos) {
			return a_name;
		}
		auto rest = a_name.substr(close + 1);
		while (!rest.empty() && rest.front() == ' ') {
			rest.remove_prefix(1);
		}
		return rest.empty() ? a_name : rest;
	}

	// A number the way a menu shows one: no decimals where there is nothing
	// after the point, which is most of the time.
	[[nodiscard]] std::string Number(float a_value)
	{
		if (std::abs(a_value - std::round(a_value)) < 0.05F) {
			return std::format("{:.0f}", a_value);
		}
		return std::format("{:.1f}", a_value);
	}

	void Add(std::string& a_line, std::string_view a_piece)
	{
		if (a_piece.empty()) {
			return;
		}
		if (!a_line.empty()) {
			a_line += "   ";
		}
		a_line += a_piece;
	}

	// The resistances a damage type list carries, named by the actor value
	// each one resists: "Damage Resistance", "Energy Resistance". The short
	// form is the first letter of each word, which is how the Pip-Boy's own
	// headers read once they run out of room.
	[[nodiscard]] std::string Initials(std::string_view a_name)
	{
		std::string initials;
		bool atStart = true;
		for (const auto character : a_name) {
			if (character == ' ') {
				atStart = true;
				continue;
			}
			if (atStart) {
				initials.push_back(
					static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
				atStart = false;
			}
		}
		return initials;
	}

	using ValuePairs =
		RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>>;

	void AddResistances(std::string& a_line, const ValuePairs* a_types)
	{
		if (!a_types) {
			return;
		}
		for (const auto& pair : *a_types) {
			const auto* type = pair.first ? pair.first->As<RE::BGSDamageType>() : nullptr;
			const auto* resisted = type ? type->data.resistance : nullptr;
			if (!resisted) {
				continue;
			}
			// The value is a float in this pair -- the same union carries an
			// integer for other users of it, and reading the wrong half of a
			// union gives a number that looks like an address.
			const auto value = pair.second.f;
			if (value <= 0.0F) {
				continue;
			}
			Add(a_line,
				std::format(
					"{} {}",
					Initials(RE::TESFullName::GetFullName(*resisted)),
					Number(value)));
		}
	}

	void DescribeWeapon(
		std::string& a_line,
		RE::TESObjectWEAP* a_weapon,
		const RE::TBO_InstanceData* a_instance)
	{
		// A weapon with mods on it is a different weapon from the one in the
		// plugin, and the stack is where that difference lives.
		const auto* data = a_instance
			? static_cast<const RE::TESObjectWEAP::InstanceData*>(a_instance)
			: &a_weapon->weaponData;

		if (data->attackDamage > 0) {
			Add(a_line, std::format("DMG {}", data->attackDamage));
		}

		if (const auto* ammo = data->ammo) {
			auto name = std::string(RE::TESFullName::GetFullName(*ammo));
			const auto held = Find(const_cast<RE::TESAmmo*>(ammo)).count;
			Add(a_line,
				held > 0 ? std::format("{} {}", WithoutTag(name, true), held)
						 : std::string(WithoutTag(name, true)));
		}

		AddResistances(a_line, data->damageTypes);
	}

	void DescribeArmor(
		std::string& a_line,
		RE::TESObjectARMO* a_armor,
		const RE::TBO_InstanceData* a_instance)
	{
		const auto* data = a_instance
			? static_cast<const RE::TESObjectARMO::InstanceData*>(a_instance)
			: &a_armor->armorData;

		AddResistances(a_line, data->damageTypes);
	}
}

detail::Lines detail::Describe(RE::TESBoundObject* a_object, bool a_stripTags)
{
	Lines lines;
	if (!a_object) {
		return lines;
	}

	lines.name =
		std::string(WithoutTag(RE::TESFullName::GetFullName(*a_object), a_stripTags));

	const auto carried = Find(a_object);

	switch (a_object->GetFormType()) {
	case RE::ENUM_FORM_ID::kWEAP:
		if (auto* weapon = a_object->As<RE::TESObjectWEAP>()) {
			DescribeWeapon(lines.what, weapon, carried.instance);
		}
		break;

	case RE::ENUM_FORM_ID::kARMO:
		if (auto* armor = a_object->As<RE::TESObjectARMO>()) {
			DescribeArmor(lines.what, armor, carried.instance);
		}
		break;

	default:
		break;
	}

	// How many there are, last, because it is the one thing that is true of
	// everything and the least worth reading first.
	if (carried.count > 1) {
		Add(lines.what, std::format("x{}", carried.count));
	}
	return lines;
}
