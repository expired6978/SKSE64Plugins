#include "skse64/PluginAPI.h"

#include "skse64/GameRTTI.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"

#include "skse64/NiExtraData.h"

#include "skse64/PapyrusArgs.h"

#include "PapyrusNiOverride.h"

#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "BodyMorphInterface.h"
#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "NiTransformInterface.h"

#include "ShaderUtilities.h"
#include "Utilities.h"

extern SKSETaskInterface	* g_task;
extern OverrideInterface	g_overrideInterface;
extern OverlayInterface		g_overlayInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern DyeMap				g_dyeMap;
extern NiTransformInterface	g_transformInterface;

extern UInt32	g_numBodyOverlays;
extern UInt32	g_numHandOverlays;
extern UInt32	g_numFeetOverlays;
extern UInt32	g_numFaceOverlays;

extern bool		g_playerOnly;
extern UInt32	g_numSpellBodyOverlays;
extern UInt32	g_numSpellHandOverlays;
extern UInt32	g_numSpellFeetOverlays;
extern UInt32	g_numSpellFaceOverlays;

class MorpedReferenceEventFunctor : public IFunctionArguments
{
public:
	MorpedReferenceEventFunctor(const BSFixedString & a_eventName, TESForm * receiver, TESObjectREFR * actor)
		: m_eventName(a_eventName.data), m_actor(actor), m_receiver(receiver) {}

	virtual bool Copy(Output * dst)
	{
		VMClassRegistry * registry = (*g_skyrimVM)->GetClassRegistry();

		dst->Resize(1);
		PackValue(dst->Get(0), &m_actor, registry);

		return true;
	}

	void Run()
	{
		VMClassRegistry * registry = (*g_skyrimVM)->GetClassRegistry();
		IObjectHandlePolicy	* policy = registry->GetHandlePolicy();
		UInt64 handle = policy->Create(m_receiver->formType, m_receiver);
		registry->QueueEvent(handle, &m_eventName, this);
	}

private:
	BSFixedString	m_eventName;
	TESForm			* m_receiver;
	TESObjectREFR	* m_actor;
};

namespace papyrusNiOverride
{
	void AddOverlays(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(refr)
			g_overlayInterface.AddOverlays(refr);
	}

	bool HasOverlays(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(!refr)
			return false;

		return g_overlayInterface.HasOverlays(refr);
	}

	void RemoveOverlays(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RemoveOverlays(refr);
		}
	}

	void RevertOverlays(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RevertOverlays(refr, false);
		}
	}

	void RevertOverlay(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask)
	{
		if(refr) {
			g_overlayInterface.RevertOverlay(refr, nodeName, armorMask, addonMask, false);
		}
	}

	void RevertHeadOverlays(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RevertHeadOverlays(refr, false);
		}
	}

	void RevertHeadOverlay(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString nodeName, UInt32 partType, UInt32 shaderType)
	{
		if(refr) {
			g_overlayInterface.RevertHeadOverlay(refr, nodeName, partType, shaderType, false);
		}
	}

	bool HasOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.GetOverride(refr, isFemale, armor, addon, nodeName, key, index) != NULL;
	}

	bool HasArmorAddonNode(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, bool debug)
	{
		return g_overrideInterface.HasArmorAddonNode(refr, firstPerson, armor, addon, nodeName, debug);
	}

	void ApplyOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(!refr)
			return;

		UInt64 handle = VirtualMachine::GetHandle(refr, TESObjectREFR::kTypeID);
		g_overrideInterface.SetHandleProperties(handle, false);
	}

	template<typename T>
	void AddOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!armor) {
			_ERROR("%s - Failed to add override for \"%s\" node key: %d. No Armor specified.", __FUNCTION__, nodeName.data, key);
			return;
		}

		if(!addon) {
			_ERROR("%s - Failed to add override for \"%s\" node key: %d. No ArmorAddon specified.", __FUNCTION__, nodeName.data, key);
			return;
		}

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if(value.type == OverrideVariant::kType_None) {
			_ERROR("%s - Failed to pack value for \"%s\" node key: %d. Most likely invalid key for type", __FUNCTION__, nodeName.data, key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.AddOverride(refr, isFemale, armor, addon, nodeName, value);

		UInt8 gender = 0;
		TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
		if(actorBase)
			gender = CALL_MEMBER_FN(actorBase, GetSex)();

		// Applies the properties visually, only if the current gender matches
		if(isFemale == (gender == 1))
			g_overrideInterface.SetArmorAddonProperty(refr, gender == 1, armor, addon, nodeName, &value, false);
	}

	template<typename T>
	T GetOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !armor || !addon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.GetOverride(refr, isFemale, armor, addon, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetArmorAddonProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !armor || !addon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		value.key = key;
		value.index = index;

		g_overrideInterface.GetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	void ApplyNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		UInt64 handle = VirtualMachine::GetHandle(refr, TESObjectREFR::kTypeID);
		return g_overrideInterface.SetHandleNodeProperties(handle, false);
	}

	bool HasNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.GetNodeOverride(refr, isFemale, nodeName, key, index) != NULL;
	}

	template<typename T>
	void AddNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if(value.type == OverrideVariant::kType_None) {
			_ERROR("%s - Failed to pack value for \"%s\" node key: %d. Most likely invalid key for type", __FUNCTION__, nodeName.data, key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.AddNodeOverride(refr, isFemale, nodeName, value);

		UInt8 gender = 0;
		TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
		if(actorBase)
			gender = CALL_MEMBER_FN(actorBase, GetSex)();

		// Applies the properties visually, only if the current gender matches
		if(isFemale == (gender == 1))
			g_overrideInterface.SetNodeProperty(refr, nodeName, &value, false);
	}

	template<typename T>
	T GetNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.GetNodeOverride(refr, isFemale, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetNodeProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		value.key = key;
		value.index = index;

		g_overrideInterface.GetNodeProperty(refr, firstPerson, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	bool HasWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.GetWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index) != NULL;
	}

	bool HasWeaponNode(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, bool debug)
	{
		return g_overrideInterface.HasWeaponNode(refr, firstPerson, weapon, nodeName, debug);
	}

	void ApplyWeaponOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(!refr)
			return;

		UInt64 handle = VirtualMachine::GetHandle(refr, TESObjectREFR::kTypeID);
		g_overrideInterface.SetHandleWeaponProperties(handle, false);
	}

	template<typename T>
	void AddWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index, T dataType, bool persist)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		if(!weapon) {
			_ERROR("%s - Failed to add override for \"%s\" node key: %d. No weapon specified.", __FUNCTION__, nodeName.data, key);
			return;
		}

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if(value.type == OverrideVariant::kType_None) {
			_ERROR("%s - Failed to pack value for \"%s\" node key: %d. Most likely invalid key for type", __FUNCTION__, nodeName.data, key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.AddWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, value);

		UInt8 gender = 0;
		TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
		if(actorBase)
			gender = CALL_MEMBER_FN(actorBase, GetSex)();

		// Applies the properties visually, only if the current gender matches
		if(isFemale == (gender == 1))
			g_overrideInterface.SetWeaponProperty(refr, gender == 1, firstPerson, weapon, nodeName, &value, false);
	}

	template<typename T>
	T GetWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !weapon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.GetWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetWeaponProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !weapon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		value.key = key;
		value.index = index;

		g_overrideInterface.GetWeaponProperty(refr, firstPerson, weapon, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}



	bool HasSkinOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt32 key, UInt32 index)
	{
		if (key > OverrideVariant::kKeyMax)
			return false;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index) != NULL;
	}

	void ApplySkinOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if (!refr)
			return;

		UInt64 handle = VirtualMachine::GetHandle(refr, TESObjectREFR::kTypeID);
		g_overrideInterface.SetHandleSkinProperties(handle, false);
	}

	template<typename T>
	void AddSkinOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt32 key, UInt32 index, T dataType, bool persist)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		if (!slotMask) {
			_ERROR("%s - Failed to add override for \"%d\" slot key: %d. No weapon specified.", __FUNCTION__, slotMask, key);
			return;
		}

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if (value.type == OverrideVariant::kType_None) {
			_ERROR("%s - Failed to pack value for \"%d\" slot key: %d. Most likely invalid key for type", __FUNCTION__, slotMask, key);
			return;
		}

		// Adds the property to the map
		if (persist)
			g_overrideInterface.AddSkinOverride(refr, isFemale, firstPerson, slotMask, value);

		UInt8 gender = 0;
		TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
		if (actorBase)
			gender = CALL_MEMBER_FN(actorBase, GetSex)();

		// Applies the properties visually, only if the current gender matches
		if (isFemale == (gender == 1))
			g_overrideInterface.SetSkinProperty(refr, gender == 1, firstPerson, slotMask, &value, false);
	}

	template<typename T>
	T GetSkinOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt32 key, UInt32 index)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
		if (value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetSkinProperty(StaticFunctionTag* base, TESObjectREFR * refr, bool firstPerson, UInt32 slotMask, UInt32 key, UInt32 index)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		value.key = key;
		value.index = index;

		g_overrideInterface.GetSkinProperty(refr, firstPerson, slotMask, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	void RemoveAllOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.RemoveAllOverrides();
	}

	void RemoveAllReferenceOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(!refr)
			return;
		
		g_overrideInterface.RemoveAllReferenceOverrides(refr);
	}

	void RemoveAllArmorOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor)
	{
		g_overrideInterface.RemoveAllArmorOverrides(refr, isFemale, armor);
	}

	void RemoveAllArmorAddonOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon)
	{
		g_overrideInterface.RemoveAllArmorAddonOverrides(refr, isFemale, armor, addon);
	}

	void RemoveAllArmorAddonNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName)
	{
		g_overrideInterface.RemoveAllArmorAddonNodeOverrides(refr, isFemale, armor, addon, nodeName);
	}

	void RemoveArmorAddonOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, TESObjectARMO * armor, TESObjectARMA * addon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.RemoveArmorAddonOverride(refr, isFemale, armor, addon, nodeName, key, index);
	}

	void RemoveAllNodeOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.RemoveAllNodeOverrides();
	}

	void RemoveAllReferenceNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		g_overrideInterface.RemoveAllReferenceNodeOverrides(refr);
	}

	void RemoveAllNodeNameOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName)
	{
		g_overrideInterface.RemoveAllNodeNameOverrides(refr, isFemale, nodeName);
	}

	void RemoveNodeOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.RemoveNodeOverride(refr, isFemale, nodeName, key, index);
	}

	void RemoveAllWeaponBasedOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.RemoveAllWeaponBasedOverrides();
	}

	void RemoveAllReferenceWeaponOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if(!refr)
			return;

		g_overrideInterface.RemoveAllReferenceWeaponOverrides(refr);
	}

	void RemoveAllWeaponOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon)
	{
		g_overrideInterface.RemoveAllWeaponOverrides(refr, isFemale, firstPerson, weapon);
	}

	void RemoveAllWeaponNodeOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName)
	{
		g_overrideInterface.RemoveAllWeaponNodeOverrides(refr, isFemale, firstPerson, weapon, nodeName);
	}

	void RemoveWeaponOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, TESObjectWEAP * weapon, BSFixedString nodeName, UInt32 key, UInt32 index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.RemoveWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
	}

	void RemoveAllSkinBasedOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.RemoveAllSkinBasedOverrides();
	}

	void RemoveAllReferenceSkinOverrides(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_overrideInterface.RemoveAllReferenceSkinOverrides(refr);
	}

	void RemoveAllSkinOverrides(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask)
	{
		g_overrideInterface.RemoveAllSkinOverrides(refr, isFemale, firstPerson, slotMask);
	}

	void RemoveSkinOverride(StaticFunctionTag* base, TESObjectREFR * refr, bool isFemale, bool firstPerson, UInt32 slotMask, UInt32 key, UInt32 index)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.RemoveSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
	}

	UInt32 GetNumBodyOverlays(StaticFunctionTag* base)
	{
		return g_numBodyOverlays;
	}

	UInt32 GetNumHandOverlays(StaticFunctionTag* base)
	{
		return g_numHandOverlays;
	}

	UInt32 GetNumFeetOverlays(StaticFunctionTag* base)
	{
		return g_numFeetOverlays;
	}

	UInt32 GetNumFaceOverlays(StaticFunctionTag* base)
	{
		return g_numFaceOverlays;
	}

	UInt32 GetNumSpellBodyOverlays(StaticFunctionTag* base)
	{
		return g_numSpellBodyOverlays;
	}

	UInt32 GetNumSpellHandOverlays(StaticFunctionTag* base)
	{
		return g_numSpellHandOverlays;
	}

	UInt32 GetNumSpellFeetOverlays(StaticFunctionTag* base)
	{
		return g_numSpellFeetOverlays;
	}

	UInt32 GetNumSpellFaceOverlays(StaticFunctionTag* base)
	{
		return g_numSpellFaceOverlays;
	}

	bool HasBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName)
	{
		return g_bodyMorphInterface.HasBodyMorph(refr, morphName, keyName);
	}

	void SetBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName, float value)
	{
		g_bodyMorphInterface.SetMorph(refr, morphName, keyName, value);
	}

	float GetBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName)
	{
		return g_bodyMorphInterface.GetMorph(refr, morphName, keyName);
	}

	void ClearBodyMorph(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName, BSFixedString keyName)
	{
		g_bodyMorphInterface.ClearMorph(refr, morphName, keyName);
	}

	bool HasBodyMorphKey(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString keyName)
	{
		return g_bodyMorphInterface.HasBodyMorphKey(refr, keyName);
	}

	void ClearBodyMorphKeys(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString keyName)
	{
		g_bodyMorphInterface.ClearBodyMorphKeys(refr, keyName);
	}

	bool HasBodyMorphName(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName)
	{
		return g_bodyMorphInterface.HasBodyMorphName(refr, morphName);
	}

	void ClearBodyMorphNames(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName)
	{
		g_bodyMorphInterface.ClearBodyMorphNames(refr, morphName);
	}

	void ClearMorphs(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		g_bodyMorphInterface.ClearMorphs(refr);
	}

	VMResultArray<TESObjectREFR*> GetMorphedReferences(StaticFunctionTag* base)
	{
		VMResultArray<TESObjectREFR*> results;

		class Visitor : public BodyMorphInterface::ActorVisitor
		{
		public:
			Visitor(VMResultArray<TESObjectREFR*> * res) : result(res) { }

			virtual void Visit(TESObjectREFR * ref) override
			{
				result->push_back(ref);
			}
		private:
			VMResultArray<TESObjectREFR*> * result;
		};

		Visitor visitor(&results);
		g_bodyMorphInterface.VisitActors(visitor);
		return results;
	}

	void ForEachMorphedReference(StaticFunctionTag* base, BSFixedString eventName, TESForm * receiver)
	{
		class Visitor : public BodyMorphInterface::ActorVisitor
		{
		public:
			Visitor(BSFixedString eventName, TESForm * receiver) : m_eventName(eventName), m_receiver(receiver) { }

			virtual void Visit(TESObjectREFR * refr) override
			{
				MorpedReferenceEventFunctor fn(m_eventName, m_receiver, refr);
				fn.Run();
			}
		private:
			BSFixedString m_eventName;
			TESForm * m_receiver;
		};

		Visitor visitor(eventName, receiver);
		g_bodyMorphInterface.VisitActors(visitor);
	}

	void UpdateModelWeight(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		g_bodyMorphInterface.UpdateModelWeight(refr);
	}

	void EnableTintTextureCache(StaticFunctionTag* base)
	{
		g_tintMaskInterface.ManageTints();
	}

	void ReleaseTintTextureCache(StaticFunctionTag* base)
	{
		g_tintMaskInterface.ReleaseTints();
	}

	UInt32 GetItemUniqueID(StaticFunctionTag* base, TESObjectREFR * refr, UInt32 weaponSlot, UInt32 slotMask, bool makeUnique)
	{
		if (!refr)
			return 0;

		ModifiedItemIdentifier identifier;
		identifier.SetSlotMask(slotMask, weaponSlot);
		return g_itemDataInterface.GetItemUniqueID(refr, identifier, makeUnique);
	}

	UInt32 GetObjectUniqueID(StaticFunctionTag* base, TESObjectREFR * refr, bool makeUnique)
	{
		if (!refr)
			return 0;

		if (Actor * actor = DYNAMIC_CAST(refr, TESForm, Actor)) {
			_WARNING("%s - cannot be called on an actor", __FUNCTION__);
			return 0;
		}

		ModifiedItemIdentifier identifier;
		identifier.SetSelf();
		return g_itemDataInterface.GetItemUniqueID(refr, identifier, makeUnique);
	}

	TESForm * GetFormFromUniqueID(StaticFunctionTag* base, UInt32 uniqueID)
	{
		return g_itemDataInterface.GetFormFromUniqueID(uniqueID);
	}

	TESForm * GetOwnerOfUniqueID(StaticFunctionTag* base, UInt32 uniqueID)
	{
		return g_itemDataInterface.GetOwnerOfUniqueID(uniqueID);
	}

	void SetItemDyeColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 maskIndex, UInt32 color)
	{
		g_itemDataInterface.SetItemTextureLayerColor(uniqueID, 0, maskIndex, color);
	}

	UInt32 GetItemDyeColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 maskIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerColor(uniqueID, 0, maskIndex);
	}

	void ClearItemDyeColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 maskIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerColor(uniqueID, 0, maskIndex);
	}

	void UpdateItemDyeColor(StaticFunctionTag* base, TESObjectREFR * refr, UInt32 uniqueID)
	{
		UpdateItemTextureLayers(base, refr, uniqueID);
	}

	void SetItemTextureLayerColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 color)
	{
		g_itemDataInterface.SetItemTextureLayerColor(uniqueID, textureIndex, layerIndex, color);
	}

	UInt32 GetItemTextureLayerColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerColor(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerColor(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerColor(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerType(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 color)
	{
		g_itemDataInterface.SetItemTextureLayerType(uniqueID, textureIndex, layerIndex, color);
	}

	UInt32 GetItemTextureLayerType(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerType(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerType(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerType(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerTexture(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, BSFixedString texture)
	{
		g_itemDataInterface.SetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex, texture);
	}

	BSFixedString GetItemTextureLayerTexture(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerTexture(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerTexture(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerBlendMode(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, BSFixedString texture)
	{
		g_itemDataInterface.SetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex, texture);
	}

	BSFixedString GetItemTextureLayerBlendMode(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerBlendMode(StaticFunctionTag* base, UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex);
	}

	void UpdateItemTextureLayers(StaticFunctionTag* base, TESObjectREFR * refr, UInt32 uniqueID)
	{
		if (Actor * actor = DYNAMIC_CAST(refr, TESForm, Actor)) {
			ModifiedItemIdentifier identifier;
			identifier.SetRankID(uniqueID);
			g_task->AddTask(new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, false));
		}
	}

	bool IsFormDye(StaticFunctionTag* base, TESForm * form)
	{
		return form ? g_dyeMap.IsValidDye(form) : false;
	}

	UInt32 GetFormDyeColor(StaticFunctionTag* base, TESForm * form)
	{
		return form ? g_dyeMap.GetDyeColor(form) : 0;
	}

	void RegisterFormDyeColor(StaticFunctionTag* base, TESForm * form, UInt32 color)
	{
		if (form) {
			g_dyeMap.RegisterDyeForm(form, color);
		}
	}

	void UnregisterFormDyeColor(StaticFunctionTag* base, TESForm * form)
	{
		if (form) {
			g_dyeMap.UnregisterDyeForm(form);
		}
	}



	bool HasNodeTransformPosition(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		NiTransform transform;
		return g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
	}

	void AddNodeTransformPosition(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name, VMArray<float> dataType)
	{
		if (!refr)
			return;

		if (name == BSFixedString("")) {
			_ERROR("%s - Cannot add empty key for node \"%s\".", __FUNCTION__, node.data);
			return;
		}

		if (dataType.Length() != 3) {
			_ERROR("%s - Failed to unpack array value for \"%s\". Invalid array size must be 3", __FUNCTION__, node.data);
			return;
		}

		float pos[3];
		OverrideVariant posV[3];
		for (UInt32 i = 0; i < 3; i++) {
			dataType.Get(&pos[i], i);
			PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &pos[i]);
			g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, name, posV[i]);
		}		
	}

	
	VMResultArray<float> GetNodeTransformPosition(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		VMResultArray<float> position;
		if (!refr)
			return position;
			
		position.resize(3, 0.0);
		NiTransform transform;
		bool ret = g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
		position[0] = transform.pos.x;
		position[1] = transform.pos.y;
		position[2] = transform.pos.z;
		return position;
	}

	bool RemoveNodeTransformPosition(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		bool ret = false;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 0))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 1))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 2))
			ret = true;
		return ret;
	}

	bool HasNodeTransformScale(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		NiTransform transform;
		return g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, &transform);
	}

	void AddNodeTransformScale(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name, float fScale)
	{
		if (!refr)
			return;

		if (name == BSFixedString("")) {
			_ERROR("%s - Cannot add empty key for node \"%s\".", __FUNCTION__, node.data);
			return;
		}

		OverrideVariant scale;
		PackValue<float>(&scale, OverrideVariant::kParam_NodeTransformScale, 0, &fScale);
		g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, name, scale);
	}

	float GetNodeTransformScale(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return 0;

		NiTransform transform;
		g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, &transform);
		return transform.scale;
	}

	bool RemoveNodeTransformScale(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, 0);
	}


	bool HasNodeTransformRotation(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		NiTransform transform;
		return g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);
	}

	void AddNodeTransformRotation(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name, VMArray<float> dataType)
	{
		if (!refr)
			return;

		NiMatrix33 rotation;
		if (dataType.Length() == 3) {
			float heading, attitude, bank;
			
			dataType.Get(&heading, 0);
			dataType.Get(&attitude, 1);
			dataType.Get(&bank, 2);

			heading *= MATH_PI / 180;
			attitude *= MATH_PI / 180;
			bank *= MATH_PI / 180;

			rotation.SetEulerAngles(heading, attitude, bank);
		}
		else if (dataType.Length() == 9) {
			for (UInt32 i = 0; i < 9; i++)
				dataType.Get(&rotation.arr[i], i);
		}
		else {
			_ERROR("%s - Failed to unpack array value for \"%s\". Invalid array size must be 3 or 9", __FUNCTION__, node.data);
			return;
		}

		OverrideVariant rotV[9];
		for (UInt32 i = 0; i < 9; i++) {
			PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &rotation.arr[i]);
			g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, name, rotV[i]);
		}
	}


	VMResultArray<float> GetNodeTransformRotation(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name, UInt32 type)
	{
		VMResultArray<float> rotation;
		if (!refr)
			return rotation;

		NiTransform transform;
		bool ret = g_transformInterface.GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);

		switch (type) {
			case 0:
			{
				rotation.resize(3, 0.0);
				float heading, attitude, bank;
				transform.rot.GetEulerAngles(&heading, &attitude, &bank);

				// Radians to degrees
				heading *= 180 / MATH_PI;
				attitude *= 180 / MATH_PI;
				bank *= 180 / MATH_PI;

				rotation[0] = heading;
				rotation[1] = attitude;
				rotation[2] = bank;
				break;
			}
			case 1:
			{
				rotation.resize(9, 0.0);
				for (UInt32 i = 0; i < 9; i++) {
					rotation[i] = ((float*)transform.rot.data)[i];
				}
				break;
			}
			default:
			{
				_ERROR("%s - Failed to create array value for \"%s\". Invalid type %d", __FUNCTION__, node.data, type);
				break;
			}
		}


		return rotation;
	}
	
	bool RemoveNodeTransformRotation(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString name)
	{
		if (!refr)
			return false;

		bool ret = false;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 0))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 1))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 2))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 3))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 4))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 5))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 6))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 7))
			ret = true;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 8))
			ret = true;

		return ret;
	}

	void UpdateAllReferenceTransforms(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_transformInterface.UpdateNodeAllTransforms(refr);
	}

	void RemoveAllTransforms(StaticFunctionTag* base)
	{
		g_transformInterface.Revert();
	}

	void RemoveAllReferenceTransforms(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_transformInterface.RemoveAllReferenceTransforms(refr);
	}

	void UpdateNodeTransform(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node)
	{
		if (!refr)
			return;

		UInt8 gender = isFemale ? 1 : 0;
		UInt8 realGender = 0;
		TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
		if (actorBase)
			realGender = CALL_MEMBER_FN(actorBase, GetSex)();

		if (gender != realGender)
			return;

		g_transformInterface.UpdateNodeTransforms(refr, isFirstPerson, isFemale, node);
	}

	void SetNodeDestination(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node, BSFixedString destination)
	{
		if (!refr)
			return;

		OverrideVariant value;
		SKEEFixedString str = destination;
		PackValue(&value, OverrideVariant::kParam_NodeDestination, -1, &str);
		g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, "NodeDestination", value);
	}

	BSFixedString GetNodeDestination(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node)
	{
		BSFixedString ret("");
		if (!refr)
			return ret;

		OverrideVariant value = g_transformInterface.GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "NodeDestination", OverrideVariant::kParam_NodeDestination, -1);
		SKEEFixedString str;
		UnpackValue(&str, &value);
		return str;
	}

	bool RemoveNodeDestination(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node)
	{
		if (!refr)
			return false;

		bool ret = false;
		if (g_transformInterface.RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node, "NodeDestination", OverrideVariant::kParam_NodeDestination, -1))
			ret = true;
		return ret;
	}

	float GetInverseTransform(StaticFunctionTag* base, VMArray<float> position, VMArray<float> rotation, float scale)
	{
		NiTransform transform, ixForm;
		transform.scale = scale;
		if (position.Length() == 3) {
			position.Get(&transform.pos.x, 0);
			position.Get(&transform.pos.y, 1);
			position.Get(&transform.pos.z, 2);
		}
		if (rotation.Length() == 9) {
			for (UInt32 i = 0; i < 9; i++)
				rotation.Get((float*)transform.rot.data[i], i);
		}
		if (rotation.Length() == 3) {
			float heading, attitude, bank;
			rotation.Get(&heading, 0);
			rotation.Get(&attitude, 1);
			rotation.Get(&bank, 2);

			transform.rot.SetEulerAngles(heading, attitude, bank);
		}
		transform.Invert(ixForm);
		if (position.Length() == 3) {
			position.Set(&ixForm.pos.x, 0);
			position.Set(&ixForm.pos.y, 1);
			position.Set(&ixForm.pos.z, 2);
		}
		if (rotation.Length() == 9) {
			for (UInt32 i = 0; i < 9; i++)
				rotation.Set((float*)ixForm.rot.data[i], i);
		}
		if (rotation.Length() == 3) {
			float heading, attitude, bank;
			
			ixForm.rot.GetEulerAngles(&heading, &attitude, &bank);

			// Radians to degrees
			heading *= 180 / MATH_PI;
			attitude *= 180 / MATH_PI;
			bank *= 180 / MATH_PI;

			rotation.Set(&heading, 0);
			rotation.Set(&attitude, 1);
			rotation.Set(&bank, 2);
		}

		return ixForm.scale;
	}

	VMResultArray<BSFixedString> GetNodeTransformNames(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale)
	{
		VMResultArray<BSFixedString> result;
		if (!refr)
			return result;

		g_transformInterface.VisitNodes(refr, isFirstPerson, isFemale, [&](BSFixedString key, OverrideRegistration<StringTableItem>*value)
		{
			result.push_back(key);
			return false;
		});

		return result;
	}

	VMResultArray<BSFixedString> GetNodeTransformKeys(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, bool isFemale, BSFixedString node)
	{
		VMResultArray<BSFixedString> result;
		if (!refr)
			return result;

		g_transformInterface.VisitNodes(refr, isFirstPerson, isFemale, [&](BSFixedString nodeName, OverrideRegistration<StringTableItem>*value)
		{
			if (nodeName == node) {
				for (auto key : *value)
					result.push_back(*key.first);
				return true;
			}

			return false;
		});

		return result;
	}

	VMResultArray<BSFixedString> GetMorphNames(StaticFunctionTag* base, TESObjectREFR * refr)
	{
		VMResultArray<BSFixedString> result;
		if (!refr)
			return result;

		class Visitor : public BodyMorphInterface::MorphVisitor
		{
		public:
			Visitor(VMResultArray<BSFixedString> * res) : result(res) { }

			virtual void Visit(TESObjectREFR * ref, const char* name) override
			{
				result->push_back(name);
			}
		private:
			VMResultArray<BSFixedString> * result;
		};

		Visitor visitor(&result);
		g_bodyMorphInterface.VisitMorphs(refr, visitor);
		return result;
	}

	VMResultArray<BSFixedString> GetMorphKeys(StaticFunctionTag* base, TESObjectREFR * refr, BSFixedString morphName)
	{
		VMResultArray<BSFixedString> result;
		if (!refr)
			return result;

		class Visitor : public BodyMorphInterface::MorphKeyVisitor
		{
		public:
			Visitor(VMResultArray<BSFixedString> * res) : result(res) { }

			virtual void Visit(const char* name, float values) override
			{
				result->push_back(name);
			}
		private:
			VMResultArray<BSFixedString> * result;
		};

		Visitor visitor(&result);
		g_bodyMorphInterface.VisitKeys(refr, morphName, visitor);

		return result;
	}

	template<typename T>
	void GetBaseExtraData(NiExtraData * extraData, T & data)
	{
		// No implementation
	}

	template<typename T>
	void ExtraDataInitializer(T & data)
	{

	}

	template<> void ExtraDataInitializer(float & data)
	{
		data = 0.0;
	}

	template<> void ExtraDataInitializer(bool & data)
	{
		data = false;
	}

	template<> void ExtraDataInitializer(SInt32 & data)
	{
		data = 0;
	}

	template<> void ExtraDataInitializer(BSFixedString & data)
	{
		data = BSFixedString("");
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, float & data)
	{
		NiFloatExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiFloatExtraData);
		if (pExtraData) {
			data = pExtraData->m_data;
		}
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, VMResultArray<float> & data)
	{
		NiFloatsExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiFloatsExtraData);
		if (pExtraData) {
			for (UInt32 i = 0; i < pExtraData->m_size; i++)
				data.push_back(pExtraData->m_data[i]);
		}
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, SInt32 & data)
	{
		NiIntegerExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiIntegerExtraData);
		if (pExtraData) {
			data = pExtraData->m_data;
		}
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, VMResultArray<SInt32> & data)
	{
		NiIntegersExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiIntegersExtraData);
		if (pExtraData) {
			for (UInt32 i = 0; i < pExtraData->m_size; i++)
				data.push_back(pExtraData->m_data[i]);
		}
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, BSFixedString & data)
	{
		NiStringExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiStringExtraData);
		if (pExtraData) {
			data = pExtraData->m_pString;
		}
	}

	template<> void GetBaseExtraData(NiExtraData * extraData, VMResultArray<BSFixedString> & data)
	{
		NiStringsExtraData * pExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiStringsExtraData);
		if (pExtraData) {
			for (UInt32 i = 0; i < pExtraData->m_size; i++)
				data.push_back(pExtraData->m_data[i]);
		}
	}


	template<typename T>
	T GetExtraData(StaticFunctionTag* base, TESObjectREFR * refr, bool isFirstPerson, BSFixedString node, BSFixedString dataName)
	{
		T value;
		ExtraDataInitializer(value);
		if (!refr)
			return value;

		NiNode * skeleton = refr->GetNiRootNode(isFirstPerson ? 1 : 0);
		if (skeleton) {
			skeleton->IncRef();

			NiAVObject * object = skeleton->GetObjectByName(&node.data);
			if (object) {
				object->IncRef();

				NiExtraData * extraData = object->GetExtraData(dataName);
				if (extraData) {
					extraData->IncRef();
					GetBaseExtraData<T>(extraData, value);
					extraData->DecRef();
				}

				object->DecRef();
			}

			skeleton->DecRef();
		}

		return value;
	}
}

#include "skse64/PapyrusVM.h"
#include "skse64/PapyrusNativeFunctions.h"

void papyrusNiOverride::RegisterFuncs(VMClassRegistry* registry)
{
	// Overlay Data
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumBodyOverlays", "NiOverride", papyrusNiOverride::GetNumBodyOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumHandOverlays", "NiOverride", papyrusNiOverride::GetNumHandOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumFeetOverlays", "NiOverride", papyrusNiOverride::GetNumFeetOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumFaceOverlays", "NiOverride", papyrusNiOverride::GetNumFaceOverlays, registry));

	// Spell Overlays Enabled
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumSpellBodyOverlays", "NiOverride", papyrusNiOverride::GetNumSpellBodyOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumSpellHandOverlays", "NiOverride", papyrusNiOverride::GetNumSpellHandOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumSpellFeetOverlays", "NiOverride", papyrusNiOverride::GetNumSpellFeetOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, UInt32>("GetNumSpellFaceOverlays", "NiOverride", papyrusNiOverride::GetNumSpellFaceOverlays, registry));


	// Overlays
	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("AddOverlays", "NiOverride", papyrusNiOverride::AddOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, bool, TESObjectREFR*>("HasOverlays", "NiOverride", papyrusNiOverride::HasOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RemoveOverlays", "NiOverride", papyrusNiOverride::RemoveOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RevertOverlays", "NiOverride", papyrusNiOverride::RevertOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, BSFixedString, UInt32, UInt32>("RevertOverlay", "NiOverride", papyrusNiOverride::RevertOverlay, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RevertHeadOverlays", "NiOverride", papyrusNiOverride::RevertHeadOverlays, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, BSFixedString, UInt32, UInt32>("RevertHeadOverlay", "NiOverride", papyrusNiOverride::RevertHeadOverlay, registry));


	// Armor Overrides
	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("HasOverride", "NiOverride", papyrusNiOverride::HasOverride, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32, float, bool>("AddOverrideFloat", "NiOverride", papyrusNiOverride::AddOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32, UInt32, bool>("AddOverrideInt", "NiOverride", papyrusNiOverride::AddOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32, bool, bool>("AddOverrideBool", "NiOverride", papyrusNiOverride::AddOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32, BSFixedString, bool>("AddOverrideString", "NiOverride", papyrusNiOverride::AddOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32, BGSTextureSet*, bool>("AddOverrideTextureSet", "NiOverride", papyrusNiOverride::AddOverride<BGSTextureSet*>, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("ApplyOverrides", "NiOverride", papyrusNiOverride::ApplyOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, bool>("HasArmorAddonNode", "NiOverride", papyrusNiOverride::HasArmorAddonNode, registry));

	// Get Armor Overrides
	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, float, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetOverrideFloat", "NiOverride", papyrusNiOverride::GetOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, UInt32, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetOverrideInt", "NiOverride", papyrusNiOverride::GetOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetOverrideBool", "NiOverride", papyrusNiOverride::GetOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetOverrideString", "NiOverride", papyrusNiOverride::GetOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, BGSTextureSet*, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetOverrideTextureSet", "NiOverride", papyrusNiOverride::GetOverride<BGSTextureSet*>, registry));

	// Get Armor Properties
	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, float, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetPropertyFloat", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<float>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, UInt32, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetPropertyInt", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetPropertyBool", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("GetPropertyString", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<BSFixedString>, registry));

	// Node Overrides
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("HasNodeOverride", "NiOverride", papyrusNiOverride::HasNodeOverride, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32, float, bool>("AddNodeOverrideFloat", "NiOverride", papyrusNiOverride::AddNodeOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32, UInt32, bool>("AddNodeOverrideInt", "NiOverride", papyrusNiOverride::AddNodeOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32, bool, bool>("AddNodeOverrideBool", "NiOverride", papyrusNiOverride::AddNodeOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32, BSFixedString, bool>("AddNodeOverrideString", "NiOverride", papyrusNiOverride::AddNodeOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32, BGSTextureSet*, bool>("AddNodeOverrideTextureSet", "NiOverride", papyrusNiOverride::AddNodeOverride<BGSTextureSet*>, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("ApplyNodeOverrides", "NiOverride", papyrusNiOverride::ApplyNodeOverrides, registry));

	// Get Node Overrides
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, float, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodeOverrideFloat", "NiOverride", papyrusNiOverride::GetNodeOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, UInt32, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodeOverrideInt", "NiOverride", papyrusNiOverride::GetNodeOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodeOverrideBool", "NiOverride", papyrusNiOverride::GetNodeOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodeOverrideString", "NiOverride", papyrusNiOverride::GetNodeOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, BGSTextureSet*, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodeOverrideTextureSet", "NiOverride", papyrusNiOverride::GetNodeOverride<BGSTextureSet*>, registry));

	// Get Node Properties
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, float, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodePropertyFloat", "NiOverride", papyrusNiOverride::GetNodeProperty<float>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, UInt32, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodePropertyInt", "NiOverride", papyrusNiOverride::GetNodeProperty<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodePropertyBool", "NiOverride", papyrusNiOverride::GetNodeProperty<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("GetNodePropertyString", "NiOverride", papyrusNiOverride::GetNodeProperty<BSFixedString>, registry));


	// Weapon Overrides
	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, bool, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("HasWeaponOverride", "NiOverride", papyrusNiOverride::HasWeaponOverride, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32, float, bool>("AddWeaponOverrideFloat", "NiOverride", papyrusNiOverride::AddWeaponOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32, UInt32, bool>("AddWeaponOverrideInt", "NiOverride", papyrusNiOverride::AddWeaponOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32, bool, bool>("AddWeaponOverrideBool", "NiOverride", papyrusNiOverride::AddWeaponOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32, BSFixedString, bool>("AddWeaponOverrideString", "NiOverride", papyrusNiOverride::AddWeaponOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction9<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32, BGSTextureSet*, bool>("AddWeaponOverrideTextureSet", "NiOverride", papyrusNiOverride::AddWeaponOverride<BGSTextureSet*>, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("ApplyWeaponOverrides", "NiOverride", papyrusNiOverride::ApplyWeaponOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectWEAP*, BSFixedString, bool>("HasWeaponNode", "NiOverride", papyrusNiOverride::HasWeaponNode, registry));

	// Get Weapon Overrides
	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, float, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponOverrideFloat", "NiOverride", papyrusNiOverride::GetWeaponOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, UInt32, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponOverrideInt", "NiOverride", papyrusNiOverride::GetWeaponOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, bool, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponOverrideBool", "NiOverride", papyrusNiOverride::GetWeaponOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponOverrideString", "NiOverride", papyrusNiOverride::GetWeaponOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, BGSTextureSet*, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponOverrideTextureSet", "NiOverride", papyrusNiOverride::GetWeaponOverride<BGSTextureSet*>, registry));

	// Get Weapon Properties
	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, float, TESObjectREFR*, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponPropertyFloat", "NiOverride", papyrusNiOverride::GetWeaponProperty<float>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, UInt32, TESObjectREFR*, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponPropertyInt", "NiOverride", papyrusNiOverride::GetWeaponProperty<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, bool, TESObjectREFR*, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponPropertyBool", "NiOverride", papyrusNiOverride::GetWeaponProperty<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("GetWeaponPropertyString", "NiOverride", papyrusNiOverride::GetWeaponProperty<BSFixedString>, registry));


	// Skin Overrides
	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, bool, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("HasSkinOverride", "NiOverride", papyrusNiOverride::HasSkinOverride, registry));

	registry->RegisterFunction(
		new NativeFunction8<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32, float, bool>("AddSkinOverrideFloat", "NiOverride", papyrusNiOverride::AddSkinOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction8<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32, UInt32, bool>("AddSkinOverrideInt", "NiOverride", papyrusNiOverride::AddSkinOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction8<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32, bool, bool>("AddSkinOverrideBool", "NiOverride", papyrusNiOverride::AddSkinOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction8<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32, BSFixedString, bool>("AddSkinOverrideString", "NiOverride", papyrusNiOverride::AddSkinOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction8<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32, BGSTextureSet*, bool>("AddSkinOverrideTextureSet", "NiOverride", papyrusNiOverride::AddSkinOverride<BGSTextureSet*>, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("ApplySkinOverrides", "NiOverride", papyrusNiOverride::ApplySkinOverrides, registry));


	// Get Skin Overrides
	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, float, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("GetSkinOverrideFloat", "NiOverride", papyrusNiOverride::GetSkinOverride<float>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, UInt32, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("GetSkinOverrideInt", "NiOverride", papyrusNiOverride::GetSkinOverride<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, bool, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("GetSkinOverrideBool", "NiOverride", papyrusNiOverride::GetSkinOverride<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("GetSkinOverrideString", "NiOverride", papyrusNiOverride::GetSkinOverride<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, BGSTextureSet*, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("GetSkinOverrideTextureSet", "NiOverride", papyrusNiOverride::GetSkinOverride<BGSTextureSet*>, registry));

	// Get Skin Properties
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, float, TESObjectREFR*, bool, UInt32, UInt32, UInt32>("GetSkinPropertyFloat", "NiOverride", papyrusNiOverride::GetSkinProperty<float>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, UInt32, TESObjectREFR*, bool, UInt32, UInt32, UInt32>("GetSkinPropertyInt", "NiOverride", papyrusNiOverride::GetSkinProperty<UInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR*, bool, UInt32, UInt32, UInt32>("GetSkinPropertyBool", "NiOverride", papyrusNiOverride::GetSkinProperty<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, BSFixedString, TESObjectREFR*, bool, UInt32, UInt32, UInt32>("GetSkinPropertyString", "NiOverride", papyrusNiOverride::GetSkinProperty<BSFixedString>, registry));



	// Remove functions
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("RemoveAllOverrides", "NiOverride", papyrusNiOverride::RemoveAllOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RemoveAllReferenceOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*>("RemoveAllArmorOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*>("RemoveAllArmorAddonOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorAddonOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString>("RemoveAllArmorAddonNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorAddonNodeOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, TESObjectARMO*, TESObjectARMA*, BSFixedString, UInt32, UInt32>("RemoveOverride", "NiOverride", papyrusNiOverride::RemoveArmorAddonOverride, registry));

	// Node Remove functions
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("RemoveAllNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllNodeOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RemoveAllReferenceNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceNodeOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString>("RemoveAllNodeNameOverrides", "NiOverride", papyrusNiOverride::RemoveAllNodeNameOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, void, TESObjectREFR*, bool, BSFixedString, UInt32, UInt32>("RemoveNodeOverride", "NiOverride", papyrusNiOverride::RemoveNodeOverride, registry));

	// Remove Weapon functions
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("RemoveAllWeaponBasedOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponBasedOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RemoveAllReferenceWeaponOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceWeaponOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*>("RemoveAllWeaponOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString>("RemoveAllWeaponNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponNodeOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction7<StaticFunctionTag, void, TESObjectREFR*, bool, bool, TESObjectWEAP*, BSFixedString, UInt32, UInt32>("RemoveWeaponOverride", "NiOverride", papyrusNiOverride::RemoveWeaponOverride, registry));


	// Remove Skin functions
	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("RemoveAllSkinBasedOverrides", "NiOverride", papyrusNiOverride::RemoveAllSkinBasedOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("RemoveAllReferenceSkinOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceSkinOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32>("RemoveAllSkinOverrides", "NiOverride", papyrusNiOverride::RemoveAllSkinOverrides, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, void, TESObjectREFR*, bool, bool, UInt32, UInt32, UInt32>("RemoveSkinOverride", "NiOverride", papyrusNiOverride::RemoveSkinOverride, registry));


	// Body Morph Manipulation
	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, bool, TESObjectREFR*, BSFixedString, BSFixedString>("HasBodyMorph", "NiOverride", papyrusNiOverride::HasBodyMorph, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR*, BSFixedString, BSFixedString, float>("SetBodyMorph", "NiOverride", papyrusNiOverride::SetBodyMorph, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, float, TESObjectREFR*, BSFixedString, BSFixedString>("GetBodyMorph", "NiOverride", papyrusNiOverride::GetBodyMorph, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, TESObjectREFR*, BSFixedString, BSFixedString>("ClearBodyMorph", "NiOverride", papyrusNiOverride::ClearBodyMorph, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, bool, TESObjectREFR*, BSFixedString>("HasBodyMorphKey", "NiOverride", papyrusNiOverride::HasBodyMorphKey, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, TESObjectREFR*, BSFixedString>("ClearBodyMorphKeys", "NiOverride", papyrusNiOverride::ClearBodyMorphKeys, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, bool, TESObjectREFR*, BSFixedString>("HasBodyMorphName", "NiOverride", papyrusNiOverride::HasBodyMorphName, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, TESObjectREFR*, BSFixedString>("ClearBodyMorphNames", "NiOverride", papyrusNiOverride::ClearBodyMorphNames, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("ClearMorphs", "NiOverride", papyrusNiOverride::ClearMorphs, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR*>("UpdateModelWeight", "NiOverride", papyrusNiOverride::UpdateModelWeight, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, VMResultArray<BSFixedString>, TESObjectREFR*>("GetMorphNames", "NiOverride", papyrusNiOverride::GetMorphNames, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, VMResultArray<BSFixedString>, TESObjectREFR*, BSFixedString>("GetMorphKeys", "NiOverride", papyrusNiOverride::GetMorphKeys, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, VMResultArray<TESObjectREFR*>>("GetMorphedReferences", "NiOverride", papyrusNiOverride::GetMorphedReferences, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, BSFixedString, TESForm*>("ForEachMorphedReference", "NiOverride", papyrusNiOverride::ForEachMorphedReference, registry));


	// Unique Item manipulation
	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, UInt32, TESObjectREFR*, UInt32, UInt32, bool>("GetItemUniqueID", "NiOverride", papyrusNiOverride::GetItemUniqueID, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, UInt32, TESObjectREFR*, bool>("GetObjectUniqueID", "NiOverride", papyrusNiOverride::GetObjectUniqueID, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, TESForm*, UInt32>("GetFormFromUniqueID", "NiOverride", papyrusNiOverride::GetFormFromUniqueID, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, TESForm*, UInt32>("GetOwnerOfUniqueID", "NiOverride", papyrusNiOverride::GetOwnerOfUniqueID, registry));


	// DyeManager V1
	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, UInt32, SInt32, UInt32>("SetItemDyeColor", "NiOverride", papyrusNiOverride::SetItemDyeColor, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, UInt32, UInt32, SInt32>("GetItemDyeColor", "NiOverride", papyrusNiOverride::GetItemDyeColor, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, UInt32, SInt32>("ClearItemDyeColor", "NiOverride", papyrusNiOverride::ClearItemDyeColor, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, TESObjectREFR*, UInt32>("UpdateItemDyeColor", "NiOverride", papyrusNiOverride::UpdateItemDyeColor, registry));


	// DyeManager V2
	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, UInt32, SInt32, SInt32, UInt32>("SetItemTextureLayerColor", "NiOverride", papyrusNiOverride::SetItemTextureLayerColor, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, UInt32, UInt32, SInt32, SInt32>("GetItemTextureLayerColor", "NiOverride", papyrusNiOverride::GetItemTextureLayerColor, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, UInt32, SInt32, SInt32>("ClearItemTextureLayerColor", "NiOverride", papyrusNiOverride::ClearItemTextureLayerColor, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, UInt32, SInt32, SInt32, UInt32>("SetItemTextureLayerType", "NiOverride", papyrusNiOverride::SetItemTextureLayerType, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, UInt32, UInt32, SInt32, SInt32>("GetItemTextureLayerType", "NiOverride", papyrusNiOverride::GetItemTextureLayerType, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, UInt32, SInt32, SInt32>("ClearItemTextureLayerType", "NiOverride", papyrusNiOverride::ClearItemTextureLayerType, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, UInt32, SInt32, SInt32, BSFixedString>("SetItemTextureLayerTexture", "NiOverride", papyrusNiOverride::SetItemTextureLayerTexture, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, BSFixedString, UInt32, SInt32, SInt32>("GetItemTextureLayerTexture", "NiOverride", papyrusNiOverride::GetItemTextureLayerTexture, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, UInt32, SInt32, SInt32>("ClearItemTextureLayerTexture", "NiOverride", papyrusNiOverride::ClearItemTextureLayerTexture, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, UInt32, SInt32, SInt32, BSFixedString>("SetItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::SetItemTextureLayerBlendMode, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, BSFixedString, UInt32, SInt32, SInt32>("GetItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::GetItemTextureLayerBlendMode, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, void, UInt32, SInt32, SInt32>("ClearItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::ClearItemTextureLayerBlendMode, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, TESObjectREFR*, UInt32>("UpdateItemTextureLayers", "NiOverride", papyrusNiOverride::UpdateItemTextureLayers, registry));


	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("EnableTintTextureCache", "NiOverride", papyrusNiOverride::EnableTintTextureCache, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("ReleaseTintTextureCache", "NiOverride", papyrusNiOverride::ReleaseTintTextureCache, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, bool, TESForm*>("IsFormDye", "NiOverride", papyrusNiOverride::IsFormDye, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, UInt32, TESForm*>("GetFormDyeColor", "NiOverride", papyrusNiOverride::GetFormDyeColor, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, TESForm*, UInt32>("RegisterFormDyeColor", "NiOverride", papyrusNiOverride::RegisterFormDyeColor, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESForm*>("UnregisterFormDyeColor", "NiOverride", papyrusNiOverride::UnregisterFormDyeColor, registry));

	// Position Transforms
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("HasNodeTransformPosition", "NiOverride", papyrusNiOverride::HasNodeTransformPosition, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, void, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString, VMArray<float>>("AddNodeTransformPosition", "NiOverride", papyrusNiOverride::AddNodeTransformPosition, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, VMResultArray<float>, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("GetNodeTransformPosition", "NiOverride", papyrusNiOverride::GetNodeTransformPosition, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("RemoveNodeTransformPosition", "NiOverride", papyrusNiOverride::RemoveNodeTransformPosition, registry));

	// Scale Transforms
	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("HasNodeTransformScale", "NiOverride", papyrusNiOverride::HasNodeTransformScale, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, void, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString, float>("AddNodeTransformScale", "NiOverride", papyrusNiOverride::AddNodeTransformScale, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, float, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("GetNodeTransformScale", "NiOverride", papyrusNiOverride::GetNodeTransformScale, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("RemoveNodeTransformScale", "NiOverride", papyrusNiOverride::RemoveNodeTransformScale, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("HasNodeTransformRotation", "NiOverride", papyrusNiOverride::HasNodeTransformRotation, registry));

	// Rotation Transforms
	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, void, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString, VMArray<float>>("AddNodeTransformRotation", "NiOverride", papyrusNiOverride::AddNodeTransformRotation, registry));

	registry->RegisterFunction(
		new NativeFunction6<StaticFunctionTag, VMResultArray<float>, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString, UInt32>("GetNodeTransformRotation", "NiOverride", papyrusNiOverride::GetNodeTransformRotation, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("RemoveNodeTransformRotation", "NiOverride", papyrusNiOverride::RemoveNodeTransformRotation, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR *>("UpdateAllReferenceTransforms", "NiOverride", papyrusNiOverride::UpdateAllReferenceTransforms, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, void, TESObjectREFR *, bool, bool, BSFixedString>("UpdateNodeTransform", "NiOverride", papyrusNiOverride::UpdateNodeTransform, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, TESObjectREFR *>("RemoveAllReferenceTransforms", "NiOverride", papyrusNiOverride::RemoveAllReferenceTransforms, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("RemoveAllTransforms", "NiOverride", papyrusNiOverride::RemoveAllTransforms, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, float, VMArray<float>, VMArray<float>, float>("GetInverseTransform", "NiOverride", papyrusNiOverride::GetInverseTransform, registry));

	registry->RegisterFunction(
		new NativeFunction5<StaticFunctionTag, void, TESObjectREFR *, bool, bool, BSFixedString, BSFixedString>("SetNodeDestination", "NiOverride", papyrusNiOverride::SetNodeDestination, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, bool, TESObjectREFR *, bool, bool, BSFixedString>("RemoveNodeDestination", "NiOverride", papyrusNiOverride::RemoveNodeDestination, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, BSFixedString, TESObjectREFR *, bool, bool, BSFixedString>("GetNodeDestination", "NiOverride", papyrusNiOverride::GetNodeDestination, registry));

	registry->RegisterFunction(
		new NativeFunction3<StaticFunctionTag, VMResultArray<BSFixedString>, TESObjectREFR *, bool, bool>("GetNodeTransformNames", "NiOverride", papyrusNiOverride::GetNodeTransformNames, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, VMResultArray<BSFixedString>, TESObjectREFR *, bool, bool, BSFixedString>("GetNodeTransformKeys", "NiOverride", papyrusNiOverride::GetNodeTransformKeys, registry));


	// Extra Data Testers
	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, bool, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetBooleanExtraData", "NiOverride", papyrusNiOverride::GetExtraData<bool>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, SInt32, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetIntegerExtraData", "NiOverride", papyrusNiOverride::GetExtraData<SInt32>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, VMResultArray<SInt32>, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetIntegersExtraData", "NiOverride", papyrusNiOverride::GetExtraData<VMResultArray<SInt32>>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, float, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetFloatExtraData", "NiOverride", papyrusNiOverride::GetExtraData<float>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, VMResultArray<float>, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetFloatsExtraData", "NiOverride", papyrusNiOverride::GetExtraData<VMResultArray<float>>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, BSFixedString, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetStringExtraData", "NiOverride", papyrusNiOverride::GetExtraData<BSFixedString>, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, VMResultArray<BSFixedString>, TESObjectREFR *, bool, BSFixedString, BSFixedString>("GetStringsExtraData", "NiOverride", papyrusNiOverride::GetExtraData<VMResultArray<BSFixedString>>, registry));

	// Extra data manipulation
	registry->SetFunctionFlags("NiOverride", "GetBooleanExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetIntegerExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetIntegersExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetFloatExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetFloatsExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetStringExtraData", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetStringsExtraData", VMClassRegistry::kFunctionFlag_NoWait);

	// Overlay numerics
	registry->SetFunctionFlags("NiOverride", "GetNumBodyOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumHandOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumFeetOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumFaceOverlays", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetNumSpellBodyOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumSpellHandOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumSpellFeetOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNumSpellFaceOverlays", VMClassRegistry::kFunctionFlag_NoWait);

	// Armor based overrides
	registry->SetFunctionFlags("NiOverride", "HasOverride", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "AddOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "ApplyOverrides", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetPropertyFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetPropertyInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetPropertyBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetPropertyString", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasArmorAddonNode", VMClassRegistry::kFunctionFlag_NoWait);

	// Node based overrides
	registry->SetFunctionFlags("NiOverride", "HasNodeOverride", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "AddNodeOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetNodeOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetNodePropertyFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodePropertyInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodePropertyBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodePropertyString", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "ApplyNodeOverrides", VMClassRegistry::kFunctionFlag_NoWait);

	// Weapon based overrides
	registry->SetFunctionFlags("NiOverride", "HasWeaponOverride", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "AddWeaponOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddWeaponOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddWeaponOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddWeaponOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddWeaponOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetWeaponOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "ApplyWeaponOverrides", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetWeaponPropertyFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponPropertyInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponPropertyBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetWeaponPropertyString", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasWeaponNode", VMClassRegistry::kFunctionFlag_NoWait);

	// Skin based overrides
	registry->SetFunctionFlags("NiOverride", "HasSkinOverride", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "AddSkinOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddSkinOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddSkinOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddSkinOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddSkinOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetSkinOverrideFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinOverrideInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinOverrideBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinOverrideString", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinOverrideTextureSet", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "ApplySkinOverrides", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetSkinPropertyFloat", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinPropertyInt", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinPropertyBool", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetSkinPropertyString", VMClassRegistry::kFunctionFlag_NoWait);

	// Armor based overrides
	registry->SetFunctionFlags("NiOverride", "RemoveAllOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllReferenceOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllArmorOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllArmorAddonOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllArmorAddonNodeOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveOverride", VMClassRegistry::kFunctionFlag_NoWait);

	// Node based overrides
	registry->SetFunctionFlags("NiOverride", "RemoveAllNodeOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllReferenceNodeOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllNodeNameOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveNodeOverride", VMClassRegistry::kFunctionFlag_NoWait);

	// Weapon based overrides
	registry->SetFunctionFlags("NiOverride", "RemoveAllWeaponBasedOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllReferenceWeaponOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllWeaponOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllWeaponNodeOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveWeaponOverride", VMClassRegistry::kFunctionFlag_NoWait);

	// Skin based overrides
	registry->SetFunctionFlags("NiOverride", "RemoveAllSkinBasedOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllReferenceSkinOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllSkinOverrides", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveSkinOverride", VMClassRegistry::kFunctionFlag_NoWait);


	registry->SetFunctionFlags("NiOverride", "AddOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "HasOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RevertOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RevertOverlay", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RevertHeadOverlays", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RevertHeadOverlay", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasBodyMorph", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "SetBodyMorph", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetBodyMorph", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearBodyMorph", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "HasBodyMorphName", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearBodyMorphNames", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "HasBodyMorphKey", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearBodyMorphKeys", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearMorphs", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "UpdateModelWeight", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "EnableTintTextureCache", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ReleaseTintTextureCache", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "SetItemDyeColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetItemDyeColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearItemDyeColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "UpdateItemDyeColor", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "SetItemTextureLayerColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetItemTextureLayerColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearItemTextureLayerColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "SetItemTextureLayerTexture", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetItemTextureLayerTexture", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearItemTextureLayerTexture", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "SetItemTextureLayerBlendMode", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetItemTextureLayerBlendMode", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearItemTextureLayerBlendMode", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "SetItemTextureLayerType", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetItemTextureLayerType", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "ClearItemTextureLayerType", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "UpdateItemTextureLayer", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "IsFormDye", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetFormDyeColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RegisterFormDyeColor", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "UnregisterFormDyeColor", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasNodeTransformPosition", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeTransformPosition", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeTransformPosition", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveNodeTransformPosition", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasNodeTransformScale", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeTransformScale", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeTransformScale", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveNodeTransformScale", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "HasNodeTransformRotation", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "AddNodeTransformRotation", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeTransformRotation", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveNodeTransformRotation", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetNodeDestination", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveNodeDestination", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "GetNodeTransformNames", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetNodeTransformKeys", VMClassRegistry::kFunctionFlag_NoWait);

	registry->SetFunctionFlags("NiOverride", "UpdateAllReferenceTransforms", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllReferenceTransforms", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "RemoveAllTransforms", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "UpdateNodeTransform", VMClassRegistry::kFunctionFlag_NoWait);
	registry->SetFunctionFlags("NiOverride", "GetInverseTransform", VMClassRegistry::kFunctionFlag_NoWait);
}