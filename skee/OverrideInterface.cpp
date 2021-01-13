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

UInt32 OverrideInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverrideInterface::AddRawOverride(OverrideHandle handle, bool isFemale, OverrideHandle armorHandle, OverrideHandle addonHandle, BSFixedString nodeName, OverrideVariant & value)
{
	armorData.Lock();
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[handle][isFemale ? 1 : 0][armorHandle][addonHandle][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::AddOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle formId = refr->formID;
	OverrideHandle armorFormId = armor->formID;
	OverrideHandle addonFormId = addon->formID;
	armorData.Lock();
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].erase(value);
	armorData.m_data[formId][isFemale ? 1 : 0][armorFormId][addonFormId][g_stringTable.GetString(nodeName)].insert(value);
	armorData.Release();
}

void OverrideInterface::AddRawNodeOverride(OverrideHandle handle, bool isFemale, BSFixedString nodeName, OverrideVariant & value)
{
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::AddNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	nodeData.Lock();
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].erase(value);
	nodeData.m_data[handle][isFemale ? 1 : 0][g_stringTable.GetString(nodeName)].insert(value);
	nodeData.Release();
}

void OverrideInterface::AddRawWeaponOverride(OverrideHandle handle, bool isFemale, bool firstPerson, OverrideHandle weaponHandle, BSFixedString nodeName, OverrideVariant & value)
{
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::AddWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant & value)
{
	OverrideHandle handle = refr->formID;
	OverrideHandle weaponHandle = weapon->formID;
	weaponData.Lock();
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].erase(value);
	weaponData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][weaponHandle][g_stringTable.GetString(nodeName)].insert(value);
	weaponData.Release();
}

void OverrideInterface::AddRawSkinOverride(OverrideHandle handle, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value)
{
	skinData.Lock();
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].erase(value);
	skinData.m_data[handle][isFemale ? 1 : 0][firstPerson ? 1 : 0][slotMask].insert(value);
	skinData.Release();
}

void OverrideInterface::AddSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value)
{
	AddRawSkinOverride(refr->formID, isFemale, firstPerson, slotMask, value);
}

OverrideVariant * OverrideInterface::GetOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto & ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto & dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto & oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					OverrideVariant ovr;
					ovr.key = key;
					ovr.index = index;
					auto & ost = oit->second.find(ovr);
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

OverrideVariant * OverrideInterface::GetNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
	auto & it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto & oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto & ost = oit->second.find(ovr);
			if(ost != oit->second.end())
			{
				return const_cast<OverrideVariant*>(&(*ost));
			}
		}
	}

	return NULL;
}

OverrideVariant * OverrideInterface::GetWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&weaponData.m_lock);
	auto & it = weaponData.m_data.find(refr->formID);
	if (it != weaponData.m_data.end())
	{
		auto & ait = it->second[gender][firstPerson].find(weapon->formID);
		if (ait != it->second[gender][firstPerson].end())
		{
			auto & oit = ait->second.find(g_stringTable.GetString(nodeName));
			if (oit != ait->second.end())
			{
				OverrideVariant ovr;
				ovr.key = key;
				ovr.index = index;
				auto & ost = oit->second.find(ovr);
				if (ost != oit->second.end())
				{
					return const_cast<OverrideVariant*>(&(*ost));
				}
			}
		}
	}

	return NULL;
}

OverrideVariant * OverrideInterface::GetSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&skinData.m_lock);
	auto & it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto & slot = it->second[gender][firstPerson].find(slotMask);
		if(slot != it->second[gender][firstPerson].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto & ost = slot->second.find(ovr);
			if (ost != slot->second.end())
			{
				return const_cast<OverrideVariant*>(&(*ost));
			}
		}
	}

	return NULL;
}

void OverrideInterface::RemoveAllReferenceOverrides(TESObjectREFR * refr)
{
	RemoveAllReferenceOverrides(refr->formID);
}

void OverrideInterface::RemoveAllReferenceNodeOverrides(TESObjectREFR * refr)
{
	RemoveAllReferenceNodeOverrides(refr->formID);
}

void OverrideInterface::RemoveAllReferenceWeaponOverrides(TESObjectREFR * refr)
{
	RemoveAllReferenceWeaponOverrides(refr->formID);
}

void OverrideInterface::RemoveAllReferenceSkinOverrides(TESObjectREFR * refr)
{
	RemoveAllReferenceSkinOverrides(refr->formID);
}

void OverrideInterface::RemoveAllReferenceOverrides(OverrideHandle handle)
{
	armorData.Lock();
	armorData.m_data.erase(handle);
	armorData.Release();
}

void OverrideInterface::RemoveAllReferenceNodeOverrides(OverrideHandle handle)
{
	nodeData.Lock();
	nodeData.m_data.erase(handle);
	nodeData.Release();
}

void OverrideInterface::RemoveAllReferenceWeaponOverrides(OverrideHandle handle)
{
	weaponData.Lock();
	weaponData.m_data.erase(handle);
	weaponData.Release();
}

void OverrideInterface::RemoveAllReferenceSkinOverrides(OverrideHandle handle)
{
	skinData.Lock();
	skinData.m_data.erase(handle);
	skinData.Release();
}

void OverrideInterface::RemoveAllArmorOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto & nit = it->second[gender].find(armor->formID);
		if(nit != it->second[gender].end()) {
			nit->second.clear();
		}
	}
}

void OverrideInterface::RemoveAllArmorAddonOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto & ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto & dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				ait->second.erase(dit);
			}
		}
	}
}

void OverrideInterface::RemoveAllArmorAddonNodeOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto & ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto & dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto & oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					dit->second.erase(oit);
				}
			}
		}
	}
}

void OverrideInterface::RemoveArmorAddonOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID);
	if(it != armorData.m_data.end())
	{
		auto & ait = it->second[gender].find(armor->formID);
		if(ait != it->second[gender].end())
		{
			auto & dit = ait->second.find(addon->formID);
			if(dit != ait->second.end())
			{
				auto & oit = dit->second.find(g_stringTable.GetString(nodeName));
				if(oit != dit->second.end())
				{
					OverrideVariant ovr;
					ovr.key = key;
					ovr.index = index;
					auto & ost = oit->second.find(ovr);
					if(ost != oit->second.end())
					{
						oit->second.erase(ost);
					}
				}
			}
		}
	}
}

void OverrideInterface::RemoveAllNodeNameOverrides(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
	auto & it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto & oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			it->second[gender].erase(oit);
		}
	}
}

void OverrideInterface::RemoveNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&nodeData.m_lock);
	auto & it = nodeData.m_data.find(refr->formID);
	if(it != nodeData.m_data.end())
	{
		auto & oit = it->second[gender].find(g_stringTable.GetString(nodeName));
		if(oit != it->second[gender].end())
		{
			OverrideVariant ovr;
			ovr.key = key;
			ovr.index = index;
			auto & ost = oit->second.find(ovr);
			if(ost != oit->second.end())
			{
				oit->second.erase(ost);
			}
		}
	}
}

void OverrideInterface::RemoveAllWeaponOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&weaponData.m_lock);
	auto & it = weaponData.m_data.find(refr->formID);
	if(it != weaponData.m_data.end())
	{
		WeaponRegistration::iterator ait = it->second[gender][firstPerson ? 1 : 0].find(weapon->formID);
		if(ait != it->second[gender][firstPerson ? 1 : 0].end())
		{
			it->second[gender][firstPerson ? 1 : 0].erase(ait);
		}
	}
}

void OverrideInterface::RemoveAllWeaponNodeOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&weaponData.m_lock);
	auto & it = weaponData.m_data.find(refr->formID);
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

void OverrideInterface::RemoveWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&weaponData.m_lock);
	auto & it = weaponData.m_data.find(refr->formID);
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

void OverrideInterface::RemoveAllSkinOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask)
{
	UInt8 gender = isFemale ? 1 : 0;
	SimpleLocker locker(&skinData.m_lock);
	auto & it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto & slot = it->second[gender][firstPerson].find(slotMask);
		if (slot != it->second[gender][firstPerson].end())
		{
			it->second[gender][firstPerson].erase(slot);
		}
	}
}

void OverrideInterface::RemoveSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index)
{
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fPerson = firstPerson ? 1 : 0;

	SimpleLocker locker(&skinData.m_lock);
	auto & it = skinData.m_data.find(refr->formID);
	if (it != skinData.m_data.end())
	{
		auto & slot = it->second[gender][firstPerson].find(slotMask);
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

void OverrideInterface::SetArmorAddonProperty(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (actor) {
		VisitArmorAddon(actor, armor, addon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
		{
			bool isRoot = nodeName == BSFixedString("");
			if (!armorNode->GetAsNiNode() && !isRoot) {
				_WARNING("%s - Warning, override for Armor %08X Addon %08X has no children, use an empty string for the node name to access the root instead.", __FUNCTION__, armor->formID, addon->formID);
			}
			NiAVObject * foundNode = isRoot ? armorNode : armorNode->GetObjectByName(&nodeName.data);
			if (foundNode) {
				SetShaderProperty(foundNode, value, immediate);
			}
		});
	}
}

void OverrideInterface::GetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value)
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

bool OverrideInterface::HasArmorAddonNode(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, bool debug)
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

void OverrideInterface::SetWeaponProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	char weaponString[MAX_PATH];

	UInt8 gender = isFemale ? 1 : 0;

	memset(weaponString, 0, MAX_PATH);
	weapon->GetNodeName(weaponString);

	NiNode * root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		root->IncRef();
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
		root->DecRef();
	}
}

void OverrideInterface::GetWeaponProperty(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value)
{
	char weaponString[MAX_PATH];

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	memset(weaponString, 0, MAX_PATH);
	weapon->GetNodeName(weaponString);

	NiNode * root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		root->IncRef();
		BSFixedString weaponName(weaponString); // Find the Armor name from the root
		NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
		if(weaponNode) {
			NiAVObject * foundNode = nodeName == BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
			if(foundNode) {
				GetShaderProperty(foundNode, value);
			}
		}
		root->DecRef();
	}
}

bool OverrideInterface::HasWeaponNode(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, bool debug)
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

	NiNode * root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		root->IncRef();
		BSFixedString weaponName(weaponString); // Find the Armor name from the root
		NiAVObject * weaponNode = root->GetObjectByName(&weaponName.data);
		if(weaponNode) {
			NiAVObject * foundNode = nodeName == BSFixedString("") ? weaponNode : weaponNode->GetObjectByName(&nodeName.data);
			if(foundNode) {
				if(debug)	_DMESSAGE("%s - Success, found node name '%s' for Weapon %08X.", __FUNCTION__, nodeName.data, weapon->formID);
				return true;
			} else if(debug)
				_DMESSAGE("%s - Failed to find node name '%s' for Weapon %08X.", __FUNCTION__, nodeName.data, weapon->formID);
		} else if(debug)
			_DMESSAGE("%s - Failed to acquire weapon node '%s' for Weapon %08X.", __FUNCTION__, weaponName.data, weapon->formID);
		root->DecRef();
	} else if(debug)
		_DMESSAGE("%s - Failed to acquire skeleton for Reference %08X", __FUNCTION__, refr->formID);

	return false;
}

void OverrideInterface::SetSkinProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant * value, bool immediate)
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

void OverrideInterface::GetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value)
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

void OverrideInterface::SetProperties(OverrideHandle formId, bool immediate)
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
	auto & it = armorData.m_data.find(formId); // Find ActorHandle
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
								SetShaderProperty(foundNode, value, immediate);
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

void OverrideInterface::SetNodeProperty(TESObjectREFR * refr, BSFixedString nodeName, OverrideVariant * value, bool immediate)
{
	NiNode * lastRoot = NULL;
	for(UInt32 i = 0; i <= 1; i++)
	{
		NiNode * root = refr->GetNiRootNode(i); // Apply to third and first person
		if(root == lastRoot) // First and Third are the same, skip
			continue;

		if(root) {
			root->IncRef();
			NiAVObject * foundNode = root->GetObjectByName(&nodeName.data);
			if(foundNode) {
				SetShaderProperty(foundNode, value, immediate);
			}
			root->DecRef();
		}

		lastRoot = root;
	}
}

void OverrideInterface::GetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value)
{
	NiNode * root = refr->GetNiRootNode(firstPerson ? 1 : 0); // Apply to third and first person
	if(root) {
		root->IncRef();
		NiAVObject * foundNode = root->GetObjectByName(&nodeName.data);
		if(foundNode) {
			GetShaderProperty(foundNode, value);
		}
		root->DecRef();
	}
}

void OverrideInterface::SetNodeProperties(OverrideHandle formId, bool immediate)
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
	auto & nit = nodeData.m_data.find(formId); // Find ActorHandle
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

void OverrideInterface::SetWeaponProperties(OverrideHandle formId, bool immediate)
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
	auto & it = weaponData.m_data.find(refr->formID); // Find ActorHandle
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

				NiNode * lastNode = NULL;
				BSFixedString weaponName(weaponString);

				NiNode * root = refr->GetNiRootNode(i);
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					root->IncRef();
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
					root->DecRef();
				}

				lastNode = root;
			}
		}
	}
}

void OverrideInterface::SetSkinProperties(OverrideHandle formId, bool immediate)
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
	auto & it = skinData.m_data.find(refr->formID); // Find ActorHandle
	if (it != skinData.m_data.end())
	{
		for (UInt8 fp = 0; fp <= 1; fp++)
		{
			for (SkinRegistration::iterator ait = it->second[gender][fp].begin(); ait != it->second[gender][fp].end(); ++ait) // Loop Armors
			{
				NiNode * lastNode = NULL;
				NiPointer<NiNode> root = refr->GetNiRootNode(fp);
				if (root == lastNode) // First and Third are the same, skip
					continue;

				if (root)
				{
					TESForm * pForm = GetSkinForm(actor, ait->first);
					TESObjectARMO * armor = DYNAMIC_CAST(pForm, TESForm, TESObjectARMO);
					if (armor) {
						for (UInt32 i = 0; i < armor->armorAddons.count; i++) {
							TESObjectARMA * arma = NULL;
							if (armor->armorAddons.GetNthItem(i, arma)) {
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
														ait->second.Visit([&](OverrideVariant * value)
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
	auto & nit = nodeData.m_data.find(refr->formID); // Find ActorHandle
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
	auto & it = skinData.m_data.find(refr->formID);
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

/*
void OverrideInterface::SetHandleArmorAddonProperties(UInt64 handle, UInt64 armorHandle, UInt64 addonHandle, bool immediate)
{
	TESObjectREFR * refr = (TESObjectREFR *)VirtualMachine::GetObject(handle, TESObjectREFR::kTypeID);
	if(!refr) {
		armorData.Lock();
		armorData.m_data.erase(handle);
		armorData.Release();
		_MESSAGE("Error applying override: No such reference %016llX", handle);
		return;
	}

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	ActorRegistrationMapHolder::RegMap::iterator it = armorData.m_data.find(handle); // Find ActorHandle
	if(it != armorData.m_data.end())
	{
		TESObjectARMO * armor = (TESObjectARMO *)VirtualMachine::GetObject(armorHandle, TESObjectARMO::kTypeID);
		if(!armor) {
			armorData.Lock();
			armorData.m_data[handle][gender].erase(armorHandle);
			armorData.Release();
			_MESSAGE("Error applying override: No such armor %016llX", armorHandle);
			return;
		}
		ArmorRegistration::iterator ait = it->second[gender].find(armorHandle); // Find ArmorHandle
		if(ait != it->second[gender].end())
		{
			TESObjectARMA * addon = (TESObjectARMA *)VirtualMachine::GetObject(addonHandle, TESObjectARMA::kTypeID);
			if(!addon) {
				armorData.Lock();
				armorData.m_data[handle][gender][armorHandle].erase(addonHandle);
				armorData.Release();
				_MESSAGE("Error applying override: No such addon %016llX", addonHandle);
				return;
			}

			AddonRegistration::iterator dit = ait->second.find(addonHandle); // Find AddonHandle
			if(dit != ait->second.end())
			{
				char addonString[MAX_PATH];
				addon->GetNodeName(addonString, refr, armor, -1);

				NiNode * lastRoot = NULL;
				BSFixedString addonName(addonString);
				for(UInt8 i = 0; i <= 1; i++)
				{
					NiNode * root = refr->GetNiRootNode(i);
					if(root == lastRoot) // First and third are the same, skip
						continue;

					if(root)
					{
						NiAVObject * armorNode = root->GetObjectByName(&addonName.data);
						if(armorNode) {
							OverrideRegistration<BSFixedString>::SetVisitor visitor(armorNode, immediate);
							dit->second.Visit(&visitor);
						}
					}

					lastRoot = root;
				}
			}
		}
	}
}*/

class NodeOverrideApplicator : public GeometryVisitor
{
public:
	NodeOverrideApplicator::NodeOverrideApplicator(OverrideRegistration<StringTableItem> * overrides, bool immediate) : m_overrides(overrides), m_immediate(immediate) {}

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
	OverrideApplicator::OverrideApplicator(OverrideRegistration<StringTableItem> * overrides, bool immediate) : m_overrides(overrides), m_immediate(immediate) {}

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
	SkinOverrideApplicator::SkinOverrideApplicator(TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, OverrideSet * overrides, bool immediate) : m_armor(armor), m_addon(addon), m_overrides(overrides), m_slotMask(slotMask), m_immediate(immediate) {}

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
/*
void OverrideInterface::GetGeometryCount(NiAVObject * object, SInt32 * count)
{
	NiGeometry * geometry = object->GetAsNiGeometry();
	if(geometry) {
		(*count)++;
		return;
	}
	
	NiNode * niNode = object->GetAsNiNode();
	if(niNode) {
		if(niNode->m_children.m_emptyRunStart > 0 && niNode->m_children.m_data) {
			for(int i = 0; i < niNode->m_children.m_emptyRunStart; i++)
			{
				NiAVObject * object = niNode->m_children.m_data[i];
				if(object)
					GetGeometryCount(object, count);
			}
		}
	}
}

template<>
void OverrideInterface::SetOverrides(NiGeometry * geometry, OverrideRegistration<BSFixedString> * overrides, bool immediate, SInt32 geoCount)
{
	BSFixedString objectName(geoCount == 1 ? "" : geometry->m_name);
	OverrideRegistration<BSFixedString>::iterator nit = overrides->find(objectName);
	if(nit != overrides->end())
	{
		OverrideSet::PropertyVisitor visitor(geometry, immediate);
		nit->second.Visit(&visitor);
	}
}

template<typename T>
void OverrideInterface::SetOverridesRecursive(NiAVObject * node, OverrideRegistration<T> * overrides, bool immediate, SInt32 geoCount)
{
	NiNode * niNode = node->GetAsNiNode();
	NiGeometry * geometry = node->GetAsNiGeometry();
	if(geometry)
		SetOverrides(geometry, overrides, immediate, geoCount);
	else if(niNode && niNode->m_children.m_emptyRunStart > 0)
	{
		for(int i = 0; i < niNode->m_children.m_emptyRunStart; i++)
		{
			NiAVObject * object = niNode->m_children.m_data[i];
			if(object)
				SetOverridesRecursive(object, overrides, immediate, geoCount);
		}
	}
}
*/

void OverrideInterface::ApplyNodeOverrides(TESObjectREFR * refr, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&nodeData.m_lock);
	auto & nit = nodeData.m_data.find(refr->formID);
	if(nit != nodeData.m_data.end()) {
		NodeOverrideApplicator applicator(&nit->second[gender], immediate);
		VisitGeometry(object, &applicator);
	}
}

void OverrideInterface::ApplyOverrides(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool immediate)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&armorData.m_lock);
	auto & it = armorData.m_data.find(refr->formID); // Find ActorHandle
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

void OverrideInterface::ApplyWeaponOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, NiAVObject * object, bool immediate)
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

void OverrideInterface::ApplySkinOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, NiAVObject * object, bool immediate)
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
	for(auto it = begin(); it != end(); ++it) {
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

void OverrideInterface::RemoveAllOverrides()
{
	armorData.Lock();
	armorData.m_data.clear();
	armorData.Release();
}

void OverrideInterface::RemoveAllNodeOverrides()
{
	nodeData.Lock();
	nodeData.m_data.clear();
	nodeData.Release();
}

void OverrideInterface::RemoveAllWeaponBasedOverrides()
{
	weaponData.Lock();
	weaponData.m_data.clear();
	weaponData.Release();
}

void OverrideInterface::RemoveAllSkinBasedOverrides()
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
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
				_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
						emplace(key, set);
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
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
				_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
					_MESSAGE("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
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
	bool error = false;
	
	UInt64 handle = 0;
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<OverrideRegistration<StringTableItem>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading override gender registrations", __FUNCTION__);
		error = true;
		return error;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return error;
	}

	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID)
	{
		*outHandle = 0;
		return error;
	}

	if(reg.empty()) {
		*outHandle = 0;
		return error;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();
	return error;
}

void NodeRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
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
	bool error = false;

	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<ArmorRegistration,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading armor gender registrations", __FUNCTION__);
		error = true;
		return error;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return error;
	}

	// Invalid handle
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID)
	{
		*outHandle = 0;
		return error;
	}
	
	if(reg.empty()) {
		*outHandle = 0;
		return error;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif

	//SetHandleProperties(newHandle, false);
	return error;
}


void WeaponRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
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
	bool error = false;

	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<MultiRegistration<WeaponRegistration,2>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading weapon registrations", __FUNCTION__);
		error = true;
		return error;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return error;
	}

	// Invalid handle
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID)
	{
		*outHandle = 0;
		return error;
	}

	if(reg.empty()) {
		*outHandle = 0;
		return error;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif

	//SetHandleProperties(newHandle, false);
	return error;
}


void SkinRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
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
	bool error = false;

	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_MESSAGE("%s - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<MultiRegistration<SkinRegistration, 2>, 2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_MESSAGE("%s - Error loading skin registrations", __FUNCTION__);
		error = true;
		return error;
	}

	OverrideHandle formId = handle & 0xFFFFFFFF;
	OverrideHandle newFormId = 0;

	// Skip if handle is no longer valid.
	if (!ResolveAnyForm(intfc, formId, &newFormId)) {
		*outHandle = 0;
		return error;
	}

	// Invalid handle
	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID)
	{
		*outHandle = 0;
		return error;
	}

	if (reg.empty()) {
		*outHandle = 0;
		return error;
	}

	*outHandle = newFormId;

	Lock();
	m_data[newFormId] = reg;
	Release();

#ifdef _DEBUG
	_MESSAGE("%s - Loaded %08X", __FUNCTION__, newFormId);
#endif

	//SetHandleProperties(newHandle, false);
	return error;
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

#ifdef _DEBUG
void OverrideInterface::DumpMap()
{
	_MESSAGE("Dumping Overrides");
	for(ActorRegistrationMapHolder::RegMap::iterator it = armorData.m_data.begin(); it != armorData.m_data.end(); ++it)
	{
		for(UInt8 gender = 0; gender < 2; gender++)
		{
			_MESSAGE("Actor Handle: %016llX children %d", it->first, it->second[gender].size());
			for(ArmorRegistration::iterator ait = it->second[gender].begin(); ait != it->second[gender].end(); ++ait) // Loop Armors
			{
				_MESSAGE("Armor Handle: %016llX children %d", ait->first, ait->second.size());
				for(AddonRegistration::iterator dit = ait->second.begin(); dit != ait->second.end(); ++dit) // Loop Addons
				{
					_MESSAGE("Addon Handle: %016llX children %d", dit->first, dit->second.size());
					for(auto nit = dit->second.begin(); nit != dit->second.end(); ++nit) // Loop Overrides
					{
						_MESSAGE("Override Node: %s children %d", nit->first->c_str(), nit->second.size());
						for(OverrideSet::iterator iter = nit->second.begin(); iter != nit->second.end(); ++iter)
						{
							switch((*iter).type)
							{
							case OverrideVariant::kType_String:
								_MESSAGE("Override: Key %d Value %s", (*iter).key, (*iter).str->c_str());
								break;
							case OverrideVariant::kType_Float:
								_MESSAGE("Override: Key %d Value %f", (*iter).key, (*iter).data.f);
								break;
							default:
								_MESSAGE("Override: Key %d Value %X", (*iter).key, (*iter).data.u);
								break;
							}
						}
					}
				}
			}
		}
	}
	for(NodeRegistrationMapHolder::RegMap::iterator nit = nodeData.m_data.begin(); nit != nodeData.m_data.end(); ++nit)
	{
		for(UInt8 gender = 0; gender < 2; gender++)
		{
			_MESSAGE("Node Handle: %016llX children %d", nit->first, nit->second[gender].size());
			for(auto oit = nit->second[gender].begin(); oit != nit->second[gender].end(); ++oit) // Loop Overrides
			{
				_MESSAGE("Override Node: %s children %d", oit->first->c_str(), oit->second.size());
				for(OverrideSet::iterator iter = oit->second.begin(); iter != oit->second.end(); ++iter)
				{
					switch((*iter).type)
					{
					case OverrideVariant::kType_String:
						_MESSAGE("Override: Key %d Value %s", (*iter).key, (*iter).str->c_str());
						break;
					case OverrideVariant::kType_Float:
						_MESSAGE("Override: Key %d Value %f", (*iter).key, (*iter).data.f);
						break;
					default:
						_MESSAGE("Override: Key %d Value %X", (*iter).key, (*iter).data.u);
						break;
					}
				}
			}
		}
	}
}
#endif

extern bool	g_immediateArmor;

void OverrideInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	ApplyOverrides(refr, armor, addon, object, g_immediateArmor);

	UInt32 armorMask = armor->bipedObject.GetSlotMask();
	UInt32 addonMask = addon->biped.GetSlotMask();
	ApplySkinOverrides(refr, isFirstPerson, armor, addon, armorMask & addonMask, object, g_immediateArmor);
}
