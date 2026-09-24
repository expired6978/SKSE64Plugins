#include "OverrideInterface.h"
#include "SKEETasks.h"

#include "ShaderUtilities.h"
#include "OverrideVariant.h"
#include "StringTable.h"
#include "NifUtils.h"
#include "NiRTTIUtils.h"
#include "Utilities.h"
#include "ActorUpdateManager.h"


#include "RE/N/NiGeometry.h"
#include <cstdint>


extern ActorUpdateManager	g_actorUpdateManager;
extern OverrideInterface	g_overrideInterface;
extern const SKSE::TaskInterface* g_task;
extern StringTable			g_stringTable;

skee_u32 OverrideInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverrideInterface::Impl_AddRawOverride(OverrideHandle handle, bool isFemale, OverrideHandle armorHandle, OverrideHandle addonHandle, RE::BSFixedString nodeName, OverrideVariant & value)
{
	armorData.Lock();
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::Impl_AddOverride(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle formId = refr->formID;
	OverrideHandle armorFormId = armor->formID;
	OverrideHandle addonFormId = addon->formID;
	armorData.Lock();
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::Impl_AddRawNodeOverride(OverrideHandle handle, bool isFemale, RE::BSFixedString nodeName, OverrideVariant & value)
{
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::Impl_AddNodeOverride(RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::Impl_AddRawWeaponOverride(OverrideHandle handle, bool isFemale, bool firstPerson, OverrideHandle weaponHandle, RE::BSFixedString nodeName, OverrideVariant & value)
{
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::Impl_AddWeaponOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	OverrideHandle weaponHandle = weapon->formID;
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::Impl_AddRawSkinOverride(OverrideHandle handle, bool isFemale, bool firstPerson, std::uint32_t slotMask, OverrideVariant & value)
{
	skinData.Lock();
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].erase(value);
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].insert(value);
	skinData.Release();
}

void OverrideInterface::Impl_AddSkinOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, OverrideVariant & value)
{
	Impl_AddRawSkinOverride(refr->formID, isFemale, firstPerson, slotMask, value);
}

OverrideVariant * OverrideInterface::Impl_GetOverride(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					OverrideVariant ovr;
					ovr.key = key;
					ovr.index = index;
					auto ost = oit->second.find(ovr);
					if(ost != oit->second.end())
					{
						return const_cast<OverrideVariant*>(&(*ost));
					}
				}
			}
		}
	}

	return NULL;
}

OverrideVariant * OverrideInterface::Impl_GetNodeOverride(RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto ost = oit->second.find(ovr);
			if(ost != oit->second.end())
			{
				return const_cast<OverrideVariant*>(&(*ost));
			}
		}
	}

	return NULL;
}

OverrideVariant * OverrideInterface::Impl_GetWeaponOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID);
	if (it != weaponData.m_data.end())
	{
		auto ait = it->second[gender][firstPerson].find(weapon->formID);
		if (ait != it->second[gender][firstPerson].end())
		{
			auto oit = ait->second.find(g_stringTable.GetString(nodeName));
			if (oit != ait->second.end())
			{
				OverrideVariant ovr;
				ovr.key = key;
				ovr.index = index;
				auto ost = oit->second.find(ovr);
				if (ost != oit->second.end())
				{
					return const_cast<OverrideVariant*>(&(*ost));
				}
			}
		}
	}

	return NULL;
}

OverrideVariant * OverrideInterface::Impl_GetSkinOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto slot = it->second[gender][firstPerson].find(slotMask);
		if(slot != it->second[gender][firstPerson].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto ost = slot->second.find(ovr);
			if (ost != slot->second.end())
			{
				return const_cast<OverrideVariant*>(&(*ost));
			}
		}
	}

	return NULL;
}

void OverrideInterface::Impl_RemoveAllReferenceOverrides(RE::TESObjectREFR * refr)
{
	armorData.Lock();
	armorData.m_data.erase(refr->formID);
	armorData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceNodeOverrides(RE::TESObjectREFR * refr)
{
	nodeData.Lock();
	nodeData.m_data.erase(refr->formID);
	nodeData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceWeaponOverrides(RE::TESObjectREFR * refr)
{
	weaponData.Lock();
	weaponData.m_data.erase(refr->formID);
	weaponData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceSkinOverrides(RE::TESObjectREFR * refr)
{
	skinData.Lock();
	skinData.m_data.erase(refr->formID);
	skinData.Release();
}

void OverrideInterface::Impl_RemoveAllArmorOverrides(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto nit = it->second[gender].find(armor->formID);
		if(nit != it->second[gender].end()) {
			nit->second.clear();
		}
	}
}

void OverrideInterface::Impl_RemoveAllArmorAddonOverrides(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				ait->second.erase(dit);
			}
		}
	}
}

void OverrideInterface::Impl_RemoveAllArmorAddonNodeOverrides(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					dit->second.erase(oit);
				}
			}
		}
	}
}

void OverrideInterface::Impl_RemoveArmorAddonOverride(RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					OverrideVariant ovr;
					ovr.key = key;
					ovr.index = index;
					auto ost = oit->second.find(ovr);
					if(ost != oit->second.end())
					{
						oit->second.erase(ost);
					}
				}
			}
		}
	}
}

void OverrideInterface::Impl_RemoveAllNodeNameOverrides(RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			it->second[gender].erase(oit);
		}
	}
}

void OverrideInterface::Impl_RemoveNodeOverride(RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto ost = oit->second.find(ovr);
			if(ost != oit->second.end())
			{
				oit->second.erase(ost);
			}
		}
	}
}

void OverrideInterface::Impl_RemoveAllWeaponOverrides(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID);
	if(it != weaponData.m_data.end())
	{
		WeaponRegistration::iterator ait = it->second[gender][firstPerson ? 1 : 0].find(weapon->formID);
		if(ait != it->second[gender][firstPerson ? 1 : 0].end())
		{
			it->second[gender][firstPerson ? 1 : 0].erase(ait);
		}
	}
}

void OverrideInterface::Impl_RemoveAllWeaponNodeOverrides(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fPerson = firstPerson ? 1 : 0;

	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID);
	if(it != weaponData.m_data.end())
	{
		WeaponRegistration::iterator ait = it->second[gender][fPerson].find(weapon->formID);
		if(ait != it->second[gender][firstPerson].end())
		{
			OverrideRegistration<StringTableItem>::iterator oit = ait->second.find(g_stringTable.GetString(nodeName));
			if(oit != ait->second.end())
			{
				ait->second.erase(oit);
			}
		}
	}
}

void OverrideInterface::Impl_RemoveWeaponOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fPerson = firstPerson ? 1 : 0;

	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID);
	if(it != weaponData.m_data.end())
	{
		WeaponRegistration::iterator ait = it->second[gender][fPerson].find(weapon->formID);
		if(ait != it->second[gender][firstPerson].end())
		{
			OverrideRegistration<StringTableItem>::iterator oit = ait->second.find(g_stringTable.GetString(nodeName));
			if(oit != ait->second.end())
			{
				OverrideVariant ovr;
				ovr.key = key;
				ovr.index = index;
				OverrideSet::iterator ost = oit->second.find(ovr);
				if(ost != oit->second.end())
				{
					oit->second.erase(ost);
				}
			}
		}
	}
}

void OverrideInterface::Impl_RemoveAllSkinOverrides(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto slot = it->second[gender][firstPerson].find(slotMask);
		if (slot != it->second[gender][firstPerson].end())
		{
			it->second[gender][firstPerson].erase(slot);
		}
	}
}

void OverrideInterface::Impl_RemoveSkinOverride(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index)
{
	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fPerson = firstPerson ? 1 : 0;

	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto slot = it->second[gender][firstPerson].find(slotMask);
		if (slot != it->second[gender][firstPerson].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			OverrideSet::iterator ost = slot->second.find(ovr);
			if (ost != slot->second.end())
			{
				slot->second.erase(ost);
			}
		}
	}
}

void OverrideInterface::Impl_SetArmorAddonProperty(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				bool isRoot = nodeName == RE::BSFixedString("");
				if (!armorNode ? armorNode->AsNode() : nullptr && !isRoot) {
					SKSE::log::warn("{} - Warning, override for Armor {:08X} Addon {:08X} has no children, use an empty string for the node name to access the root instead.", __FUNCTION__, armor->formID, addon->formID);
				}
				RE::NiAVObject* foundNode = isRoot ? armorNode : armorNode->GetObjectByName(nodeName);
				if (foundNode) {
					SetShaderProperty(foundNode, value, immediate);
				}
			}
		});
	}
}

void OverrideInterface::Impl_GetArmorAddonProperty(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, OverrideVariant * value)
{
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? armorNode : armorNode->GetObjectByName(nodeName);
				if (foundNode) {
					GetShaderProperty(foundNode, value);
				}
			}
		});
	}
}

bool OverrideInterface::Impl_HasArmorAddonNode(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, bool debug)
{
	if(!refr) {
		if(debug)
			SKSE::log::debug("{} - No reference", __FUNCTION__);
		return false;
	}
	bool found = false;
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? armorNode : armorNode->GetObjectByName(nodeName);
				if (foundNode) {
					if (debug)
						SKSE::log::debug("{} - Success, found node name '{}' for Armor {:08X}, Addon {:08X}.", __FUNCTION__, nodeName.c_str(), armor->formID, addon->formID);

					found = true;
				}
				else if (debug)
					SKSE::log::debug("{} - Failed to find node name '{}' for Armor {:08X}, Addon {:08X}.", __FUNCTION__, nodeName.c_str(), armor->formID, addon->formID);
			}
		});
	}

	return found;
}

void OverrideInterface::Impl_SetWeaponProperty(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	char weaponString[REX::W32::MAX_PATH];

	std::uint8_t gender = isFemale ? 1 : 0;

	memset(weaponString, 0, REX::W32::MAX_PATH);
	weapon->GetNodeName(weaponString);

	RE::NiPointer<RE::NiNode> root{refr->Get3D(firstPerson) ? refr->Get3D(firstPerson)->AsNode() : nullptr}; // Apply to third and first person
	if(root) {
		RE::BSFixedString weaponName(weaponString); // Find the Armor name from the root
		RE::NiAVObject * weaponNode = root->GetObjectByName(weaponName);
		if(weaponNode) {
			bool isRoot = nodeName == RE::BSFixedString("");
			if(!weaponNode ? weaponNode->AsNode() : nullptr && !isRoot) {
				SKSE::log::warn("{} - Warning, override for Weapon {:08X} has no children, use an empty string for the node name to access the root instead.", __FUNCTION__, weapon->formID);
			}
			RE::NiAVObject * foundNode = isRoot ? weaponNode : weaponNode->GetObjectByName(nodeName);
			if(foundNode) {
				SetShaderProperty(foundNode, value, immediate);
			}
		}
	}
}

void OverrideInterface::Impl_GetWeaponProperty(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, OverrideVariant * value)
{
	char weaponString[REX::W32::MAX_PATH];

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	memset(weaponString, 0, REX::W32::MAX_PATH);
	weapon->GetNodeName(weaponString);

	RE::NiPointer<RE::NiNode> root{refr->Get3D(firstPerson) ? refr->Get3D(firstPerson)->AsNode() : nullptr}; // Apply to third and first person
	if(root) {
		RE::BSFixedString weaponName(weaponString); // Find the Armor name from the root
		RE::NiAVObject * weaponNode = root->GetObjectByName(weaponName);
		if(weaponNode) {
			RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(nodeName);
			if(foundNode) {
				GetShaderProperty(foundNode, value);
			}
		}
	}
}

bool OverrideInterface::Impl_HasWeaponNode(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, bool debug)
{
	if(!refr) {
		if(debug)
			SKSE::log::debug("{} - No reference", __FUNCTION__);
		return false;
	}
	char weaponString[REX::W32::MAX_PATH];

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	memset(weaponString, 0, REX::W32::MAX_PATH);
	weapon->GetNodeName(weaponString);

	RE::NiPointer<RE::NiNode> root{refr->Get3D(firstPerson) ? refr->Get3D(firstPerson)->AsNode() : nullptr}; // Apply to third and first person
	if(root) {
		RE::BSFixedString weaponName(weaponString); // Find the Armor name from the root
		RE::NiAVObject * weaponNode = root->GetObjectByName(weaponName);
		if(weaponNode) {
			RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(nodeName);
			if(foundNode) {
				if(debug)	
					SKSE::log::debug("{} - Success, found node name '{}' for Weapon {:08X}.", __FUNCTION__, nodeName.c_str(), weapon->formID);
				return true;
			} else if(debug)
				SKSE::log::debug("{} - Failed to find node name '{}' for Weapon {:08X}.", __FUNCTION__, nodeName.c_str(), weapon->formID);
		} else if(debug)
			SKSE::log::debug("{} - Failed to acquire weapon node '{}' for Weapon {:08X}.", __FUNCTION__, weaponName.c_str(), weapon->formID);
	} else if(debug)
		SKSE::log::debug("{} - Failed to acquire skeleton for Reference {:08X}", __FUNCTION__, refr->formID);

	return false;
}

void OverrideInterface::Impl_SetSkinProperty(RE::TESObjectREFR * refr, bool firstPerson, std::uint32_t slotMask, OverrideVariant * value, bool immediate)
{
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (actor) {
		RE::TESForm * pForm = GetSkinForm(actor, slotMask);
		RE::TESObjectARMO * armor = pForm ? pForm->As<RE::TESObjectARMO>() : nullptr;
		if (armor) {
			for (std::uint32_t i = 0; i < armor->armorAddons.size(); i++) {
				RE::TESObjectARMA* addon = armor->armorAddons[i];
				if (addon) {
					VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
					{
						if (firstPerson == isFP)
						{
							VisitObjects(armorNode, [&](RE::NiAVObject* object)
							{
								RE::BSGeometry * geometry = object ? object->AsGeometry() : nullptr;
								if (geometry)
								{
									RE::BSShaderProperty * shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
									if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
									{
										RE::BSLightingShaderMaterial * material = (RE::BSLightingShaderMaterial *)shaderProperty->material;
										if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint)
										{
											SetShaderProperty(geometry, value, immediate);
										}
									}
								}
								return false;
							});
						}
					});
				}
			}
		}
	}
}

void OverrideInterface::Impl_GetSkinProperty(RE::TESObjectREFR * refr, bool firstPerson, std::uint32_t slotMask, OverrideVariant * value)
{
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (actor) {
		RE::TESForm * pForm = GetSkinForm(actor, slotMask);
		RE::TESObjectARMO * armor = pForm ? pForm->As<RE::TESObjectARMO>() : nullptr;
		if (armor) {
			for (std::uint32_t i = 0; i < armor->armorAddons.size(); i++) {
				RE::TESObjectARMA* addon = armor->armorAddons[i];
				if (addon) {

					VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
					{
						if (firstPerson == isFP)
						{
							VisitObjects(armorNode, [&](RE::NiAVObject* object)
							{
								RE::BSGeometry * geometry = object ? object->AsGeometry() : nullptr;
								if (geometry)
								{
									RE::BSShaderProperty * shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
									if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
									{
										RE::BSLightingShaderMaterial * material = (RE::BSLightingShaderMaterial *)shaderProperty->material;
										if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint)
										{
											GetShaderProperty(geometry, value);
										}
									}
								}
								return false;
							});
						}
					});
				}
			}
		}
	}
}

void OverrideInterface::Impl_SetProperties(OverrideHandle formId, bool immediate)
{
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		return;
	}

	RE::Actor* actor = static_cast<RE::Actor*>(form);
	if (!actor) {
		return;
	}

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(formId); // Find ActorHandle
	if(it != armorData.m_data.end())
	{
		for(ArmorRegistration::iterator ait = it->second[gender].begin(); ait != it->second[gender].end(); ++ait) // Loop Armors
		{
			RE::TESObjectARMO * armor = static_cast<RE::TESObjectARMO *>(RE::TESForm::LookupByID(ait->first));
			if(!armor)
				continue;

			for(AddonRegistration::iterator dit = ait->second.begin(); dit != ait->second.end(); ++dit) // Loop Addons
			{
				RE::TESObjectARMA * addon = static_cast<RE::TESObjectARMA *>(RE::TESForm::LookupByID(dit->first));
				if(!addon)
					continue;

				VisitArmorAddon(actor, armor, addon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
				{
					dit->second.Visit([&](const StringTableItem & key, OverrideSet * set)
					{
						RE::BSFixedString nodeName(key->c_str());
						RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? armorNode : armorNode->GetObjectByName(nodeName);
						if (foundNode) {
							set->Visit([&](OverrideVariant * value)
							{
								if (!immediate) {
									SKEE_AddTask(g_task, new NIOVTaskSetShaderProperty(foundNode, *value));
								}
								else {
									SetShaderProperty(foundNode, value, true);
								}
								return false;
							});
						}

						return false;
					});
				});
			}
		}
	}
}

void OverrideInterface::Impl_SetNodeProperty(RE::TESObjectREFR * refr, bool firstPerson, RE::BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	RE::NiPointer<RE::NiNode> root{refr->Get3D(firstPerson) ? refr->Get3D(firstPerson)->AsNode() : nullptr}; // Apply to third and first person
	if(root) {
		RE::NiAVObject * foundNode = root->GetObjectByName(nodeName);
		if(foundNode) {
			if (!immediate) {
				SKEE_AddTask(g_task, new NIOVTaskSetShaderProperty(foundNode, *value));
			}
			else {
				SetShaderProperty(foundNode, value, true);
			}
		}
	}
}

void OverrideInterface::Impl_GetNodeProperty(RE::TESObjectREFR * refr, bool firstPerson, RE::BSFixedString nodeName, OverrideVariant * value)
{
	RE::NiPointer<RE::NiNode> root{refr->Get3D(firstPerson) ? refr->Get3D(firstPerson)->AsNode() : nullptr}; // Apply to third and first person
	if(root) {
		RE::NiAVObject * foundNode = root->GetObjectByName(nodeName);
		if(foundNode) {
			GetShaderProperty(foundNode, value);
		}
	}
}

void OverrideInterface::Impl_SetNodeProperties(OverrideHandle formId, bool immediate)
{
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		return;
	}

	RE::TESObjectREFR* refr = static_cast<RE::TESObjectREFR*>(form);
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto nit = nodeData.m_data.find(formId); // Find ActorHandle
	if(nit != nodeData.m_data.end())
	{
		RE::NiNode * lastRoot = NULL;
		for(std::uint8_t i = 0; i <= 1; i++)
		{
			RE::NiNode* root = refr->Get3D(i != 0) ? refr->Get3D(i != 0)->AsNode() : nullptr;
			if(root == lastRoot) // First and third are the same, skip
				continue;

			if(root)
			{
				root->IncRefCount();
				nit->second[gender].Visit([&](const StringTableItem & key, OverrideSet * set)
				{
					RE::BSFixedString nodeName(key->c_str());
					RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? root : root->GetObjectByName(nodeName);
					if (foundNode) {
						set->Visit([&](OverrideVariant * value)
						{
							SetShaderProperty(foundNode, value, immediate);
							return false;
						});
					}

					return false;
				});
				root->DecRefCount();
			}

			lastRoot = root;
		}
	}
}

void OverrideInterface::Impl_SetWeaponProperties(OverrideHandle formId, bool immediate)
{
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		return;
	}

	RE::TESObjectREFR* refr = static_cast<RE::TESObjectREFR*>(form);

	char weaponString[REX::W32::MAX_PATH];

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID); // Find ActorHandle
	if (it != weaponData.m_data.end())
	{
		for (std::uint8_t i = 0; i <= 1; i++)
		{
			for (WeaponRegistration::iterator ait = it->second[gender][i].begin(); ait != it->second[gender][i].end(); ++ait) // Loop Armors
			{
				RE::TESObjectWEAP * weapon = static_cast<RE::TESObjectWEAP *>(RE::TESForm::LookupByID(ait->first));
				if (!weapon)
					continue;

				memset(weaponString, 0, REX::W32::MAX_PATH);
				weapon->GetNodeName(weaponString);

				RE::NiPointer<RE::NiNode> lastNode = nullptr;
				RE::BSFixedString weaponName(weaponString);

				RE::NiPointer<RE::NiNode> root{refr->Get3D(i != 0) ? refr->Get3D(i != 0)->AsNode() : nullptr};
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					// Find the Armor node
					RE::NiAVObject * weaponNode = root->GetObjectByName(weaponName);
					if (weaponNode) {
						ait->second.Visit([&](const StringTableItem & key, OverrideSet * set)
						{
							RE::BSFixedString nodeName(key->c_str());
							RE::NiAVObject * foundNode = nodeName == RE::BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(nodeName);
							if (foundNode) {
								set->Visit([&](OverrideVariant * value)
								{
									SetShaderProperty(foundNode, value, immediate);
									return false;
								});
							}

							return false;
						});
					}
				}

				lastNode = root;
			}
		}
	}
}

void OverrideInterface::Impl_SetSkinProperties(OverrideHandle formId, bool immediate)
{
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		return;
	}

	RE::TESObjectREFR* refr = static_cast<RE::TESObjectREFR*>(form);
	RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (!actor) {
		return;
	}

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID); // Find ActorHandle
	if (it != skinData.m_data.end())
	{
		for (std::uint8_t fp = 0; fp <= 1; fp++)
		{
			for (auto overridePair : it->second[gender][fp]) // Loop Armors
			{
				RE::NiPointer<RE::NiNode> lastNode = nullptr;
				RE::NiPointer<RE::NiNode> root{refr->Get3D(fp) ? refr->Get3D(fp)->AsNode() : nullptr};
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					RE::TESForm * pForm = GetSkinForm(actor, overridePair.first);
					RE::TESObjectARMO * armor = pForm ? pForm->As<RE::TESObjectARMO>() : nullptr;
					if (armor) {
						for (std::uint32_t i = 0; i < armor->armorAddons.size(); i++) {
							RE::TESObjectARMA* arma = armor->armorAddons[i];
							if (arma) {
								if (!IsSlotMatch(actor, armor, arma, overridePair.first)) {
									continue;
								}
								VisitArmorAddon(actor, armor, arma, [&](bool isFirstPerson, RE::NiAVObject * rootNode, RE::NiAVObject * parent)
								{
									if ((fp == 0 && isFirstPerson) || (fp == 1 && !isFirstPerson))
									{
										VisitObjects(parent, [&](RE::NiAVObject* object)
										{
											RE::BSGeometry * geometry = object ? object->AsGeometry() : nullptr;
											if (geometry)
											{
												RE::BSShaderProperty * shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
												if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
												{
													RE::BSLightingShaderMaterial * material = (RE::BSLightingShaderMaterial *)shaderProperty->material;
													if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint)
													{
														overridePair.second.Visit([&](OverrideVariant * value)
														{
															SetShaderProperty(object, value, immediate);
															return false;
														});
													}
												}
											}
											return false;
										});
									}
								});
							}
						}
					}
				}
				lastNode = root;
			}
		}
	}
}

void OverrideInterface::VisitNodes(RE::TESObjectREFR * refr, std::function<void(SKEEFixedString, OverrideVariant&)> functor)
{	
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto nit = nodeData.m_data.find(refr->formID); // Find ActorHandle
	if (nit != nodeData.m_data.end())
	{
		for (auto & ovr : nit->second[gender]) // Loop Overrides
		{
			for (auto prop : ovr.second) {
				functor(*ovr.first, prop);
			}
		}
	}
}

void OverrideInterface::VisitSkin(RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::function<void(std::uint32_t, OverrideVariant&)> functor)
{
	std::uint8_t fp = firstPerson ? 1 : 0;
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		for (auto & ovr : it->second[gender][fp])
		{
			for (auto prop : ovr.second) {
				functor(ovr.first, prop);
			}
		}
	}
}

class NodeOverrideApplicator : public GeometryVisitor
{
public:
	NodeOverrideApplicator(OverrideRegistration<StringTableItem> * overrides, bool immediate) : m_overrides(overrides), m_immediate(immediate) {}

	virtual bool Accept(RE::BSGeometry * geometry)
	{
		SKEEFixedString nodeName(geometry->name);
		auto nit = m_overrides->find(g_stringTable.GetString(nodeName));
		if(nit != m_overrides->end())
		{
			nit->second.Visit([&](OverrideVariant * value)
			{
				SetShaderProperty(geometry, value, m_immediate);
				return false;
			});
		}
		return false;
	}

	OverrideRegistration<StringTableItem>	* m_overrides;
	bool									m_immediate;
};

class OverrideApplicator : public GeometryVisitor
{
public:
	OverrideApplicator(OverrideRegistration<StringTableItem> * overrides, bool immediate) : m_overrides(overrides), m_immediate(immediate) {}

	virtual bool Accept(RE::BSGeometry * geometry)
	{
		m_geometryList.push_back(geometry);
		return false;
	}

	void Apply()
	{
		for(auto & geometry : m_geometryList)
		{
			SKEEFixedString objectName(m_geometryList.size() == 1 ? "" : geometry->name);
			auto nit = m_overrides->find(g_stringTable.GetString(objectName));
			if(nit != m_overrides->end())
			{
				nit->second.Visit([&](OverrideVariant* value)
				{
					SetShaderProperty(geometry, value, m_immediate);
					return false;
				});
			}
		}
	}

	std::vector<RE::BSGeometry*>				m_geometryList;
	OverrideRegistration<StringTableItem>	* m_overrides;
	bool									m_immediate;
};

class SkinOverrideApplicator : public GeometryVisitor
{
public:
	SkinOverrideApplicator(RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, std::uint32_t slotMask, OverrideSet * overrides, bool immediate) : m_armor(armor), m_addon(addon), m_overrides(overrides), m_slotMask(slotMask), m_immediate(immediate) {}

	virtual bool Accept(RE::BSGeometry * geometry)
	{
		std::uint32_t armorMask = m_armor->GetSlotMask().underlying();
		std::uint32_t addonMask = m_addon->GetSlotMask().underlying();

		if ((armorMask & m_slotMask) == m_slotMask && (addonMask & m_slotMask) == m_slotMask)
		{
			m_geometryList.push_back(geometry);
		}

		return false;
	}

	void Apply()
	{
		for (auto & geometry : m_geometryList)
		{
			RE::BSShaderProperty * shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
			if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
			{
				RE::BSLightingShaderMaterial * material = (RE::BSLightingShaderMaterial *)shaderProperty->material;
				if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint)
				{
					if (m_overrides) {
						m_overrides->Visit([&](OverrideVariant* value)
						{
							SetShaderProperty(geometry, value, m_immediate);
							return false;
						});
					}
				}
			}
		}
	}

	std::vector<RE::BSGeometry*>	m_geometryList;
	RE::TESObjectARMO * m_armor;
	RE::TESObjectARMA * m_addon;
	OverrideSet	* m_overrides;
	std::uint32_t	m_slotMask;
	bool	m_immediate;
};

void OverrideInterface::Impl_ApplyNodeOverrides(RE::TESObjectREFR * refr, RE::NiAVObject * object, bool immediate)
{
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(nodeData.m_lock);
	auto nit = nodeData.m_data.find(refr->formID);
	if(nit != nodeData.m_data.end()) {
		NodeOverrideApplicator applicator(&nit->second[gender], immediate);
		VisitGeometry(object, &applicator);
	}
}

void OverrideInterface::Impl_ApplyOverrides(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool immediate)
{
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID); // Find ActorHandle
	if(it != armorData.m_data.end())
	{
		auto ait = it->second[gender].find(armor->formID); // Find ArmorHandle
		if(ait != it->second[gender].end())
		{
			auto dit = ait->second.find(addon->formID); // Find AddonHandle
			if(dit != ait->second.end())
			{
				OverrideApplicator applicator(&dit->second, immediate);
				VisitGeometry(object, &applicator);
				applicator.Apply();
			}
		}
	}
}

void OverrideInterface::Impl_ApplyWeaponOverrides(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectWEAP * weapon, RE::NiAVObject * object, bool immediate)
{
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if(actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID); // Find ActorHandle
	if(it != weaponData.m_data.end())
	{
		auto ait = it->second[gender][firstPerson ? 1 : 0].find(weapon->formID); // Find WeaponHandle
		if(ait != it->second[gender][firstPerson ? 1 : 0].end())
		{
			OverrideApplicator applicator(&ait->second, immediate);
			VisitGeometry(object, &applicator);
			applicator.Apply();
		}
	}
}

void OverrideInterface::Impl_ApplySkinOverrides(RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, std::uint32_t slotMask, RE::NiAVObject * object, bool immediate)
{
	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = actorBase->GetSex();

	std::lock_guard<std::recursive_mutex> locker(skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID); // Find ActorHandle
	if (it != skinData.m_data.end())
	{
		auto ait = it->second[gender][firstPerson ? 1 : 0].find(slotMask); // Find WeaponHandle
		if (ait != it->second[gender][firstPerson ? 1 : 0].end())
		{
			SkinOverrideApplicator applicator(armor, addon, slotMask, &ait->second, immediate);
			VisitGeometry(object, &applicator);
			applicator.Apply();
		}
	}
}

void OverrideSet::Visit(std::function<bool(OverrideVariant*)> functor)
{
	for(auto it = begin(); it != end(); ++it) {
		if(functor(const_cast<OverrideVariant*>(&(*it))))
			break;
	}
}

template<typename T>
void OverrideRegistration<T>::Visit(std::function<bool(const T & key, OverrideSet * set)> functor)
{
	for(auto it = OverrideRegistration<T>::template begin(); it != OverrideRegistration<T>::template end(); ++it) {
		if(functor(it->first, &it->second))
			break;
	}
}

void OverrideInterface::Revert()
{
	armorData.Lock();
	armorData.m_data.clear();
	armorData.Release();

	nodeData.Lock();
	nodeData.m_data.clear();
	nodeData.Release();

	weaponData.Lock();
	weaponData.m_data.clear();
	weaponData.Release();

	skinData.Lock();
	skinData.m_data.clear();
	skinData.Release();
}

void OverrideInterface::Impl_RemoveAllOverrides()
{
	armorData.Lock();
	armorData.m_data.clear();
	armorData.Release();
}

void OverrideInterface::Impl_RemoveAllNodeOverrides()
{
	nodeData.Lock();
	nodeData.m_data.clear();
	nodeData.Release();
}

void OverrideInterface::Impl_RemoveAllWeaponBasedOverrides()
{
	weaponData.Lock();
	weaponData.m_data.clear();
	weaponData.Release();
}

void OverrideInterface::Impl_RemoveAllSkinBasedOverrides()
{
	skinData.Lock();
	skinData.m_data.clear();
	skinData.Release();
}

// OverrideVariant
void OverrideVariant::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('OVRV', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}
	// Key
	intfc->WriteRecordData(&key, sizeof(key));

	intfc->WriteRecordData(&type, sizeof(type));

	if(IsIndexValid(key))
		intfc->WriteRecordData(&index, sizeof(index));

	switch(type) {
		case kType_Int:
			intfc->WriteRecordData(&data.u, sizeof(data.u));
			break;
		case kType_Float:
			intfc->WriteRecordData(&data.f, sizeof(data.f));
			break;
		case kType_Bool:
			intfc->WriteRecordData(&data.b, sizeof(data.b));
			break;
		case kType_String:
			{
				g_stringTable.WriteString(intfc, str);
			}
			break;
		case kType_Identifier:
			{
				RE::BSScript::IObjectHandlePolicy* policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
				std::uint64_t handle = policy->GetHandleForObject(RE::FormType::TextureSet, static_cast<const RE::TESForm*>(data.p));
				intfc->WriteRecordData(&handle, sizeof(handle));
			}
			break;
	}

#ifdef _DEBUG
	SKSE::log::info("Saving {} {} {:X} {}", key, type, data.u, data.f);
#endif
}

bool OverrideVariant::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	SetNone();

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'OVRV':
			{
				std::uint16_t keyValue;
				// Key
				if (!intfc->ReadRecordData(&keyValue, sizeof(keyValue)))
				{
					SKSE::log::error("{} - Error loading override value key", __FUNCTION__);
					error = true;
					return error;
				}

				this->key = keyValue;

				if (!intfc->ReadRecordData(&this->type, sizeof(this->type)))
				{
					SKSE::log::error("{} - Error loading override value type", __FUNCTION__);
					error = true;
					return error;
				}

				if(IsIndexValid(this->key))
				{
					if (!intfc->ReadRecordData(&this->index, sizeof(this->index)))
					{
						SKSE::log::error("{} - Error loading override value index", __FUNCTION__);
						error = true;
						return error;
					}
				}

				switch(this->type)
				{
					case kType_Int:
						{
							if (!intfc->ReadRecordData(&data.u, sizeof(data.u))) {
								SKSE::log::error("{} - Error loading override value data", __FUNCTION__);
								error = true;
								return error;
							}
						}
						break;
					case kType_Float:
						{
							if (!intfc->ReadRecordData(&data.f, sizeof(data.f))) {
								SKSE::log::error("{} - Error loading override value data", __FUNCTION__);
								error = true;
								return error;
							}
						}
						break;
					case kType_Bool:
						{
							if (!intfc->ReadRecordData(&data.b, sizeof(data.b))) {
								SKSE::log::error("{} - Error loading override value data", __FUNCTION__);
								error = true;
								return error;
							}
						}
						break;
					case kType_String:
						{
							if (kVersion >= OverrideInterface::kSerializationVersion3)
							{
								this->str = g_stringTable.ReadString(intfc, stringTable);
							}
							else if (kVersion >= OverrideInterface::kSerializationVersion1)
							{
								SKEEFixedString str;
								Serialization::ReadData(intfc, &str);
								this->str = g_stringTable.GetString(str);
							}
						}
						break;
					case kType_Identifier:
						{
							std::uint64_t handle;
							if (!intfc->ReadRecordData(&handle, sizeof(handle)))
							{
								SKSE::log::error("{} - Error loading override value key handle", __FUNCTION__);
								error = true;
								return error;
							}

							std::uint64_t newHandle = 0;
							if (intfc->ResolveHandle(handle, newHandle))
							{
								RE::BSScript::IObjectHandlePolicy* policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();

								if(policy->HandleIsType(RE::FormType::TextureSet, newHandle))
									data.p = (void*)policy->GetObjectForHandle(RE::FormType::TextureSet, newHandle);
								else
									SetNone();
							}
							else
								SetNone();
						}
						break;
				}

				break;
			}
		default:
			{
				SKSE::log::error("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

void OverrideInterface::VisitStrings(std::function<void(SKEEFixedString)> functor)
{
	for (auto & i1 : armorData.m_data){
		for (std::uint8_t gender = 0; gender <= 1; gender++) {
			for (std::uint8_t fp = 0; fp <= 1; fp++) {
				for (auto & i2 : i1.second[gender][fp]) {
					for (auto & i3 : i2.second) {
						functor(*i3.first);
						for (auto & i4 : i3.second){
							if (i4.type == OverrideVariant::kType_String) {
								functor(*i4.str);
							}
						}
					}
				}
			}
		}
	}

	for (auto & i1 : weaponData.m_data) {
		for (std::uint8_t gender = 0; gender <= 1; gender++) {
			for (std::uint8_t fp = 0; fp <= 1; fp++) {
				for (auto & i2 : i1.second[gender][fp]) {
					for (auto & i3 : i2.second) {
						functor(*i3.first);
						for (auto & i4 : i3.second) {
							if (i4.type == OverrideVariant::kType_String) {
								functor(*i4.str);
							}
						}
					}
				}
			}
		}
	}

	for (auto & i1 : nodeData.m_data) {
		for (std::uint8_t fp = 0; fp <= 1; fp++) {
			for (auto & i2 : i1.second[fp]) {
				functor(*i2.first);
				for (auto & i3 : i2.second) {
					if (i3.type == OverrideVariant::kType_String) {
						functor(*i3.str);
					}
				}
			}
		}
	}

	for (auto & i1 : skinData.m_data) {
		for (std::uint8_t gender = 0; gender <= 1; gender++) {
			for (std::uint8_t fp = 0; fp <= 1; fp++) {
				for (auto & i2 : i1.second[gender][fp]) {
					for (auto & i3 : i2.second) {
						if (i3.type == OverrideVariant::kType_String) {
							functor(*i3.str);
						}
					}
				}
			}
		}
	}
}

// ValueSet
void OverrideSet::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('OVST', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}

	std::uint32_t numOverrides = this->size();
	intfc->WriteRecordData(&numOverrides, sizeof(numOverrides));

#ifdef _DEBUG
	SKSE::log::info("Saving {} values", numOverrides);
#endif

	for(auto it = this->begin(); it != this->end(); ++it)
		const_cast<OverrideVariant&>((*it)).Save(intfc, kVersion);
}

bool OverrideSet::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'OVST':
			{
				// Override Count
				std::uint32_t numOverrides = 0;
				if (!intfc->ReadRecordData(&numOverrides, sizeof(numOverrides)))
				{
					SKSE::log::info("{} - Error loading override count", __FUNCTION__);
					error = true;
					return error;
				}

				for (std::uint32_t i = 0; i < numOverrides; i++)
				{
					OverrideVariant value;
					if (!value.Load(intfc, version, stringTable))
					{
						if(value.type == OverrideVariant::kType_None)
							continue;

#ifdef _DEBUG
						if (value.type != OverrideVariant::kType_String)
							SKSE::log::info("Loaded override value {} {:X}", value.key, value.data.u);
						else
							SKSE::log::info("Loaded override value {} {}", value.key, value.str->c_str());
#endif

						this->insert(value);
					}
					else
					{
						SKSE::log::info("{} - Error loading override value", __FUNCTION__);
						error = true;
						return error;
					}
				}

				break;
			}
		default:
			{
				SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// OverrideRegistration
template<>
bool ReadKey(SKSE::SerializationInterface * intfc, StringTableItem & key, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	key = StringTable::ReadString(intfc, stringTable);
	return false;
}

template<>
bool ReadKey(SKSE::SerializationInterface * intfc, std::uint32_t & key, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	if(!intfc->ReadRecordData(&key, sizeof(key))) {
		return true;
	}

	return false;
}

template<>
void WriteKey(SKSE::SerializationInterface * intfc, const StringTableItem key, std::uint32_t kVersion)
{
	g_stringTable.WriteString(intfc, key);

#ifdef _DEBUG
	SKSE::log::info("Saving Key {}", key->c_str());
#endif
}

template<>
void WriteKey(SKSE::SerializationInterface * intfc, const std::uint32_t key, std::uint32_t kVersion)
{
	intfc->WriteRecordData(&key, sizeof(key));
#ifdef _DEBUG
	SKSE::log::info("Saving Key {}", key);
#endif
}

template<typename T>
void OverrideRegistration<T>::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numNodes = this->size();
	intfc->WriteRecordData(&numNodes, sizeof(numNodes));

	for(auto it = this->begin(); it != this->end(); ++it)
	{
		if (!intfc->OpenRecord('NOEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		WriteKey<T>(intfc, it->first, kVersion);

		// Value
		it->second.Save(intfc, kVersion);
	}
}

template<typename T>
bool OverrideRegistration<T>::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	// Handle count
	std::uint32_t numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		SKSE::log::info("{} - Error loading override registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(std::uint32_t i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(type, version, length))
		{
			switch (type)
			{
			case 'NOEN':
				{
					T key;
					if(ReadKey<T>(intfc, key, kVersion, stringTable)) {
						SKSE::log::info("{} - Error loading node entry key", __FUNCTION__);
						error = true;
						return error;
					}

					// operator[] not working for some odd reason
					bool loadError = false;
					auto iter = this->find(key); // Find existing first
					if(iter != this->end()) {
						error = iter->second.Load(intfc, version, stringTable);
					} else { // No existing, create
						OverrideSet set;
						error = set.Load(intfc, version, stringTable);
						this->emplace(key, set);
					}
					if(loadError)
					{
						SKSE::log::info("{} - Error loading node overrides", __FUNCTION__);
						error = true;
						return error;
					}
					break;
				}
			default:
				{
					SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

// AddonRegistration
void AddonRegistration::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		if (!intfc->OpenRecord('AAEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("Saving ArmorAddon Handle %016llX", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool AddonRegistration::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	// Handle count
	std::uint32_t numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		SKSE::log::info("{} - Error loading Addon Registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(std::uint32_t i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(type, version, length))
		{
			switch (type)
			{
			case 'AAEN':
				{
					std::uint64_t handle;
					// Key
					if (!intfc->ReadRecordData(&handle, sizeof(handle)))
					{
						SKSE::log::info("{} - Error loading ArmorAddon key", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideRegistration<StringTableItem> overrideRegistration;
					if (overrideRegistration.Load(intfc, version, stringTable))
					{
						SKSE::log::info("{} - Error loading ArmorAddon override registrations", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideHandle formId = handle & 0xFFFFFFFF;
					OverrideHandle newFormId = 0;

					// Skip if handle is no longer valid.
					if (!ResolveAnyForm(intfc, formId, &newFormId))
						return false;

					if(overrideRegistration.empty())
						return false;

					emplace(newFormId, overrideRegistration);
	#ifdef _DEBUG
					SKSE::log::info("Loaded ArmorAddon {:08X}", newFormId);
	#endif
					break;
				}
			default:
				{
					SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

// ArmorRegistration
void ArmorRegistration::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		if (!intfc->OpenRecord('AREN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("Saving Armor {:08X}", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool ArmorRegistration::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'AREN':
			{
				std::uint64_t handle;
				// Key
				if (!intfc->ReadRecordData(&handle, sizeof(handle)))
				{
					SKSE::log::info("{} - Error loading Armor key", __FUNCTION__);
					error = true;
					return error;
				}

				AddonRegistration addonRegistration;
				if (addonRegistration.Load(intfc, version, stringTable))
				{
					SKSE::log::info("{} - Error loading ArmorAddon registrations", __FUNCTION__);
					error = true;
					return error;
				}

				std::uint32_t formId = handle & 0xFFFFFFFF;
				std::uint32_t newFormId = 0;

				// Skip if handle is no longer valid.
				if (!ResolveAnyForm(intfc, formId, &newFormId))
					return false;

				if(addonRegistration.empty())
					return false;

				emplace(newFormId, addonRegistration);
#ifdef _DEBUG
				SKSE::log::info("Loaded Armor {:08X}", newFormId);
#endif

				break;
			}
		default:
			{
				SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// WeaponRegistration
void WeaponRegistration::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		if (!intfc->OpenRecord('WAEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("Saving Weapon Handle {:08X}", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool WeaponRegistration::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	// Handle count
	std::uint32_t numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		SKSE::log::info("{} - Error loading Weapon registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(std::uint32_t i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(type, version, length))
		{
			switch (type)
			{
			case 'WAEN':
				{
					std::uint64_t handle;
					// Key
					if (!intfc->ReadRecordData(&handle, sizeof(handle)))
					{
						SKSE::log::info("{} - Error loading Weapon key", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideRegistration<StringTableItem> overrideRegistration;
					if (overrideRegistration.Load(intfc, version, stringTable))
					{
						SKSE::log::info("{} - Error loading Weapon override registrations", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideHandle formId = handle & 0xFFFFFFFF;
					OverrideHandle newFormId = 0;

					// Skip if handle is no longer valid.
					if (!ResolveAnyForm(intfc, formId, &newFormId))
						return false;

					if(overrideRegistration.empty())
						return false;

					emplace(newFormId, overrideRegistration);
	#ifdef _DEBUG
					SKSE::log::info("{} - Loaded Weapon {:08X}", __FUNCTION__, newFormId);
	#endif
					break;
				}
			default:
				{
					SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

// WeaponRegistration
void SkinRegistration::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for (auto it = this->begin(); it != this->end(); ++it) {
		if (!intfc->OpenRecord('SKND', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("Saving Skin Handle {:08X}", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool SkinRegistration::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	// Handle count
	std::uint32_t numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		SKSE::log::info("{} - Error loading skin registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for (std::uint32_t i = 0; i < numRegs; i++)
	{
		if (intfc->GetNextRecordInfo(type, version, length))
		{
			switch (type)
			{
				case 'SKND':
				{
					std::uint32_t slotMask;
					// Key
					if (!intfc->ReadRecordData(&slotMask, sizeof(slotMask)))
					{
						SKSE::log::info("{} - Error loading skin slotMask", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideSet overrideSet;
					if (overrideSet.Load(intfc, version, stringTable))
					{
						SKSE::log::info("{} - Error loading skin override set", __FUNCTION__);
						error = true;
						return error;
					}

					if (overrideSet.empty())
						return false;

					insert_or_assign(slotMask, overrideSet);
#ifdef _DEBUG
					SKSE::log::info("{} - Loaded Skin SlotMask {:08X}", __FUNCTION__, slotMask);
#endif
					break;
				}
				default:
				{
					SKSE::log::info("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

bool NodeRegistrationMapHolder::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{	
	std::uint64_t handle = 0;
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::info("{} - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<OverrideRegistration<StringTableItem>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		SKSE::log::info("{} - Error loading override gender registrations", __FUNCTION__);
		return true;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return true;
	}

	if(reg.empty()) {
		*outHandle = 0;
		return true;
	}

	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form) {
		SKSE::log::warn("{} - Discarding node overrides for (%08llX) form is invalid", __FUNCTION__, newFormId);
		*outHandle = 0;
		return true;
	}
	else if (form->IsNot(RE::FormType::Reference) && form->IsNot(RE::FormType::ActorCharacter)) {
		SKSE::log::warn("{} - Discarding node overrides for ({:08X}) form is not a reference ({})", __FUNCTION__, newFormId, form->formType.underlying());
		*outHandle = 0;
		return true;
	}
	else if (form->IsDeleted()) {
		SKSE::log::warn("{} - Discarding node overrides for (%08llX) form is deleted", __FUNCTION__, newFormId);
		*outHandle = 0;
		return true;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();
#ifdef _DEBUG
	SKSE::log::debug("{} - Loaded overrides for handle (%08llX) reference ({})", __FUNCTION__, newFormId, static_cast<RE::TESObjectREFR*>(form)->GetName());
#endif
	return false;
}

void NodeRegistrationMapHolder::Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		if (!intfc->OpenRecord('NDEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("{} - Saving Handle {:08X}", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

void ActorRegistrationMapHolder::Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		if (!intfc->OpenRecord('ACEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("{} - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool ActorRegistrationMapHolder::Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{
	std::uint64_t handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::info("{} - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<ArmorRegistration,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		SKSE::log::info("{} - Error loading armor gender registrations", __FUNCTION__);
		return true;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return true;
	}

	// Invalid handle
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || (form->IsNot(RE::FormType::Reference) && form->IsNot(RE::FormType::ActorCharacter)))
	{
		*outHandle = 0;
		return true;
	}
	
	if(reg.empty()) {
		*outHandle = 0;
		return true;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	SKSE::log::info("{} - Loaded {:08X}", __FUNCTION__, newFormId);
#endif
	return false;
}

void WeaponRegistrationMapHolder::Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		if (!intfc->OpenRecord('WPEN', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("{} - Saving Handle {:08X}", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool WeaponRegistrationMapHolder::Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{
	std::uint64_t handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::info("{} - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<MultiRegistration<WeaponRegistration,2>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		SKSE::log::info("{} - Error loading weapon registrations", __FUNCTION__);
		return true;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return true;
	}

	// Invalid handle
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || (form->IsNot(RE::FormType::Reference) && form->IsNot(RE::FormType::ActorCharacter))) {
		*outHandle = 0;
		return true;
	}

	if(reg.empty()) {
		*outHandle = 0;
		return true;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	SKSE::log::info("{} - Loaded {:08X}", __FUNCTION__, newFormId);
#endif
	return false;
}

void SkinRegistrationMapHolder::Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	for (auto it = m_data.begin(); it != m_data.end(); ++it) {
		if (!intfc->OpenRecord('SKNR', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("{} - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool SkinRegistrationMapHolder::Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, OverrideHandle* outHandle, const StringIdMap & stringTable)
{
	std::uint64_t handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::info("{} - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<MultiRegistration<SkinRegistration, 2>, 2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		SKSE::log::info("{} - Error loading skin registrations", __FUNCTION__);
		return true;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return true;
	}

	// Invalid handle
	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		*outHandle = 0;
		return true;
	}
	if (reg.empty()) {
		*outHandle = 0;
		return true;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	SKSE::log::info("{} - Loaded {:08X}", __FUNCTION__, newFormId);
#endif
	return false;
}

// ActorRegistration
void OverrideInterface::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	armorData.Save(intfc, kVersion);
	nodeData.Save(intfc, kVersion);
	weaponData.Save(intfc, kVersion);
	skinData.Save(intfc, kVersion);
}

bool OverrideInterface::LoadWeaponOverrides(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	SKSE::log::info("{} - Loading Weapon Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!weaponData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddWeaponOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadOverrides(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	SKSE::log::info("{} - Loading Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!armorData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddAddonOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadNodeOverrides(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	SKSE::log::info("{} - Loading Node Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!nodeData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddNodeOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadSkinOverrides(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	SKSE::log::info("{} - Loading Skin Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if (!skinData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddSkinOverrideUpdate(handle);
	}

	return false;
}

void OverrideInterface::PrintDiagnostics()
{
	Console_Print("OverrideInterface Diagnostics:");
	armorData.Lock();
	Console_Print("\t%llu actors with armor overrides", armorData.m_data.size());
	armorData.Release();
	nodeData.Lock();
	Console_Print("\t%llu actors with node overrides", nodeData.m_data.size());
	nodeData.Release();
	skinData.Lock();
	Console_Print("\t%llu actors with skin overrides", skinData.m_data.size());
	skinData.Release();
	weaponData.Lock();
	Console_Print("\t%llu actors with weapon overrides", weaponData.m_data.size());
	weaponData.Release();
}

void OverrideInterface::Dump()
{
	SKSE::log::info("Dumping Overrides");
	armorData.Lock();
	SKSE::log::info("Dumping ({}) actor overrides", armorData.m_data.size());
	for(auto it : armorData.m_data)
	{
		for(std::uint8_t gender = 0; gender < 2; gender++)
		{
			SKSE::log::info("RE::Actor Handle: (%016llX) children ({}) Gender ({})", it.first, it.second[gender].size(), gender);
			for(auto ait : it.second[gender]) // Loop Armors
			{
				SKSE::log::info("\tArmor Handle: (%016llX) children ({})", ait.first, ait.second.size());
				for(auto dit : ait.second) // Loop Addons
				{
					SKSE::log::info("\t\tAddon Handle: (%016llX) children ({})", dit.first, dit.second.size());
					for(auto nit : dit.second) // Loop Overrides
					{
						SKSE::log::info("\t\t\tOverride Node: ({}) children ({})", nit.first->c_str(), nit.second.size());
						for(auto ovr: nit.second)
						{
							switch(ovr.type)
							{
							case OverrideVariant::kType_String:
								SKSE::log::info("\t\t\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.str->c_str());
								break;
							case OverrideVariant::kType_Float:
								SKSE::log::info("\t\t\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.data.f);
								break;
							default:
								SKSE::log::info("\t\t\t\tOverride: Key ({}) Value ({:X})", ovr.key, ovr.data.u);
								break;
							}
						}
					}
				}
			}
		}
	}
	armorData.Release();

	nodeData.Lock();
	SKSE::log::info("Dumping ({}) node overrides", nodeData.m_data.size());
	for(auto nit : nodeData.m_data)
	{
		for(std::uint8_t gender = 0; gender < 2; gender++)
		{
			SKSE::log::info("Node Handle: (%016llX) children ({}) Gender ({})", nit.first, nit.second[gender].size(), gender);
			for(auto oit : nit.second[gender]) // Loop Overrides
			{
				SKSE::log::info("\tOverride Node: ({}) children ({})", oit.first->c_str(), oit.second.size());
				for(auto ovr : oit.second)
				{
					switch(ovr.type)
					{
					case OverrideVariant::kType_String:
						SKSE::log::info("\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.str->c_str());
						break;
					case OverrideVariant::kType_Float:
						SKSE::log::info("\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.data.f);
						break;
					default:
						SKSE::log::info("\t\tOverride: Key ({}) Value ({:X})", ovr.key, ovr.data.u);
						break;
					}
				}
			}
		}
	}
	nodeData.Release();

	skinData.Lock();
	SKSE::log::info("Dumping ({}) skin overrides", skinData.m_data.size());
	for (auto nit : skinData.m_data)
	{
		for (std::uint8_t gender = 0; gender <= 1; gender++)
		{
			for (std::uint8_t perspective = 0; perspective <= 1; perspective++)
			{
				SKSE::log::info("Skin Handle: (%016llX) Gender ({}) Perspective ({})", nit.first, gender, perspective);
				for (auto oit : nit.second[gender][perspective]) // Loop Overrides
				{
					SKSE::log::info("\tSkin Override: Slot ({:08X}) children ({})", oit.first, oit.second.size());
					for (auto ovr : oit.second)
					{
						switch (ovr.type)
						{
						case OverrideVariant::kType_String:
							SKSE::log::info("\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.str->c_str());
							break;
						case OverrideVariant::kType_Float:
							SKSE::log::info("\t\tOverride: Key ({}) Value ({})", ovr.key, ovr.data.f);
							break;
						default:
							SKSE::log::info("\t\tOverride: Key ({}) Value ({:X})", ovr.key, ovr.data.u);
							break;
						}
					}
				}
			}
		}
	}
	skinData.Release();
}

extern bool	g_immediateArmor;

void OverrideInterface::OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root)
{
	Impl_ApplyOverrides(refr, armor, addon, object, g_immediateArmor);

	std::uint32_t armorMask = armor->GetSlotMask().underlying();
	std::uint32_t addonMask = addon->GetSlotMask().underlying();
	Impl_ApplySkinOverrides(refr, isFirstPerson, armor, addon, armorMask & addonMask, object, g_immediateArmor);
}

bool OverrideInterface::HasArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr)
		return false;

	if (!OverrideVariant::IsIndexValid(key))
		index = OverrideVariant::kIndexMax;

	return Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index) != nullptr;
}

bool OverrideInterface::HasArmorAddonNode(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, bool debug)
{
	return Impl_HasArmorAddonNode(refr, firstPerson, armor, addon, nodeName, debug);
}

void OverrideInterface::SetValueVariant(OverrideVariant& variant, skee_u16 key, skee_u8 index, SetVariant& value)
{
	if (!OverrideVariant::IsIndexValid(key))
		index = OverrideVariant::kIndexMax;

	switch (value.GetType())
	{
	case SetVariant::Type::Int:
	{
		std::int32_t i = value.Int();
		PackValue<std::int32_t>(&variant, key, index, &i);
		break;
	}
	case SetVariant::Type::Float:
	{
		float f = value.Float();
		PackValue<float>(&variant, key, index, &f);
		break;
	}
	case SetVariant::Type::Bool:
	{
		bool b = value.Bool();
		PackValue<bool>(&variant, key, index, &b);
		break;
	}
	case SetVariant::Type::TextureSet:
	{
		RE::BGSTextureSet* ts = value.TextureSet();
		PackValue<RE::BGSTextureSet*>(&variant, key, index, &ts);
		break;
	}
	case SetVariant::Type::String:
	{
		SKEEFixedString str(value.String());
		PackValue<SKEEFixedString>(&variant, key, index, &str);
		break;
	}
	}
}

bool OverrideInterface::GetValueVariant(OverrideVariant& variant, skee_u16 key, skee_u8 index, GetVariant& value)
{
	switch (variant.type)
	{
	case OverrideVariant::kType_Identifier:
	{
		RE::BGSTextureSet* textureSet = nullptr;
		UnpackValue<RE::BGSTextureSet*>(&textureSet, &variant);
		value.TextureSet(textureSet);
		return true;
		break;
	}
	case OverrideVariant::kType_String:
	{
		SKEEFixedString str;
		UnpackValue(&str, &variant);
		value.String(str.c_str());
		return true;
		break;
	}
	case OverrideVariant::kType_Int:
	{
		std::int32_t i = 0;
		UnpackValue(&i, &variant);
		value.Int(i);
		return true;
		break;
	}
	case OverrideVariant::kType_Float:
	{
		float f = 0.0f;
		UnpackValue(&f, &variant);
		value.Float(f);
		return true;
		break;
	}
	case OverrideVariant::kType_Bool:
	{
		bool b = false;
		UnpackValue(&b, &variant);
		value.Bool(b);
		return true;
		break;
	}
	}
	return false;
}

void OverrideInterface::AddArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddOverride(refr, isFemale, armor, addon, nodeName, variant);
}

bool OverrideInterface::GetArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr || !armor || !addon || !nodeName)
		return false;

	OverrideVariant* value = Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetArmorProperties(RE::TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetProperties(refr->formID, immediate);
}

void OverrideInterface::SetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &variant, immediate);
}

bool OverrideInterface::GetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplyArmorOverrides(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool immediate)
{
	if (!refr || !armor || !addon || !object)
		return;

	Impl_ApplyOverrides(refr, armor, addon, object, immediate);
}

void OverrideInterface::RemoveAllArmorOverrides()
{
	Impl_RemoveAllOverrides();
}

void OverrideInterface::RemoveAllArmorOverridesByReference(RE::TESObjectREFR* refr)
{
	if (!refr)
		return;

	Impl_RemoveAllReferenceOverrides(refr);
}

void OverrideInterface::RemoveAllArmorOverridesByArmor(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor)
{
	if (!refr || !armor)
		return;

	Impl_RemoveAllArmorOverrides(refr, isFemale, armor);
}

void OverrideInterface::RemoveAllArmorOverridesByAddon(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon)
{
	if (!refr || !armor || !addon)
		return;

	Impl_RemoveAllArmorAddonOverrides(refr, isFemale, armor, addon);
}

void OverrideInterface::RemoveAllArmorOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName)
{
	if (!refr || !armor || !addon || !nodeName)
		return;

	Impl_RemoveAllArmorAddonNodeOverrides(refr, isFemale, armor, addon, nodeName);
}

void OverrideInterface::RemoveArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr || !armor || !addon || !nodeName)
		return;

	Impl_RemoveArmorAddonOverride(refr, isFemale, armor, addon, nodeName, key, index);
}

bool OverrideInterface::HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr || !nodeName)
		return false;

	return Impl_GetNodeOverride(refr, isFemale, nodeName, key, index) != nullptr;
}

void OverrideInterface::AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddNodeOverride(refr, isFemale, nodeName, variant);
}

bool OverrideInterface::GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr)
		return false;

	OverrideVariant* value = Impl_GetNodeOverride(refr, isFemale, nodeName, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetNodeProperties(RE::TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetNodeProperties(refr->formID, immediate);
}

void OverrideInterface::SetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetNodeProperty(refr, firstPerson, nodeName, &variant, immediate);
}

bool OverrideInterface::GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetNodeProperty(refr, firstPerson, nodeName, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplyNodeOverrides(RE::TESObjectREFR* refr, RE::NiAVObject* object, bool immediate)
{
	Impl_ApplyNodeOverrides(refr, object, immediate);
}

void OverrideInterface::RemoveAllNodeOverrides()
{
	Impl_RemoveAllNodeOverrides();
}

void OverrideInterface::RemoveAllNodeOverridesByReference(RE::TESObjectREFR* reference)
{
	Impl_RemoveAllReferenceNodeOverrides(reference);
}

void OverrideInterface::RemoveAllNodeOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName)
{
	Impl_RemoveAllNodeNameOverrides(refr, isFemale, nodeName);
}

void OverrideInterface::RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index)
{
	Impl_RemoveNodeOverride(refr, isFemale, nodeName, key, index);
}

bool OverrideInterface::HasSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index)
{
	if (!refr)
		return false;

	return Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index) != nullptr;
}

void OverrideInterface::AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddSkinOverride(refr, isFemale, firstPerson, slotMask, variant);
}

bool OverrideInterface::GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr)
		return false;

	OverrideVariant* value = Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetSkinProperties(RE::TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetSkinProperties(refr->formID, immediate);
}

void OverrideInterface::SetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetSkinProperty(refr, firstPerson, slotMask, &variant, immediate);
}

bool OverrideInterface::GetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetSkinProperty(refr, firstPerson, slotMask, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplySkinOverrides(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, skee_u32 slotMask, RE::NiAVObject* object, bool immediate)
{
	if (!refr || !armor || !addon || !object)
		return;
	
	Impl_ApplySkinOverrides(refr, firstPerson, armor, addon, slotMask, object, immediate);
}

void OverrideInterface::RemoveAllSkinOverridesBySlot(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask)
{
	Impl_RemoveAllSkinOverrides(refr, isFemale, firstPerson, slotMask);
}

void OverrideInterface::RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index)
{
	Impl_RemoveSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
}

void OverrideInterface::RemoveAllSkinOverrides()
{
	Impl_RemoveAllSkinBasedOverrides();
}

void OverrideInterface::RemoveAllSkinOverridesByReference(RE::TESObjectREFR* reference)
{
	Impl_RemoveAllReferenceSkinOverrides(reference);
}
