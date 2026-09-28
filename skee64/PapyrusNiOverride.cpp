#include "SKEETasks.h"
#include "SKEEDelayFunctors.h"

#include "RE/N/NiExtraData.h"
#include "RE/A/Array.h"           // RE::BSScript::Array (legacy VMArray equiv)
#include "RE/V/Variable.h"        // RE::BSScript::Variable (legacy VMValue equiv)
#include "RE/V/VirtualMachine.h"  // RE::BSScript::Internal::VirtualMachine

#include <RE/I/IFunctionArguments.h>
#include <RE/P/PackUnpack.h>
#include <numbers>

#include "PapyrusNiOverride.h"

#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "BodyMorphInterface.h"
#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "NiTransformInterface.h"
#include "AttachmentInterface.h"

#include "ShaderUtilities.h"
#include "NifUtils.h"
#include "Utilities.h"
#include <cstdint>

extern const SKSE::TaskInterface* g_task;

extern OverrideInterface	g_overrideInterface;
extern OverlayInterface		g_overlayInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern DyeMap				g_dyeMap;
extern NiTransformInterface	g_transformInterface;
extern AttachmentInterface	g_attachmentInterface;

extern std::uint32_t	g_numBodyOverlays;
extern std::uint32_t	g_numHandOverlays;
extern std::uint32_t	g_numFeetOverlays;
extern std::uint32_t	g_numFaceOverlays;

extern bool		g_playerOnly;
extern std::uint32_t	g_numSpellBodyOverlays;
extern std::uint32_t	g_numSpellHandOverlays;
extern std::uint32_t	g_numSpellFeetOverlays;
extern std::uint32_t	g_numSpellFaceOverlays;

using RE::StaticFunctionTag;

class MorpedReferenceEventFunctor : public RE::BSScript::IFunctionArguments
{
public:
	MorpedReferenceEventFunctor(const RE::BSFixedString & a_eventName, RE::TESForm * receiver, RE::TESObjectREFR * actor)
		: m_eventName(a_eventName.c_str()), m_actor(actor), m_receiver(receiver) {}

	bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& a_dst) const override
	{
		a_dst.resize(1);
		RE::BSScript::PackValue(&a_dst[0], m_actor);
		return true;
	}

	void Run()
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (vm && m_receiver) {
			auto* policy = vm->GetObjectHandlePolicy();
			if (policy) {
				RE::VMHandle handle = policy->GetHandleForObject(m_receiver->GetFormType(), m_receiver);
				vm->SendEvent(handle, m_eventName, this);
			}
		}
	}

private:
	RE::BSFixedString	m_eventName;
	RE::TESForm			* m_receiver;
	RE::TESObjectREFR	* m_actor;
};

namespace papyrusNiOverride
{
	void AddOverlays(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(refr)
			g_overlayInterface.AddOverlays(refr);
	}

	bool HasOverlays(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(!refr)
			return false;

		return g_overlayInterface.HasOverlays(refr);
	}

	void RemoveOverlays(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RemoveOverlays(refr);
		}
	}

	void RevertOverlays(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RevertOverlays(refr, false);
		}
	}

	void RevertOverlay(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString nodeName, std::uint32_t armorMask, std::uint32_t addonMask)
	{
		if(refr) {
			g_overlayInterface.RevertOverlay(refr, nodeName.c_str(), armorMask, addonMask, false);
		}
	}

	void RevertHeadOverlays(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(refr) {
			g_overlayInterface.RevertHeadOverlays(refr, false);
		}
	}

	void RevertHeadOverlay(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString nodeName, std::uint32_t partType, std::uint32_t shaderType)
	{
		if(refr) {
			g_overlayInterface.RevertHeadOverlay(refr, nodeName.c_str(), partType, shaderType, false);
		}
	}

	bool HasOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index) != NULL;
	}

	bool HasArmorAddonNode(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, bool debug)
	{
		if (!refr)
			return false;

		return g_overrideInterface.HasArmorAddonNode(refr, firstPerson, armor, addon, nodeName.c_str(), debug);
	}

	void ApplyOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(!refr)
			return;

		g_overrideInterface.Impl_SetProperties(refr->formID, false);
	}

	template<typename T>
	void AddOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index, T dataType, bool persist)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!armor) {
			SKSE::log::error("{} - Failed to add override for \"{}\" node key: {}. No Armor specified.", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		if(!addon) {
			SKSE::log::error("{} - Failed to add override for \"{}\" node key: {}. No ArmorAddon specified.", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if(value.type == OverrideVariant::kType_None) {
			SKSE::log::error("{} - Failed to pack value for \"{}\" node key: {}. Most likely invalid key for type", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.Impl_AddOverride(refr, isFemale, armor, addon, nodeName, value);

		std::uint8_t gender = 0;
		RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if(actorBase)
			gender = actorBase->GetSex();

		// Applies the properties visually, only if the current gender matches
		if (isFemale == (gender == 1)) {
			g_overrideInterface.Impl_SetArmorAddonProperty(refr, false, armor, addon, nodeName, &value, false);
			if (refr->Get3D(false) != refr->Get3D(true)) {
				g_overrideInterface.Impl_SetArmorAddonProperty(refr, true, armor, addon, nodeName, &value, false);
			}
		}
	}

	template<typename T>
	T GetOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !armor || !addon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.Impl_GetOverride(refr, isFemale, armor, addon, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetArmorAddonProperty(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
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

		g_overrideInterface.Impl_GetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	void ApplyNodeOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		return g_overrideInterface.Impl_SetNodeProperties(refr->formID, false);
	}

	bool HasNodeOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.Impl_GetNodeOverride(refr, isFemale, nodeName, key, index) != NULL;
	}

	template<typename T>
	void AddNodeOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index, T dataType, bool persist)
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
			SKSE::log::error("{} - Failed to pack value for \"{}\" node key: {}. Most likely invalid key for type", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.Impl_AddNodeOverride(refr, isFemale, nodeName, value);

		std::uint8_t gender = 0;
		RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if(actorBase)
			gender = actorBase->GetSex();

		// Applies the properties visually, only if the current gender matches
		if (isFemale == (gender == 1)) {
			g_overrideInterface.Impl_SetNodeProperty(refr, false, nodeName, &value, false);
			if(refr->Get3D(false) != refr->Get3D(true))
				g_overrideInterface.Impl_SetNodeProperty(refr, true, nodeName, &value, false);
		}
	}

	template<typename T>
	T GetNodeOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.Impl_GetNodeOverride(refr, isFemale, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetNodeProperty(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
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

		g_overrideInterface.Impl_GetNodeProperty(refr, firstPerson, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	bool HasWeaponOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(key > OverrideVariant::kKeyMax)
			return false;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.Impl_GetWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index) != NULL;
	}

	bool HasWeaponNode(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, bool debug)
	{
		if (!refr)
			return false;

		return g_overrideInterface.Impl_HasWeaponNode(refr, firstPerson, weapon, nodeName, debug);
	}

	void ApplyWeaponOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(!refr)
			return;
		g_overrideInterface.Impl_SetWeaponProperties(refr->formID, false);
	}

	template<typename T>
	void AddWeaponOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index, T dataType, bool persist)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		if(!weapon) {
			SKSE::log::error("{} - Failed to add override for \"{}\" node key: {}. No weapon specified.", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if(value.type == OverrideVariant::kType_None) {
			SKSE::log::error("{} - Failed to pack value for \"{}\" node key: {}. Most likely invalid key for type", __FUNCTION__, nodeName.c_str(), key);
			return;
		}

		// Adds the property to the map
		if(persist)
			g_overrideInterface.Impl_AddWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, value);

		std::uint8_t gender = 0;
		RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if(actorBase)
			gender = actorBase->GetSex();

		// Applies the properties visually, only if the current gender matches
		if(isFemale == (gender == 1))
			g_overrideInterface.Impl_SetWeaponProperty(refr, gender == 1, firstPerson, weapon, nodeName, &value, false);
	}

	template<typename T>
	T GetWeaponOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax || !weapon)
			return 0;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.Impl_GetWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
		if(value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetWeaponProperty(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
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

		g_overrideInterface.Impl_GetWeaponProperty(refr, firstPerson, weapon, nodeName, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	bool HasSkinOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint32_t key, std::uint32_t index)
	{
		if (key > OverrideVariant::kKeyMax)
			return false;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		return g_overrideInterface.Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index) != NULL;
	}

	void ApplySkinOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;
		g_overrideInterface.Impl_SetSkinProperties(refr->formID, false);
	}

	template<typename T>
	void AddSkinOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint32_t key, std::uint32_t index, T dataType, bool persist)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		if (!slotMask) {
			SKSE::log::error("{} - Failed to add override for \"{}\" slot key: {}. No weapon specified.", __FUNCTION__, slotMask, key);
			return;
		}

		OverrideVariant value;
		PackValue<T>(&value, key, index, &dataType);

		if (value.type == OverrideVariant::kType_None) {
			SKSE::log::error("{} - Failed to pack value for \"{}\" slot key: {}. Most likely invalid key for type", __FUNCTION__, slotMask, key);
			return;
		}

		// Adds the property to the map
		if (persist)
			g_overrideInterface.Impl_AddSkinOverride(refr, isFemale, firstPerson, slotMask, value);

		std::uint8_t gender = 0;
		RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if (actorBase)
			gender = actorBase->GetSex();

		// Applies the properties visually, only if the current gender matches
		if (isFemale == (gender == 1))
			g_overrideInterface.Impl_SetSkinProperty(refr, firstPerson, slotMask, &value, false);
	}

	template<typename T>
	T GetSkinOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint32_t key, std::uint32_t index)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return 0;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		T dest = 0;
		OverrideVariant * value = g_overrideInterface.Impl_GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
		if (value) {
			UnpackValue<T>(&dest, value);
		}

		return dest;
	}

	template<typename T>
	T GetSkinProperty(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool firstPerson, std::uint32_t slotMask, std::uint32_t key, std::uint32_t index)
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

		g_overrideInterface.Impl_GetSkinProperty(refr, firstPerson, slotMask, &value);

		T dest;
		UnpackValue<T>(&dest, &value);
		return dest;
	}

	void RemoveAllOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.Impl_RemoveAllOverrides();
	}

	void RemoveAllReferenceOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(!refr)
			return;
		
		g_overrideInterface.Impl_RemoveAllReferenceOverrides(refr);
	}

	void RemoveAllArmorOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor)
	{
		g_overrideInterface.Impl_RemoveAllArmorOverrides(refr, isFemale, armor);
	}

	void RemoveAllArmorAddonOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon)
	{
		g_overrideInterface.Impl_RemoveAllArmorAddonOverrides(refr, isFemale, armor, addon);
	}

	void RemoveAllArmorAddonNodeOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName)
	{
		g_overrideInterface.Impl_RemoveAllArmorAddonNodeOverrides(refr, isFemale, armor, addon, nodeName);
	}

	void RemoveArmorAddonOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.Impl_RemoveArmorAddonOverride(refr, isFemale, armor, addon, nodeName, key, index);
	}

	void RemoveAllNodeOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.Impl_RemoveAllNodeOverrides();
	}

	void RemoveAllReferenceNodeOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		g_overrideInterface.Impl_RemoveAllReferenceNodeOverrides(refr);
	}

	void RemoveAllNodeNameOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName)
	{
		g_overrideInterface.Impl_RemoveAllNodeNameOverrides(refr, isFemale, nodeName);
	}

	void RemoveNodeOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.Impl_RemoveNodeOverride(refr, isFemale, nodeName, key, index);
	}

	void RemoveAllWeaponBasedOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.Impl_RemoveAllWeaponBasedOverrides();
	}

	void RemoveAllReferenceWeaponOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if(!refr)
			return;

		g_overrideInterface.Impl_RemoveAllReferenceWeaponOverrides(refr);
	}

	void RemoveAllWeaponOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon)
	{
		g_overrideInterface.Impl_RemoveAllWeaponOverrides(refr, isFemale, firstPerson, weapon);
	}

	void RemoveAllWeaponNodeOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName)
	{
		g_overrideInterface.Impl_RemoveAllWeaponNodeOverrides(refr, isFemale, firstPerson, weapon, nodeName);
	}

	void RemoveWeaponOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP * weapon, RE::BSFixedString nodeName, std::uint32_t key, std::uint32_t index)
	{
		if(!refr || key > OverrideVariant::kKeyMax)
			return;

		if(!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if(index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.Impl_RemoveWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
	}

	void RemoveAllSkinBasedOverrides(StaticFunctionTag* base)
	{
		g_overrideInterface.Impl_RemoveAllSkinBasedOverrides();
	}

	void RemoveAllReferenceSkinOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_overrideInterface.Impl_RemoveAllReferenceSkinOverrides(refr);
	}

	void RemoveAllSkinOverrides(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask)
	{
		g_overrideInterface.Impl_RemoveAllSkinOverrides(refr, isFemale, firstPerson, slotMask);
	}

	void RemoveSkinOverride(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint32_t key, std::uint32_t index)
	{
		if (!refr || key > OverrideVariant::kKeyMax)
			return;

		if (!OverrideVariant::IsIndexValid(key))
			index = OverrideVariant::kIndexMax;
		if (index > OverrideVariant::kIndexMax)
			index = OverrideVariant::kIndexMax;

		g_overrideInterface.Impl_RemoveSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
	}

	std::uint32_t GetNumBodyOverlays(StaticFunctionTag* base)
	{
		return g_numBodyOverlays;
	}

	std::uint32_t GetNumHandOverlays(StaticFunctionTag* base)
	{
		return g_numHandOverlays;
	}

	std::uint32_t GetNumFeetOverlays(StaticFunctionTag* base)
	{
		return g_numFeetOverlays;
	}

	std::uint32_t GetNumFaceOverlays(StaticFunctionTag* base)
	{
		return g_numFaceOverlays;
	}

	std::uint32_t GetNumSpellBodyOverlays(StaticFunctionTag* base)
	{
		return g_numSpellBodyOverlays;
	}

	std::uint32_t GetNumSpellHandOverlays(StaticFunctionTag* base)
	{
		return g_numSpellHandOverlays;
	}

	std::uint32_t GetNumSpellFeetOverlays(StaticFunctionTag* base)
	{
		return g_numSpellFeetOverlays;
	}

	std::uint32_t GetNumSpellFaceOverlays(StaticFunctionTag* base)
	{
		return g_numSpellFaceOverlays;
	}

	bool HasBodyMorph(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName, RE::BSFixedString keyName)
	{
		if (!refr)
			return false;

		return g_bodyMorphInterface.HasBodyMorph(refr, morphName.c_str(), keyName.c_str());
	}

	void SetBodyMorph(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName, RE::BSFixedString keyName, float value)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.SetMorph(refr, morphName.c_str(), keyName.c_str(), value);
	}

	float GetBodyMorph(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName, RE::BSFixedString keyName)
	{
		if (!refr)
			return 0.0f;

		return g_bodyMorphInterface.GetMorph(refr, morphName.c_str(), keyName.c_str());
	}

	void ClearBodyMorph(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName, RE::BSFixedString keyName)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.ClearMorph(refr, morphName.c_str(), keyName.c_str());
	}

	bool HasBodyMorphKey(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString keyName)
	{
		if (!refr)
			return false;

		return g_bodyMorphInterface.HasBodyMorphKey(refr, keyName.c_str());
	}

	void ClearBodyMorphKeys(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString keyName)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.ClearBodyMorphKeys(refr, keyName.c_str());
	}

	bool HasBodyMorphName(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName)
	{
		if (!refr)
			return false;

		return g_bodyMorphInterface.HasBodyMorphName(refr, morphName.c_str());
	}

	void ClearBodyMorphNames(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.ClearBodyMorphNames(refr, morphName.c_str());
	}

	void ClearMorphs(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.ClearMorphs(refr);
	}

	std::vector<RE::TESObjectREFR*> GetMorphedReferences(StaticFunctionTag* base)
	{
		std::vector<RE::TESObjectREFR*> results;

		class Visitor : public BodyMorphInterface::ActorVisitor
		{
		public:
			Visitor(std::vector<RE::TESObjectREFR*> * res) : result(res) { }

			virtual void Visit(RE::TESObjectREFR * ref) override
			{
				result->push_back(ref);
			}
		private:
			std::vector<RE::TESObjectREFR*> * result;
		};

		Visitor visitor(&results);
		g_bodyMorphInterface.VisitActors(visitor);
		return results;
	}

	void ForEachMorphedReference(StaticFunctionTag* base, RE::BSFixedString eventName, RE::TESForm * receiver)
	{
		if (!receiver)
			return;

		class Visitor : public BodyMorphInterface::ActorVisitor
		{
		public:
			Visitor(RE::BSFixedString eventName, RE::TESForm * receiver) : m_eventName(eventName), m_receiver(receiver) { }

			virtual void Visit(RE::TESObjectREFR * refr) override
			{
				MorpedReferenceEventFunctor fn(m_eventName, m_receiver, refr);
				fn.Run();
			}
		private:
			RE::BSFixedString m_eventName;
			RE::TESForm * m_receiver;
		};

		Visitor visitor(eventName, receiver);
		g_bodyMorphInterface.VisitActors(visitor);
	}

	void UpdateModelWeight(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_bodyMorphInterface.UpdateModelWeight(refr);
	}

	std::vector<RE::BSFixedString> GetCachedMorphNames(StaticFunctionTag* base)
	{
		std::vector<RE::BSFixedString> results;
		for (auto morph : g_bodyMorphInterface.GetCachedMorphNames())
		{
			results.push_back(morph);
		}
		return results;
	}

	void EnableTintTextureCache(StaticFunctionTag* base)
	{
		g_tintMaskInterface.ManageTints();
	}

	void ReleaseTintTextureCache(StaticFunctionTag* base)
	{
		g_tintMaskInterface.ReleaseTints();
	}

	std::uint32_t GetItemUniqueID(StaticFunctionTag* base, RE::TESObjectREFR * refr, std::uint32_t weaponSlot, std::uint32_t slotMask, bool makeUnique)
	{
		if (!refr)
			return 0;

		IItemDataInterface::Identifier identifier;
		identifier.SetSlotMask(slotMask, weaponSlot);
		return g_itemDataInterface.GetItemUniqueID(refr, identifier, makeUnique);
	}

	std::uint32_t GetObjectUniqueID(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool makeUnique)
	{
		if (!refr)
			return 0;

		if (RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr) {
			SKSE::log::warn("{} - cannot be called on an actor", __FUNCTION__);
			return 0;
		}

		IItemDataInterface::Identifier identifier;
		identifier.SetSelf();
		return g_itemDataInterface.GetItemUniqueID(refr, identifier, makeUnique);
	}

	RE::TESForm * GetFormFromUniqueID(StaticFunctionTag* base, std::uint32_t uniqueID)
	{
		return g_itemDataInterface.GetFormFromUniqueID(uniqueID);
	}

	RE::TESForm * GetOwnerOfUniqueID(StaticFunctionTag* base, std::uint32_t uniqueID)
	{
		return g_itemDataInterface.GetOwnerOfUniqueID(uniqueID);
	}

	void SetItemDyeColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t maskIndex, std::uint32_t color)
	{
		g_itemDataInterface.SetItemTextureLayerColor(uniqueID, 0, maskIndex, color);
	}

	std::uint32_t GetItemDyeColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t maskIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerColor(uniqueID, 0, maskIndex);
	}

	void ClearItemDyeColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t maskIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerColor(uniqueID, 0, maskIndex);
	}

	void UpdateItemDyeColor(StaticFunctionTag* base, RE::TESObjectREFR * refr, std::uint32_t uniqueID)
	{
		UpdateItemTextureLayers(base, refr, uniqueID);
	}

	void SetItemTextureLayerColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t color)
	{
		g_itemDataInterface.SetItemTextureLayerColor(uniqueID, textureIndex, layerIndex, color);
	}

	std::uint32_t GetItemTextureLayerColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerColor(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerColor(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerColor(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerType(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t color)
	{
		g_itemDataInterface.SetItemTextureLayerType(uniqueID, textureIndex, layerIndex, color);
	}

	std::uint32_t GetItemTextureLayerType(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		return g_itemDataInterface.GetItemTextureLayerType(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerType(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerType(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerTexture(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, RE::BSFixedString texture)
	{
		g_itemDataInterface.Impl_SetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex, texture);
	}

	RE::BSFixedString GetItemTextureLayerTexture(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		return g_itemDataInterface.Impl_GetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerTexture(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerTexture(uniqueID, textureIndex, layerIndex);
	}

	void SetItemTextureLayerBlendMode(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, RE::BSFixedString texture)
	{
		g_itemDataInterface.Impl_SetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex, texture);
	}

	RE::BSFixedString GetItemTextureLayerBlendMode(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		return g_itemDataInterface.Impl_GetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex);
	}

	void ClearItemTextureLayerBlendMode(StaticFunctionTag* base, std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex)
	{
		g_itemDataInterface.ClearItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex);
	}

	void UpdateItemTextureLayers(StaticFunctionTag* base, RE::TESObjectREFR * refr, std::uint32_t uniqueID)
	{
		if (RE::Actor * actor = refr ? refr->As<RE::Actor>() : nullptr) {
			IItemDataInterface::Identifier identifier;
			identifier.SetRankID(uniqueID);
			SKEE_AddTask(g_task, new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, false));
		}
	}

	bool IsFormDye(StaticFunctionTag* base, RE::TESForm * form)
	{
		return form ? g_dyeMap.IsValidDye(form) : false;
	}

	std::uint32_t GetFormDyeColor(StaticFunctionTag* base, RE::TESForm * form)
	{
		return form ? g_dyeMap.GetDyeColor(form) : 0;
	}

	void RegisterFormDyeColor(StaticFunctionTag* base, RE::TESForm * form, std::uint32_t color)
	{
		if (form) {
			g_dyeMap.RegisterDyeForm(form, color);
		}
	}

	void UnregisterFormDyeColor(StaticFunctionTag* base, RE::TESForm * form)
	{
		if (form) {
			g_dyeMap.UnregisterDyeForm(form);
		}
	}

	bool HasNodeTransformPosition(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.HasNodeTransformPosition(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	void AddNodeTransformPosition(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name, std::vector<float> dataType)
	{
		if (!refr)
			return;

		if (name == RE::BSFixedString("")) {
			SKSE::log::error("{} - Cannot add empty key for node \"{}\".", __FUNCTION__, node.c_str());
			return;
		}

		if (dataType.size() != 3) {
			SKSE::log::error("{} - Failed to unpack array value for \"{}\". Invalid array size must be 3", __FUNCTION__, node.c_str());
			return;
		}

		float pos[3];
		OverrideVariant posV[3];
		for (std::uint32_t i = 0; i < 3; i++) {
			pos[i] = dataType[i];
			PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &pos[i]);
			g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), posV[i]);
		}		
	}

	
	std::vector<float> GetNodeTransformPosition(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		std::vector<float> position;
		if (!refr)
			return position;
			
		position.resize(3, 0.0);
		RE::NiTransform transform;
		bool ret = g_transformInterface.Impl_GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), OverrideVariant::kParam_NodeTransformPosition, &transform);
		position[0] = transform.translate.x;
		position[1] = transform.translate.y;
		position[2] = transform.translate.z;
		return position;
	}

	bool RemoveNodeTransformPosition(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.RemoveNodeTransformPosition(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	bool HasNodeTransformScale(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.HasNodeTransformScale(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	void AddNodeTransformScale(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name, float fScale)
	{
		if (!refr)
			return;

		if (name == RE::BSFixedString("")) {
			SKSE::log::error("{} - Cannot add empty key for node \"{}\".", __FUNCTION__, node.c_str());
			return;
		}

		g_transformInterface.AddNodeTransformScale(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), fScale);
	}

	float GetNodeTransformScale(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return 0;

		return g_transformInterface.GetNodeTransformScale(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	bool RemoveNodeTransformScale(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.RemoveNodeTransformScale(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	bool HasNodeTransformScaleMode(StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.HasNodeTransformScaleMode(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}
;
	void AddNodeTransformScaleMode(StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name, std::uint32_t iScaleMode)
	{
		if (!refr)
			return;

		if (name == RE::BSFixedString("")) {
			SKSE::log::error("{} - Cannot add empty key for node \"{}\".", __FUNCTION__, node.c_str());
			return;
		}

		g_transformInterface.AddNodeTransformScaleMode(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), iScaleMode);
	}

	std::uint32_t GetNodeTransformScaleMode(StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return 0;

		return g_transformInterface.GetNodeTransformScaleMode(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	bool RemoveNodeTransformScaleMode(StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.RemoveNodeTransformScaleMode(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	bool HasNodeTransformRotation(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.HasNodeTransformRotation(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	void AddNodeTransformRotation(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name, std::vector<float> dataType)
	{
		if (!refr)
			return;

		RE::NiMatrix3 rotation;
		if (dataType.size() == 3) {
			float heading, attitude, bank;
			
			heading = dataType[0];
			attitude = dataType[1];
			bank = dataType[2];

			heading *= std::numbers::pi_v<float> / 180;
			attitude *= std::numbers::pi_v<float> / 180;
			bank *= std::numbers::pi_v<float> / 180;

			// SetEulerAngles not in CommonLib - stub
			rotation = RE::NiMatrix3();
		}
		else if (dataType.size() == 9) {
			for (std::uint32_t i = 0; i < 9; i++)
				rotation.entry[i/3][i%3] = dataType[i];
		}
		else {
			SKSE::log::error("{} - Failed to unpack array value for \"{}\". Invalid array size must be 3 or 9", __FUNCTION__, node.c_str());
			return;
		}

		OverrideVariant rotV[9];
		for (std::uint32_t i = 0; i < 9; i++) {
			PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &rotation.entry[i/3][i%3]);
			g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), rotV[i]);
		}
	}

	std::vector<float> GetNodeTransformRotation(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name, std::uint32_t type)
	{
		std::vector<float> rotation;
		if (!refr)
			return rotation;

		RE::NiTransform transform;
		bool ret = g_transformInterface.Impl_GetOverrideNodeTransform(refr, isFirstPerson, isFemale, node.c_str(), name.c_str(), OverrideVariant::kParam_NodeTransformRotation, &transform);

		switch (type) {
			case 0:
			{
				rotation.resize(3, 0.0);
				float heading, attitude, bank;
				heading = attitude = bank = 0.0f; // GetEulerAngles not in CommonLib

				// Radians to degrees
				heading *= 180 / std::numbers::pi_v<float>;
				attitude *= 180 / std::numbers::pi_v<float>;
				bank *= 180 / std::numbers::pi_v<float>;

				rotation[0] = heading;
				rotation[1] = attitude;
				rotation[2] = bank;
				break;
			}
			case 1:
			{
				rotation.resize(9, 0.0);
				for (std::uint32_t i = 0; i < 9; i++) {
					rotation[i] = (&transform.rotate.entry[0][0])[i];
				}
				break;
			}
			default:
			{
				SKSE::log::error("{} - Failed to create array value for \"{}\". Invalid type {}", __FUNCTION__, node.c_str(), type);
				break;
			}
		}

		return rotation;
	}
	
	bool RemoveNodeTransformRotation(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString name)
	{
		if (!refr)
			return false;

		return g_transformInterface.RemoveNodeTransformRotation(refr, isFirstPerson, isFemale, node.c_str(), name.c_str());
	}

	void UpdateAllReferenceTransforms(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_transformInterface.Impl_UpdateNodeAllTransforms(refr);
	}

	void RemoveAllTransforms(StaticFunctionTag* base)
	{
		g_transformInterface.Revert();
	}

	void RemoveAllReferenceTransforms(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		if (!refr)
			return;

		g_transformInterface.Impl_RemoveAllReferenceTransforms(refr);
	}

	void UpdateNodeTransform(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node)
	{
		if (!refr)
			return;

		std::uint8_t gender = isFemale ? 1 : 0;
		std::uint8_t realGender = 0;
		RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if (actorBase)
			realGender = actorBase->GetSex();

		if (gender != realGender)
			return;

		g_transformInterface.Impl_UpdateNodeTransforms(refr, isFirstPerson, isFemale, node.c_str());
	}

	void SetNodeDestination(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node, RE::BSFixedString destination)
	{
		if (!refr)
			return;

		OverrideVariant value;
		SKEEFixedString str = destination;
		PackValue(&value, OverrideVariant::kParam_NodeDestination, -1, &str);
		g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node.c_str(), "NodeDestination", value);
	}

	RE::BSFixedString GetNodeDestination(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node)
	{
		RE::BSFixedString ret("");
		if (!refr)
			return ret;

		OverrideVariant value = g_transformInterface.Impl_GetOverrideNodeValue(refr, isFirstPerson, isFemale, node.c_str(), "NodeDestination", OverrideVariant::kParam_NodeDestination, -1);
		SKEEFixedString str;
		UnpackValue(&str, &value);
		return str;
	}

	bool RemoveNodeDestination(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node)
	{
		if (!refr)
			return false;

		bool ret = false;
		if (g_transformInterface.Impl_RemoveNodeTransformComponent(refr, isFirstPerson, isFemale, node.c_str(), "NodeDestination", OverrideVariant::kParam_NodeDestination, -1))
			ret = true;
		return ret;
	}

	// Legacy VMArray<float> params. CommonLib's native function layer unpacks Papyrus
	// arrays by value (std::vector), so the legacy write-back into the caller's arrays
	// cannot propagate to the script — only the returned scale is observable.
	float GetInverseTransform(StaticFunctionTag* base, std::vector<float> position, std::vector<float> rotation, float scale)
	{
		RE::NiTransform transform, ixForm;
		transform.scale = scale;
		if (position.size() == 3) {
			transform.translate.x = position[0];
			transform.translate.y = position[1];
			transform.translate.z = position[2];
		}
		if (rotation.size() == 9) {
			for (std::uint32_t i = 0; i < 9; i++)
				transform.rotate.entry[i/3][i%3] = rotation[i];
		}
		if (rotation.size() == 3) {
			float heading, attitude, bank;
			heading = rotation[0];
			attitude = rotation[1];
			bank = rotation[2];

			// SetEulerAngles not in CommonLib - stub
			transform.rotate = RE::NiMatrix3();
		}
		ixForm = transform.Invert();
		if (position.size() == 3) {
			position[0] = ixForm.translate.x;
			position[1] = ixForm.translate.y;
			position[2] = ixForm.translate.z;
		}
		if (rotation.size() == 9) {
			for (std::uint32_t i = 0; i < 9; i++)
				rotation[i] = ixForm.rotate.entry[i/3][i%3];
		}
		if (rotation.size() == 3) {
			float heading, attitude, bank;
			
			heading = attitude = bank = 0.0f; // GetEulerAngles not in CommonLib

			// Radians to degrees
			heading *= 180 / std::numbers::pi_v<float>;
			attitude *= 180 / std::numbers::pi_v<float>;
			bank *= 180 / std::numbers::pi_v<float>;

			rotation[0] = heading;
			rotation[1] = attitude;
			rotation[2] = bank;
		}

		return ixForm.scale;
	}

	std::vector<RE::BSFixedString> GetNodeTransformNames(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale)
	{
		std::vector<RE::BSFixedString> result;
		if (!refr)
			return result;

		g_transformInterface.Impl_VisitNodes(refr, isFirstPerson, isFemale, [&](RE::BSFixedString key, OverrideRegistration<StringTableItem>*value)
		{
			result.push_back(key);
			return false;
		});

		return result;
	}

	std::vector<RE::BSFixedString> GetNodeTransformKeys(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, bool isFemale, RE::BSFixedString node)
	{
		std::vector<RE::BSFixedString> result;
		if (!refr)
			return result;

		g_transformInterface.Impl_VisitNodes(refr, isFirstPerson, isFemale, [&](RE::BSFixedString nodeName, OverrideRegistration<StringTableItem>*value)
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

	std::vector<RE::BSFixedString> GetMorphNames(StaticFunctionTag* base, RE::TESObjectREFR * refr)
	{
		std::vector<RE::BSFixedString> result;
		if (!refr)
			return result;

		class Visitor : public BodyMorphInterface::MorphVisitor
		{
		public:
			Visitor(std::vector<RE::BSFixedString> * res) : result(res) { }

			virtual void Visit(RE::TESObjectREFR * ref, const char* name) override
			{
				result->push_back(name);
			}
		private:
			std::vector<RE::BSFixedString> * result;
		};

		Visitor visitor(&result);
		g_bodyMorphInterface.VisitMorphs(refr, visitor);
		return result;
	}

	std::vector<RE::BSFixedString> GetMorphKeys(StaticFunctionTag* base, RE::TESObjectREFR * refr, RE::BSFixedString morphName)
	{
		std::vector<RE::BSFixedString> result;
		if (!refr)
			return result;

		class Visitor : public BodyMorphInterface::MorphKeyVisitor
		{
		public:
			Visitor(std::vector<RE::BSFixedString> * res) : result(res) { }

			virtual void Visit(const char* name, float values) override
			{
				result->push_back(name);
			}
		private:
			std::vector<RE::BSFixedString> * result;
		};

		Visitor visitor(&result);
		g_bodyMorphInterface.VisitKeys(refr, morphName.c_str(), visitor);

		return result;
	}

	template<typename T>
	void GetBaseExtraData(RE::NiExtraData * extraData, T & data)
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

	template<> void ExtraDataInitializer(std::int32_t & data)
	{
		data = 0;
	}

	template<> void ExtraDataInitializer(RE::BSFixedString & data)
	{
		data = RE::BSFixedString("");
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, float & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiFloatExtraData*>(extraData)) {
			data = pExtraData->value;
		}
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, std::vector<float> & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiFloatsExtraData*>(extraData)) {
			for (std::uint32_t i = 0; i < pExtraData->size; i++)
				data.push_back(pExtraData->value[i]);
		}
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, std::int32_t & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiIntegerExtraData*>(extraData)) {
			data = pExtraData->value;
		}
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, std::vector<std::int32_t> & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiIntegersExtraData*>(extraData)) {
			for (std::uint32_t i = 0; i < pExtraData->size; i++)
				data.push_back(pExtraData->value[i]);
		}
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, RE::BSFixedString & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiStringExtraData*>(extraData)) {
			data = RE::BSFixedString(pExtraData->value);
		}
	}

	template<> void GetBaseExtraData(RE::NiExtraData * extraData, std::vector<RE::BSFixedString> & data)
	{
		if (auto * pExtraData = netimmerse_cast<RE::NiStringsExtraData*>(extraData)) {
			for (std::uint32_t i = 0; i < pExtraData->size; i++)
				data.push_back(RE::BSFixedString(pExtraData->value[i]));
		}
	}

	template<typename T>
	T GetExtraData(StaticFunctionTag* base, RE::TESObjectREFR * refr, bool isFirstPerson, RE::BSFixedString node, RE::BSFixedString dataName)
	{
		T value;
		ExtraDataInitializer(value);
		if (!refr)
			return value;

		RE::NiNode * skeleton = refr->Get3D(isFirstPerson) ? refr->Get3D(isFirstPerson)->AsNode() : nullptr;
		if (skeleton) {

			RE::NiAVObject * object = skeleton->GetObjectByName(node.c_str());
			if (object) {

				RE::NiExtraData * extraData = object->GetExtraData(dataName);
				if (extraData) {
					GetBaseExtraData<T>(extraData, value);
				}

			}

		}

		return value;
	}

	class AttachMeshLatentFunctor : public SKSE::LatentSKSEDelayFunctor
	{
	public:
		virtual const char* ClassName() const override { return "AttachMeshLatentFunctor"; }
		virtual std::uint32_t		ClassVersion() const override { return 1; }

		explicit AttachMeshLatentFunctor(SKSE::SerializationTag tag) : LatentSKSEDelayFunctor(tag) { }
		explicit AttachMeshLatentFunctor(std::uint32_t stackId, RE::TESObjectREFR* refr, bool isFirstPerson, const RE::BSFixedString& filePath, const RE::BSFixedString& name, bool replace, const std::vector<RE::BSFixedString>& filter)
			: LatentSKSEDelayFunctor(stackId)
			, m_ref(refr)
			, m_isFirstPerson(isFirstPerson)
			, m_filePath(filePath)
			, m_name(name)
			, m_replace(replace)
			, m_filter(filter)
		{}

		virtual bool Save(SKSE::SerializationInterface* intfc) override
		{
			using namespace Serialization;

			if (!LatentSKSEDelayFunctor::Save(intfc))
				return false;

			std::uint32_t formId = m_ref ? m_ref->formID : 0;
			if (!WriteData(intfc, &formId))
				return false;
			if (!WriteData(intfc, &m_isFirstPerson))
				return false;
			if (!WriteData(intfc, &m_filePath))
				return false;
			if (!WriteData(intfc, &m_name))
				return false;
			if (!WriteData(intfc, &m_replace))
				return false;
			std::uint32_t size = static_cast<std::uint32_t>(m_filter.size());
			if (WriteData(intfc, &size))
				return false;
			for (size_t i = 0; i < m_filter.size(); ++i)
			{
				if (!WriteData(intfc, &m_filter[i]))
					return false;
			}
			return true;
		}

		virtual bool Load(SKSE::SerializationInterface* intfc, std::uint32_t version) override
		{
			using namespace Serialization;

			if (!LatentSKSEDelayFunctor::Load(intfc, version))
				return false;

			std::uint32_t formId = 0;
			if (!ReadData(intfc, &formId))
				return false;
			if (!ReadData(intfc, &m_isFirstPerson))
				return false;
			if (!ReadData(intfc, &m_filePath))
				return false;
			if (!ReadData(intfc, &m_name))
				return false;
			if (!ReadData(intfc, &m_replace))
				return false;
			std::uint32_t size = 0;
			if (ReadData(intfc, &size))
				return false;

			m_filter.clear();
			m_filter.reserve(size);

			for (std::uint32_t i = 0; i < size; ++i)
			{
				RE::BSFixedString filter;
				if (!ReadData(intfc, &filter))
					return false;

				m_filter.push_back(filter);
			}

			std::uint32_t newFormId;
			if (!ResolveAnyForm(intfc, formId, &newFormId))
				m_ref = nullptr;

			RE::TESForm* form = RE::TESForm::LookupByID(newFormId);
			if (!form || (form->IsNot(RE::FormType::ActorCharacter) && form->IsNot(RE::FormType::Reference)))
				m_ref = nullptr;

			m_ref = static_cast<RE::TESObjectREFR*>(form);
			return true;
		}

		virtual void Run(RE::BSScript::Variable& a_result) override
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!m_ref)
			{
				vm->TraceStack("Must be used on a reference that exists", StackId(), RE::BSScript::IVirtualMachine::Severity::kError);
				a_result.SetBool(false);
				return;
			}

			std::unique_ptr<const char*[]> filter = nullptr;
			if (m_filter.size())
			{
				filter = std::make_unique<const char*[]>(m_filter.size());
				for (size_t i = 0; i < m_filter.size(); ++i)
				{
					filter[i] = m_filter[i].c_str();
				}
			}

			RE::NiAVObject* outRoot = nullptr;
			char errBuf[512];
			if (g_attachmentInterface.AttachMesh(m_ref, m_filePath.c_str(), m_name.c_str(), m_isFirstPerson, m_replace, filter.get(), m_filter.size(), outRoot, errBuf, 512))
			{
				g_bodyMorphInterface.ApplyVertexDiff(m_ref, outRoot);
				g_overrideInterface.Impl_ApplyNodeOverrides(m_ref, outRoot, true);
				a_result.SetBool(true);
			}
			else
			{
				vm->TraceStack(errBuf, StackId(), RE::BSScript::IVirtualMachine::Severity::kError);
				a_result.SetBool(false);
			}
		}

	protected:
		RE::TESObjectREFR*				m_ref;
		bool						m_isFirstPerson;
		RE::BSFixedString				m_filePath;
		RE::BSFixedString				m_name;
		bool						m_replace;
		std::vector<RE::BSFixedString>	m_filter;
	};

	RE::BSScript::LatentStatus AttachMesh(RE::BSScript::Internal::VirtualMachine* a_vm, RE::VMStackID stackId, RE::StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, RE::BSFixedString filePath, RE::BSFixedString name, bool replace, std::vector<RE::BSFixedString> filter)
	{
		if (!refr) {
			a_vm->TraceStack("Must be used on a reference that exists", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
			return RE::BSScript::LatentStatus::kFailed;
		}

		SKSE::GetObjectInterface()->GetDelayFunctorManager().Enqueue(new AttachMeshLatentFunctor(stackId, refr, isFirstPerson, filePath, name, replace, filter));
		return RE::BSScript::LatentStatus::kStarted;
	}

	class DetachMeshLatentFunctor : public SKSE::LatentSKSEDelayFunctor
	{
	public:
		virtual const char* ClassName() const override { return "DetachMeshLatentFunctor"; }
		virtual std::uint32_t		ClassVersion() const override { return 1; }

		explicit DetachMeshLatentFunctor(SKSE::SerializationTag tag) : LatentSKSEDelayFunctor(tag) { }
		explicit DetachMeshLatentFunctor(std::uint32_t stackId, RE::TESObjectREFR* refr, bool isFirstPerson, const RE::BSFixedString& name)
			: LatentSKSEDelayFunctor(stackId)
			, m_ref(refr)
			, m_isFirstPerson(isFirstPerson)
			, m_name(name)
		{}

		virtual bool Save(SKSE::SerializationInterface* intfc) override
		{
			using namespace Serialization;

			if (!LatentSKSEDelayFunctor::Save(intfc))
				return false;
			if (!WriteData(intfc, &m_ref->formID))
				return false;
			if (!WriteData(intfc, &m_isFirstPerson))
				return false;
			if (!WriteData(intfc, &m_name))
				return false;
			return true;
		}

		virtual bool Load(SKSE::SerializationInterface* intfc, std::uint32_t version) override
		{
			using namespace Serialization;

			if (!LatentSKSEDelayFunctor::Load(intfc, version))
				return false;

			std::uint32_t formId = 0;
			if (!ReadData(intfc, &formId))
				return false;
			if (!ReadData(intfc, &m_isFirstPerson))
				return false;
			if (!ReadData(intfc, &m_name))
				return false;

			std::uint32_t newFormId;
			if (!ResolveAnyForm(intfc, formId, &newFormId))
				m_ref = nullptr;

			RE::TESForm* form = RE::TESForm::LookupByID(newFormId);
			if (!form || form->IsNot(RE::FormType::ActorCharacter))
				m_ref = nullptr;

			m_ref = static_cast<RE::TESObjectREFR*>(form);
			return true;
		}

		virtual void Run(RE::BSScript::Variable& a_result) override
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!m_ref)
			{
				vm->TraceStack("Must be used on a reference that exists", StackId(), RE::BSScript::IVirtualMachine::Severity::kError);
				a_result.SetBool(false);
				return;
			}

			a_result.SetBool(g_attachmentInterface.DetachMesh(m_ref, m_name.c_str(), m_isFirstPerson));
		}

	protected:
		RE::TESObjectREFR*				m_ref;
		bool						m_isFirstPerson;
		RE::BSFixedString				m_name;
	};

	RE::BSScript::LatentStatus DetachMesh(RE::BSScript::Internal::VirtualMachine* a_vm, RE::VMStackID stackId, RE::StaticFunctionTag* base, RE::TESObjectREFR* refr, bool isFirstPerson, RE::BSFixedString name)
	{
		if (!refr) {
			a_vm->TraceStack("Must be used on a reference that exists", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
			return RE::BSScript::LatentStatus::kFailed;
		}

		SKSE::GetObjectInterface()->GetDelayFunctorManager().Enqueue(new DetachMeshLatentFunctor(stackId, refr, isFirstPerson, name));
		return RE::BSScript::LatentStatus::kStarted;
	}
}

void papyrusNiOverride::RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
{
	// Overlay Data
	a_vm->RegisterFunction("GetNumBodyOverlays", "NiOverride", papyrusNiOverride::GetNumBodyOverlays);
	a_vm->RegisterFunction("GetNumHandOverlays", "NiOverride", papyrusNiOverride::GetNumHandOverlays);
	a_vm->RegisterFunction("GetNumFeetOverlays", "NiOverride", papyrusNiOverride::GetNumFeetOverlays);
	a_vm->RegisterFunction("GetNumFaceOverlays", "NiOverride", papyrusNiOverride::GetNumFaceOverlays);
	// Spell Overlays Enabled
	a_vm->RegisterFunction("GetNumSpellBodyOverlays", "NiOverride", papyrusNiOverride::GetNumSpellBodyOverlays);
	a_vm->RegisterFunction("GetNumSpellHandOverlays", "NiOverride", papyrusNiOverride::GetNumSpellHandOverlays);
	a_vm->RegisterFunction("GetNumSpellFeetOverlays", "NiOverride", papyrusNiOverride::GetNumSpellFeetOverlays);
	a_vm->RegisterFunction("GetNumSpellFaceOverlays", "NiOverride", papyrusNiOverride::GetNumSpellFaceOverlays);
	// Overlays
	a_vm->RegisterFunction("AddOverlays", "NiOverride", papyrusNiOverride::AddOverlays);
	a_vm->RegisterFunction("HasOverlays", "NiOverride", papyrusNiOverride::HasOverlays);
	a_vm->RegisterFunction("RemoveOverlays", "NiOverride", papyrusNiOverride::RemoveOverlays);
	a_vm->RegisterFunction("RevertOverlays", "NiOverride", papyrusNiOverride::RevertOverlays);
	a_vm->RegisterFunction("RevertOverlay", "NiOverride", papyrusNiOverride::RevertOverlay);
	a_vm->RegisterFunction("RevertHeadOverlays", "NiOverride", papyrusNiOverride::RevertHeadOverlays);
	a_vm->RegisterFunction("RevertHeadOverlay", "NiOverride", papyrusNiOverride::RevertHeadOverlay);
	// Armor Overrides
	a_vm->RegisterFunction("HasOverride", "NiOverride", papyrusNiOverride::HasOverride);
	a_vm->RegisterFunction("AddOverrideFloat", "NiOverride", papyrusNiOverride::AddOverride<float>);
	a_vm->RegisterFunction("AddOverrideInt", "NiOverride", papyrusNiOverride::AddOverride<std::uint32_t>);
	a_vm->RegisterFunction("AddOverrideBool", "NiOverride", papyrusNiOverride::AddOverride<bool>);
	a_vm->RegisterFunction("AddOverrideString", "NiOverride", papyrusNiOverride::AddOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("AddOverrideTextureSet", "NiOverride", papyrusNiOverride::AddOverride<RE::BGSTextureSet*>);
	a_vm->RegisterFunction("ApplyOverrides", "NiOverride", papyrusNiOverride::ApplyOverrides);
	a_vm->RegisterFunction("HasArmorAddonNode", "NiOverride", papyrusNiOverride::HasArmorAddonNode);
	// Get Armor Overrides
	a_vm->RegisterFunction("GetOverrideFloat", "NiOverride", papyrusNiOverride::GetOverride<float>);
	a_vm->RegisterFunction("GetOverrideInt", "NiOverride", papyrusNiOverride::GetOverride<std::uint32_t>);
	a_vm->RegisterFunction("GetOverrideBool", "NiOverride", papyrusNiOverride::GetOverride<bool>);
	a_vm->RegisterFunction("GetOverrideString", "NiOverride", papyrusNiOverride::GetOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("GetOverrideTextureSet", "NiOverride", papyrusNiOverride::GetOverride<RE::BGSTextureSet*>);
	// Get Armor Properties
	a_vm->RegisterFunction("GetPropertyFloat", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<float>);
	a_vm->RegisterFunction("GetPropertyInt", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<std::uint32_t>);
	a_vm->RegisterFunction("GetPropertyBool", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<bool>);
	a_vm->RegisterFunction("GetPropertyString", "NiOverride", papyrusNiOverride::GetArmorAddonProperty<RE::BSFixedString>);
	// Node Overrides
	a_vm->RegisterFunction("HasNodeOverride", "NiOverride", papyrusNiOverride::HasNodeOverride);
	a_vm->RegisterFunction("AddNodeOverrideFloat", "NiOverride", papyrusNiOverride::AddNodeOverride<float>);
	a_vm->RegisterFunction("AddNodeOverrideInt", "NiOverride", papyrusNiOverride::AddNodeOverride<std::uint32_t>);
	a_vm->RegisterFunction("AddNodeOverrideBool", "NiOverride", papyrusNiOverride::AddNodeOverride<bool>);
	a_vm->RegisterFunction("AddNodeOverrideString", "NiOverride", papyrusNiOverride::AddNodeOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("AddNodeOverrideTextureSet", "NiOverride", papyrusNiOverride::AddNodeOverride<RE::BGSTextureSet*>);
	a_vm->RegisterFunction("ApplyNodeOverrides", "NiOverride", papyrusNiOverride::ApplyNodeOverrides);
	// Get Node Overrides
	a_vm->RegisterFunction("GetNodeOverrideFloat", "NiOverride", papyrusNiOverride::GetNodeOverride<float>);
	a_vm->RegisterFunction("GetNodeOverrideInt", "NiOverride", papyrusNiOverride::GetNodeOverride<std::uint32_t>);
	a_vm->RegisterFunction("GetNodeOverrideBool", "NiOverride", papyrusNiOverride::GetNodeOverride<bool>);
	a_vm->RegisterFunction("GetNodeOverrideString", "NiOverride", papyrusNiOverride::GetNodeOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("GetNodeOverrideTextureSet", "NiOverride", papyrusNiOverride::GetNodeOverride<RE::BGSTextureSet*>);
	// Get Node Properties
	a_vm->RegisterFunction("GetNodePropertyFloat", "NiOverride", papyrusNiOverride::GetNodeProperty<float>);
	a_vm->RegisterFunction("GetNodePropertyInt", "NiOverride", papyrusNiOverride::GetNodeProperty<std::uint32_t>);
	a_vm->RegisterFunction("GetNodePropertyBool", "NiOverride", papyrusNiOverride::GetNodeProperty<bool>);
	a_vm->RegisterFunction("GetNodePropertyString", "NiOverride", papyrusNiOverride::GetNodeProperty<RE::BSFixedString>);
	// Weapon Overrides
	a_vm->RegisterFunction("HasWeaponOverride", "NiOverride", papyrusNiOverride::HasWeaponOverride);
	a_vm->RegisterFunction("AddWeaponOverrideFloat", "NiOverride", papyrusNiOverride::AddWeaponOverride<float>);
	a_vm->RegisterFunction("AddWeaponOverrideInt", "NiOverride", papyrusNiOverride::AddWeaponOverride<std::uint32_t>);
	a_vm->RegisterFunction("AddWeaponOverrideBool", "NiOverride", papyrusNiOverride::AddWeaponOverride<bool>);
	a_vm->RegisterFunction("AddWeaponOverrideString", "NiOverride", papyrusNiOverride::AddWeaponOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("AddWeaponOverrideTextureSet", "NiOverride", papyrusNiOverride::AddWeaponOverride<RE::BGSTextureSet*>);
	a_vm->RegisterFunction("ApplyWeaponOverrides", "NiOverride", papyrusNiOverride::ApplyWeaponOverrides);
	a_vm->RegisterFunction("HasWeaponNode", "NiOverride", papyrusNiOverride::HasWeaponNode);
	// Get Weapon Overrides
	a_vm->RegisterFunction("GetWeaponOverrideFloat", "NiOverride", papyrusNiOverride::GetWeaponOverride<float>);
	a_vm->RegisterFunction("GetWeaponOverrideInt", "NiOverride", papyrusNiOverride::GetWeaponOverride<std::uint32_t>);
	a_vm->RegisterFunction("GetWeaponOverrideBool", "NiOverride", papyrusNiOverride::GetWeaponOverride<bool>);
	a_vm->RegisterFunction("GetWeaponOverrideString", "NiOverride", papyrusNiOverride::GetWeaponOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("GetWeaponOverrideTextureSet", "NiOverride", papyrusNiOverride::GetWeaponOverride<RE::BGSTextureSet*>);
	// Get Weapon Properties
	a_vm->RegisterFunction("GetWeaponPropertyFloat", "NiOverride", papyrusNiOverride::GetWeaponProperty<float>);
	a_vm->RegisterFunction("GetWeaponPropertyInt", "NiOverride", papyrusNiOverride::GetWeaponProperty<std::uint32_t>);
	a_vm->RegisterFunction("GetWeaponPropertyBool", "NiOverride", papyrusNiOverride::GetWeaponProperty<bool>);
	a_vm->RegisterFunction("GetWeaponPropertyString", "NiOverride", papyrusNiOverride::GetWeaponProperty<RE::BSFixedString>);
	// Skin Overrides
	a_vm->RegisterFunction("HasSkinOverride", "NiOverride", papyrusNiOverride::HasSkinOverride);
	a_vm->RegisterFunction("AddSkinOverrideFloat", "NiOverride", papyrusNiOverride::AddSkinOverride<float>);
	a_vm->RegisterFunction("AddSkinOverrideInt", "NiOverride", papyrusNiOverride::AddSkinOverride<std::uint32_t>);
	a_vm->RegisterFunction("AddSkinOverrideBool", "NiOverride", papyrusNiOverride::AddSkinOverride<bool>);
	a_vm->RegisterFunction("AddSkinOverrideString", "NiOverride", papyrusNiOverride::AddSkinOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("AddSkinOverrideTextureSet", "NiOverride", papyrusNiOverride::AddSkinOverride<RE::BGSTextureSet*>);
	a_vm->RegisterFunction("ApplySkinOverrides", "NiOverride", papyrusNiOverride::ApplySkinOverrides);
	// Get Skin Overrides
	a_vm->RegisterFunction("GetSkinOverrideFloat", "NiOverride", papyrusNiOverride::GetSkinOverride<float>);
	a_vm->RegisterFunction("GetSkinOverrideInt", "NiOverride", papyrusNiOverride::GetSkinOverride<std::uint32_t>);
	a_vm->RegisterFunction("GetSkinOverrideBool", "NiOverride", papyrusNiOverride::GetSkinOverride<bool>);
	a_vm->RegisterFunction("GetSkinOverrideString", "NiOverride", papyrusNiOverride::GetSkinOverride<RE::BSFixedString>);
	a_vm->RegisterFunction("GetSkinOverrideTextureSet", "NiOverride", papyrusNiOverride::GetSkinOverride<RE::BGSTextureSet*>);
	// Get Skin Properties
	a_vm->RegisterFunction("GetSkinPropertyFloat", "NiOverride", papyrusNiOverride::GetSkinProperty<float>);
	a_vm->RegisterFunction("GetSkinPropertyInt", "NiOverride", papyrusNiOverride::GetSkinProperty<std::uint32_t>);
	a_vm->RegisterFunction("GetSkinPropertyBool", "NiOverride", papyrusNiOverride::GetSkinProperty<bool>);
	a_vm->RegisterFunction("GetSkinPropertyString", "NiOverride", papyrusNiOverride::GetSkinProperty<RE::BSFixedString>);
	// Remove functions
	a_vm->RegisterFunction("RemoveAllOverrides", "NiOverride", papyrusNiOverride::RemoveAllOverrides);
	a_vm->RegisterFunction("RemoveAllReferenceOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceOverrides);
	a_vm->RegisterFunction("RemoveAllArmorOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorOverrides);
	a_vm->RegisterFunction("RemoveAllArmorAddonOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorAddonOverrides);
	a_vm->RegisterFunction("RemoveAllArmorAddonNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllArmorAddonNodeOverrides);
	a_vm->RegisterFunction("RemoveOverride", "NiOverride", papyrusNiOverride::RemoveArmorAddonOverride);
	// Node Remove functions
	a_vm->RegisterFunction("RemoveAllNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllNodeOverrides);
	a_vm->RegisterFunction("RemoveAllReferenceNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceNodeOverrides);
	a_vm->RegisterFunction("RemoveAllNodeNameOverrides", "NiOverride", papyrusNiOverride::RemoveAllNodeNameOverrides);
	a_vm->RegisterFunction("RemoveNodeOverride", "NiOverride", papyrusNiOverride::RemoveNodeOverride);
	// Remove Weapon functions
	a_vm->RegisterFunction("RemoveAllWeaponBasedOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponBasedOverrides);
	a_vm->RegisterFunction("RemoveAllReferenceWeaponOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceWeaponOverrides);
	a_vm->RegisterFunction("RemoveAllWeaponOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponOverrides);
	a_vm->RegisterFunction("RemoveAllWeaponNodeOverrides", "NiOverride", papyrusNiOverride::RemoveAllWeaponNodeOverrides);
	a_vm->RegisterFunction("RemoveWeaponOverride", "NiOverride", papyrusNiOverride::RemoveWeaponOverride);
	// Remove Skin functions
	a_vm->RegisterFunction("RemoveAllSkinBasedOverrides", "NiOverride", papyrusNiOverride::RemoveAllSkinBasedOverrides);
	a_vm->RegisterFunction("RemoveAllReferenceSkinOverrides", "NiOverride", papyrusNiOverride::RemoveAllReferenceSkinOverrides);
	a_vm->RegisterFunction("RemoveAllSkinOverrides", "NiOverride", papyrusNiOverride::RemoveAllSkinOverrides);
	a_vm->RegisterFunction("RemoveSkinOverride", "NiOverride", papyrusNiOverride::RemoveSkinOverride);
	// Body Morph Manipulation
	a_vm->RegisterFunction("HasBodyMorph", "NiOverride", papyrusNiOverride::HasBodyMorph);
	a_vm->RegisterFunction("SetBodyMorph", "NiOverride", papyrusNiOverride::SetBodyMorph);
	a_vm->RegisterFunction("GetBodyMorph", "NiOverride", papyrusNiOverride::GetBodyMorph);
	a_vm->RegisterFunction("ClearBodyMorph", "NiOverride", papyrusNiOverride::ClearBodyMorph);
	a_vm->RegisterFunction("HasBodyMorphKey", "NiOverride", papyrusNiOverride::HasBodyMorphKey);
	a_vm->RegisterFunction("ClearBodyMorphKeys", "NiOverride", papyrusNiOverride::ClearBodyMorphKeys);
	a_vm->RegisterFunction("HasBodyMorphName", "NiOverride", papyrusNiOverride::HasBodyMorphName);
	a_vm->RegisterFunction("ClearBodyMorphNames", "NiOverride", papyrusNiOverride::ClearBodyMorphNames);
	a_vm->RegisterFunction("ClearMorphs", "NiOverride", papyrusNiOverride::ClearMorphs);
	a_vm->RegisterFunction("UpdateModelWeight", "NiOverride", papyrusNiOverride::UpdateModelWeight);
	a_vm->RegisterFunction("GetMorphNames", "NiOverride", papyrusNiOverride::GetMorphNames);
	a_vm->RegisterFunction("GetMorphKeys", "NiOverride", papyrusNiOverride::GetMorphKeys);
	a_vm->RegisterFunction("GetMorphedReferences", "NiOverride", papyrusNiOverride::GetMorphedReferences);
	a_vm->RegisterFunction("ForEachMorphedReference", "NiOverride", papyrusNiOverride::ForEachMorphedReference);
	a_vm->RegisterFunction("GetCachedMorphNames", "NiOverride", papyrusNiOverride::GetCachedMorphNames);
	// Unique Item manipulation
	a_vm->RegisterFunction("GetItemUniqueID", "NiOverride", papyrusNiOverride::GetItemUniqueID);
	a_vm->RegisterFunction("GetObjectUniqueID", "NiOverride", papyrusNiOverride::GetObjectUniqueID);
	a_vm->RegisterFunction("GetFormFromUniqueID", "NiOverride", papyrusNiOverride::GetFormFromUniqueID);
	a_vm->RegisterFunction("GetOwnerOfUniqueID", "NiOverride", papyrusNiOverride::GetOwnerOfUniqueID);
	// DyeManager V1
	a_vm->RegisterFunction("SetItemDyeColor", "NiOverride", papyrusNiOverride::SetItemDyeColor);
	a_vm->RegisterFunction("GetItemDyeColor", "NiOverride", papyrusNiOverride::GetItemDyeColor);
	a_vm->RegisterFunction("ClearItemDyeColor", "NiOverride", papyrusNiOverride::ClearItemDyeColor);
	a_vm->RegisterFunction("UpdateItemDyeColor", "NiOverride", papyrusNiOverride::UpdateItemDyeColor);
	// DyeManager V2
	a_vm->RegisterFunction("SetItemTextureLayerColor", "NiOverride", papyrusNiOverride::SetItemTextureLayerColor);
	a_vm->RegisterFunction("GetItemTextureLayerColor", "NiOverride", papyrusNiOverride::GetItemTextureLayerColor);
	a_vm->RegisterFunction("ClearItemTextureLayerColor", "NiOverride", papyrusNiOverride::ClearItemTextureLayerColor);
	a_vm->RegisterFunction("SetItemTextureLayerType", "NiOverride", papyrusNiOverride::SetItemTextureLayerType);
	a_vm->RegisterFunction("GetItemTextureLayerType", "NiOverride", papyrusNiOverride::GetItemTextureLayerType);
	a_vm->RegisterFunction("ClearItemTextureLayerType", "NiOverride", papyrusNiOverride::ClearItemTextureLayerType);
	a_vm->RegisterFunction("SetItemTextureLayerTexture", "NiOverride", papyrusNiOverride::SetItemTextureLayerTexture);
	a_vm->RegisterFunction("GetItemTextureLayerTexture", "NiOverride", papyrusNiOverride::GetItemTextureLayerTexture);
	a_vm->RegisterFunction("ClearItemTextureLayerTexture", "NiOverride", papyrusNiOverride::ClearItemTextureLayerTexture);
	a_vm->RegisterFunction("SetItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::SetItemTextureLayerBlendMode);
	a_vm->RegisterFunction("GetItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::GetItemTextureLayerBlendMode);
	a_vm->RegisterFunction("ClearItemTextureLayerBlendMode", "NiOverride", papyrusNiOverride::ClearItemTextureLayerBlendMode);
	a_vm->RegisterFunction("UpdateItemTextureLayers", "NiOverride", papyrusNiOverride::UpdateItemTextureLayers);
	a_vm->RegisterFunction("EnableTintTextureCache", "NiOverride", papyrusNiOverride::EnableTintTextureCache);
	a_vm->RegisterFunction("ReleaseTintTextureCache", "NiOverride", papyrusNiOverride::ReleaseTintTextureCache);
	a_vm->RegisterFunction("IsFormDye", "NiOverride", papyrusNiOverride::IsFormDye);
	a_vm->RegisterFunction("GetFormDyeColor", "NiOverride", papyrusNiOverride::GetFormDyeColor);
	a_vm->RegisterFunction("RegisterFormDyeColor", "NiOverride", papyrusNiOverride::RegisterFormDyeColor);
	a_vm->RegisterFunction("UnregisterFormDyeColor", "NiOverride", papyrusNiOverride::UnregisterFormDyeColor);
	// Position Transforms
	a_vm->RegisterFunction("HasNodeTransformPosition", "NiOverride", papyrusNiOverride::HasNodeTransformPosition);
	a_vm->RegisterFunction("AddNodeTransformPosition", "NiOverride", papyrusNiOverride::AddNodeTransformPosition);
	a_vm->RegisterFunction("GetNodeTransformPosition", "NiOverride", papyrusNiOverride::GetNodeTransformPosition);
	a_vm->RegisterFunction("RemoveNodeTransformPosition", "NiOverride", papyrusNiOverride::RemoveNodeTransformPosition);
	// Scale Transforms
	a_vm->RegisterFunction("HasNodeTransformScale", "NiOverride", papyrusNiOverride::HasNodeTransformScale);
	a_vm->RegisterFunction("AddNodeTransformScale", "NiOverride", papyrusNiOverride::AddNodeTransformScale);
	a_vm->RegisterFunction("GetNodeTransformScale", "NiOverride", papyrusNiOverride::GetNodeTransformScale);
	a_vm->RegisterFunction("RemoveNodeTransformScale", "NiOverride", papyrusNiOverride::RemoveNodeTransformScale);
	// Rotation Transforms
	a_vm->RegisterFunction("HasNodeTransformRotation", "NiOverride", papyrusNiOverride::HasNodeTransformRotation);
	a_vm->RegisterFunction("AddNodeTransformRotation", "NiOverride", papyrusNiOverride::AddNodeTransformRotation);
	a_vm->RegisterFunction("GetNodeTransformRotation", "NiOverride", papyrusNiOverride::GetNodeTransformRotation);
	a_vm->RegisterFunction("RemoveNodeTransformRotation", "NiOverride", papyrusNiOverride::RemoveNodeTransformRotation);
	// ScaleMode Transforms
	a_vm->RegisterFunction("HasNodeTransformScaleMode", "NiOverride", papyrusNiOverride::HasNodeTransformScaleMode);
	a_vm->RegisterFunction("AddNodeTransformScaleMode", "NiOverride", papyrusNiOverride::AddNodeTransformScaleMode);
	a_vm->RegisterFunction("GetNodeTransformScaleMode", "NiOverride", papyrusNiOverride::GetNodeTransformScaleMode);
	a_vm->RegisterFunction("RemoveNodeTransformScaleMode", "NiOverride", papyrusNiOverride::RemoveNodeTransformScaleMode);
	a_vm->RegisterFunction("UpdateAllReferenceTransforms", "NiOverride", papyrusNiOverride::UpdateAllReferenceTransforms);
	a_vm->RegisterFunction("UpdateNodeTransform", "NiOverride", papyrusNiOverride::UpdateNodeTransform);
	a_vm->RegisterFunction("RemoveAllReferenceTransforms", "NiOverride", papyrusNiOverride::RemoveAllReferenceTransforms);
	a_vm->RegisterFunction("RemoveAllTransforms", "NiOverride", papyrusNiOverride::RemoveAllTransforms);
	a_vm->RegisterFunction("GetInverseTransform", "NiOverride", papyrusNiOverride::GetInverseTransform);
	a_vm->RegisterFunction("SetNodeDestination", "NiOverride", papyrusNiOverride::SetNodeDestination);
	a_vm->RegisterFunction("RemoveNodeDestination", "NiOverride", papyrusNiOverride::RemoveNodeDestination);
	a_vm->RegisterFunction("GetNodeDestination", "NiOverride", papyrusNiOverride::GetNodeDestination);
	a_vm->RegisterFunction("GetNodeTransformNames", "NiOverride", papyrusNiOverride::GetNodeTransformNames);
	a_vm->RegisterFunction("GetNodeTransformKeys", "NiOverride", papyrusNiOverride::GetNodeTransformKeys);
	// Extra Data Testers
	a_vm->RegisterFunction("GetBooleanExtraData", "NiOverride", papyrusNiOverride::GetExtraData<bool>);
	a_vm->RegisterFunction("GetIntegerExtraData", "NiOverride", papyrusNiOverride::GetExtraData<std::int32_t>);
	a_vm->RegisterFunction("GetIntegersExtraData", "NiOverride", papyrusNiOverride::GetExtraData<std::vector<std::int32_t>>);
	a_vm->RegisterFunction("GetFloatExtraData", "NiOverride", papyrusNiOverride::GetExtraData<float>);
	a_vm->RegisterFunction("GetFloatsExtraData", "NiOverride", papyrusNiOverride::GetExtraData<std::vector<float>>);
	a_vm->RegisterFunction("GetStringExtraData", "NiOverride", papyrusNiOverride::GetExtraData<RE::BSFixedString>);
	a_vm->RegisterFunction("GetStringsExtraData", "NiOverride", papyrusNiOverride::GetExtraData<std::vector<RE::BSFixedString>>);
	// Mesh Manipulation
	// R is the type returned to the script via ReturnLatentResult (legacy LatentNativeFunction result type: bool)
	a_vm->RegisterLatentFunction<bool>("AttachMesh", "NiOverride", papyrusNiOverride::AttachMesh);
	a_vm->RegisterLatentFunction<bool>("DetachMesh", "NiOverride", papyrusNiOverride::DetachMesh);

	// Legacy g_objectInterface->GetObjectRegistry().RegisterClass<T>() — the SKSE object
	// registry maps ClassName() to a factory so co-saved latent functors can be rebuilt.
	SKSE::GetObjectInterface()->GetObjectRegistry().RegisterFactory(new SKSE::ConcreteSKSEObjectFactory<AttachMeshLatentFunctor>());
	SKSE::GetObjectInterface()->GetObjectRegistry().RegisterFactory(new SKSE::ConcreteSKSEObjectFactory<DetachMeshLatentFunctor>());
	// Extra data manipulation
	a_vm->SetCallableFromTasklets("NiOverride", "GetBooleanExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetIntegerExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetIntegersExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetFloatExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetFloatsExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetStringExtraData", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetStringsExtraData", true);
	// Overlay numerics
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumBodyOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumHandOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumFeetOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumFaceOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumSpellBodyOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumSpellHandOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumSpellFeetOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNumSpellFaceOverlays", true);
	// Armor based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "HasOverride", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ApplyOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetPropertyFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetPropertyInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetPropertyBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetPropertyString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasArmorAddonNode", true);
	// Node based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "HasNodeOverride", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodePropertyFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodePropertyInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodePropertyBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodePropertyString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ApplyNodeOverrides", true);
	// Weapon based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "HasWeaponOverride", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddWeaponOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddWeaponOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddWeaponOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddWeaponOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddWeaponOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ApplyWeaponOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponPropertyFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponPropertyInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponPropertyBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetWeaponPropertyString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasWeaponNode", true);
	// Skin based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "HasSkinOverride", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddSkinOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddSkinOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddSkinOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddSkinOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddSkinOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinOverrideFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinOverrideInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinOverrideBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinOverrideString", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinOverrideTextureSet", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ApplySkinOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinPropertyFloat", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinPropertyInt", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinPropertyBool", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetSkinPropertyString", true);
	// Armor based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllReferenceOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllArmorOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllArmorAddonOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllArmorAddonNodeOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveOverride", true);
	// Node based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllNodeOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllReferenceNodeOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllNodeNameOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveNodeOverride", true);
	// Weapon based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllWeaponBasedOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllReferenceWeaponOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllWeaponOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllWeaponNodeOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveWeaponOverride", true);
	// Skin based overrides
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllSkinBasedOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllReferenceSkinOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllSkinOverrides", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveSkinOverride", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RevertOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RevertOverlay", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RevertHeadOverlays", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RevertHeadOverlay", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasBodyMorph", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetBodyMorph", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetBodyMorph", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearBodyMorph", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasBodyMorphName", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearBodyMorphNames", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasBodyMorphKey", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearBodyMorphKeys", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearMorphs", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UpdateModelWeight", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetCachedMorphNames", true);
	a_vm->SetCallableFromTasklets("NiOverride", "EnableTintTextureCache", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ReleaseTintTextureCache", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetItemDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetItemDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearItemDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UpdateItemDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetItemTextureLayerColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetItemTextureLayerColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearItemTextureLayerColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetItemTextureLayerTexture", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetItemTextureLayerTexture", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearItemTextureLayerTexture", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetItemTextureLayerBlendMode", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetItemTextureLayerBlendMode", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearItemTextureLayerBlendMode", true);
	a_vm->SetCallableFromTasklets("NiOverride", "SetItemTextureLayerType", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetItemTextureLayerType", true);
	a_vm->SetCallableFromTasklets("NiOverride", "ClearItemTextureLayerType", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UpdateItemTextureLayers", true);
	a_vm->SetCallableFromTasklets("NiOverride", "IsFormDye", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetFormDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RegisterFormDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UnregisterFormDyeColor", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasNodeTransformPosition", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeTransformPosition", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeTransformPosition", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveNodeTransformPosition", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasNodeTransformScale", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeTransformScale", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeTransformScale", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveNodeTransformScale", true);
	a_vm->SetCallableFromTasklets("NiOverride", "HasNodeTransformRotation", true);
	a_vm->SetCallableFromTasklets("NiOverride", "AddNodeTransformRotation", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeTransformRotation", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveNodeTransformRotation", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeDestination", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveNodeDestination", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeTransformNames", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetNodeTransformKeys", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UpdateAllReferenceTransforms", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllReferenceTransforms", true);
	a_vm->SetCallableFromTasklets("NiOverride", "RemoveAllTransforms", true);
	a_vm->SetCallableFromTasklets("NiOverride", "UpdateNodeTransform", true);
	a_vm->SetCallableFromTasklets("NiOverride", "GetInverseTransform", true);
}