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
	: public IPluginInterface
	, public IAddonAttachmentInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion3 = 2,
		kSerializationVersion = kSerializationVersion3
	};
	virtual UInt32 GetVersion();

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion) { return false; };
	virtual void Revert();

	virtual bool LoadOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual bool LoadNodeOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual bool LoadWeaponOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);

	// Specific overrides
	virtual void AddRawOverride(OverrideHandle formId, bool isFemale, OverrideHandle armorHandle, OverrideHandle addonHandle, BSFixedString nodeName, OverrideVariant & value);
	virtual void AddOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant & value);

	// Non-specific overrides
	virtual void AddRawNodeOverride(OverrideHandle handle, bool isFemale, BSFixedString nodeName, OverrideVariant & value);
	virtual void AddNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, OverrideVariant & value);

	// Applies all properties for a handle
	void SetProperties(OverrideHandle handle, bool immediate);

	// Applies all properties for an armor
	//void SetHandleArmorAddonProperties(UInt64 handle, UInt64 armorHandle, UInt64 addonHandle, bool immediate);

	// Applies node properties for a handle
	void SetNodeProperties(OverrideHandle handle, bool immediate);

	// Set/Get a single property
	virtual void SetArmorAddonProperty(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	virtual void GetArmorAddonProperty(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, OverrideVariant * value);

	// Applies a single node property
	virtual void SetNodeProperty(TESObjectREFR * refr, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	virtual void GetNodeProperty(TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, OverrideVariant * value);

	// Determines whether the node could be found
	virtual bool HasArmorAddonNode(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, bool debug);

	// Applies all node overrides to a particular node
	virtual void ApplyNodeOverrides(TESObjectREFR * refr, NiAVObject * object, bool immediate);

	// Applies all armor overrides to a particular armor
	virtual void ApplyOverrides(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool immediate);

	virtual void RemoveAllOverrides();
	virtual void RemoveAllReferenceOverrides(TESObjectREFR * reference);

	virtual void RemoveAllArmorOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor);
	virtual void RemoveAllArmorAddonOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon);
	virtual void RemoveAllArmorAddonNodeOverrides(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName);
	virtual void RemoveArmorAddonOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index);

	virtual void RemoveAllNodeOverrides();
	virtual void RemoveAllReferenceNodeOverrides(TESObjectREFR * reference);

	virtual void RemoveAllNodeNameOverrides(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName);
	virtual void RemoveNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index);

	virtual OverrideVariant * GetOverride(TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt16 key, UInt8 index);
	virtual OverrideVariant * GetNodeOverride(TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt16 key, UInt8 index);

	virtual void AddRawWeaponOverride(OverrideHandle handle, bool isFemale, bool firstPerson, OverrideHandle weaponHandle, BSFixedString nodeName, OverrideVariant & value);
	virtual void AddWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant & value);
	virtual OverrideVariant * GetWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index);
	virtual void ApplyWeaponOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, NiAVObject * object, bool immediate);

	virtual void RemoveAllWeaponBasedOverrides();
	virtual void RemoveAllReferenceWeaponOverrides(TESObjectREFR * reference);

	virtual void RemoveAllWeaponOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon);
	virtual void RemoveAllWeaponNodeOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName);
	virtual void RemoveWeaponOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt16 key, UInt8 index);

	virtual bool HasWeaponNode(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, bool debug);
	virtual void SetWeaponProperties(OverrideHandle handle, bool immediate);

	virtual void SetWeaponProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value, bool immediate);
	virtual void GetWeaponProperty(TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, OverrideVariant * value);

	// Skin API
	virtual bool LoadSkinOverrides(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual void AddRawSkinOverride(OverrideHandle handle, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value);
	virtual void AddSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant & value);
	virtual OverrideVariant * GetSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index);
	virtual void ApplySkinOverrides(TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, UInt32 slotMask, NiAVObject * object, bool immediate);
	virtual void RemoveAllSkinOverrides(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask);
	virtual void RemoveSkinOverride(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt16 key, UInt8 index);
	virtual void SetSkinProperties(OverrideHandle handle, bool immediate);
	virtual void RemoveAllSkinBasedOverrides();
	virtual void RemoveAllReferenceSkinOverrides(TESObjectREFR * reference);
	void RemoveAllReferenceSkinOverrides(OverrideHandle handle);
	virtual void SetSkinProperty(TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, OverrideVariant * value, bool immediate);
	virtual void GetSkinProperty(TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, OverrideVariant * value);


	virtual void VisitNodes(TESObjectREFR * refr, std::function<void(SKEEFixedString, OverrideVariant&)> functor);
	virtual void VisitSkin(TESObjectREFR * refr, bool isFemale, bool firstPerson, std::function<void(UInt32, OverrideVariant&)> functor);
	virtual void VisitStrings(std::function<void(SKEEFixedString)> functor);

	void Dump();

private:
	ActorRegistrationMapHolder armorData;
	NodeRegistrationMapHolder nodeData;
	WeaponRegistrationMapHolder weaponData;
	SkinRegistrationMapHolder skinData;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};