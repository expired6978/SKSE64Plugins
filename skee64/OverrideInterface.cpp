#include "OverrideInterface.h"
#include "skse64/PluginAPI.h"
#include "skse64/PapyrusVM.h"

#include "ShaderUtilities.h"
#include "OverrideVariant.h"
#include "StringTable.h"
#include "NifUtils.h"
#include "Utilities.h"
#include "ActorUpdateManager.h"

#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameTypes.h"

#include "skse64/NiGeometry.h"

extern ActorUpdateManager	g_actorUpdateManager;
extern OverrideInterface	g_overrideInterface;
extern SKSETaskInterface	* g_task;
extern StringTable			g_stringTable;

skee_u32 OverrideInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverrideInterface::Impl_AddRawOverride(OverrideHandle handle, bool isFemale, OverrideHandle armorHandle, OverrideHandle addonHandle, BSFixedString nodeName, OverrideVariant & value)
{
	armorData.Lock();
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::Impl_AddOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle formId = refr->formID;
	OverrideHandle armorFormId = armor->formID;
	OverrideHandle addonFormId = addon->formID;
	armorData.Lock();
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::Impl_AddRawNodeOverride(OverrideHandle handle, bool isFemale, BSFixedString nodeName, OverrideVariant & value)
{
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::Impl_AddNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::Impl_AddRawWeaponOverride(OverrideHandle handle, bool isFemale, bool firstPerson, OverrideHandle weaponHandle, BSFixedString nodeName, OverrideVariant & value)
{
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::Impl_AddWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	OverrideHandle weaponHandle = weapon->formID;
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::Impl_AddRawSkinOverride(OverrideHandle handle, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value)
{
	skinData.Lock();
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].erase(value);
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].insert(value);
	skinData.Release();
}

void OverrideInterface::Impl_AddSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value)
{
	Impl_AddRawSkinOverride(refr->formID, isFemale, firstPerson, slotMask, value);
}

OverrideVariant * OverrideInterface::Impl_GetOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
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

OverrideVariant * OverrideInterface::Impl_GetNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
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

OverrideVariant * OverrideInterface::Impl_GetWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&weaponData.m_lock);
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

OverrideVariant * OverrideInterface::Impl_GetSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&skinData.m_lock);
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

void OverrideInterface::Impl_RemoveAllReferenceOverrides(TESObjectREFR * refr)
{
	armorData.Lock();
	armorData.m_data.erase(refr->formID);
	armorData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceNodeOverrides(TESObjectREFR * refr)
{
	nodeData.Lock();
	nodeData.m_data.erase(refr->formID);
	nodeData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceWeaponOverrides(TESObjectREFR * refr)
{
	weaponData.Lock();
	weaponData.m_data.erase(refr->formID);
	weaponData.Release();
}

void OverrideInterface::Impl_RemoveAllReferenceSkinOverrides(TESObjectREFR * refr)
{
	skinData.Lock();
	skinData.m_data.erase(refr->formID);
	skinData.Release();
}

void OverrideInterface::Impl_RemoveAllArmorOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto nit = it->second[gender].find(armor->formID);
		if(nit != it->second[gender].end()) {
			nit->second.clear();
		}
	}
}

void OverrideInterface::Impl_RemoveAllArmorAddonOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
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

void OverrideInterface::Impl_RemoveAllArmorAddonNodeOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
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

void OverrideInterface::Impl_RemoveArmorAddonOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
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

void OverrideInterface::Impl_RemoveAllNodeNameOverrides(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
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

void OverrideInterface::Impl_RemoveNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
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

void OverrideInterface::Impl_RemoveAllWeaponOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&weaponData.m_lock);
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

void OverrideInterface::Impl_RemoveAllWeaponNodeOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&weaponData.m_lock);
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

void OverrideInterface::Impl_RemoveWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&weaponData.m_lock);
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

void OverrideInterface::Impl_RemoveAllSkinOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&skinData.m_lock);
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

void OverrideInterface::Impl_RemoveSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&skinData.m_lock);
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

void OverrideInterface::Impl_SetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				bool isRoot = nodeName == BSFixedString("");
				if (!armorNode->GetAsNiNode() && !isRoot) {
					_WARNING("%s - Warning, override for Armor %08X Addon %08X has no children, use an empty string for the node name to access the root instead.", __FUNCTION__, armor->formID, addon->formID);
				}
				NiAVObject* foundNode = isRoot ? armorNode : armorNode->GetObjectByName(&nodeName.data);
				if (foundNode) {
					SetShaderProperty(foundNode, value, immediate);
				}
			}
		});
	}
}

void OverrideInterface::Impl_GetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				NiAVObject * foundNode = nodeName == BSFixedString("") ? armorNode : armorNode->GetObjectByName(&nodeName.data);
				if (foundNode) {
					GetShaderProperty(foundNode, value);
				}
			}
		});
	}
}

bool OverrideInterface::Impl_HasArmorAddonNode(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, bool debug)
{
	if(!refr) {
		if(debug)
			_DMESSAGE("%s - No reference", __FUNCTION__);
		return false;
	}
	bool found = false;
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
		{
			if (firstPerson == isFP) {
				NiAVObject * foundNode = nodeName == BSFixedString("") ? armorNode : armorNode->GetObjectByName(&nodeName.data);
				if (foundNode) {
					if (debug)
						_DMESSAGE("%s - Success, found node name '%s' for Armor %08X, Addon %08X.", __FUNCTION__, nodeName.data, armor->formID, addon->formID);

					found = true;
				}
				else if (debug)
					_DMESSAGE("%s - Failed to find node name '%s' for Armor %08X, Addon %08X.", __FUNCTION__, nodeName.data, armor->formID, addon->formID);
			}
		});
	}

	return found;
}

void OverrideInterface::Impl_SetWeaponProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	char weaponString[MAX_PATH];

	UInt8 gender = isFemale ? 1 : 0;

	memset(weaponString, 0, MAX_PATH);
	weapon->GetNodeName(weaponString);

	NiPointer<NiNode> root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		BSFixedString weaponName(weaponString); // Find the Armor name from the root
		NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
		if(weaponNode) {
			bool isRoot = nodeName == BSFixedString("");
			if(!weaponNode->GetAsNiNode() && !isRoot) {
				_WARNING("%s - Warning, override for Weapon %08X has no children, use an empty string for the node name to access the root instead.", __FUNCTION__, weapon->formID);
			}
			NiAVObject * foundNode = isRoot ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
			if(foundNode) {
				SetShaderProperty(foundNode, value, immediate);
			}
		}
	}
}

void OverrideInterface::Impl_GetWeaponProperty(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value)
{
	char weaponString[MAX_PATH];

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	memset(weaponString, 0, MAX_PATH);
	weapon->GetNodeName(weaponString);

	NiPointer<NiNode> root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		BSFixedString weaponName(weaponString); // Find the Armor name from the root
		NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
		if(weaponNode) {
			NiAVObject * foundNode = nodeName == BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
			if(foundNode) {
				GetShaderProperty(foundNode, value);
			}
		}
	}
}

bool OverrideInterface::Impl_HasWeaponNode(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, bool debug)
{
	if(!refr) {
		if(debug)
			_DMESSAGE("%s - No reference", __FUNCTION__);
		return false;
	}
	char weaponString[MAX_PATH];

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	memset(weaponString, 0, MAX_PATH);
	weapon->GetNodeName(weaponString);

	NiPointer<NiNode> root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		BSFixedString weaponName(weaponString); // Find the Armor name from the root
		NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
		if(weaponNode) {
			NiAVObject * foundNode = nodeName == BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
			if(foundNode) {
				if(debug)	
					_DMESSAGE("%s - Success, found node name '%s' for Weapon %08X.", __FUNCTION__, nodeName.data, weapon->formID);
				return true;
			} else if(debug)
				_DMESSAGE("%s - Failed to find node name '%s' for Weapon %08X.", __FUNCTION__, nodeName.data, weapon->formID);
		} else if(debug)
			_DMESSAGE("%s - Failed to acquire weapon node '%s' for Weapon %08X.", __FUNCTION__, weaponName.data, weapon->formID);
	} else if(debug)
		_DMESSAGE("%s - Failed to acquire skeleton for Reference %08X", __FUNCTION__, refr->formID);

	return false;
}

void OverrideInterface::Impl_SetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value, bool immediate)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		TESForm * pForm = GetSkinForm(actor, slotMask);
		TESObjectARMO * armor = DYNAMIC_CAST(pForm, TESForm, TESObjectARMO);
		if (armor) {
			for (UInt32 i = 0; i < armor->armorAddons.count; i++) {
				TESObjectARMA * addon = NULL;
				if (armor->armorAddons.GetNthItem(i, addon)) {
					VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
					{
						if (firstPerson == isFP)
						{
							VisitObjects(armorNode, [&](NiAVObject* object)
							{
								BSGeometry * geometry = object->GetAsBSGeometry();
								if (geometry)
								{
									BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
									if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
									{
										BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
										if (material && material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
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

void OverrideInterface::Impl_GetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		TESForm * pForm = GetSkinForm(actor, slotMask);
		TESObjectARMO * armor = DYNAMIC_CAST(pForm, TESForm, TESObjectARMO);
		if (armor) {
			for (UInt32 i = 0; i < armor->armorAddons.count; i++) {
				TESObjectARMA * addon = NULL;
				if (armor->armorAddons.GetNthItem(i, addon)) {

					VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
					{
						if (firstPerson == isFP)
						{
							VisitObjects(armorNode, [&](NiAVObject* object)
							{
								BSGeometry * geometry = object->GetAsBSGeometry();
								if (geometry)
								{
									BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
									if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
									{
										BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
										if (material && material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
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
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
		return;
	}

	Actor* actor = static_cast<Actor*>(form);
	if (!actor) {
		return;
	}

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&armorData.m_lock);
	auto it = armorData.m_data.find(formId); // Find ActorHandle
	if(it != armorData.m_data.end())
	{
		for(ArmorRegistration::iterator ait = it->second[gender].begin(); ait != it->second[gender].end(); ++ait) // Loop Armors
		{
			TESObjectARMO * armor = static_cast<TESObjectARMO *>(LookupFormByID(ait->first));
			if(!armor)
				continue;

			for(AddonRegistration::iterator dit = ait->second.begin(); dit != ait->second.end(); ++dit) // Loop Addons
			{
				TESObjectARMA * addon = static_cast<TESObjectARMA *>(LookupFormByID(dit->first));
				if(!addon)
					continue;

				VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
				{
					dit->second.Visit([&](const StringTableItem & key, OverrideSet * set)
					{
						BSFixedString nodeName(key->c_str());
						NiAVObject * foundNode = nodeName == BSFixedString("") ? armorNode : armorNode->GetObjectByName(&nodeName.data);
						if (foundNode) {
							set->Visit([&](OverrideVariant * value)
							{
								if (!immediate) {
									g_task->AddTask(new NIOVTaskSetShaderProperty(foundNode, *value));
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

void OverrideInterface::Impl_SetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	NiPointer<NiNode> root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		NiAVObject * foundNode = root->GetObjectByName(&nodeName.data);
		if(foundNode) {
			if (!immediate) {
				g_task->AddTask(new NIOVTaskSetShaderProperty(foundNode, *value));
			}
			else {
				SetShaderProperty(foundNode, value, true);
			}
		}
	}
}

void OverrideInterface::Impl_GetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value)
{
	NiPointer<NiNode> root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		NiAVObject * foundNode = root->GetObjectByName(&nodeName.data);
		if(foundNode) {
			GetShaderProperty(foundNode, value);
		}
	}
}

void OverrideInterface::Impl_SetNodeProperties(OverrideHandle formId, bool immediate)
{
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
		return;
	}

	TESObjectREFR* refr = static_cast<TESObjectREFR*>(form);
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&nodeData.m_lock);
	auto nit = nodeData.m_data.find(formId); // Find ActorHandle
	if(nit != nodeData.m_data.end())
	{
		NiNode * lastRoot = NULL;
		for(UInt8 i = 0; i <= 1; i++)
		{
			NiNode * root = refr->GetNiRootNode(i);
			if(root == lastRoot) // First and third are the same, skip
				continue;

			if(root)
			{
				root->IncRef();
				nit->second[gender].Visit([&](const StringTableItem & key, OverrideSet * set)
				{
					BSFixedString nodeName(key->c_str());
					NiAVObject * foundNode = nodeName == BSFixedString("") ? root : root->GetObjectByName(&nodeName.data);
					if (foundNode) {
						set->Visit([&](OverrideVariant * value)
						{
							SetShaderProperty(foundNode, value, immediate);
							return false;
						});
					}

					return false;
				});
				root->DecRef();
			}

			lastRoot = root;
		}
	}
}

void OverrideInterface::Impl_SetWeaponProperties(OverrideHandle formId, bool immediate)
{
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
		return;
	}

	TESObjectREFR* refr = static_cast<TESObjectREFR*>(form);

	char weaponString[MAX_PATH];

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&weaponData.m_lock);
	auto it = weaponData.m_data.find(refr->formID); // Find ActorHandle
	if (it != weaponData.m_data.end())
	{
		for (UInt8 i = 0; i <= 1; i++)
		{
			for (WeaponRegistration::iterator ait = it->second[gender][i].begin(); ait != it->second[gender][i].end(); ++ait) // Loop Armors
			{
				TESObjectWEAP * weapon = static_cast<TESObjectWEAP *>(LookupFormByID(ait->first));
				if (!weapon)
					continue;

				memset(weaponString, 0, MAX_PATH);
				weapon->GetNodeName(weaponString);

				NiPointer<NiNode> lastNode = nullptr;
				BSFixedString weaponName(weaponString);

				NiPointer<NiNode> root = refr->GetNiRootNode(i);
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					// Find the Armor node
					NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
					if (weaponNode) {
						ait->second.Visit([&](const StringTableItem & key, OverrideSet * set)
						{
							BSFixedString nodeName(key->c_str());
							NiAVObject * foundNode = nodeName == BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
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
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
		return;
	}

	TESObjectREFR* refr = static_cast<TESObjectREFR*>(form);
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (!actor) {
		return;
	}

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&skinData.m_lock);
	auto it = skinData.m_data.find(refr->formID); // Find ActorHandle
	if (it != skinData.m_data.end())
	{
		for (UInt8 fp = 0; fp <= 1; fp++)
		{
			for (auto overridePair : it->second[gender][fp]) // Loop Armors
			{
				NiPointer<NiNode> lastNode = nullptr;
				NiPointer<NiNode> root = refr->GetNiRootNode(fp);
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					TESForm * pForm = GetSkinForm(actor, overridePair.first);
					TESObjectARMO * armor = DYNAMIC_CAST(pForm, TESForm, TESObjectARMO);
					if (armor) {
						for (UInt32 i = 0; i < armor->armorAddons.count; i++) {
							TESObjectARMA * arma = NULL;
							if (armor->armorAddons.GetNthItem(i, arma)) {
								if (!IsSlotMatch(arma, overridePair.first)) {
									continue;
								}
								VisitArmorAddon(actor, armor, arma, [&](bool isFirstPerson, NiAVObject * rootNode, NiAVObject * parent)
								{
									if ((fp == 0 && isFirstPerson) || (fp == 1 && !isFirstPerson))
									{
										VisitObjects(parent, [&](NiAVObject* object)
										{
											BSGeometry * geometry = object->GetAsBSGeometry();
											if (geometry)
											{
												BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
												if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
												{
													BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
													if (material && material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
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

void OverrideInterface::VisitNodes(TESObjectREFR * refr, std::function<void(SKEEFixedString, OverrideVariant&)> functor)
{	
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&nodeData.m_lock);
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

void OverrideInterface::VisitSkin(TESObjectREFR * refr, bool isFemale, bool firstPerson, std::function<void(UInt32, OverrideVariant&)> functor)
{
	UInt8 fp = firstPerson ? 1 : 0;
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&skinData.m_lock);
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

	virtual bool Accept(BSGeometry * geometry)
	{
		SKEEFixedString nodeName(geometry->m_name);
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

	virtual bool Accept(BSGeometry * geometry)
	{
		m_geometryList.push_back(geometry);
		return false;
	}

	void Apply()
	{
		for(auto & geometry : m_geometryList)
		{
			SKEEFixedString objectName(m_geometryList.size() == 1 ? "" : geometry->m_name);
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

	std::vector<BSGeometry*>				m_geometryList;
	OverrideRegistration<StringTableItem>	* m_overrides;
	bool									m_immediate;
};


class SkinOverrideApplicator : public GeometryVisitor
{
public:
	SkinOverrideApplicator(TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, OverrideSet * overrides, bool immediate) : m_armor(armor), m_addon(addon), m_overrides(overrides), m_slotMask(slotMask), m_immediate(immediate) {}

	virtual bool Accept(BSGeometry * geometry)
	{
		UInt32 armorMask = m_armor->bipedObject.GetSlotMask();
		UInt32 addonMask = m_addon->biped.GetSlotMask();

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
			BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
			if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
			{
				BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
				if (material && material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
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

	std::vector<BSGeometry*>	m_geometryList;
	TESObjectARMO * m_armor;
	TESObjectARMA * m_addon;
	OverrideSet	* m_overrides;
	UInt32	m_slotMask;
	bool	m_immediate;
};

void OverrideInterface::Impl_ApplyNodeOverrides(TESObjectREFR * refr, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&nodeData.m_lock);
	auto nit = nodeData.m_data.find(refr->formID);
	if(nit != nodeData.m_data.end()) {
		NodeOverrideApplicator applicator(&nit->second[gender], immediate);
		VisitGeometry(object, &applicator);
	}
}

void OverrideInterface::Impl_ApplyOverrides(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&armorData.m_lock);
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

void OverrideInterface::Impl_ApplyWeaponOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&weaponData.m_lock);
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

void OverrideInterface::Impl_ApplySkinOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&skinData.m_lock);
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
void OverrideVariant::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('OVRV', kVersion);
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
				VMClassRegistry		* registry =	(*g_skyrimVM)->GetClassRegistry();
				IObjectHandlePolicy	* policy =		registry->GetHandlePolicy();
				UInt64 handle = policy->Create(BGSTextureSet::kTypeID, data.p);
				intfc->WriteRecordData(&handle, sizeof(handle));
			}
			break;
	}

#ifdef _DEBUG
	_MESSAGE("Saving %d %d %X %f", key, type, data.u, data.f);
#endif
}

bool OverrideVariant::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	SetNone();

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'OVRV':
			{
				UInt16 keyValue;
				// Key
				if (!intfc->ReadRecordData(&keyValue, sizeof(keyValue)))
				{
					_ERROR("%s - Error loading override value key", __FUNCTION__);
					error = true;
					return error;
				}

				this->key = keyValue;

				if (!intfc->ReadRecordData(&this->type, sizeof(this->type)))
				{
					_ERROR("%s - Error loading override value type", __FUNCTION__);
					error = true;
					return error;
				}

				if(IsIndexValid(this->key))
				{
					if (!intfc->ReadRecordData(&this->index, sizeof(this->index)))
					{
						_ERROR("%s - Error loading override value index", __FUNCTION__);
						error = true;
						return error;
					}
				}

				switch(this->type)
				{
					case kType_Int:
						{
							if (!intfc->ReadRecordData(&data.u, sizeof(data.u))) {
								_ERROR("%s - Error loading override value data", __FUNCTION__);
								error = true;
								return error;
							}
						}
						break;
					case kType_Float:
						{
							if (!intfc->ReadRecordData(&data.f, sizeof(data.f))) {
								_ERROR("%s - Error loading override value data", __FUNCTION__);
								error = true;
								return error;
							}
						}
						break;
					case kType_Bool:
						{
							if (!intfc->ReadRecordData(&data.b, sizeof(data.b))) {
								_ERROR("%s - Error loading override value data", __FUNCTION__);
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
							UInt64 handle;
							if (!intfc->ReadRecordData(&handle, sizeof(handle)))
							{
								_ERROR("%s - Error loading override value key handle", __FUNCTION__);
								error = true;
								return error;
							}

							UInt64 newHandle = 0;
							if (intfc->ResolveHandle(handle, &newHandle))
							{
								VMClassRegistry		* registry =	(*g_skyrimVM)->GetClassRegistry();
								IObjectHandlePolicy	* policy =		registry->GetHandlePolicy();

								if(policy->IsType(BGSTextureSet::kTypeID, newHandle))
									data.p = (void*)policy->Resolve(BGSTextureSet::kTypeID, newHandle);
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
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
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
		for (UInt8 gender = 0; gender <= 1; gender++) {
			for (UInt8 fp = 0; fp <= 1; fp++) {
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
		for (UInt8 gender = 0; gender <= 1; gender++) {
			for (UInt8 fp = 0; fp <= 1; fp++) {
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
		for (UInt8 fp = 0; fp <= 1; fp++) {
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
		for (UInt8 gender = 0; gender <= 1; gender++) {
			for (UInt8 fp = 0; fp <= 1; fp++) {
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
void OverrideSet::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('OVST', kVersion);

	UInt32 numOverrides = this->size();
	intfc->WriteRecordData(&numOverrides, sizeof(numOverrides));

#ifdef _DEBUG
	_MESSAGE("Saving %d values", numOverrides);
#endif

	for(auto it = this->begin(); it != this->end(); ++it)
		const_cast<OverrideVariant&>((*it)).Save(intfc, kVersion);
}

bool OverrideSet::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'OVST':
			{
				// Override Count
				UInt32 numOverrides = 0;
				if (!intfc->ReadRecordData(&numOverrides, sizeof(numOverrides)))
				{
					_MESSAGE("%s - Error loading override count", __FUNCTION__);
					error = true;
					return error;
				}

				for (UInt32 i = 0; i < numOverrides; i++)
				{
					OverrideVariant value;
					if (!value.Load(intfc, version, stringTable))
					{
						if(value.type == OverrideVariant::kType_None)
							continue;

#ifdef _DEBUG
						if (value.type != OverrideVariant::kType_String)
							_MESSAGE("Loaded override value %d %X", value.key, value.data.u);
						else
							_MESSAGE("Loaded override value %d %s", value.key, value.str->c_str());
#endif

						this->insert(value);
					}
					else
					{
						_MESSAGE("%s - Error loading override value", __FUNCTION__);
						error = true;
						return error;
					}
				}

				break;
			}
		default:
			{
				_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// OverrideRegistration
template<>
bool ReadKey(SKSESerializationInterface * intfc, StringTableItem & key, UInt32 kVersion, const StringIdMap & stringTable)
{
	key = StringTable::ReadString(intfc, stringTable);
	return false;
}

template<>
bool ReadKey(SKSESerializationInterface * intfc, UInt32 & key, UInt32 kVersion, const StringIdMap & stringTable)
{
	if(!intfc->ReadRecordData(&key, sizeof(key))) {
		return true;
	}

	return false;
}

template<>
void WriteKey(SKSESerializationInterface * intfc, const StringTableItem key, UInt32 kVersion)
{
	g_stringTable.WriteString(intfc, key);

#ifdef _DEBUG
	_MESSAGE("Saving Key %s", key->c_str());
#endif
}

template<>
void WriteKey(SKSESerializationInterface * intfc, const UInt32 key, UInt32 kVersion)
{
	intfc->WriteRecordData(&key, sizeof(key));
#ifdef _DEBUG
	_MESSAGE("Saving Key %d", key);
#endif
}

template<typename T>
void OverrideRegistration<T>::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numNodes = this->size();
	intfc->WriteRecordData(&numNodes, sizeof(numNodes));

	for(auto it = this->begin(); it != this->end(); ++it)
	{
		intfc->OpenRecord('NOEN', kVersion);

		// Key
		WriteKey<T>(intfc, it->first, kVersion);

		// Value
		it->second.Save(intfc, kVersion);
	}
}

template<typename T>
bool OverrideRegistration<T>::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		_MESSAGE("%s - Error loading override registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(UInt32 i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
			case 'NOEN':
				{
					T key;
					if(ReadKey<T>(intfc, key, kVersion, stringTable)) {
						_MESSAGE("%s - Error loading node entry key", __FUNCTION__);
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
						_MESSAGE("%s - Error loading node overrides", __FUNCTION__);
						error = true;
						return error;
					}
					break;
				}
			default:
				{
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}


// AddonRegistration
void AddonRegistration::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		intfc->OpenRecord('AAEN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("Saving ArmorAddon Handle %016llX", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool AddonRegistration::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		_MESSAGE("%s - Error loading Addon Registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(UInt32 i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
			case 'AAEN':
				{
					UInt64 handle;
					// Key
					if (!intfc->ReadRecordData(&handle, sizeof(handle)))
					{
						_MESSAGE("%s - Error loading ArmorAddon key", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideRegistration<StringTableItem> overrideRegistration;
					if (overrideRegistration.Load(intfc, version, stringTable))
					{
						_MESSAGE("%s - Error loading ArmorAddon override registrations", __FUNCTION__);
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
					_MESSAGE("Loaded ArmorAddon %08X", newFormId);
	#endif
					break;
				}
			default:
				{
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

// ArmorRegistration
void ArmorRegistration::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		intfc->OpenRecord('AREN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("Saving Armor %08X", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool ArmorRegistration::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'AREN':
			{
				UInt64 handle;
				// Key
				if (!intfc->ReadRecordData(&handle, sizeof(handle)))
				{
					_MESSAGE("%s - Error loading Armor key", __FUNCTION__);
					error = true;
					return error;
				}

				AddonRegistration addonRegistration;
				if (addonRegistration.Load(intfc, version, stringTable))
				{
					_MESSAGE("%s - Error loading ArmorAddon registrations", __FUNCTION__);
					error = true;
					return error;
				}

				UInt32 formId = handle & 0xFFFFFFFF;
				UInt32 newFormId = 0;

				// Skip if handle is no longer valid.
				if (!ResolveAnyForm(intfc, formId, &newFormId))
					return false;

				if(addonRegistration.empty())
					return false;

				emplace(newFormId, addonRegistration);
#ifdef _DEBUG
				_MESSAGE("Loaded Armor %08X", newFormId);
#endif

				break;
			}
		default:
			{
				_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// WeaponRegistration
void WeaponRegistration::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for(auto it = this->begin(); it != this->end(); ++it) {
		intfc->OpenRecord('WAEN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("Saving Weapon Handle %08X", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool WeaponRegistration::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		_MESSAGE("%s - Error loading Weapon registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for(UInt32 i = 0; i < numRegs; i++)
	{
		if(intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
			case 'WAEN':
				{
					UInt64 handle;
					// Key
					if (!intfc->ReadRecordData(&handle, sizeof(handle)))
					{
						_MESSAGE("%s - Error loading Weapon key", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideRegistration<StringTableItem> overrideRegistration;
					if (overrideRegistration.Load(intfc, version, stringTable))
					{
						_MESSAGE("%s - Error loading Weapon override registrations", __FUNCTION__);
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
					_MESSAGE("%s - Loaded Weapon %08X", __FUNCTION__, newFormId);
	#endif
					break;
				}
			default:
				{
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

// WeaponRegistration
void SkinRegistration::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numRegs = this->size();
	intfc->WriteRecordData(&numRegs, sizeof(numRegs));

	for (auto it = this->begin(); it != this->end(); ++it) {
		intfc->OpenRecord('SKND', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("Saving Skin Handle %08X", handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool SkinRegistration::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		_MESSAGE("%s - Error loading skin registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for (UInt32 i = 0; i < numRegs; i++)
	{
		if (intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
				case 'SKND':
				{
					UInt32 slotMask;
					// Key
					if (!intfc->ReadRecordData(&slotMask, sizeof(slotMask)))
					{
						_MESSAGE("%s - Error loading skin slotMask", __FUNCTION__);
						error = true;
						return error;
					}

					OverrideSet overrideSet;
					if (overrideSet.Load(intfc, version, stringTable))
					{
						_MESSAGE("%s - Error loading skin override set", __FUNCTION__);
						error = true;
						return error;
					}

					if (overrideSet.empty())
						return false;

					insert_or_assign(slotMask, overrideSet);
#ifdef _DEBUG
					_MESSAGE("%s - Loaded Skin SlotMask %08X", __FUNCTION__, slotMask);
#endif
					break;
				}
				default:
				{
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

bool NodeRegistrationMapHolder::Load(SKSESerializationInterface * intfc, UInt32 kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{	
	UInt64 handle = 0;
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<OverrideRegistration<StringTableItem>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading override gender registrations", __FUNCTION__);
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

	TESForm* form = LookupFormByID(formId);
	if (!form) {
		_WARNING("%s - Discarding node overrides for (%08llX) form is invalid", __FUNCTION__, newFormId);
		*outHandle = 0;
		return true;
	}
	else if (form->formType != kFormType_Reference && form->formType != kFormType_Character) {
		_WARNING("%s - Discarding node overrides for (%08llX) form is not a reference (%d)", __FUNCTION__, newFormId, form->formType);
		*outHandle = 0;
		return true;
	}
	else if ((form->flags & TESForm::kFlagIsDeleted) == TESForm::kFlagIsDeleted) {
		_WARNING("%s - Discarding node overrides for (%08llX) form is deleted", __FUNCTION__, newFormId);
		*outHandle = 0;
		return true;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();
#ifdef _DEBUG
	_DMESSAGE("%s - Loaded overrides for handle (%08llX) reference (%s)", __FUNCTION__, newFormId, CALL_MEMBER_FN(static_cast<TESObjectREFR*>(form), GetReferenceName)());
#endif
	return false;
}

void NodeRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	SimpleLocker locker(&m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		intfc->OpenRecord('NDEN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Handle %08X", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

void ActorRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	SimpleLocker locker(&m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		intfc->OpenRecord('ACEN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool ActorRegistrationMapHolder::Load(SKSESerializationInterface* intfc, UInt32 kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{
	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<ArmorRegistration,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading armor gender registrations", __FUNCTION__);
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
	TESForm* form = LookupFormByID(formId);
	if (!form || (form->formType != kFormType_Reference && form->formType != kFormType_Character))
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
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif
	return false;
}


void WeaponRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	SimpleLocker locker(&m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it) {
		intfc->OpenRecord('WPEN', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Handle %08X", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool WeaponRegistrationMapHolder::Load(SKSESerializationInterface* intfc, UInt32 kVersion, OverrideHandle * outHandle, const StringIdMap & stringTable)
{
	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<MultiRegistration<WeaponRegistration,2>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading weapon registrations", __FUNCTION__);
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
	TESForm* form = LookupFormByID(formId);
	if (!form || (form->formType != kFormType_Reference && form->formType != kFormType_Character)) {
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
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif
	return false;
}


void SkinRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	SimpleLocker locker(&m_lock);
	for (auto it = m_data.begin(); it != m_data.end(); ++it) {
		intfc->OpenRecord('SKNR', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool SkinRegistrationMapHolder::Load(SKSESerializationInterface* intfc, UInt32 kVersion, OverrideHandle* outHandle, const StringIdMap & stringTable)
{
	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		return true;
	}

	MultiRegistration<MultiRegistration<SkinRegistration, 2>, 2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading skin registrations", __FUNCTION__);
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
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
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
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif
	return false;
}

// ActorRegistration
void OverrideInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	armorData.Save(intfc, kVersion);
	nodeData.Save(intfc, kVersion);
	weaponData.Save(intfc, kVersion);
	skinData.Save(intfc, kVersion);
}

bool OverrideInterface::LoadWeaponOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	_MESSAGE("%s - Loading Weapon Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!weaponData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddWeaponOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	_MESSAGE("%s - Loading Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!armorData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddAddonOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadNodeOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	_MESSAGE("%s - Loading Node Overrides...", __FUNCTION__);
#endif
	OverrideHandle handle = 0;
	if(!nodeData.Load(intfc, kVersion, &handle, stringTable))
	{
		g_actorUpdateManager.AddNodeOverrideUpdate(handle);
	}

	return false;
}

bool OverrideInterface::LoadSkinOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
#ifdef _DEBUG
	_MESSAGE("%s - Loading Skin Overrides...", __FUNCTION__);
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
	_MESSAGE("Dumping Overrides");
	armorData.Lock();
	_MESSAGE("Dumping (%lld) actor overrides", armorData.m_data.size());
	for(auto it : armorData.m_data)
	{
		for(UInt8 gender = 0; gender < 2; gender++)
		{
			_MESSAGE("Actor Handle: (%016llX) children (%d) Gender (%d)", it.first, it.second[gender].size(), gender);
			for(auto ait : it.second[gender]) // Loop Armors
			{
				_MESSAGE("\tArmor Handle: (%016llX) children (%lld)", ait.first, ait.second.size());
				for(auto dit : ait.second) // Loop Addons
				{
					_MESSAGE("\t\tAddon Handle: (%016llX) children (%lld)", dit.first, dit.second.size());
					for(auto nit : dit.second) // Loop Overrides
					{
						_MESSAGE("\t\t\tOverride Node: (%s) children (%lld)", nit.first->c_str(), nit.second.size());
						for(auto ovr: nit.second)
						{
							switch(ovr.type)
							{
							case OverrideVariant::kType_String:
								_MESSAGE("\t\t\t\tOverride: Key (%d) Value (%s)", ovr.key, ovr.str->c_str());
								break;
							case OverrideVariant::kType_Float:
								_MESSAGE("\t\t\t\tOverride: Key (%d) Value (%f)", ovr.key, ovr.data.f);
								break;
							default:
								_MESSAGE("\t\t\t\tOverride: Key (%d) Value (%X)", ovr.key, ovr.data.u);
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
	_MESSAGE("Dumping (%lld) node overrides", nodeData.m_data.size());
	for(auto nit : nodeData.m_data)
	{
		for(UInt8 gender = 0; gender < 2; gender++)
		{
			_MESSAGE("Node Handle: (%016llX) children (%lld) Gender (%d)", nit.first, nit.second[gender].size(), gender);
			for(auto oit : nit.second[gender]) // Loop Overrides
			{
				_MESSAGE("\tOverride Node: (%s) children (%lld)", oit.first->c_str(), oit.second.size());
				for(auto ovr : oit.second)
				{
					switch(ovr.type)
					{
					case OverrideVariant::kType_String:
						_MESSAGE("\t\tOverride: Key (%d) Value (%s)", ovr.key, ovr.str->c_str());
						break;
					case OverrideVariant::kType_Float:
						_MESSAGE("\t\tOverride: Key (%d) Value (%f)", ovr.key, ovr.data.f);
						break;
					default:
						_MESSAGE("\t\tOverride: Key (%d) Value (%X)", ovr.key, ovr.data.u);
						break;
					}
				}
			}
		}
	}
	nodeData.Release();

	skinData.Lock();
	_MESSAGE("Dumping (%lld) skin overrides", skinData.m_data.size());
	for (auto nit : skinData.m_data)
	{
		for (UInt8 gender = 0; gender <= 1; gender++)
		{
			for (UInt8 perspective = 0; perspective <= 1; perspective++)
			{
				_MESSAGE("Skin Handle: (%016llX) Gender (%d) Perspective (%d)", nit.first, gender, perspective);
				for (auto oit : nit.second[gender][perspective]) // Loop Overrides
				{
					_MESSAGE("\tSkin Override: Slot (%08X) children (%lld)", oit.first, oit.second.size());
					for (auto ovr : oit.second)
					{
						switch (ovr.type)
						{
						case OverrideVariant::kType_String:
							_MESSAGE("\t\tOverride: Key (%d) Value (%s)", ovr.key, ovr.str->c_str());
							break;
						case OverrideVariant::kType_Float:
							_MESSAGE("\t\tOverride: Key (%d) Value (%f)", ovr.key, ovr.data.f);
							break;
						default:
							_MESSAGE("\t\tOverride: Key (%d) Value (%X)", ovr.key, ovr.data.u);
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

void OverrideInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	Impl_ApplyOverrides(refr, armor, addon, object, g_immediateArmor);

	UInt32 armorMask = armor->bipedObject.GetSlotMask();
	UInt32 addonMask = addon->biped.GetSlotMask();
	Impl_ApplySkinOverrides(refr, isFirstPerson, armor, addon, armorMask & addonMask, object, g_immediateArmor);
}

bool OverrideInterface::HasArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr)
		return false;

	if (!OverrideVariant::IsIndexValid(key))
		index = OverrideVariant::kIndexMax;

	return Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index) != nullptr;
}

bool OverrideInterface::HasArmorAddonNode(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, bool debug)
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
		SInt32 i = value.Int();
		PackValue<SInt32>(&variant, key, index, &i);
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
		BGSTextureSet* ts = value.TextureSet();
		PackValue<BGSTextureSet*>(&variant, key, index, &ts);
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
		BGSTextureSet* textureSet = nullptr;
		UnpackValue<BGSTextureSet*>(&textureSet, &variant);
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
		SInt32 i = 0;
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

void OverrideInterface::AddArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddOverride(refr, isFemale, armor, addon, nodeName, variant);
}

bool OverrideInterface::GetArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr || !armor || !addon || !nodeName)
		return false;

	OverrideVariant* value = Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetArmorProperties(TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetProperties(refr->formID, immediate);
}

void OverrideInterface::SetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &variant, immediate);
}

bool OverrideInterface::GetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplyArmorOverrides(TESObjectREFR* refr, TESObjectARMO* armor, TESObjectARMA* addon, NiAVObject* object, bool immediate)
{
	if (!refr || !armor || !addon || !object)
		return;

	Impl_ApplyOverrides(refr, armor, addon, object, immediate);
}

void OverrideInterface::RemoveAllArmorOverrides()
{
	Impl_RemoveAllOverrides();
}

void OverrideInterface::RemoveAllArmorOverridesByReference(TESObjectREFR* refr)
{
	if (!refr)
		return;

	Impl_RemoveAllReferenceOverrides(refr);
}

void OverrideInterface::RemoveAllArmorOverridesByArmor(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor)
{
	if (!refr || !armor)
		return;

	Impl_RemoveAllArmorOverrides(refr, isFemale, armor);
}

void OverrideInterface::RemoveAllArmorOverridesByAddon(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon)
{
	if (!refr || !armor || !addon)
		return;

	Impl_RemoveAllArmorAddonOverrides(refr, isFemale, armor, addon);
}

void OverrideInterface::RemoveAllArmorOverridesByNode(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName)
{
	if (!refr || !armor || !addon || !nodeName)
		return;

	Impl_RemoveAllArmorAddonNodeOverrides(refr, isFemale, armor, addon, nodeName);
}

void OverrideInterface::RemoveArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr || !armor || !addon || !nodeName)
		return;

	Impl_RemoveArmorAddonOverride(refr, isFemale, armor, addon, nodeName, key, index);
}

bool OverrideInterface::HasNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index)
{
	if (!refr || !nodeName)
		return false;

	return Impl_GetNodeOverride(refr, isFemale, nodeName, key, index) != nullptr;
}

void OverrideInterface::AddNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddNodeOverride(refr, isFemale, nodeName, variant);
}

bool OverrideInterface::GetNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr)
		return false;

	OverrideVariant* value = Impl_GetNodeOverride(refr, isFemale, nodeName, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetNodeProperties(TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetNodeProperties(refr->formID, immediate);
}

void OverrideInterface::SetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetNodeProperty(refr, firstPerson, nodeName, &variant, immediate);
}

bool OverrideInterface::GetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetNodeProperty(refr, firstPerson, nodeName, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplyNodeOverrides(TESObjectREFR* refr, NiAVObject* object, bool immediate)
{
	Impl_ApplyNodeOverrides(refr, object, immediate);
}

void OverrideInterface::RemoveAllNodeOverrides()
{
	Impl_RemoveAllNodeOverrides();
}

void OverrideInterface::RemoveAllNodeOverridesByReference(TESObjectREFR* reference)
{
	Impl_RemoveAllReferenceNodeOverrides(reference);
}

void OverrideInterface::RemoveAllNodeOverridesByNode(TESObjectREFR* refr, bool isFemale, const char* nodeName)
{
	Impl_RemoveAllNodeNameOverrides(refr, isFemale, nodeName);
}

void OverrideInterface::RemoveNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index)
{
	Impl_RemoveNodeOverride(refr, isFemale, nodeName, key, index);
}

bool OverrideInterface::HasSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index)
{
	if (!refr)
		return false;

	return Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index) != nullptr;
}

void OverrideInterface::AddSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value)
{
	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_AddSkinOverride(refr, isFemale, firstPerson, slotMask, variant);
}

bool OverrideInterface::GetSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& visitor)
{
	if (!refr)
		return false;

	OverrideVariant* value = Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
	if (!value)
		return false;

	return GetValueVariant(*value, key, index, visitor);
}

void OverrideInterface::SetSkinProperties(TESObjectREFR* refr, bool immediate)
{
	if (!refr)
		return;

	Impl_SetSkinProperties(refr->formID, immediate);
}

void OverrideInterface::SetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate)
{
	if (!refr)
		return;

	OverrideVariant variant;
	SetValueVariant(variant, key, index, value);
	Impl_SetSkinProperty(refr, firstPerson, slotMask, &variant, immediate);
}

bool OverrideInterface::GetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& value)
{
	if (!refr)
		return false;

	OverrideVariant variant;
	variant.key = key;
	variant.index = index;
	Impl_GetSkinProperty(refr, firstPerson, slotMask, &variant);
	return GetValueVariant(variant, key, index, value);
}

void OverrideInterface::ApplySkinOverrides(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, skee_u32 slotMask, NiAVObject* object, bool immediate)
{
	if (!refr || !armor || !addon || !object)
		return;
	
	Impl_ApplySkinOverrides(refr, firstPerson, armor, addon, slotMask, object, immediate);
}

void OverrideInterface::RemoveAllSkinOverridesBySlot(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask)
{
	Impl_RemoveAllSkinOverrides(refr, isFemale, firstPerson, slotMask);
}

void OverrideInterface::RemoveSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index)
{
	Impl_RemoveSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
}

void OverrideInterface::RemoveAllSkinOverrides()
{
	Impl_RemoveAllSkinBasedOverrides();
}

void OverrideInterface::RemoveAllSkinOverridesByReference(TESObjectREFR* reference)
{
	Impl_RemoveAllReferenceSkinOverrides(reference);
}
