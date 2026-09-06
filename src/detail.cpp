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

	// The initials of a resistance's name: "Energy Resistance" becomes ER,
	// which is how the Pip-Boy's own headers read once they run out of room.
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
				initials.push_back(static_cast<char>(
					std::toupper(static_cast<unsigned char>(character))));
				atStart = false;
			}
		}
		return initials;
	}

	using ValuePairs =
		RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>>;

	// The value in one of those pairs is a union of an integer and a float,
	// and which half is meant is not written down anywhere. Reading the float
	// half of an integer gives a denormal -- a number just above zero, which
	// printed as "ER 0" and looked like an armour with no resistance rather
	// than like a misreading. So the integer is the answer, and the float is
	// only trusted where the integer cannot be one: a real resistance is a
	// small number, and anything above this is the other half showing through.
	constexpr std::uint32_t kNotAnAmount = 100000;

	[[nodiscard]] float AmountOf(RE::BGSTypedFormValuePair::SharedVal a_value)
	{
		if (a_value.i < kNotAnAmount) {
			return static_cast<float>(a_value.i);
		}
		const auto asFloat = a_value.f;
		return std::isfinite(asFloat) && asFloat < static_cast<float>(kNotAnAmount)
			? asFloat
			: 0.0F;
	}

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
			// A resistance of nothing is not worth the room it takes. The
			// Pip-Boy lists every kind because it has a column for each;
			// one line has to earn its words.
			const auto value = AmountOf(pair.second);
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

	// What a thing does when it goes off, rather than what it does when you
	// hit somebody with it. A grenade's own attack damage is 1 -- that is the
	// thrown object striking a body -- and reading it out as the damage was
	// the sort of number that makes a reader distrust every other number on
	// the panel.
	[[nodiscard]] float ExplosionDamage(
		const RE::TESObjectWEAP::InstanceData* a_data)
	{
		const RE::BGSProjectile* projectile = nullptr;
		if (a_data->rangedData) {
			projectile = a_data->rangedData->overrideProjectile;
		}
		if (!projectile && a_data->ammo) {
			projectile = a_data->ammo->data.projectile;
		}
		if (!projectile || !projectile->data.explosionType) {
			return 0.0F;
		}
		return projectile->data.explosionType->data.damage;
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

		const auto thrown = data->type == RE::WEAPON_TYPE::kGrenade ||
			data->type == RE::WEAPON_TYPE::kMine;
		const auto blast = thrown ? ExplosionDamage(data) : 0.0F;

		if (blast > 0.0F) {
			Add(a_line, std::format("DMG {}", Number(blast)));
		} else if (data->attackDamage > 0 && !thrown) {
			Add(a_line, std::format("DMG {}", data->attackDamage));
		}

		// Ammunition, and how much of it is left -- the one number a player
		// reaches for a weapon to find out.
		if (const auto* ammo = data->ammo) {
			const auto name = std::string(RE::TESFullName::GetFullName(*ammo));
			const auto held = Find(const_cast<RE::TESAmmo*>(ammo)).count;
			Add(a_line,
				std::format("{} ({})", WithoutTag(name, true), held));
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
