#pragma once

class VMClassRegistry;
struct StaticFunctionTag;
class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class TESObjectWEAP;

#include "skse64/GameTypes.h"

namespace papyrusNiOverride
{
	void RegisterFuncs(VMClassRegistry* registry);

	// Armor Overrides
	bool HasOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	void AddOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist);

	template<typename T>
	T GetOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	T GetArmorAddonProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index);


	// Node Overrides
	bool HasNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	void AddNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist);

	template<typename T>
	T GetNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	T GetNodeProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, UInt32 key, UInt32 index);


	// Weapon overrides
	bool HasWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	void AddWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist);

	template<typename T>
	T GetWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index);

	template<typename T>
	T GetWeaponProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index);


	void RemoveAllOverrides(StaticFunctionTag* base);
	void RemoveAllReferenceOverrides(StaticFunctionTag* base, TESObjectREFR * refr);
	void RemoveAllArmorOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor);
	void RemoveAllArmorAddonOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon);
	void RemoveAllArmorAddonNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName);
	void RemoveArmorAddonOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index);

	void RemoveAllNodeOverrides(StaticFunctionTag* base);
	void RemoveAllReferenceNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr);
	void RemoveAllNodeNameOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName);
	void RemoveNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index);

	void RemoveAllWeaponBasedOverrides(StaticFunctionTag* base);
	void RemoveAllReferenceWeaponOverrides(StaticFunctionTag* base, TESObjectREFR * refr);
	void RemoveAllWeaponOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon);
	void RemoveAllWeaponNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName);
	void RemoveWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index);


	UInt32 GetNumBodyOverlays(StaticFunctionTag* base);
	UInt32 GetNumHandOverlays(StaticFunctionTag* base);
	UInt32 GetNumFeetOverlays(StaticFunctionTag* base);
	UInt32 GetNumFaceOverlays(StaticFunctionTag* base);

	UInt32 GetNumSpellBodyOverlays(StaticFunctionTag* base);
	UInt32 GetNumSpellHandOverlays(StaticFunctionTag* base);
	UInt32 GetNumSpellFeetOverlays(StaticFunctionTag* base);
	UInt32 GetNumSpellFaceOverlays(StaticFunctionTag* base);

	void AddOverlays(StaticFunctionTag* base, TESObjectREFR * refr);
	bool HasOverlays(StaticFunctionTag* base, TESObjectREFR * refr);
	void RemoveOverlays(StaticFunctionTag* base, TESObjectREFR * refr);
	void RevertOverlays(StaticFunctionTag* base, TESObjectREFR * refr);
	void RevertOverlay(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask);

	void SetBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName, float value);
	float GetBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName);
	void ClearBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName);
	void UpdateItemTextureLayers(StaticFunctionTag * base, TESObjectREFR * refr, UInt32 uniqueID);
}
