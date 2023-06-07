#pragma once

#include "IPluginInterface.h"
#include "IHashType.h"

#include "StringTable.h"

#include "skse64/GameTypes.h"
#include "skse64/NiTypes.h"

#include <set>
#include <vector>
#include <unordered_map>
#include <functional>

class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class TESObjectWEAP;
class NiAVObject;
struct SKSESerializationInterface;
class NiGeometry;
class BGSTextureSet;
class OverrideVariant;

typedef UInt32 OverrideHandle;

class OverrideSet : public std::set<OverrideVariant>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);

	virtual void Visit(std::function<bool(OverrideVariant*)> functor);
};

template<typename T>
bool ReadKey(SKSESerializationInterface * intfc, T & key, UInt32 kVersion, const StringIdMap & stringTable);

template<typename T>
void WriteKey(SKSESerializationInterface * intfc, const T key, UInt32 kVersion);

template<typename T>
class OverrideRegistration : public std::unordered_map<T, OverrideSet>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);

	virtual void Visit(std::function<bool(const T & key, OverrideSet *)> functor);
};

class AddonRegistration : public std::unordered_map<OverrideHandle, OverrideRegistration<StringTableItem>>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class ArmorRegistration : public std::unordered_map<OverrideHandle, AddonRegistration>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class WeaponRegistration : public std::unordered_map<OverrideHandle, OverrideRegistration<StringTableItem>>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class SkinRegistration : public std::unordered_map<OverrideHandle, OverrideSet>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

template<typename T, UInt32 N>
class MultiRegistration
{
public:
	// Serialization

	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
	{
		bool error = false;

		UInt8 size = 0;
		if (!intfc->ReadRecordData(&size, sizeof(size)))
		{
			_MESSAGE("%s - Error loading multi-registrations", __FUNCTION__);
			error = true;
			return error;
		}

		for (UInt32 i = 0; i < size; i++)
		{
			UInt8 index = 0;
			if (!intfc->ReadRecordData(&index, sizeof(index)))
			{
				_MESSAGE("%s - Error loading multi-registration index (%d/%d)", __FUNCTION__, i + 1, size);
				error = true;
				return error;
			}

			T regs;
			if (regs.Load(intfc, kVersion, stringTable))
			{
				_MESSAGE("%s - Error loading multi-registrations (%d/%d)", __FUNCTION__, i + 1, size);
				error = true;
				return error;
			}

			table[index] = regs;

#ifdef _DEBUG
			_MESSAGE("%s - Loaded multi-reg (%d)", __FUNCTION__, index);
#endif
		}

		return error;
	}

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion)
	{
		UInt8 size = 0;
		for (UInt8 i = 0; i < N; i++)
		{
			if (!table[i].empty())
				size++;
		}

		intfc->WriteRecordData(&size, sizeof(size));

		for (UInt8 i = 0; i < N; i++)
		{
			if (!table[i].empty())
			{
#ifdef _DEBUG
				_MESSAGE("%s - Saving Multi-Reg %d", __FUNCTION__, i);
#endif
				intfc->WriteRecordData(&i, sizeof(i));
				table[i].Save(intfc, kVersion);
			}
		}
	}

	T& operator[] (const int index)
	{
		if(index > N-1)
			return table[0];

		return table[index];
	}

	bool empty()
	{
		UInt8 emptyCount = 0;
		for(UInt8 i = 0; i < N; i++)
		{
			if(table[i].empty())
				emptyCount++;
		}
		return emptyCount == N;
	}

	T table[N];
};

class ActorRegistrationMapHolder : public SafeDataHolder<std::unordered_map<OverrideHandle, MultiRegistration<ArmorRegistration, 2>>>
{
public:
	typedef std::unordered_map<OverrideHandle, MultiRegistration<ArmorRegistration, 2>>	RegMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, UInt32 * outFormId, const StringIdMap & stringTable);

	friend class OverrideInterface;
};

class NodeRegistrationMapHolder : public SafeDataHolder<std::unordered_map<OverrideHandle, MultiRegistration<OverrideRegistration<StringTableItem>, 2>>>
{
public:
	typedef std::unordered_map<OverrideHandle, MultiRegistration<OverrideRegistration<StringTableItem>, 2>>	RegMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, OverrideHandle* outFormId, const StringIdMap & stringTable);

	friend class OverrideInterface;
};

class WeaponRegistrationMapHolder : public SafeDataHolder<std::unordered_map<OverrideHandle, MultiRegistration<MultiRegistration<WeaponRegistration, 2>, 2>>>
{
public:
	typedef std::unordered_map<OverrideHandle, MultiRegistration<MultiRegistration<WeaponRegistration, 2>, 2>>	RegMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, OverrideHandle* outFormId, const StringIdMap & stringTable);

	friend class OverrideInterface;
};

class SkinRegistrationMapHolder : public SafeDataHolder<std::unordered_map<OverrideHandle, MultiRegistration<MultiRegistration<SkinRegistration, 2>, 2>>>
{
public:
	typedef std::unordered_map<OverrideHandle, MultiRegistration<MultiRegistration<SkinRegistration, 2>, 2>>	RegMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, OverrideHandle* outFormId, const StringIdMap & stringTable);

	friend class OverrideInterface;
};

class OverrideInterface
	: public IOverrideInterface
	, public IAddonAttachmentInterface
{
public:
	virtual skee_u32 GetVersion();

	virtual bool HasArmorAddonNode(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, bool debug) override;

	virtual bool HasArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) override;
	virtual void AddArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) override;
	virtual bool GetArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) override;
	virtual void RemoveArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) override;
	virtual void SetArmorProperties(TESObjectREFR* refr, bool immediate) override;
	virtual void SetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) override;
	virtual bool GetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) override;
	virtual void ApplyArmorOverrides(TESObjectREFR* refr, TESObjectARMO* armor, TESObjectARMA* addon, NiAVObject* object, bool immediate) override;
	virtual void RemoveAllArmorOverrides() override;
	virtual void RemoveAllArmorOverridesByReference(TESObjectREFR* reference) override;
	virtual void RemoveAllArmorOverridesByArmor(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor) override;
	virtual void RemoveAllArmorOverridesByAddon(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon) override;
	virtual void RemoveAllArmorOverridesByNode(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName) override;

	virtual bool HasNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) override;
	virtual void AddNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) override;
	virtual bool GetNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) override;
	virtual void RemoveNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) override;
	virtual void SetNodeProperties(TESObjectREFR* refr, bool immediate) override;
	virtual void SetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) override;
	virtual bool GetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) override;
	virtual void ApplyNodeOverrides(TESObjectREFR* refr, NiAVObject* object, bool immediate) override;
	virtual void RemoveAllNodeOverrides() override;
	virtual void RemoveAllNodeOverridesByReference(TESObjectREFR* reference) override;
	virtual void RemoveAllNodeOverridesByNode(TESObjectREFR* refr, bool isFemale, const char* nodeName) override;

	virtual bool HasSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) override;
	virtual void AddSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value) override;
	virtual bool GetSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& visitor) override;
	virtual void RemoveSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) override;
	virtual void SetSkinProperties(TESObjectREFR* refr, bool immediate) override;
	virtual void SetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) override;
	virtual bool GetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& value) override;
	virtual void ApplySkinOverrides(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, skee_u32 slotMask, NiAVObject* object, bool immediate) override;
	virtual void RemoveAllSkinOverridesBySlot(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask) override;
	virtual void RemoveAllSkinOverrides() override;
	virtual void RemoveAllSkinOverridesByReference(TESObjectREFR* reference) override;

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion) { return false; };
	void Revert();

	bool LoadOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	bool LoadNodeOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	bool LoadWeaponOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);

	// Specific overrides
	void Impl_AddRawOverride(OverrideHandle formId, bool isFemale, OverrideHandle armorHandle, OverrideHandle addonHandle, BSFixedString nodeName, OverrideVariant & value);
	void Impl_AddOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant & value);

	// Non-specific overrides
	void Impl_AddRawNodeOverride(OverrideHandle handle, bool isFemale, BSFixedString nodeName, OverrideVariant & value);
	void Impl_AddNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, OverrideVariant & value);

	// Applies all properties for a handle
	void Impl_SetProperties(OverrideHandle handle, bool immediate);

	// Applies node properties for a handle
	void Impl_SetNodeProperties(OverrideHandle handle, bool immediate);

	// Set/Get a single property
	void Impl_SetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	void Impl_GetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value);

	// Applies a single node property
	void Impl_SetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	void Impl_GetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value);

	// Determines whether the node could be found
	bool Impl_HasArmorAddonNode(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, bool debug);

	// Applies all node overrides to a particular node
	void Impl_ApplyNodeOverrides(TESObjectREFR * refr, NiAVObject * object, bool immediate);

	// Applies all armor overrides to a particular armor
	void Impl_ApplyOverrides(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool immediate);

	void Impl_RemoveAllOverrides();
	void Impl_RemoveAllReferenceOverrides(TESObjectREFR * reference);

	void Impl_RemoveAllArmorOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor);
	void Impl_RemoveAllArmorAddonOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon);
	void Impl_RemoveAllArmorAddonNodeOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName);
	void Impl_RemoveArmorAddonOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index);

	void Impl_RemoveAllNodeOverrides();
	void Impl_RemoveAllReferenceNodeOverrides(TESObjectREFR * reference);

	void Impl_RemoveAllNodeNameOverrides(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName);
	void Impl_RemoveNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index);

	OverrideVariant * Impl_GetOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index);
	OverrideVariant * Impl_GetNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index);

	void Impl_AddRawWeaponOverride(OverrideHandle handle, bool isFemale, bool firstPerson, OverrideHandle weaponHandle, BSFixedString nodeName, OverrideVariant & value);
	void Impl_AddWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant & value);
	OverrideVariant * Impl_GetWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index);
	void Impl_ApplyWeaponOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, NiAVObject * object, bool immediate);

	void Impl_RemoveAllWeaponBasedOverrides();
	void Impl_RemoveAllReferenceWeaponOverrides(TESObjectREFR * reference);

	void Impl_RemoveAllWeaponOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon);
	void Impl_RemoveAllWeaponNodeOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName);
	void Impl_RemoveWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index);

	bool Impl_HasWeaponNode(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, bool debug);
	void Impl_SetWeaponProperties(OverrideHandle handle, bool immediate);

	void Impl_SetWeaponProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	void Impl_GetWeaponProperty(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value);

	// Skin API
	bool LoadSkinOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	void Impl_AddRawSkinOverride(OverrideHandle handle, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value);
	void Impl_AddSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value);
	OverrideVariant * Impl_GetSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index);
	void Impl_ApplySkinOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, NiAVObject * object, bool immediate);
	void Impl_RemoveAllSkinOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask);
	void Impl_RemoveSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index);
	void Impl_SetSkinProperties(OverrideHandle handle, bool immediate);
	void Impl_RemoveAllSkinBasedOverrides();
	void Impl_RemoveAllReferenceSkinOverrides(TESObjectREFR * reference);
	void Impl_SetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value, bool immediate);
	void Impl_GetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value);

	void VisitNodes(TESObjectREFR * refr, std::function<void(SKEEFixedString, OverrideVariant&)> functor);
	void VisitSkin(TESObjectREFR * refr, bool isFemale, bool firstPerson, std::function<void(UInt32, OverrideVariant&)> functor);
	void VisitStrings(std::function<void(SKEEFixedString)> functor);

	void SetValueVariant(OverrideVariant& variant, UInt16 key, UInt8 index, SetVariant& value);
	bool GetValueVariant(OverrideVariant& variant, UInt16 key, UInt8 index, GetVariant& value);

	void Dump();
	void PrintDiagnostics();

private:
	ActorRegistrationMapHolder armorData;
	NodeRegistrationMapHolder nodeData;
	WeaponRegistrationMapHolder weaponData;
	SkinRegistrationMapHolder skinData;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};