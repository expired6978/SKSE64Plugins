#include <REX/W32/KERNEL32.h>
#include "SKEETasks.h"
#include <REX/W32/USER32.h>

#include <RE/B/BGSHeadPart.h>
#include <RE/B/BSFaceGenManager.h>
#include <RE/B/BSFaceGenModel.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSTriShape.h>
#include <RE/B/BSDynamicTriShape.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSLightingShaderMaterialHairTint.h>
#include <RE/B/BSPointerHandle.h>
#include <RE/B/BipedAnim.h>
#include <RE/C/CommandTable.h>
#include <RE/E/ExtraDataList.h>
#include <RE/E/ExtraRank.h>
#include <RE/E/ExtraUniqueID.h>
#include <RE/F/FxDelegate.h>
#include <RE/G/GArray.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <RE/I/Inventory3DManager.h>
#include "InventoryPreviewPolicy.h"
#include <RE/N/NiAVObject.h>
#include <RE/N/NiGeometryData.h>
#include <RE/N/NiBooleanExtraData.h>
#include <RE/N/NiColor.h>
#include <RE/N/NiExtraData.h>
#include <RE/N/NiNode.h>
#include <RE/R/RaceSexMenu.h>
#include <RE/R/Renderer.h>
#include <RE/S/Script.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESRace.h>

#include "RE/N/NiRTTI.h"

#include "SKEEHooks.h"

#include "ActorUpdateManager.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "TintMaskInterface.h"
#include "ItemDataInterface.h"
#include "NiTransformInterface.h"
#include "AttachmentInterface.h"
#include "CommandInterface.h"
#include "PresetInterface.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"

#include "SkeletonExtender.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"

#include "PatternScan.h"

#include <vector>

#include "REL/Relocation.h"
#include "SKSE/Trampoline.h"
#include "xbyak/xbyak.h"

#include <queue>

#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <optional>

// The real windows.h (via the PCH) maps the Interlocked* API names to their
// underscored forms as macros; drop them so REX::W32's declarations resolve.
#ifdef InterlockedDecrement
#undef InterlockedDecrement
#endif
#ifdef InterlockedIncrement
#undef InterlockedIncrement
#endif

// Win32 MessageBox constants (REX/W32 does not define them; values from winuser.h).
namespace
{
	constexpr std::uint32_t kMB_ICONWARNING = 0x00000030;
	constexpr std::uint32_t kMB_YESNO = 0x00000004;
	constexpr std::int32_t kIDYES = 0x0006;
}

extern const SKSE::TaskInterface* g_task;

extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern OverrideInterface	g_overrideInterface;
extern ActorUpdateManager	g_actorUpdateManager;
extern NiTransformInterface	g_transformInterface;
extern CommandInterface		g_commandInterface;
extern PresetInterface		g_presetInterface;

extern bool					g_enableFaceOverlays;
extern bool					g_enableOverlays;
extern bool					g_enableSculpting;
extern bool					g_enableHeadExport;
extern bool					g_enableTintSync;
extern bool					g_enableTintInventory;
extern std::uint32_t				g_numFaceOverlays;

extern bool					g_playerOnly;
extern std::uint32_t				g_numSpellFaceOverlays;

extern bool					g_immediateArmor;
extern bool					g_immediateFace;

extern bool					g_enableEquippableTransforms;
extern bool					g_disableFaceGenCache;

RE::Actor						* g_weaponHookActor = NULL;
RE::TESObjectWEAP			* g_weaponHookWeapon = nullptr;
std::uint32_t						g_firstPerson = 0;

extern FaceMorphInterface			g_morphInterface;
extern PartSet				g_partSet;
extern std::uint32_t				g_customDataMax;
extern bool					g_externalHeads;
extern bool					g_extendedMorphs;
extern bool					g_allowAllMorphs;
extern bool					g_allowAnyRacePart;
extern bool					g_allowAnyGenderPart;

// Hook Toggles
extern bool					g_hookBipedAttach;
extern bool					g_hookNativeSliders;
extern bool					g_hookSliderCallbacks;
extern bool					g_hookHeadPreprocessing;
extern bool					g_hookMorphUpdates;
extern bool					g_hookMorphExtensions;
extern bool					g_suppressPatternWarnings;
extern bool					g_hookTintInventory;
extern bool					g_hookTinting;
extern bool					g_hookFaceOverlays;

// Originals of hooked functions (declared in SKEEHooks.h). InstallSKEEHooks()
// sets each to a code-cave trampoline that calls through to the unpatched
// game function; nullptr until the corresponding hook is installed.
AttachBipedObjectFn        AttachBipedObject_Original = nullptr;
RegenerateHeadFn           RegenerateHead_Original = nullptr;
BSFaceGenModelApplyMorphFn BSFaceGenModel_ApplyMorph_Original = nullptr;
SetInventoryItemModelFn    SetInventoryItemModel_Original = nullptr;
TransferItemUIDFn          TransferItemUID_Original = nullptr;

#if defined(ENABLE_SKYRIM_VR)
namespace
{
	using GetHeadPartsVRFn = void* (*)(void*, void*);
	using AddRaceMenuSliderVRFn = std::int32_t (*)(RE::RaceMenuSliderArray*, RE::RaceMenuSlider*);
	using DoubleMorphCallbackVRFn = void (*)(RE::RaceSexMenu*, float, std::uint32_t);
	using UpdateNPCMorphsVRFn = void (*)(RE::TESNPC*, void*, RE::BSFaceGenNiNode*);
	using UpdateNPCMorphVRFn = void (*)(RE::TESNPC*, RE::BGSHeadPart*, RE::BSFaceGenNiNode*);
	using UpdateHeadStateVRFn = std::int32_t (*)(RE::TESNPC*, RE::Actor*, std::uint32_t);
	using SetNewInventoryItemModelVRFn = void (*)(RE::Inventory3DManager*, RE::TESForm*, RE::TESForm*, RE::NiNode**);

	GetHeadPartsVRFn             g_getHeadPartsVROriginal{ nullptr };
	AddRaceMenuSliderVRFn        g_addRaceMenuSliderVROriginal{ nullptr };
	DoubleMorphCallbackVRFn      g_doubleMorphCallbackVROriginal{ nullptr };
	BSFaceGenModelApplyMorphFn   g_applyRaceMorphVROriginal{ nullptr };
	UpdateNPCMorphsVRFn          g_updateNPCMorphsVROriginal{ nullptr };
	UpdateNPCMorphVRFn           g_updateNPCMorphVROriginal{ nullptr };
	UpdateHeadStateVRFn          g_updateHeadStateVROriginal{ nullptr };
	SetNewInventoryItemModelVRFn g_setNewInventoryItemModelVROriginal{ nullptr };
}
#endif

namespace SKEE
{
	bool HasQualifiedVRHelperAddresses() noexcept
	{
#if defined(ENABLE_SKYRIM_VR)
		if (!REL::Module::IsVR() || REL::Module::get().version() != REL::Version{ 1, 4, 15, 0 }) {
			return false;
		}

		// Cache the result before any hook commits. Several helpers are themselves
		// detour targets, so re-reading their prologs after installation would
		// correctly observe our branches rather than the pristine executable.
		static const bool qualified = []() {
			const auto text = REL::Module::get().segment(REL::Segment::Name::textx);
			const auto textBegin = text.address();
			const auto textEnd = textBegin + text.size();
			auto validate = [textBegin, textEnd]<std::size_t N>(
				std::string_view a_name,
				std::uintptr_t a_rva,
				const std::array<std::uint8_t, N>& a_expected) {
				const auto address = REL::Offset(a_rva).address();
				const bool inside = address >= textBegin && address <= textEnd &&
					a_expected.size() <= textEnd - address;
				if (!inside || std::memcmp(reinterpret_cast<const void*>(address), a_expected.data(), a_expected.size()) != 0) {
					SKSE::log::critical(
						"SkyrimVR helper {} failed exact 1.4.15 qualification at RVA 0x{:X}",
						a_name,
						a_rva);
					return false;
				}
				return true;
			};

			// These entry windows were read independently from the retained exact
			// SkyrimVR.exe image (timestamp 0x5AEADAA0, SizeOfImage 0x3959000).
			// They protect locally implemented RaceMenu NG calls; no SKEEVR code or
			// binary is used to define the functions or their behavior.
			return
				validate("BSLightingShaderProperty::InitializeShader", 0x01303AC0,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x6C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x18 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 } }) &&
				validate("BSLightingShaderMaterial::CopyFrom", 0x0130DE50,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x74 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x18 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x20 } }) &&
				validate("TaskQueueInterface::SetNiGeometryTexture", 0x005CF190,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x53 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x81 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0xB0 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 } }) &&
				validate("BSFaceGenManager::ApplyMorph", 0x003E1B80,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x6C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x18 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x7C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x20 }, std::uint8_t{ 0x41 } }) &&
				validate("BSFaceGenManager::ApplyMorphByPart", 0x003E1CE0,
					std::array{ std::uint8_t{ 0x4D }, std::uint8_t{ 0x85 }, std::uint8_t{ 0xC0 }, std::uint8_t{ 0x0F }, std::uint8_t{ 0x84 }, std::uint8_t{ 0x3E }, std::uint8_t{ 0x01 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x50 } }) &&
				validate("BSFaceGenModel::ApplyRaceMorph", 0x003E3FA0,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x40 } }) &&
				validate("UpdateNPCMorphs", 0x00370140,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0xC4 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x81 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x80 }, std::uint8_t{ 0x01 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 } }) &&
				validate("UpdateNPCMorph", 0x00370330,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0xC4 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x81 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x80 }, std::uint8_t{ 0x01 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 } }) &&
				validate("UpdateHeadState", 0x003727B0,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x57 } }) &&
				validate("Actor::ChangeHeadPart", 0x003EBD30,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x74 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x18 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x20 } }) &&
				validate("UpdateModelFace", 0x003EB710,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x28 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x8D }, std::uint8_t{ 0x54 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x38 }, std::uint8_t{ 0xC6 }, std::uint8_t{ 0x44 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x38 }, std::uint8_t{ 0x01 } }) &&
				validate("UpdateModelSkin", 0x003EC090,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x54 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x53 } }) &&
				validate("UpdateModelHair", 0x003EC150,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x54 }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x53 } }) &&
				validate("AddRaceMenuSlider", 0x008E9850,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x40 } }) &&
				validate("DoubleMorphCallback", 0x008E23B0,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0xC4 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x54 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x57 } }) &&
				validate("RaceSexMenu::LoadSliders", 0x008E39B0,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0xC4 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x50 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x08 } }) &&
				validate("NiTriBasedGeom::ctor", 0x00CC7420,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x4C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x08 }, std::uint8_t{ 0x55 }, std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 } }) &&
				validate("CreateBSTriShape", 0x00CAD6D0,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x30 } }) &&
				validate("CreateBSDynamicTriShape", 0x00CB8530,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x30 } }) &&
				validate("NiStream::ctor", 0x00C9EC40,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x4C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x08 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x40 } }) &&
				validate("NiStream::dtor", 0x00C9EEA0,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x30 } }) &&
				validate("NiStream::AddObject", 0x00C9F090,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x30 } }) &&
				validate("BSFadeNode::ctor", 0x012C8230,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x53 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x20 } }) &&
				validate("CreateSourceTexture", 0x00CAEF60,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x30 } }) &&
				validate("SetNewInventoryItemModel", 0x008B5B40,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x55 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x56 } }) &&
				validate("InventoryChanges::SetUniqueID", 0x001FD7D0,
					std::array{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x85 }, std::uint8_t{ 0xD2 }, std::uint8_t{ 0x0F }, std::uint8_t{ 0x84 }, std::uint8_t{ 0xC0 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 } }) &&
				validate("FxDelegate::Invoke target", 0x00F342E0,
					std::array{ std::uint8_t{ 0x40 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x48 }, std::uint8_t{ 0x83 }, std::uint8_t{ 0xEC }, std::uint8_t{ 0x40 } });
		}();

		static const bool reported = []() {
			if (qualified) {
				SKSE::log::info("All direct SkyrimVR helper entry points match the qualified 1.4.15 executable windows");
			}
			return true;
		}();
		(void)reported;
		return qualified;
#else
		return false;
#endif
	}

	void InvalidateTextures(RE::BSLightingShaderProperty* a_this, std::uint32_t a_unk1)
	{
		if (!a_this) {
			return;
		}

#if defined(ENABLE_SKYRIM_VR)
		if (REL::Module::IsVR()) {
			a_this->InvalidateTextures(a_unk1);
			return;
		}
#endif

		static REL::Relocation<void (*)(RE::BSLightingShaderProperty*, std::uint32_t)> func{
			REL::RelocationID(0, kID_InvalidateTextures)
		};
		func(a_this, a_unk1);
	}

	bool LookupREFRByHandle(std::uint32_t& a_handle, RE::NiPointer<RE::TESObjectREFR>& a_refr)
	{
		return RE::TESObjectREFR::LookupByHandle(a_handle, a_refr);
	}
}

static void InstallArmorAddonHook(RE::TESObjectREFR* refr, RE::TESForm* armor, RE::TESObjectARMA* addon, RE::NiNode* boneTree, RE::NiAVObject* resultNode);
static void __stdcall InstallFaceOverlayHook(RE::TESObjectREFR* refr, bool attemptUninstall, bool immediate);

void __stdcall InstallWeaponHook(RE::Actor * actor, RE::TESObjectWEAP * weapon, RE::NiAVObject * resultNode1, RE::NiAVObject * resultNode2, std::uint32_t firstPerson)
{
	if (!actor) {
#ifdef _DEBUG
		SKSE::log::info("{} - Error no reference found skipping overrides.", __FUNCTION__);
#endif
		return;
	}
	if (!weapon) {
#ifdef _DEBUG
		SKSE::log::info("{} - Error no weapon found skipping overrides.", __FUNCTION__);
#endif
		return;
	}

	std::vector<RE::TESObjectWEAP*> flattenedWeapons;
	flattenedWeapons.push_back(weapon);
	RE::TESObjectWEAP * templateWeapon = weapon->templateWeapon;
	while (templateWeapon) {
		flattenedWeapons.push_back(templateWeapon);
		templateWeapon = templateWeapon->templateWeapon;
	}

	// Apply top-most parent properties first
	for (std::vector<RE::TESObjectWEAP*>::reverse_iterator it = flattenedWeapons.rbegin(); it != flattenedWeapons.rend(); ++it)
	{
		if (resultNode1)
			g_overrideInterface.Impl_ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode1, true);
		if (resultNode2)
			g_overrideInterface.Impl_ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode2, true);
	}
}

#ifdef FIXME
typedef RE::NiAVObject * (*_CreateWeaponNode)(std::uint32_t * unk1, std::uint32_t unk2, RE::Actor * actor, std::uint32_t ** unk4, std::uint32_t * unk5);
extern const _CreateWeaponNode CreateWeaponNode = (_CreateWeaponNode)0x0046F530;

// Store stack values here, they would otherwise be lost
enum
{
	kWeaponHook_EntryStackOffset1 = 0x40,
	kWeaponHook_EntryStackOffset2 = 0x20,
	kWeaponHook_VarObj = 0x04
};

static const std::uint32_t kInstallWeaponFPHook_Base = 0x0046F870 + 0x143;
static const std::uint32_t kInstallWeaponFPHook_Entry_retn = kInstallWeaponFPHook_Base + 0x5;

__declspec(naked) void CreateWeaponNodeFPHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kWeaponHook_EntryStackOffset1 + kWeaponHook_VarObj + kWeaponHook_EntryStackOffset2]
		mov		g_weaponHookWeapon, eax
		mov		g_weaponHookActor, edi
		mov		g_firstPerson, 1
		popad

		call[CreateWeaponNode]

		mov		g_weaponHookWeapon, NULL
		mov		g_weaponHookActor, NULL

		jmp[kInstallWeaponFPHook_Entry_retn]
	}
}

static const std::uint32_t kInstallWeapon3PHook_Base = 0x0046F870 + 0x17E;
static const std::uint32_t kInstallWeapon3PHook_Entry_retn = kInstallWeapon3PHook_Base + 0x5;

__declspec(naked) void CreateWeaponNode3PHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kWeaponHook_EntryStackOffset1 + kWeaponHook_VarObj + kWeaponHook_EntryStackOffset2]
		mov		g_weaponHookWeapon, eax
		mov		g_weaponHookActor, edi
		mov		g_firstPerson, 0
		popad

		call[CreateWeaponNode]

		mov		g_weaponHookWeapon, NULL
		mov		g_weaponHookActor, NULL

		jmp[kInstallWeapon3PHook_Entry_retn]
	}
}

// Recall stack values here
static const std::uint32_t kInstallWeaponHook_Base = 0x0046F530 + 0x28A;
static const std::uint32_t kInstallWeaponHook_Entry_retn = kInstallWeaponHook_Base + 0x5;

__declspec(naked) void InstallWeaponNodeHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, g_firstPerson
		push	eax
		push	ebp
		push	edx
		mov		eax, g_weaponHookWeapon
		push	eax
		mov		eax, g_weaponHookActor
		push	eax
		call	InstallWeaponHook
		popad

		push	ebx
		push	ecx
		push	edx
		push	ebp
		push	esi

		jmp[kInstallWeaponHook_Entry_retn]
	}
}

std::uint8_t * g_unk1 = (std::uint8_t*)0x001240DE8;
std::uint32_t * g_unk2 = (std::uint32_t*)0x001310588;

#endif

RE::NiAVObject * AttachBipedObject_Hooked(RE::BipedAnim * bipedInfo, RE::NiNode * objectRoot, std::uint32_t bipedIndex, std::uint8_t unkIndex, std::uint8_t unk5, std::uint64_t unk6)
{
	RE::NiAVObject * retVal = AttachBipedObject_Original(bipedInfo, objectRoot, bipedIndex, unkIndex, unk5, unk6);

	RE::NiPointer<RE::TESObjectREFR> reference;
	std::uint32_t handle = bipedInfo->actorRef.native_handle();
	SKEE::LookupREFRByHandle(handle, reference);
	if (reference)
		InstallArmorAddonHook(
			reference.get(),
			bipedInfo->objects[bipedIndex].item,
			bipedInfo->objects[bipedIndex].addon,
			bipedInfo->root,
			retVal);

	return retVal;
}

static void InstallArmorAddonHook(RE::TESObjectREFR* refr, RE::TESForm* armor, RE::TESObjectARMA* addon, RE::NiNode* boneTree, RE::NiAVObject* resultNode)
{
	if (!refr) {
#ifdef _DEBUG
		SKSE::log::error("{} - Error no reference found", __FUNCTION__);
#endif
		return;
	}
	if (!armor || !addon) {
#ifdef _DEBUG
		SKSE::log::error("{} - Armor or ArmorAddon found.", __FUNCTION__);
#endif
		return;
	}
	if (!boneTree) {
#ifdef _DEBUG
		SKSE::log::error("{} - Error no bone tree found", __FUNCTION__);
#endif
		return;
	}
	if (!resultNode) {
#ifdef _DEBUG
		std::uint32_t addonFormid = addon ? addon->formID : 0;
		std::uint32_t armorFormid = armor ? armor->formID : 0;
		SKSE::log::error("{} - Error no node found on Reference ({:08X}) while attaching ArmorAddon ({:08X}) of Armor ({:08X})", __FUNCTION__, refr->formID, addonFormid, armorFormid);
#endif
		return;
	}

	RE::NiNode * node3P = static_cast<RE::NiNode*>(refr->Get3D(false));
	RE::NiNode * node1P = static_cast<RE::NiNode*>(refr->Get3D(true));

	// Go up to the root and see which one it is
	RE::NiNode * rootNode = nullptr;
	RE::NiNode * parent = boneTree->parent;
	do
	{
		if (parent == node1P)
			rootNode = node1P;
		if (parent == node3P)
			rootNode = node3P;
		parent = parent->parent;
	} while (parent);

	bool isFirstPerson = (rootNode == node1P);
	if (node1P == node3P) { // Theres only one node, theyre the same, no 1st person
		isFirstPerson = false;
	}

	if (rootNode != node1P && rootNode != node3P) {
#ifdef _DEBUG
		SKSE::log::debug("{} - Mismatching root nodes, bone tree not for this reference ({:08X})", __FUNCTION__, refr->formID);
#endif
		return;
	}
	if (armor->IsArmor() && addon->Is(RE::TESObjectARMA::FORMTYPE))
	{
		g_actorUpdateManager.OnAttach(refr, static_cast<RE::TESObjectARMO*>(armor), addon, resultNode, isFirstPerson, isFirstPerson ? node1P : node3P, boneTree);
	}

	
}

static void __stdcall InstallFaceOverlayHook(RE::TESObjectREFR* refr, bool attemptUninstall, bool immediate)
{
	if (!refr) {
#ifdef _DEBUG
		SKSE::log::debug("{} - Warning no reference found skipping overlay", __FUNCTION__);
#endif
		return;
	}

	if (!refr->GetFaceNodeSkinned()) {
#ifdef _DEBUG
		SKSE::log::debug("{} - Warning no head node for {:08X} skipping overlay", __FUNCTION__, refr->formID);
#endif
		return;
	}

#ifdef _DEBUG
	SKSE::log::debug("{} - Attempting to install face overlay to {:08X} - Flags {:08X}", __FUNCTION__, refr->formID, refr->GetFaceNodeSkinned()->flags);
#endif

	if ((refr == RE::PlayerCharacter::GetSingleton() && g_playerOnly) || !g_playerOnly || g_overlayInterface.HasOverlays(refr))
	{
		// Face
		for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, nodeName.c_str());
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					SKEE_AddTask(g_task, task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, nodeName.c_str(), FACE_MESH, RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				SKEE_AddTask(g_task, task);
			}
		}
		for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE_SPELL, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, nodeName.c_str());
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					SKEE_AddTask(g_task, task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, nodeName.c_str(), FACE_MAGIC_MESH, RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				SKEE_AddTask(g_task, task);
			}
		}
	}
}

std::int32_t UpdateHeadState_Enable_Hooked(RE::TESNPC * npc, RE::Actor * actor, std::uint32_t unk1)
{
	std::int32_t ret = 0;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		ret = g_updateHeadStateVROriginal ? g_updateHeadStateVROriginal(npc, actor, unk1) : 0;
	} else
#endif
	{
		ret = SKEE::UpdateHeadState(npc, actor, unk1);
	}
	InstallFaceOverlayHook(actor, true, g_immediateFace);
	return ret;
}

std::int32_t UpdateHeadState_Disabled_Hooked(RE::TESNPC * npc, RE::Actor * actor, std::uint32_t unk1)
{
	std::int32_t ret = 0;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		ret = g_updateHeadStateVROriginal ? g_updateHeadStateVROriginal(npc, actor, unk1) : 0;
	} else
#endif
	{
		ret = SKEE::UpdateHeadState(npc, actor, unk1);
	}
	InstallFaceOverlayHook(actor, false, g_immediateFace);
	return ret;
}

#include "REX/W32/D3D11_4.h"
#include "CDXD3DDevice.h"
#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXCamera.h"
#include "Utilities.h"


extern CDXD3DDevice					* g_Device;
extern CDXNifScene					g_World;
extern CDXModelViewerCamera			g_Camera;

void RaceSexMenu_Render_Hooked(RE::RaceSexMenu * rsm)
{
	if (g_Device && g_World.IsVisible() && g_World.GetRenderTargetView().Get()) {
		utils::ScopedCriticalSection cs(&RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().lock);
		g_World.Begin(&g_Camera, g_Device);
		g_World.Render(&g_Camera, g_Device);
		g_World.End(&g_Camera, g_Device);
	}

	if (rsm->uiMovie)
		rsm->uiMovie->Display();
}

void RegenerateHead_Hooked(RE::BSFaceGenManager * faceGen, RE::BSFaceGenNiNode * headNode, RE::BGSHeadPart * headPart, RE::TESNPC * npc)
{
	RegenerateHead_Original(faceGen, headNode, headPart, npc);
	g_presetInterface.ApplyMappedPreset(npc, headNode, headPart);
}

bool UsePreprocessedHead(RE::TESNPC * npc)
{
	// For some reason the NPC vanilla preset data is reset when the actor is disable/enabled
	auto presetData = g_presetInterface.GetMappedPreset(npc);
	if (presetData) {
		if (!npc->faceData)
			npc->faceData = (RE::TESNPC::FaceData*)RE::malloc(sizeof(RE::TESNPC::FaceData));

		std::uint32_t i = 0;
		for (auto & preset : presetData->presets) {
			npc->faceData->parts[i] = preset;
			i++;
		}

		i = 0;
		for (auto & morph : presetData->morphs) {
			npc->faceData->morphs[i] = morph;
			i++;
		}
	}
	if (REL::Module::IsVR()) {
		static REL::Relocation<bool> useFaceGenPreProcessedHeads{ REL::Offset(0x01EA71B0) };
		return presetData == nullptr && useFaceGenPreProcessedHeads.get();
	}

	static REL::Relocation<bool> useFaceGenPreProcessedHeads{ REL::RelocationID(0, kID_useFaceGenPreProcessedHeads) };
	return presetData == nullptr && useFaceGenPreProcessedHeads.get();
}

void _cdecl ClearFaceGenCache_Hooked()
{
	g_morphInterface.RevertInternals();
	g_partSet.Revert(); // Cleanup HeadPart List before loading new ones
}

void UpdateMorphs_Hooked(RE::TESNPC * npc, void * unk1, RE::BSFaceGenNiNode * faceNode)
{
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		if (g_updateNPCMorphsVROriginal) {
			g_updateNPCMorphsVROriginal(npc, unk1, faceNode);
		}
	} else
#endif
	{
		SKEE::UpdateNPCMorphs(npc, unk1, faceNode);
	}
#ifdef _DEBUG_HOOK
	SKSE::log::debug("UpdateMorphs_Hooked - Applying custom morphs");
#endif
	try
	{
		g_morphInterface.ApplyMorphs(npc, faceNode);
	}
	catch (...)
	{
		SKSE::log::debug("{} - Fatal error", __FUNCTION__);
	}
}

void UpdateMorph_Hooked(RE::TESNPC * npc, RE::BGSHeadPart * headPart, RE::BSFaceGenNiNode * faceNode)
{
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		if (g_updateNPCMorphVROriginal) {
			g_updateNPCMorphVROriginal(npc, headPart, faceNode);
		}
	} else
#endif
	{
		SKEE::UpdateNPCMorph(npc, headPart, faceNode);
	}
#ifdef _DEBUG_HOOK
	SKSE::log::debug("UpdateMorph_Hooked - Applying single custom morph");
#endif
	try
	{
		g_morphInterface.ApplyMorph(npc, headPart, faceNode);
	}
	catch (...)
	{
		SKSE::log::debug("{} - Fatal error", __FUNCTION__);
	}
}

#ifdef _DEBUG_HOOK
class DumpPartVisitor : public PartSet::Visitor
{
public:
	bool Accept(std::uint32_t key, RE::BGSHeadPart * headPart)
	{
		SKSE::log::debug("DumpPartVisitor - Key: {} Part: {}", key, headPart->GetFormEditorID());
		return false;
	}
};
#endif

bool IsPlayable_Hooked(RE::BGSHeadPart* headPart)
{
	if (headPart->type.underlying() >= (std::uint32_t)RE::BGSHeadPart::HeadPartType::kTotal)
	{
		return false;
	}

	return headPart->flags.any(RE::BGSHeadPart::Flag::kPlayable);
}

std::uint8_t GetSex_Hooked(RE::TESNPC* npc)
{
	g_partSet.Revert();

	std::uint8_t gender = static_cast<std::uint8_t>(npc->GetSex());
	bool isFemale = gender == 1;

	RE::TESRace* playerRace0 = RE::PlayerCharacter::GetSingleton()->race;
	auto& headPartsArray = RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSHeadPart>();
	for (std::size_t i = 0; i < headPartsArray.size(); ++i)
	{
		RE::BGSHeadPart* headPart = headPartsArray[i];

		bool isPlayable = headPart->flags.any(RE::BGSHeadPart::Flag::kPlayable);
		bool isValidForRace = g_allowAnyRacePart || (headPart->validRaces ? headPart->validRaces->HasForm(playerRace0) : false);
		bool isValidForGender = g_allowAnyGenderPart || (isFemale ? headPart->flags.all(RE::BGSHeadPart::Flag::kFemale) : headPart->flags.all(RE::BGSHeadPart::Flag::kMale));

		if (isPlayable && isValidForRace && isValidForGender)
		{
			if (headPart->type.underlying() >= (std::uint32_t)RE::BGSHeadPart::HeadPartType::kTotal) {
				if (!headPart->flags.any(RE::BGSHeadPart::Flag::kIsExtraPart)) { // Skip Extra Parts
					if (strcmp(headPart->GetModel(), "") == 0)
						g_partSet.SetDefaultPart(headPart->type.underlying(), headPart);
					else
						g_partSet.AddPart(headPart->type.underlying(), headPart);
				}
			}
			else if (!headPart->flags.any(RE::BGSHeadPart::Flag::kIsExtraPart) && isPlayable)
			{
				// maps the pre-existing part to this type
				g_partSet.AddPart(headPart->type.underlying(), headPart);

				if (g_partSet.GetDefaultPart(headPart->type.underlying()) == nullptr) {
					auto playerRace = playerRace0;
					if (playerRace) {
						auto chargenData = playerRace->faceRelatedData[gender];
						if (chargenData) {
							auto headParts = chargenData->headParts;
							if (headParts) {
								for (std::size_t i = 0; i < headParts->size(); i++) {
									RE::BGSHeadPart * part = (*headParts)[i];
									if (part->type.underlying() == headPart->type.underlying())
										g_partSet.SetDefaultPart(part->type.underlying(), part);
								}
							}
						}
					}
				}
			}
		}
	}

	return gender;
}

void* GetHeadParts_Hooked(void* a_unk1, void* a_unk2)
{
	void* result = nullptr;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR() && g_getHeadPartsVROriginal) {
		result = g_getHeadPartsVROriginal(a_unk1, a_unk2);
	}
#endif

	auto* player = RE::PlayerCharacter::GetSingleton();
	if (player) {
		if (auto* npc = player->GetActorBase()) {
			GetSex_Hooked(npc);
		}
	}

	return result;
}

class MorphVisitor : public MorphMap::Visitor
{
public:
	MorphVisitor(RE::BSFaceGenModel * model, SKEEFixedString morphName, RE::NiAVObject ** headNode, float relative, std::uint8_t unk1)
	{
		m_model = model;
		m_morphName = morphName;
		m_headNode = headNode;
		m_relative = relative;
		m_unk1 = unk1;
	}
	bool Accept(const SKEEFixedString & morphName) override
	{
		TRIModelData & morphData = g_morphInterface.GetExtendedModelTri(morphName, true);
		if (morphData.morphModel && morphData.triFile) {
			RE::BSGeometry * geometry = nullptr;
			if (m_headNode && (*m_headNode))
				geometry = (*m_headNode)->AsGeometry();

			if (geometry)
				morphData.triFile->Apply(geometry, m_morphName, m_relative);
		}

		return false;
	}
private:
	RE::BSFaceGenModel	* m_model;
	SKEEFixedString	m_morphName;
	RE::NiAVObject		** m_headNode;
	float			m_relative;
	std::uint8_t			m_unk1;
};

std::uint8_t ApplyRaceMorph_Hooked(RE::BSFaceGenModel * model, RE::BSFixedString * morphName, RE::TESModelTri * modelMorph, RE::NiAVObject ** headNode, float relative, std::uint8_t unk1)
{
	std::uint8_t ret = 0;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		ret = g_applyRaceMorphVROriginal ? g_applyRaceMorphVROriginal(model, morphName, modelMorph, headNode, relative, unk1) : 0;
	} else
#endif
	{
		ret = SKEE::BSFaceGenModel_ApplyRaceMorph(model, morphName, modelMorph, headNode, relative, unk1);
	}

	try
	{
		MorphVisitor morphVisitor(model, *morphName, headNode, relative, unk1);
		g_morphInterface.VisitMorphMap(modelMorph->GetModel(), morphVisitor);
	}
	catch (...)
	{
		SKSE::log::error("{} - fatal error while applying morph ({})", __FUNCTION__, *morphName);
	}

	return ret;
}

std::uint8_t ApplyChargenMorph_Hooked(RE::BSFaceGenModel * model, RE::BSFixedString * morphName, RE::TESModelTri * modelMorph, RE::NiAVObject ** headNode, float relative, std::uint8_t unk1)
{
	std::uint8_t ret = 0;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		ret = g_applyRaceMorphVROriginal ? g_applyRaceMorphVROriginal(model, morphName, modelMorph, headNode, relative, unk1) : 0;
	} else
#endif
	{
		ret = BSFaceGenModel_ApplyMorph_Original(model, morphName, modelMorph, headNode, relative, unk1);
	}

	try
	{
		MorphVisitor morphVisitor(model, *morphName, headNode, relative, unk1);
		g_morphInterface.VisitMorphMap(modelMorph->GetModel(), morphVisitor);
	}
	catch (...)
	{
		SKSE::log::error("{} - fatal error while applying morph ({})", __FUNCTION__, *morphName);
	}

	return ret;
}

void SetRelativeMorph(RE::TESNPC * npc, RE::BSFaceGenNiNode * faceNode, RE::BSFixedString name, float relative)
{
	float absRel = abs(relative);
	if (absRel > 1.0) {
		float max = 0.0;
		if (relative < 0.0)
			max = -1.0;
		if (relative > 0.0)
			max = 1.0;
		std::uint32_t count = (std::uint32_t)absRel;
		for (std::uint32_t i = 0; i < count; i++) {
			g_morphInterface.SetMorph(npc, faceNode, name.data(), max);
			relative -= max;
		}
	}
	g_morphInterface.SetMorph(npc, faceNode, name.data(), relative);
}

namespace RE
{
	class FxResponseArgsList : public FxResponseArgsBase
	{
	public:
		FxResponseArgsList() = default;
		virtual ~FxResponseArgsList() = default;

		virtual std::uint32_t	GetValues(GFxValue** params) override
		{
			*params = &args[0];
			return args.GetSize();
		}

		GArray <GFxValue>	args;
	};
}


void InvokeCategoryList_Hook(RE::GFxMovieView * movie, const char * fnName, RE::FxResponseArgsList& arguments)
{
    arguments.args.PushBack(RE::GFxValue("$EXTRA"));
	arguments.args.PushBack(RE::GFxValue(SLIDER_CATEGORY_EXTRA));
	arguments.args.PushBack(RE::GFxValue("$EXPRESSIONS"));
	arguments.args.PushBack(RE::GFxValue(SLIDER_CATEGORY_EXPRESSIONS));
	RE::FxDelegate::Invoke(movie, fnName, arguments);
}

std::int32_t AddSlider_Hook(RE::RaceMenuSliderArray * sliders, RE::RaceMenuSlider * slider)
{
	std::int32_t totalSliders = 0;
#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		totalSliders = g_addRaceMenuSliderVROriginal ? g_addRaceMenuSliderVROriginal(sliders, slider) : 0;
	} else
#endif
	{
		totalSliders = SKEE::AddRaceMenuSlider(sliders, slider);
	}
	totalSliders = g_morphInterface.LoadSliders(sliders, slider);
	return totalSliders;
}

float SliderLookup_Hooked(RE::RaceMenuSlider * slider)
{
	return slider->value;
}

void DoubleMorphCallback_Hook(RE::RaceSexMenu * menu, float newValue, std::uint32_t sliderId)
{
	std::uint8_t gender = 0;
	RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC * actorBase = player->GetActorBase();
	if (actorBase)
		gender = static_cast<std::uint8_t>(actorBase->GetSex());
	RE::BSFaceGenNiNode * faceNode = player->GetFaceNodeSkinned();

	RE::RaceMenuSlider* slider = skee::GetActiveRaceMenuSlider(menu, gender, sliderId);

	if (slider) {
#ifdef _DEBUG_HOOK
		SKSE::log::debug("Name: {} Value: {} Callback: {} Index: {}")(slider->name, slider->value, slider->callback, slider->index);
#endif
		if (slider->index >= SLIDER_OFFSET) {
			std::uint32_t sliderIndex = slider->index - SLIDER_OFFSET;
			SliderInternalPtr sliderInternal = g_morphInterface.GetSliderByIndex(player->race, sliderIndex);
			if (!sliderInternal)
				return;

			float currentValue = g_morphInterface.GetMorphValueByName(actorBase, sliderInternal->name);
			if (newValue == FLT_MAX || newValue == -FLT_MAX)
			{
				//slider->value = 0.0f;
				return;
			}

			float relative = newValue - currentValue;

			if (relative == 0.0 && sliderInternal->type != SliderInternal::kTypeHeadPart) {
				// Nothing to morph here
#ifdef _DEBUG_HOOK
				SKSE::log::debug("Skipping Morph {}", sliderInternal->name.data);
#endif
				return;
			}

			if (sliderInternal->type == SliderInternal::kTypePreset)
			{
				slider->value = newValue;

				char buffer[REX::W32::MAX_PATH];
				slider->value = newValue;
				sprintf_s(buffer, REX::W32::MAX_PATH, "%s%d", sliderInternal->lowerBound.c_str(), (std::uint32_t)currentValue);
				g_morphInterface.SetMorph(actorBase, faceNode, buffer, -1.0);
				memset(buffer, 0, REX::W32::MAX_PATH);
				sprintf_s(buffer, REX::W32::MAX_PATH, "%s%d", sliderInternal->lowerBound.c_str(), (std::uint32_t)newValue);
				g_morphInterface.SetMorph(actorBase, faceNode, buffer, 1.0);

				g_morphInterface.SetMorphValue(actorBase, sliderInternal->name, newValue);
				return;
			}

			if (sliderInternal->type == SliderInternal::kTypeHeadPart)
			{
				slider->value = newValue;

				std::uint8_t partType = sliderInternal->presetCount;

				HeadPartList * partList = g_partSet.GetPartList(partType);
				if (partList)
				{
					if (newValue == -1.0) {
						RE::BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(static_cast<RE::BGSHeadPart::HeadPartType>(partType));
						if (oldPart) {
							RE::BGSHeadPart * defaultPart = g_partSet.GetDefaultPart(partType);
							if (defaultPart && oldPart != defaultPart) {
								actorBase->ChangeHeadPart(defaultPart);
								SKEE::ChangeActorHeadPart(player, oldPart, defaultPart);
							}
						}
						return;
					}
					RE::BGSHeadPart * targetPart = g_partSet.GetPartByIndex(partList, (std::uint32_t)newValue);
					if (targetPart) {
						RE::BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(static_cast<RE::BGSHeadPart::HeadPartType>(partType));
						if (oldPart != targetPart) {
							actorBase->ChangeHeadPart(targetPart);
							SKEE::ChangeActorHeadPart(player, oldPart, targetPart);
						}
					}
				}

				return;
			}

			// Cross from positive to negative
			if (newValue < 0.0 && currentValue > 0.0) {
				// Undo the upper morph
				SetRelativeMorph(actorBase, faceNode, sliderInternal->upperBound, -abs(currentValue));
#ifdef _DEBUG_HOOK
				SKSE::log::debug("Undoing Upper Morph: New: {} Old: {} Relative {} Remaining {}", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

			// Cross from negative to positive
			if (newValue > 0.0 && currentValue < 0.0) {
				// Undo the lower morph
				SetRelativeMorph(actorBase, faceNode, sliderInternal->lowerBound, -abs(currentValue));
#ifdef _DEBUG_HOOK
				SKSE::log::debug("Undoing Lower Morph: New: {} Old: {} Relative {} Remaining {}", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

#ifdef _DEBUG_HOOK
			SKSE::log::debug("CurrentValue: {} Relative: {} SavedValue: {}", currentValue, relative, slider->value);
#endif
			slider->value = newValue;

			RE::BSFixedString bound = sliderInternal->lowerBound;
			if (newValue < 0.0) {
				bound = sliderInternal->lowerBound;
				relative = -relative;
			}
			else if (newValue > 0.0) {
				bound = sliderInternal->upperBound;
			}
			else {
				if (currentValue > 0.0) {
					bound = sliderInternal->upperBound;
				}
				else {
					bound = sliderInternal->lowerBound;
					relative = -relative;
				}
			}

#ifdef _DEBUG_HOOK
			SKSE::log::debug("Morphing {} - {} Relative: {}", sliderIndex, bound.data, relative);
#endif

			SetRelativeMorph(actorBase, faceNode, bound, relative);
			g_morphInterface.SetMorphValue(actorBase, sliderInternal->name, newValue);
			return;
		}
	}

#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		if (g_doubleMorphCallbackVROriginal) {
			g_doubleMorphCallbackVROriginal(menu, newValue, sliderId);
		}
		return;
	}
#endif
	SKEE::DoubleMorphCallback(menu, newValue, sliderId);
}

// This tracking container is because I only verified two locations of allocation
// in the case somehow it is allocated elsewhere without the hook the destructor wont
// crash the game and instead free the original pointer
std::recursive_mutex g_cs;
std::unordered_set<void*> g_adjustedBlocks;

void * NiAllocate_Hooked(size_t size)
{
	std::lock_guard<std::recursive_mutex> scs(g_cs);
	void* ptr = RE::NiMalloc(size + 0x10);
	if (!ptr) {
		return nullptr;
	}
	*((uintptr_t*)ptr) = 1;
	*((uintptr_t*)ptr+1) = 0;
	void* adjusted = reinterpret_cast<void*>((uintptr_t)ptr + 0x10);
	g_adjustedBlocks.emplace(adjusted);
	return adjusted;
}

bool RetainAdjustedDynamicData(void* a_data)
{
	if (!a_data) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> scs(g_cs);
	if (g_adjustedBlocks.find(a_data) == g_adjustedBlocks.end()) {
		return false;
	}

	void* allocation = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(a_data) - 0x10);
	REX::W32::InterlockedIncrement(reinterpret_cast<volatile std::uint32_t*>(allocation));
	return true;
}

void NiFree_Hooked(void* ptr)
{
	std::lock_guard<std::recursive_mutex> scs(g_cs);
	auto it = g_adjustedBlocks.find(ptr);
	if (it != g_adjustedBlocks.end())
	{
		ptr = reinterpret_cast<void*>((uintptr_t)ptr - 0x10);
		if (REX::W32::InterlockedDecrement((volatile std::uint32_t*)ptr) == 0)
		{
			g_adjustedBlocks.erase(it);
			RE::NiFree(ptr);
		}
	}
	else
	{
		RE::NiFree(ptr);
	}
}

void UpdateModelColor_Recursive(RE::NiAVObject * object, RE::NiColorA *& color, RE::BSShaderMaterial::Feature shaderType)
{
	RE::BSGeometry* geometry = object->AsGeometry();
	if (geometry)
	{
		RE::BSShaderProperty * shaderProperty = geometry->shaderProperty.get();
		if (shaderProperty && netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty) != nullptr)
		{
			RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
			if (material && material->GetFeature() == shaderType)
			{
				RE::NiExtraData* extraData = shaderProperty->GetExtraData("NO_TINT");
				if (extraData) {
					RE::NiBooleanExtraData* booleanData = static_cast<RE::NiBooleanExtraData*>(extraData);
					if (booleanData->data)
					{
						return;
					}
				}
				if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint)
				{
					RE::BSLightingShaderMaterialFacegenTint* tintMaterial = (RE::BSLightingShaderMaterialFacegenTint *)shaderProperty->material;
					tintMaterial->tintColor.red = color->red;
					tintMaterial->tintColor.green = color->green;
					tintMaterial->tintColor.blue = color->blue;
				}
				else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint)
				{
					RE::BSLightingShaderMaterialHairTint* tintMaterial = (RE::BSLightingShaderMaterialHairTint *)shaderProperty->material;
					tintMaterial->tintColor.red = color->red;
					tintMaterial->tintColor.green = color->green;
					tintMaterial->tintColor.blue = color->blue;
				}
			}
		}
	}
	else
	{
		RE::NiNode * node = object->AsNode();
		if (node)
		{
			for (std::uint32_t i = 0; i < node->children.size(); i++)
			{
				RE::NiAVObject * object = node->children[i].get();
				if (object) {
					UpdateModelColor_Recursive(object, color, shaderType);
				}
			}
		}
	}
}

void UpdateModelSkin_Hooked(RE::NiAVObject * object, RE::NiColorA *& color)
{
	auto rootNode = GetRootNode(object, true);
	if (rootNode)
	{
		// Owner reference (legacy NiAVObject::m_owner at 0xF8; modeled as unkF8 in CommonLib).
		RE::TESObjectREFR* owner = rootNode->userData;
		if (owner && owner->IsActor())
		{
			std::uint32_t mask = 1;
			for (std::uint32_t i = 0; i < 32; ++i)
			{
				IItemDataInterface::Identifier identifier;
				identifier.SetSlotMask(mask);
				SKEE_AddTask(g_task, new NIOVTaskUpdateItemDye(static_cast<RE::Actor*>(owner), identifier, TintMaskInterface::kUpdate_Skin, true));
				mask <<= 1;
			}
		}
	}

	UpdateModelColor_Recursive(object, color, RE::BSShaderMaterial::Feature::kFaceGenRGBTint);
}

void UpdateModelHair_Hooked(RE::NiAVObject * object, RE::NiColorA *& color)
{
	auto rootNode = GetRootNode(object, true);
	if (rootNode)
	{
		// Owner reference (legacy NiAVObject::m_owner at 0xF8; modeled as unkF8 in CommonLib).
		RE::TESObjectREFR* owner = rootNode->userData;
		if (owner && owner->IsActor())
		{
			std::uint32_t mask = 1;
			for (std::uint32_t i = 0; i < 32; ++i)
			{
				IItemDataInterface::Identifier identifier;
				identifier.SetSlotMask(mask);
				SKEE_AddTask(g_task, new NIOVTaskUpdateItemDye(static_cast<RE::Actor*>(owner), identifier, TintMaskInterface::kUpdate_Hair, true));
				mask <<= 1;
			}
		}
	}

	UpdateModelColor_Recursive(object, color, RE::BSShaderMaterial::Feature::kHairTint);
}

void SetInventoryItemModel_Hooked(RE::Inventory3DManager * inventoryManager, RE::TESForm * baseForm, RE::ExtraDataList * baseExtraList)
{
	if (inventoryManager && baseForm && baseForm->IsArmor()) {
		RE::TESObjectARMO* armor = baseForm ? baseForm->As<RE::TESObjectARMO>() : nullptr;
		if (armor) {
			std::uint32_t rankId = 0; // Rank 0 will reset if applicable
			if (baseExtraList) {
				auto rankData = static_cast<RE::ExtraRank*>(baseExtraList->GetByType(RE::ExtraDataType::kRank));
				if (rankData) {
					rankId = rankData->rank;
				}
			}

			RE::NiNode * rootNode = nullptr;
			auto findRoot = [&](auto& loadedModels) {
				const auto* model = SKEE::InventoryPreview::FindLoadedModel(loadedModels, baseForm);
				return model && model->spModel ? model->spModel->AsNode() : nullptr;
			};
#if defined(ENABLE_SKYRIM_VR)
			if (REL::Module::IsVR()) {
				rootNode = findRoot(inventoryManager->GetVRRuntimeData().loadedModels);
			} else
#endif
			{
				rootNode = findRoot(inventoryManager->GetRuntimeData().loadedModels);
			}

			if (rootNode) {
				g_itemDataInterface.UpdateInventoryItemDye(rankId, armor, rootNode);
			}
		}
	}

	SetInventoryItemModel_Original(inventoryManager, baseForm, baseExtraList);
}

void SetNewInventoryItemModel_Hooked(RE::Inventory3DManager * inventoryManager, RE::TESForm * form1, RE::TESForm * form2, RE::NiNode ** node)
{
	if (inventoryManager && form1 && form1->IsArmor() && node && *node) {
		RE::TESObjectARMO* armor = form1 ? form1->As<RE::TESObjectARMO>() : nullptr;
		if (armor) {
			RE::ExtraDataList& baseExtraList = inventoryManager->originalExtra;

			std::uint32_t rankId = 0; // Rank 0 will reset if applicable
			auto rankData = static_cast<RE::ExtraRank*>(baseExtraList.GetByType(RE::ExtraDataType::kRank));
			if (rankData) {
				rankId = rankData->rank;
			}

			g_itemDataInterface.UpdateInventoryItemDye(rankId, armor, *node);
		}
	}

#if defined(ENABLE_SKYRIM_VR)
	if (REL::Module::IsVR()) {
		if (g_setNewInventoryItemModelVROriginal) {
			g_setNewInventoryItemModelVROriginal(inventoryManager, form1, form2, node);
		}
		return;
	}
#endif
	SKEE::SetNewInventoryItemModel(inventoryManager, form1, form2, node);
}

void TransferItemUID_Hooked(RE::InventoryChanges* extraContainerChangeData, RE::ExtraDataList* extraList, RE::TESForm* oldForm, RE::TESForm* newForm, std::uint32_t unk1)
{
	TransferItemUID_Original(extraContainerChangeData, extraList, oldForm, newForm, unk1);

	if (extraList) {
		if (extraList->HasType(RE::ExtraDataType::kRank) && !extraList->HasType(RE::ExtraDataType::kUniqueID)) {
			extraContainerChangeData->SetUniqueID(extraList, oldForm, newForm);
			RE::ExtraRank* rank = static_cast<RE::ExtraRank*>(extraList->GetByType(RE::ExtraDataType::kRank));
			RE::ExtraUniqueID* uniqueId = static_cast<RE::ExtraUniqueID*>(extraList->GetByType(RE::ExtraDataType::kUniqueID));
			if (rank && uniqueId) {
				// Re-assign mapping
				g_itemDataInterface.UpdateUIDByRank(rank->rank, uniqueId->uniqueID, uniqueId->baseID);
			}
		}
	}
}

// Console-command arg extraction is now CommonLib's RE::Script::ParseParameters
// (legacy skse64 ObScript_ExtractArgs; Reloc ID 21910).
bool SKEE_Execute(const RE::SCRIPT_PARAMETER * paramInfo, RE::SCRIPT_FUNCTION::ScriptData * scriptData, RE::TESObjectREFR * thisObj, RE::TESObjectREFR* containingObj, RE::Script* scriptObj, RE::ScriptLocals* locals, double& result, std::uint32_t& opcodeOffsetPtr)
{
	char buffer[REX::W32::MAX_PATH];
	memset(buffer, 0, REX::W32::MAX_PATH);
	char buffer2[REX::W32::MAX_PATH];
	memset(buffer2, 0, REX::W32::MAX_PATH);

	if (!RE::Script::ParseParameters(paramInfo, scriptData, opcodeOffsetPtr, thisObj, containingObj, scriptObj, locals, buffer, buffer2))
	{
		return false;
	}

	return g_commandInterface.ExecuteCommand(buffer, thisObj, buffer2);
}

// Front-loaded pattern scan: returns the hook target (match + targetOffset), or 0 and records the pattern name as failed.
static uintptr_t ScanPatternTarget(const char * patternName, const uint8_t * base, size_t range, PatternScan::BytePattern pattern, uintptr_t targetOffset, const char ** failedPatterns, int & numFailed)
{
	uintptr_t hit = PatternScan::FindUnique(base, range, pattern);
	if (hit)
		return hit + targetOffset;

	failedPatterns[numFailed++] = patternName;
	return 0;
}

// File-local trampolines (CommonLib SKSE::Trampoline replaces the legacy
// skse64_common BranchTrampoline/CodeBuffer pair).
static SKSE::Trampoline g_branchTrampoline;
static SKSE::Trampoline g_localTrampoline;

static bool InitializeSKEEHookTrampolines()
{
	static std::atomic<int> state{ 0 };
	if (state.load(std::memory_order_acquire) == 1) {
		return true;
	}
	if (state.load(std::memory_order_acquire) == -1) {
		return false;
	}

	constexpr std::size_t kTrampolineSize = 4096;
	if (const auto* trampolineIface = SKSE::GetTrampolineInterface()) {
		void* branch = trampolineIface->AllocateFromBranchPool(kTrampolineSize);
		if (!branch) {
			SKSE::log::error("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			state.store(-1, std::memory_order_release);
			return false;
		}

		g_branchTrampoline.set_trampoline(branch, kTrampolineSize);

		void* local = trampolineIface->AllocateFromLocalPool(kTrampolineSize);
		if (!local) {
			SKSE::log::error("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			state.store(-1, std::memory_order_release);
			return false;
		}

		g_localTrampoline.set_trampoline(local, kTrampolineSize);
	}
	else {
		g_branchTrampoline.create(kTrampolineSize);
		g_localTrampoline.create(kTrampolineSize);
	}

	state.store(1, std::memory_order_release);
	return true;
}

#if defined(ENABLE_SKYRIM_VR)
namespace
{
	// Address Library ID 15501 identifies Skyrim VR's biped attach routine. The
	// call window, register contract, BipedAnim slot stride, and original target
	// were independently qualified against the exact SkyrimVR 1.4.15 executable
	// captured in the retained full-memory dump. No external implementation is
	// used by this bridge.
	constexpr std::uint64_t kVRBipedAttachRoutineID = 15501;
	constexpr std::uintptr_t kVRBipedAttachCallOffset = 0xC41;
	constexpr std::uintptr_t kVRCreateArmorNodeRVA = 0x001DB680;
	constexpr std::array<std::uint8_t, 14> kVRBipedAttachExpectedBytes{
		0x45, 0x8B, 0xC7,              // mov r8d, r15d
		0x48, 0x8B, 0xD6,              // mov rdx, rsi
		0x49, 0x8B, 0xCD,              // mov rcx, r13
		0xE8, 0xE1, 0x35, 0x00, 0x00   // call SkyrimVR.exe+0x1DB680
	};

	using CreateArmorNodeVRFn = RE::NiAVObject* (*)(
		RE::BipedAnim*,
		RE::NiNode*,
		std::uint32_t,
		std::uint8_t,
		std::uint8_t,
		std::uint64_t);

	// The VR caller passes a temporary descriptor in r9.  Its first two qwords
	// precede the armor/addon pair used by RaceMenu; no other part of the
	// temporary is read here.
	struct VRBipedAttachParams
	{
		std::uint64_t     unknown00;
		std::uint64_t     unknown08;
		RE::TESForm*      armor;
		RE::TESObjectARMA* addon;
	};
	static_assert(offsetof(VRBipedAttachParams, armor) == 0x10);
	static_assert(offsetof(VRBipedAttachParams, addon) == 0x18);

	CreateArmorNodeVRFn g_createArmorNodeVROriginal{ nullptr };

	RE::NiAVObject* CreateArmorNodeVRHook(
		RE::BipedAnim* a_biped,
		RE::NiNode* a_objectRoot,
		std::uint64_t a_packedThirdAndFourth,
		VRBipedAttachParams* a_params,
		std::uint64_t a_fifth,
		std::uint64_t a_sixth)
	{
		const auto third = static_cast<std::uint32_t>(a_packedThirdAndFourth >> 32);
		const auto fourth = static_cast<std::uint8_t>(a_packedThirdAndFourth & 0xFF);
		auto* result = g_createArmorNodeVROriginal ?
			g_createArmorNodeVROriginal(a_biped, a_objectRoot, third, fourth, static_cast<std::uint8_t>(a_fifth), a_sixth) :
			nullptr;

		if (!a_biped || !a_params) {
			return result;
		}

		RE::NiPointer<RE::TESObjectREFR> reference;
		std::uint32_t handle = a_biped->actorRef.native_handle();
		if (SKEE::LookupREFRByHandle(handle, reference) && reference) {
			InstallArmorAddonHook(
				reference.get(),
				a_params->armor,
				a_params->addon,
				a_biped->root,
				result);
		}

		return result;
	}

	struct VRBipedAttachHookCode : Xbyak::CodeGenerator
	{
		VRBipedAttachHookCode(std::uintptr_t a_returnAddress) :
			Xbyak::CodeGenerator(256)
		{
			Xbyak::Label hookLabel;
			Xbyak::Label returnLabel;

			// Preserve the original stack arguments. Pack the original r8d/r9
			// pair into r8 and use r9 for the temporary descriptor expected by
			// CreateArmorNodeVRHook.
			mov(r8d, r15d);
			shl(r8, 0x20);
			and_(r9, 0xFFFFFFFF);
			or_(r8, r9);
			lea(r9, ptr[r12 + r13]);
			mov(rdx, rsi);
			mov(rcx, r13);
			call(ptr[rip + hookLabel]);
			jmp(ptr[rip + returnLabel]);

			L(hookLabel);
			dq(reinterpret_cast<std::uintptr_t>(CreateArmorNodeVRHook));

			L(returnLabel);
			dq(a_returnAddress);
		}
	};

	SKEEHookGroupResult InstallVRBodySystemHooks()
	{
		REL::Relocation<std::uintptr_t> routine{ REL::ID(kVRBipedAttachRoutineID) };
		const auto callSite = routine.address() + kVRBipedAttachCallOffset;
		const auto text = REL::Module::get().segment(REL::Segment::Name::textx);
		const auto textEnd = text.address() + text.size();
		if (!routine.address() || callSite < text.address() || callSite + kVRBipedAttachExpectedBytes.size() > textEnd) {
			SKSE::log::error("VR body hook address is unavailable or outside Skyrim's text segment: 0x{:X}", callSite);
			return { "body-systems", SKEEHookStatus::kUnavailableAddress, "armor attachment callbacks remain unavailable" };
		}

		const auto* observed = reinterpret_cast<const std::uint8_t*>(callSite);
		if (std::memcmp(observed, kVRBipedAttachExpectedBytes.data(), kVRBipedAttachExpectedBytes.size()) != 0) {
			SKSE::log::error(
				"VR body hook signature mismatch at 0x{:X}: observed={:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
				callSite,
				observed[0], observed[1], observed[2], observed[3], observed[4], observed[5], observed[6],
				observed[7], observed[8], observed[9], observed[10], observed[11], observed[12], observed[13]);
			return { "body-systems", SKEEHookStatus::kSignatureMismatch, "armor attachment callbacks remain unavailable" };
		}

		const auto callDisplacement = *reinterpret_cast<const std::int32_t*>(callSite + 10);
		const auto originalTarget = callSite + kVRBipedAttachExpectedBytes.size() + callDisplacement;
		const auto expectedTarget = REL::Offset(kVRCreateArmorNodeRVA).address();
		if (originalTarget != expectedTarget || originalTarget < text.address() || originalTarget >= textEnd) {
			SKSE::log::error(
				"VR body hook original target mismatch: decoded=0x{:X}, expected=0x{:X}",
				originalTarget,
				expectedTarget);
			return { "body-systems", SKEEHookStatus::kSignatureMismatch, "armor attachment callbacks remain unavailable" };
		}

		if (!InitializeSKEEHookTrampolines()) {
			return { "body-systems", SKEEHookStatus::kInitializationFailed, "armor attachment callbacks remain unavailable" };
		}

		VRBipedAttachHookCode code(callSite + kVRBipedAttachExpectedBytes.size());
		void* entry = g_localTrampoline.allocate(code);
		if (!entry) {
			SKSE::log::error("VR body hook could not allocate its local trampoline");
			return { "body-systems", SKEEHookStatus::kInitializationFailed, "armor attachment callbacks remain unavailable" };
		}

		g_createArmorNodeVROriginal = reinterpret_cast<CreateArmorNodeVRFn>(originalTarget);
		g_branchTrampoline.write_branch<6>(callSite, reinterpret_cast<std::uintptr_t>(entry));
		g_hookBipedAttach = true;
		SKSE::log::info(
			"VR body hook installed: callSite=0x{:X}, original=0x{:X}, continuation=0x{:X}",
			callSite,
			originalTarget,
			callSite + kVRBipedAttachExpectedBytes.size());
		return {
			"body-systems",
			SKEEHookStatus::kEnabled,
			"qualified armor attachment callbacks feed body morph, skeleton, and override observers; rendering overlays remain separately gated"
		};
	}

	struct VRCallPatch
	{
		std::string_view name;
		std::uintptr_t rva;
		std::uintptr_t hook;
		std::uintptr_t expectedTargetRva;
	};

	[[nodiscard]] bool IsVRTextAddress(std::uintptr_t a_address, std::size_t a_size = 1)
	{
		const auto text = REL::Module::get().segment(REL::Segment::Name::textx);
		return a_address >= text.address() && a_address + a_size >= a_address && a_address + a_size <= text.address() + text.size();
	}

	[[nodiscard]] bool ValidateVRCallPatch(const VRCallPatch& a_patch, std::uintptr_t* a_decodedTarget = nullptr)
	{
		const auto address = REL::Offset(a_patch.rva).address();
		if (!IsVRTextAddress(address, 5)) {
			SKSE::log::error("VR hook {} is outside SkyrimVR.exe .text: RVA=0x{:X}", a_patch.name, a_patch.rva);
			return false;
		}
		if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) {
			SKSE::log::error("VR hook {} expected CALL at RVA 0x{:X}, observed opcode 0x{:02X}", a_patch.name, a_patch.rva, *reinterpret_cast<const std::uint8_t*>(address));
			return false;
		}
		const auto displacement = *reinterpret_cast<const std::int32_t*>(address + 1);
		const auto target = address + 5 + displacement;
		const auto expectedTarget = REL::Offset(a_patch.expectedTargetRva).address();
		if (!IsVRTextAddress(target) || target != expectedTarget) {
			SKSE::log::error(
				"VR hook {} original target mismatch at RVA 0x{:X}: decoded=0x{:X}, expected=0x{:X}",
				a_patch.name,
				a_patch.rva,
				target,
				expectedTarget);
			return false;
		}
		if (a_decodedTarget) {
			*a_decodedTarget = target;
		}
		return true;
	}

	template <std::size_t N>
	[[nodiscard]] bool ValidateVRCallPatches(
		const std::array<VRCallPatch, N>& a_patches,
		std::array<std::uintptr_t, N>& a_originalTargets)
	{
		for (std::size_t i = 0; i < N; ++i) {
			if (!ValidateVRCallPatch(a_patches[i], std::addressof(a_originalTargets[i]))) {
				return false;
			}
		}
		return true;
	}

	template <std::size_t N>
	void CommitVRCallPatches(const std::array<VRCallPatch, N>& a_patches)
	{
		for (const auto& patch : a_patches) {
			const auto address = REL::Offset(patch.rva).address();
			g_branchTrampoline.write_call<5>(address, patch.hook);
			SKSE::log::info("VR hook {} installed at RVA 0x{:X}", patch.name, patch.rva);
		}
	}

	SKEEHookGroupResult InstallVRNativeSliderHooks()
	{
		if (!g_hookNativeSliders || !g_hookSliderCallbacks) {
			return { "native-face-sliders", SKEEHookStatus::kDisabledByPolicy, "disabled by the user's hook configuration" };
		}
		if (!InitializeSKEEHookTrampolines()) {
			return { "native-face-sliders", SKEEHookStatus::kInitializationFailed, "slider trampolines could not be allocated" };
		}

		constexpr std::uintptr_t loadSliders = 0x008E39B0;
		const std::array patches{
			VRCallPatch{ "head-part-list", loadSliders + 0x215, reinterpret_cast<std::uintptr_t>(GetHeadParts_Hooked), 0x00175DA0 },
			VRCallPatch{ "slider-category-list", 0x008E2DC0 + 0x9FB, reinterpret_cast<std::uintptr_t>(InvokeCategoryList_Hook), 0x00F342E0 },
			VRCallPatch{ "add-native-slider", loadSliders + 0x37E4, reinterpret_cast<std::uintptr_t>(AddSlider_Hook), 0x008E9850 },
			VRCallPatch{ "double-morph-primary", loadSliders + 0x3CD5, reinterpret_cast<std::uintptr_t>(DoubleMorphCallback_Hook), 0x008E23B0 },
			VRCallPatch{ "double-morph-secondary", 0x008DF4E0 + 0x4F, reinterpret_cast<std::uintptr_t>(DoubleMorphCallback_Hook), 0x008E23B0 }
		};
		const auto lookupSite = REL::Offset(loadSliders + 0x3895).address();
		constexpr std::array expectedLookup{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0x4C }, std::uint8_t{ 0x18 }, std::uint8_t{ 0x18 } };
		std::array<std::uintptr_t, patches.size()> originalTargets{};
		if (!ValidateVRCallPatches(patches, originalTargets) ||
			!IsVRTextAddress(lookupSite, expectedLookup.size()) ||
			std::memcmp(reinterpret_cast<const void*>(lookupSite), expectedLookup.data(), expectedLookup.size()) != 0) {
			SKSE::log::error("VR slider lookup signature mismatch at RVA 0x{:X}", loadSliders + 0x3895);
			return { "native-face-sliders", SKEEHookStatus::kSignatureMismatch, "one or more established SkyrimVR 1.4.15 slider sites or targets did not match" };
		}

		struct SliderLookupCode : Xbyak::CodeGenerator
		{
			SliderLookupCode(std::uintptr_t a_returnAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label function;
				Xbyak::Label returnAddress;
				lea(rcx, ptr[rax + rbx]);
				call(ptr[rip + function]);
				movss(xmm6, xmm0);
				mov(rcx, ptr[rcx + 0x18]);
				jmp(ptr[rip + returnAddress]);
				L(function);
				dq(reinterpret_cast<std::uintptr_t>(SliderLookup_Hooked));
				L(returnAddress);
				dq(a_returnAddress);
			}
		};

		SliderLookupCode lookupCode(lookupSite + expectedLookup.size());
		auto* lookupEntry = g_localTrampoline.allocate(lookupCode);
		if (!lookupEntry) {
			return { "native-face-sliders", SKEEHookStatus::kInitializationFailed, "the slider lookup bridge could not be allocated" };
		}

		g_getHeadPartsVROriginal = reinterpret_cast<GetHeadPartsVRFn>(originalTargets[0]);
		g_addRaceMenuSliderVROriginal = reinterpret_cast<AddRaceMenuSliderVRFn>(originalTargets[2]);
		g_doubleMorphCallbackVROriginal = reinterpret_cast<DoubleMorphCallbackVRFn>(originalTargets[3]);
		CommitVRCallPatches(patches);
		g_branchTrampoline.write_branch<5>(lookupSite, reinterpret_cast<std::uintptr_t>(lookupEntry));
		return { "native-face-sliders", SKEEHookStatus::kEnabled, "head-part discovery, extra categories, custom sliders, lookup, and slider callbacks are installed" };
	}

	SKEEHookGroupResult InstallVRHeadPresetHooks()
	{
		if (!g_hookHeadPreprocessing || g_externalHeads) {
			return { "head-preset-compatibility", SKEEHookStatus::kDisabledByPolicy, "disabled by hook configuration or external-head ownership" };
		}
		if (!InitializeSKEEHookTrampolines()) {
			return { "head-preset-compatibility", SKEEHookStatus::kInitializationFailed, "head preprocessing trampolines could not be allocated" };
		}

		constexpr std::uintptr_t preprocessing = 0x00373740;
		const auto site1 = REL::Offset(preprocessing + 0x58).address();
		const auto site2 = REL::Offset(preprocessing + 0x81).address();
		const auto site3 = REL::Offset(preprocessing + 0x67).address();
		const auto regenerate = REL::Offset(0x003E23D0).address();
		constexpr std::array site1Expected{
			std::uint8_t{ 0x0F }, std::uint8_t{ 0xB6 }, std::uint8_t{ 0x0D },
			std::uint8_t{ 0x11 }, std::uint8_t{ 0x3A }, std::uint8_t{ 0xB3 },
			std::uint8_t{ 0x01 }, std::uint8_t{ 0x84 }, std::uint8_t{ 0xC9 }
		};
		constexpr std::array site2Expected{
			std::uint8_t{ 0x0F }, std::uint8_t{ 0xB6 }, std::uint8_t{ 0x0D },
			std::uint8_t{ 0xE8 }, std::uint8_t{ 0x39 }, std::uint8_t{ 0xB3 },
			std::uint8_t{ 0x01 }, std::uint8_t{ 0x84 }, std::uint8_t{ 0xC9 }
		};
		constexpr std::array site3Expected{ std::uint8_t{ 0x85 }, std::uint8_t{ 0xC0 } };
		constexpr std::array regenerateProlog{ std::uint8_t{ 0x48 }, std::uint8_t{ 0x8B }, std::uint8_t{ 0xC4 }, std::uint8_t{ 0x55 }, std::uint8_t{ 0x41 }, std::uint8_t{ 0x54 } };
		if (!IsVRTextAddress(site1, site1Expected.size()) ||
			!IsVRTextAddress(site2, site2Expected.size()) ||
			!IsVRTextAddress(site3, site3Expected.size()) ||
			!IsVRTextAddress(regenerate, regenerateProlog.size()) ||
			std::memcmp(reinterpret_cast<const void*>(site1), site1Expected.data(), site1Expected.size()) != 0 ||
			std::memcmp(reinterpret_cast<const void*>(site2), site2Expected.data(), site2Expected.size()) != 0 ||
			std::memcmp(reinterpret_cast<const void*>(site3), site3Expected.data(), site3Expected.size()) != 0 ||
			std::memcmp(reinterpret_cast<const void*>(regenerate), regenerateProlog.data(), regenerateProlog.size()) != 0) {
			SKSE::log::error("VR head preprocessing signature/range validation failed");
			return { "head-preset-compatibility", SKEEHookStatus::kSignatureMismatch, "one or more established head preprocessing sites did not match" };
		}

		struct UsePreprocessedHeadCode : Xbyak::CodeGenerator
		{
			UsePreprocessedHeadCode(std::uintptr_t a_returnAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label function;
				Xbyak::Label returnAddress;
				mov(rcx, rdi);
				call(ptr[rip + function]);
				jmp(ptr[rip + returnAddress]);
				L(function);
				dq(reinterpret_cast<std::uintptr_t>(UsePreprocessedHead));
				L(returnAddress);
				dq(a_returnAddress);
			}
		};
		UsePreprocessedHeadCode code1(site1 + 6);
		UsePreprocessedHeadCode code2(site2 + 6);
		auto* entry1 = g_localTrampoline.allocate(code1);
		auto* entry2 = g_localTrampoline.allocate(code2);

		struct RegenerateHeadCode : Xbyak::CodeGenerator
		{
			RegenerateHeadCode(std::uintptr_t a_returnAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label returnAddress;
				mov(rax, rsp);
				push(rbp);
				push(r12);
				jmp(ptr[rip + returnAddress]);
				L(returnAddress);
				dq(a_returnAddress);
			}
		};
		RegenerateHeadCode regenerateCode(regenerate + regenerateProlog.size());
		auto* regenerateEntry = g_localTrampoline.allocate(regenerateCode);
		if (!entry1 || !entry2 || !regenerateEntry) {
			return { "head-preset-compatibility", SKEEHookStatus::kInitializationFailed, "one or more head preprocessing bridges could not be allocated" };
		}

		// Commit only after every instruction window has matched and every bridge
		// has been allocated, so this group cannot be left half-installed.
		RegenerateHead_Original = reinterpret_cast<RegenerateHeadFn>(regenerateEntry);
		g_branchTrampoline.write_branch<6>(site1, reinterpret_cast<std::uintptr_t>(entry1));
		g_branchTrampoline.write_branch<6>(site2, reinterpret_cast<std::uintptr_t>(entry2));
		constexpr std::array resultFix{ std::uint8_t{ 0x90 }, std::uint8_t{ 0x84 }, std::uint8_t{ 0xC0 } };
		constexpr std::array testFix{ std::uint8_t{ 0x85 }, std::uint8_t{ 0xDB } };
		REL::safe_write(site1 + 6, resultFix.data(), resultFix.size());
		REL::safe_write(site2 + 6, resultFix.data(), resultFix.size());
		REL::safe_write(site3, testFix.data(), testFix.size());
		g_branchTrampoline.write_branch<6>(regenerate, reinterpret_cast<std::uintptr_t>(RegenerateHead_Hooked));
		return { "head-preset-compatibility", SKEEHookStatus::kEnabled, "preprocessed-head selection and mapped-preset regeneration are installed" };
	}

	SKEEHookGroupResult InstallVRMorphHooks()
	{
		if (!g_hookMorphUpdates || !g_hookMorphExtensions) {
			return { "face-morph-application", SKEEHookStatus::kDisabledByPolicy, "disabled by the user's morph hook configuration" };
		}
		if (!InitializeSKEEHookTrampolines()) {
			return { "face-morph-application", SKEEHookStatus::kInitializationFailed, "morph hook trampolines could not be allocated" };
		}

		std::array<VRCallPatch, 4> patches{
			VRCallPatch{ "chargen-morph", 0x003E1CE0 + 0xF3, reinterpret_cast<std::uintptr_t>(ApplyChargenMorph_Hooked), 0x003E3FA0 },
			VRCallPatch{ "race-morph", 0x003E3F20 + 0x56, reinterpret_cast<std::uintptr_t>(ApplyRaceMorph_Hooked), 0x003E3FA0 },
			VRCallPatch{ "update-all-morphs", 0x003E1E50 + 0xC7, reinterpret_cast<std::uintptr_t>(UpdateMorphs_Hooked), 0x00370140 },
			VRCallPatch{ "update-one-morph", 0x003EBB30 + 0x79, reinterpret_cast<std::uintptr_t>(UpdateMorph_Hooked), 0x00370330 }
		};
		if (!g_extendedMorphs) {
			patches[0].hook = 0;
			patches[1].hook = 0;
		}
		std::array<std::uintptr_t, 4> originalTargets{};
		for (std::size_t i = 0; i < patches.size(); ++i) {
			if (patches[i].hook && !ValidateVRCallPatch(patches[i], std::addressof(originalTargets[i]))) {
				return { "face-morph-application", SKEEHookStatus::kSignatureMismatch, "one or more established face morph call sites did not match" };
			}
		}

		g_updateNPCMorphsVROriginal = reinterpret_cast<UpdateNPCMorphsVRFn>(originalTargets[2]);
		g_updateNPCMorphVROriginal = reinterpret_cast<UpdateNPCMorphVRFn>(originalTargets[3]);
		if (g_extendedMorphs) {
			g_applyRaceMorphVROriginal = reinterpret_cast<BSFaceGenModelApplyMorphFn>(originalTargets[0]);
		}
		for (const auto& patch : patches) {
			if (patch.hook) {
				g_branchTrampoline.write_call<5>(REL::Offset(patch.rva).address(), patch.hook);
				SKSE::log::info("VR hook {} installed at RVA 0x{:X}", patch.name, patch.rva);
			}
		}
		return { "face-morph-application", SKEEHookStatus::kEnabled, g_extendedMorphs ? "custom TRI application and per-head morph updates are installed" : "per-head morph updates are installed; extended TRI application is disabled by configuration" };
	}

	SKEEHookGroupResult InstallVRSculptRenderHook()
	{
		REL::Relocation<std::uintptr_t> raceSexMenuVtable{ RE::VTABLE_RaceSexMenu[0] };
		const auto slot = raceSexMenuVtable.address() + 0x30;
		const auto rdata = REL::Module::get().segment(REL::Segment::Name::rdata);
		if (slot < rdata.address() || slot + sizeof(std::uintptr_t) > rdata.address() + rdata.size()) {
			return { "sculpt-rendering", SKEEHookStatus::kUnavailableAddress, "the RaceSexMenu render vtable slot is unavailable" };
		}
		const auto original = *reinterpret_cast<const std::uintptr_t*>(slot);
		const auto expectedOriginal = REL::Offset(0x0052EDE0).address();
		if (!IsVRTextAddress(original) || original != expectedOriginal) {
			SKSE::log::error(
				"VR RaceSexMenu render vtable target mismatch: observed=0x{:X}, expected=0x{:X}",
				original,
				expectedOriginal);
			return { "sculpt-rendering", SKEEHookStatus::kSignatureMismatch, "the RaceSexMenu render vtable slot did not match SkyrimVR 1.4.15" };
		}
		const auto hook = reinterpret_cast<std::uintptr_t>(RaceSexMenu_Render_Hooked);
		REL::safe_write(slot, std::addressof(hook), sizeof(hook));
		SKSE::log::info("VR RaceSexMenu sculpt render hook installed: slot RVA=0x{:X}, original=0x{:X}", slot - REL::Module::get().base(), original);
		return { "sculpt-rendering", SKEEHookStatus::kEnabled, "RaceSexMenu renders the sculpt world before displaying its Scaleform movie" };
	}

	SKEEHookGroupResult InstallVRFaceOverlayHooks()
	{
		if (!g_hookFaceOverlays || !g_enableFaceOverlays) {
			return { "face-overlays", SKEEHookStatus::kDisabledByPolicy, "disabled by the user's face-overlay configuration" };
		}
		if (!InitializeSKEEHookTrampolines()) {
			return { "face-overlays", SKEEHookStatus::kInitializationFailed, "face-overlay hook trampolines could not be allocated" };
		}
		// Skyrim VR emits more BSDynamicTriShape allocation/free paths than the
		// flat executable.  Every path that can own the +0x10 adjusted buffer must
		// use the refcount-aware pair; otherwise an assignment or the virtual
		// deleting destructor can pass the interior data pointer to NiFree.
		const std::array patches{
			VRCallPatch{ "face-overlay-free-copy", 0x00CB82B0 + 0x8B, reinterpret_cast<std::uintptr_t>(NiFree_Hooked), 0x00C698E0 },
			VRCallPatch{ "face-overlay-allocate-copy", 0x00CB82B0 + 0x92, reinterpret_cast<std::uintptr_t>(NiAllocate_Hooked), 0x00C69680 },
			VRCallPatch{ "face-overlay-free-assignment", 0x00CB8390 + 0x65, reinterpret_cast<std::uintptr_t>(NiFree_Hooked), 0x00C698E0 },
			VRCallPatch{ "face-overlay-allocate-assignment", 0x00CB8390 + 0x6C, reinterpret_cast<std::uintptr_t>(NiAllocate_Hooked), 0x00C69680 },
			VRCallPatch{ "face-overlay-allocate-constructor", 0x00CB8600 + 0x76, reinterpret_cast<std::uintptr_t>(NiAllocate_Hooked), 0x00C69680 },
			VRCallPatch{ "face-overlay-free-destructor", 0x00CB8700 + 0x28, reinterpret_cast<std::uintptr_t>(NiFree_Hooked), 0x00C698E0 },
			VRCallPatch{ "face-overlay-free-resize", 0x00CB8800 + 0x4E, reinterpret_cast<std::uintptr_t>(NiFree_Hooked), 0x00C698E0 },
			VRCallPatch{ "face-overlay-allocate-resize", 0x00CB8800 + 0x55, reinterpret_cast<std::uintptr_t>(NiAllocate_Hooked), 0x00C69680 },
			VRCallPatch{ "face-overlay-free-deleting-destructor", 0x00CB9000 + 0x3F, reinterpret_cast<std::uintptr_t>(NiFree_Hooked), 0x00C698E0 },
			VRCallPatch{ "face-overlay-enable", 0x00373880 + 0x1E0, reinterpret_cast<std::uintptr_t>(UpdateHeadState_Enable_Hooked), 0x003727B0 },
			VRCallPatch{ "face-overlay-disable", 0x00372920 + 0x1DF, reinterpret_cast<std::uintptr_t>(UpdateHeadState_Disabled_Hooked), 0x003727B0 }
		};
		std::array<std::uintptr_t, patches.size()> originalTargets{};
		if (!ValidateVRCallPatches(patches, originalTargets)) {
			return { "face-overlays", SKEEHookStatus::kSignatureMismatch, "one or more established face-overlay allocation/lifecycle sites did not match" };
		}
		g_updateHeadStateVROriginal = reinterpret_cast<UpdateHeadStateVRFn>(originalTargets[9]);
		CommitVRCallPatches(patches);
		return { "face-overlays", SKEEHookStatus::kEnabled, "dynamic face geometry refcounting and face-overlay refresh hooks are installed" };
	}

	SKEEHookGroupResult InstallVRTintInventoryHooks()
	{
		if (!InitializeSKEEHookTrampolines()) {
			return { "tint-and-inventory", SKEEHookStatus::kInitializationFailed, "tint/inventory hook trampolines could not be allocated" };
		}

		const bool installTint = g_hookTinting && g_enableTintSync;
		const bool installInventory = g_hookTintInventory && g_enableTintInventory;
		const auto skin = REL::Offset(0x003EC090).address();
		const auto hair = REL::Offset(0x003EC150).address();
		const auto setModel = REL::Offset(0x008B60A0).address();
		const auto transfer = REL::Offset(0x001FD990).address();
		constexpr std::array tintProlog{
			std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x54 },
			std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x53 }
		};
		constexpr std::array setModelProlog{
			std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C },
			std::uint8_t{ 0x24 }, std::uint8_t{ 0x18 }
		};
		constexpr std::array transferProlog{
			std::uint8_t{ 0x40 }, std::uint8_t{ 0x53 }, std::uint8_t{ 0x55 },
			std::uint8_t{ 0x56 }, std::uint8_t{ 0x57 }, std::uint8_t{ 0x41 },
			std::uint8_t{ 0x56 }
		};

		if (installTint &&
			(!IsVRTextAddress(skin, tintProlog.size()) ||
			 !IsVRTextAddress(hair, tintProlog.size()) ||
			 std::memcmp(reinterpret_cast<const void*>(skin), tintProlog.data(), tintProlog.size()) != 0 ||
			 std::memcmp(reinterpret_cast<const void*>(hair), tintProlog.data(), tintProlog.size()) != 0)) {
			return { "tint-and-inventory", SKEEHookStatus::kSignatureMismatch, "skin/hair tint entry-point signatures did not match SkyrimVR 1.4.15" };
		}

		const VRCallPatch newModel{
			"new-inventory-model",
			0x008B6220 + 0x1B0,
			reinterpret_cast<std::uintptr_t>(SetNewInventoryItemModel_Hooked),
			0x008B5B40
		};
		std::uintptr_t newModelOriginal = 0;
		if (installInventory &&
			(!ValidateVRCallPatch(newModel, std::addressof(newModelOriginal)) ||
			 !IsVRTextAddress(setModel, setModelProlog.size()) ||
			 std::memcmp(reinterpret_cast<const void*>(setModel), setModelProlog.data(), setModelProlog.size()) != 0)) {
			return { "tint-and-inventory", SKEEHookStatus::kSignatureMismatch, "inventory model hook sites did not match SkyrimVR 1.4.15" };
		}

		if (!IsVRTextAddress(transfer, transferProlog.size()) ||
			std::memcmp(reinterpret_cast<const void*>(transfer), transferProlog.data(), transferProlog.size()) != 0) {
			return { "tint-and-inventory", SKEEHookStatus::kSignatureMismatch, "item UID transfer entry-point signature did not match SkyrimVR 1.4.15" };
		}

		struct SetInventoryModelCode : Xbyak::CodeGenerator
		{
			SetInventoryModelCode(std::uintptr_t a_returnAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label returnAddress;
				mov(ptr[rsp + 0x18], rbx);
				jmp(ptr[rip + returnAddress]);
				L(returnAddress);
				dq(a_returnAddress);
			}
		};
		struct TransferItemUIDCode : Xbyak::CodeGenerator
		{
			TransferItemUIDCode(std::uintptr_t a_returnAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label returnAddress;
				push(rbx);
				push(rbp);
				push(rsi);
				push(rdi);
				push(r14);
				jmp(ptr[rip + returnAddress]);
				L(returnAddress);
				dq(a_returnAddress);
			}
		};

		void* setModelEntry = nullptr;
		if (installInventory) {
			SetInventoryModelCode setModelCode(setModel + setModelProlog.size());
			setModelEntry = g_localTrampoline.allocate(setModelCode);
		}
		TransferItemUIDCode transferCode(transfer + 7);
		auto* transferEntry = g_localTrampoline.allocate(transferCode);
		if ((installInventory && !setModelEntry) || !transferEntry) {
			return { "tint-and-inventory", SKEEHookStatus::kInitializationFailed, "one or more tint/inventory original bridges could not be allocated" };
		}

		// Commit only after every enabled site has matched and every bridge exists.
		if (installTint) {
			g_branchTrampoline.write_branch<6>(skin, reinterpret_cast<std::uintptr_t>(UpdateModelSkin_Hooked));
			g_branchTrampoline.write_branch<6>(hair, reinterpret_cast<std::uintptr_t>(UpdateModelHair_Hooked));
		}
		if (installInventory) {
			SetInventoryItemModel_Original = reinterpret_cast<SetInventoryItemModelFn>(setModelEntry);
			g_setNewInventoryItemModelVROriginal = reinterpret_cast<SetNewInventoryItemModelVRFn>(newModelOriginal);
			// The displaced instruction is exactly five bytes; resuming at +6 would
			// enter the middle of the following mov [rsp+0x20],rsi instruction.
			g_branchTrampoline.write_branch<5>(setModel, reinterpret_cast<std::uintptr_t>(SetInventoryItemModel_Hooked));
			g_branchTrampoline.write_call<5>(REL::Offset(newModel.rva).address(), newModel.hook);
		}
		TransferItemUID_Original = reinterpret_cast<TransferItemUIDFn>(transferEntry);
		g_branchTrampoline.write_branch<6>(transfer, reinterpret_cast<std::uintptr_t>(TransferItemUID_Hooked));
		return { "tint-and-inventory", SKEEHookStatus::kEnabled, "skin/hair tint synchronization, inventory dyeing, and item UID transfer are installed" };
	}

	SKEEHookGroupResult InstallVRFaceGenCacheBypass()
	{
		if (!g_disableFaceGenCache) {
			return { "facegen-cache-bypass", SKEEHookStatus::kDisabledByPolicy, "FaceGen cache bypass is disabled by configuration" };
		}
		const auto address = REL::Offset(0x008E8930).address();
		constexpr std::array expectedProlog{
			std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C },
			std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }, std::uint8_t{ 0x48 },
			std::uint8_t{ 0x89 }, std::uint8_t{ 0x74 }, std::uint8_t{ 0x24 },
			std::uint8_t{ 0x18 }
		};
		if (!IsVRTextAddress(address, expectedProlog.size()) ||
			std::memcmp(reinterpret_cast<const void*>(address), expectedProlog.data(), expectedProlog.size()) != 0) {
			return { "facegen-cache-bypass", SKEEHookStatus::kSignatureMismatch, "the FaceGen cache entry-point signature did not match SkyrimVR 1.4.15" };
		}
		constexpr std::uint8_t ret = 0xC3;
		REL::safe_write(address, std::addressof(ret), sizeof(ret));
		SKSE::log::info("VR FaceGen cache bypass installed at RVA 0x{:X}", 0x008E8930);
		return { "facegen-cache-bypass", SKEEHookStatus::kEnabled, "the vanilla FaceGen part cache is bypassed as configured" };
	}
}
#endif

static bool InstallFlatSKEEHooks()
{
	if (!InitializeSKEEHookTrampolines()) {
		return false;
	}

	constexpr size_t kSliderFuncScanRange = 0x5000; // this function is ~0x3A00 bytes in both known builds
	const uint8_t* sliderFuncBase = reinterpret_cast<const uint8_t*>(REL::RelocationID(0, kID_LoadSliders).address());

	// Front-load all pattern scans so failures can be reported together before any game code is patched
	const char * failedPatterns[5] = { nullptr };
	int numFailed = 0;

	uintptr_t GetSexTarget = ScanPatternTarget("GetSex", sliderFuncBase, kSliderFuncScanRange, PatternScan::MakePattern("4D 8B B4 24 58 01 00 00 4C 89 B5 ?? ?? ?? ?? 49 8B CC E8 ?? ?? ?? ??"), 0x12, failedPatterns, numFailed);
	uintptr_t IsPlayableTarget = ScanPatternTarget("IsPlayable", sliderFuncBase, kSliderFuncScanRange, PatternScan::MakePattern("D1 E8 F6 D0 A8 01 74 ?? 48 8B CF E8 ?? ?? ?? ?? 84 C0"), 0x0B, failedPatterns, numFailed);

	uintptr_t SliderLookupTarget = 0;
	uintptr_t AddSliderTarget = 0;
	if (g_hookNativeSliders)
	{
		// SliderLookup is unique and also the landmark for AddSlider
		SliderLookupTarget = ScanPatternTarget("SliderLookup", sliderFuncBase, kSliderFuncScanRange, PatternScan::MakePattern("48 8B 4C 18 18"), 0, failedPatterns, numFailed);

		// Last per-slider block before the lookup: unique within this window (nearest decoy is >=0x200 earlier)
		if (SliderLookupTarget)
			AddSliderTarget = ScanPatternTarget("AddSlider", reinterpret_cast<const uint8_t*>(SliderLookupTarget - 0x200), 0x1C0, PatternScan::MakePattern("8D 4B FF 0F 57 C0 F3 48 0F 2A C1 F3 0F 11 44 24 ??"), 0x4D, failedPatterns, numFailed);
	}

	uintptr_t DoubleMorphTarget = 0;
	if (g_hookSliderCallbacks)
		DoubleMorphTarget = ScanPatternTarget("DoubleMorph callback", sliderFuncBase, kSliderFuncScanRange, PatternScan::MakePattern("44 8B C7 0F 28 CE 48 8B CE E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? F3 48 0F 2C D6"), 0x09, failedPatterns, numFailed);

	if (numFailed)
	{
		for (int i = 0; i < numFailed; i++)
			SKSE::log::error("{} - {} pattern not found in LoadRaceMenuSliders", __FUNCTION__, failedPatterns[i]);

		char message[512] = { 0 };
		size_t pos = (size_t)sprintf_s(message, sizeof(message), "The following hook patterns were not found:\n");
		for (int i = 0; i < numFailed; i++)
			pos += (size_t)sprintf_s(message + pos, sizeof(message) - pos, "  %s\n", failedPatterns[i]);
		sprintf_s(message + pos, sizeof(message) - pos, "\nHooks depending on these patterns will be skipped.\nContinue anyway?");

		if (!g_suppressPatternWarnings)
		{
			if (REX::W32::MessageBoxA(nullptr, message, "SKEE64 - Pattern Match Warning", kMB_YESNO | kMB_ICONWARNING) != kIDYES)
				return false;
		}
	}

	if (GetSexTarget)
		g_branchTrampoline.write_call<5>(GetSexTarget, (uintptr_t)GetSex_Hooked);

	if (IsPlayableTarget)
		g_branchTrampoline.write_call<5>(IsPlayableTarget, (uintptr_t)IsPlayable_Hooked);
	
	if (g_hookNativeSliders)
	{
		const std::uintptr_t InvokeCategoriesList_Target = REL::RelocationID(0, kID_InvokeCategoriesList_Target).address() + 0x4D6;
		g_branchTrampoline.write_call<5>(InvokeCategoriesList_Target, (uintptr_t)InvokeCategoryList_Hook);

		if (AddSliderTarget)
			g_branchTrampoline.write_call<5>(AddSliderTarget, (uintptr_t)AddSlider_Hook);
	}

	if (g_hookSliderCallbacks)
	{
		if (DoubleMorphTarget)
			g_branchTrampoline.write_call<5>(DoubleMorphTarget, (uintptr_t)DoubleMorphCallback_Hook);

		const std::uintptr_t DoubleMorphCallback2_Target = REL::RelocationID(0, kID_DoubleMorphCallback2_Target).address() + 0x50; // ChangeDoubleMorph callback
		g_branchTrampoline.write_call<5>(DoubleMorphCallback2_Target, (uintptr_t)DoubleMorphCallback_Hook);
	}

	if(g_hookNativeSliders && SliderLookupTarget)
	{
		struct SliderLookup_Entry_Code : Xbyak::CodeGenerator {
			SliderLookup_Entry_Code(std::uint64_t funcAddr, std::uint64_t targetAddr) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				lea(rcx, ptr[rax + rbx]);		 // Load Slider into RCX
				call(ptr[rip + funcLabel]);		 // Call function
				movss(xmm6, xmm0);				 // Move return into register
				mov(rcx, ptr[rcx + 0x18]);		 // Restore overwrite (this assumes our call doesnt clobber RCX)
				jmp(ptr[rip + retnLabel]);		 // Jump back

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(targetAddr + 0x5);
			}
		};
		SliderLookup_Entry_Code code((std::uint64_t)SliderLookup_Hooked, SliderLookupTarget);
		void* entry = g_localTrampoline.allocate(code);

		g_branchTrampoline.write_branch<5>(SliderLookupTarget, (std::uintptr_t)entry);
	}

	if (g_hookHeadPreprocessing && !g_externalHeads)
	{
		const std::uintptr_t PreprocessedHeads1_Target = REL::RelocationID(0, kID_PreprocessedHeads).address() + 0x58;
		const std::uintptr_t PreprocessedHeads2_Target = REL::RelocationID(0, kID_PreprocessedHeads).address() + 0x81;
		const std::uintptr_t PreprocessedHeads3_Target = REL::RelocationID(0, kID_PreprocessedHeads).address() + 0x67;
		{
			struct UsePreprocessedHeads_Entry_Code : Xbyak::CodeGenerator {
				UsePreprocessedHeads_Entry_Code(std::uint64_t funcAddr, std::uint64_t targetAddr) : Xbyak::CodeGenerator(256)
				{
					Xbyak::Label retnLabel;
					Xbyak::Label funcLabel;

					mov(rcx, rdi);					 // Move NPC into RCX
					call(ptr[rip + funcLabel]);		 // Call function
					jmp(ptr[rip + retnLabel]);		 // Jump back

					L(funcLabel);
					dq(funcAddr);

					L(retnLabel);
					dq(targetAddr + 0x6);
				}
			};

			UsePreprocessedHeads_Entry_Code code1(uintptr_t(UsePreprocessedHead), PreprocessedHeads1_Target);
			void* entry1 = g_localTrampoline.allocate(code1);

			UsePreprocessedHeads_Entry_Code code2(uintptr_t(UsePreprocessedHead), PreprocessedHeads2_Target);
			void* entry2 = g_localTrampoline.allocate(code2);

			std::uint8_t resultFix[] = {
				0x90,		// NOP
				0x84, 0xC0	// TEST al, al
			};
			std::uint8_t testFix[] = {
				0x85, 0xDB	// TEST ebx, ebx
			};

			g_branchTrampoline.write_branch<6>(PreprocessedHeads1_Target, (std::uintptr_t)entry1);
			REL::safe_write(PreprocessedHeads1_Target + 6, resultFix, sizeof(resultFix));
			g_branchTrampoline.write_branch<6>(PreprocessedHeads2_Target, (std::uintptr_t)entry2);
			REL::safe_write(PreprocessedHeads2_Target + 6, resultFix, sizeof(resultFix));

			REL::safe_write(PreprocessedHeads3_Target, testFix, sizeof(testFix));
		}

		// Preprocessing heads, used to restore mask and tinting where applicable
		{
			struct PreprocessedHeads_Code : Xbyak::CodeGenerator {
				PreprocessedHeads_Code(std::uint64_t funcAddress) : Xbyak::CodeGenerator(256)
				{
					Xbyak::Label retnLabel;

					mov(ptr[rsp-0x08+0x10], rcx);
					push(rbp);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(funcAddress + 6);
				}
			};

			const std::uintptr_t RegenerateHead_Address = REL::RelocationID(0, kID_RegenerateHead).address();
			PreprocessedHeads_Code code(RegenerateHead_Address);
			void* entry = g_localTrampoline.allocate(code);
			RegenerateHead_Original = (RegenerateHeadFn)entry;
			g_branchTrampoline.write_branch<6>(RegenerateHead_Address, (std::uintptr_t)RegenerateHead_Hooked);
		}
	}

	if (g_hookMorphExtensions && g_extendedMorphs)
	{
		struct BSFaceGenModel_ApplyMorph_Code : Xbyak::CodeGenerator {
			BSFaceGenModel_ApplyMorph_Code(uintptr_t address) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				push(rsi);
				push(rdi);
				push(r14);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(address + 5);
			}
		};

		const std::uintptr_t BSFaceGenModel_ApplyMorph_Address = REL::RelocationID(0, kID_BSFaceGenModel_ApplyMorph).address();
		BSFaceGenModel_ApplyMorph_Code code(BSFaceGenModel_ApplyMorph_Address);
		void* entry = g_localTrampoline.allocate(code);
		BSFaceGenModel_ApplyMorph_Original = (BSFaceGenModelApplyMorphFn)entry;
		g_branchTrampoline.write_branch<5>(BSFaceGenModel_ApplyMorph_Address, (uintptr_t)ApplyChargenMorph_Hooked);

		const std::uintptr_t ApplyRaceMorph_Target = REL::RelocationID(0, kID_ApplyRaceMorph_Target).address() + 0x124;
		g_branchTrampoline.write_call<5>(ApplyRaceMorph_Target, (uintptr_t)ApplyRaceMorph_Hooked); // Revisit
	}

	if (g_hookMorphUpdates)
	{
		const std::uintptr_t UpdateMorphs_Target = REL::RelocationID(0, kID_UpdateMorphs_Target).address() + 0xB7;
		g_branchTrampoline.write_call<5>(UpdateMorphs_Target, (uintptr_t)UpdateMorphs_Hooked);

		const std::uintptr_t UpdateMorph_Target = REL::RelocationID(0, kID_UpdateMorph_Target).address() + 0x7E;
		g_branchTrampoline.write_call<5>(UpdateMorph_Target, (uintptr_t)UpdateMorph_Hooked);
	}

	// Hooking Dynamic Geometry Alloc/Free to add intrusive refcount
	// This hook is very sad but BSDynamicTriShape render data has no refcount so we need implement it
	if(g_hookFaceOverlays && g_enableFaceOverlays)
	{
		const std::uintptr_t NiAllocate_Geom_Target = REL::RelocationID(0, kID_NiAllocate_Geom).address() + 0x147;
		g_branchTrampoline.write_call<5>(NiAllocate_Geom_Target, (uintptr_t)NiAllocate_Hooked);

		const std::uintptr_t NiFree_Geom_Target = REL::RelocationID(0, kID_NiAllocate_Geom).address() + 0x140;
		g_branchTrampoline.write_call<5>(NiFree_Geom_Target, (uintptr_t)NiFree_Hooked);

		const std::uintptr_t NiAllocate_Geom2_Target = REL::RelocationID(0, kID_NiAllocate_Geom2_Target).address() + 0x76;
		g_branchTrampoline.write_call<5>(NiAllocate_Geom2_Target, (uintptr_t)NiAllocate_Hooked);

		const std::uintptr_t NiFree_Geom2_Target = REL::RelocationID(0, kID_NiFree_Geom2_Target).address() + 0x2F;
		g_branchTrampoline.write_call<5>(NiFree_Geom2_Target, (uintptr_t)NiFree_Hooked);

		const std::uintptr_t UpdateHeadState_Target1 = REL::RelocationID(0, kID_UpdateHeadState_Target1).address() + 0x169;
		g_branchTrampoline.write_call<5>(UpdateHeadState_Target1, (uintptr_t)UpdateHeadState_Enable_Hooked);

		const std::uintptr_t UpdateHeadState_Target2 = REL::RelocationID(0, kID_UpdateHeadState_Target2).address() + 0x2EB;
		g_branchTrampoline.write_call<5>(UpdateHeadState_Target2, (uintptr_t)UpdateHeadState_Disabled_Hooked);
	}

	const std::uintptr_t RaceSexMenu_Render_Target = REL::RelocationID(0, kID_RaceSexMenu_Vtable).address() + 0x30; // ??_7RaceSexMenu@@6B@
	{ std::uintptr_t _sw64 = (std::uintptr_t)RaceSexMenu_Render_Hooked; REL::safe_write(RaceSexMenu_Render_Target, &_sw64, sizeof(_sw64)); }

	if (g_disableFaceGenCache)
	{
		const std::uintptr_t CachePartsTarget_Target = REL::RelocationID(0, kID_CachePartsTarget_Target).address();
		{ std::uint8_t _sw8 = 0xC3; REL::safe_write(CachePartsTarget_Target, &_sw8, sizeof(_sw8)); }
	}

	if(g_hookBipedAttach)
	{
		struct AttachBipedObjectHook_Entry_Code : Xbyak::CodeGenerator {
			AttachBipedObjectHook_Entry_Code(uintptr_t address) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				mov(rax, rsp);
				mov(ptr[rax + 0x20], r9b);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(address + 7);
			}
		};

		const std::uintptr_t AttachBipedObject_Address = REL::RelocationID(0, kID_AttachBipedObject).address();
		AttachBipedObjectHook_Entry_Code code(AttachBipedObject_Address);
		void* entry = g_localTrampoline.allocate(code);
		AttachBipedObject_Original = reinterpret_cast<AttachBipedObjectFn>(entry);

		g_branchTrampoline.write_branch<6>(AttachBipedObject_Address, (uintptr_t)AttachBipedObject_Hooked);
	}

	if (g_hookTinting && g_enableTintSync)
	{
		g_branchTrampoline.write_branch<6>(REL::RelocationID(0, kID_UpdateModelSkin).address(), (uintptr_t)UpdateModelSkin_Hooked);
		g_branchTrampoline.write_branch<6>(REL::RelocationID(0, kID_UpdateModelHair).address(), (uintptr_t)UpdateModelHair_Hooked);
	}

	if (g_hookTintInventory && g_enableTintInventory)
	{
		const std::uintptr_t SetNewInventoryItemModel_Target = REL::RelocationID(0, kID_SetNewInventoryItemModel_Target).address() + 0x1C3;
		struct TintInventoryItem_Code : Xbyak::CodeGenerator {
			TintInventoryItem_Code(std::uint64_t funcAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label retnLabel;

				push(rbx);
				sub(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(funcAddress + 6);
			}
		};

		const std::uintptr_t SetInventoryItemModel_Address = REL::RelocationID(0, kID_SetInventoryItemModel).address();
		TintInventoryItem_Code code(SetInventoryItemModel_Address);
		void* entry = g_localTrampoline.allocate(code);

		SetInventoryItemModel_Original = (SetInventoryItemModelFn)entry;

		g_branchTrampoline.write_branch<6>(SetInventoryItemModel_Address, (uintptr_t)SetInventoryItemModel_Hooked);

		g_branchTrampoline.write_call<5>(SetNewInventoryItemModel_Target, (uintptr_t)SetNewInventoryItemModel_Hooked);
	}

	{
		const std::uintptr_t TransferItemUID_Address = REL::RelocationID(0, kID_TransferItemUID).address();
		struct TransferItemUID_Code : Xbyak::CodeGenerator {
			TransferItemUID_Code(uintptr_t transferUIDAddress) : Xbyak::CodeGenerator(256)
			{
				Xbyak::Label retnLabel;

				push(rbx);
				push(rbp);
				push(rsi);
				push(rdi);
				push(r14);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(transferUIDAddress + 7);
			}
		};

		TransferItemUID_Code code(TransferItemUID_Address);
		void* entry = g_localTrampoline.allocate(code);

		TransferItemUID_Original = (TransferItemUIDFn)entry;

		g_branchTrampoline.write_branch<6>(TransferItemUID_Address, (uintptr_t)TransferItemUID_Hooked);
	}

	// First console command record (legacy g_firstConsoleCommand; Reloc ID 365650)
	// is now RE::SCRIPT_FUNCTION::GetFirstConsoleCommand().
	RE::SCRIPT_FUNCTION * hijackedCommand = nullptr;
	for (RE::SCRIPT_FUNCTION * iter = RE::SCRIPT_FUNCTION::GetFirstConsoleCommand(); static_cast<std::uint32_t>(iter->output) < RE::SCRIPT_FUNCTION::Commands::kConsoleCommandsEnd + RE::SCRIPT_FUNCTION::Commands::kConsoleOpBase; ++iter)
	{
		if (!strcmp(iter->functionName, "JobListTool"))
		{
			hijackedCommand = iter;
			break;
		}
	}
	if (hijackedCommand)
	{
		static RE::SCRIPT_PARAMETER params[] = {
			{"String", RE::SCRIPT_PARAM_TYPE::kChar, false},
			{"String (optional)", RE::SCRIPT_PARAM_TYPE::kChar, true}
		};

		RE::SCRIPT_FUNCTION cmd = *hijackedCommand;
		cmd.functionName = "skee";
		cmd.shortName = "skee";
		cmd.helpString = "skee help";
		cmd.referenceFunction = false;
		cmd.SetParameters(params);
		cmd.executeFunction = SKEE_Execute;
		// Zero the flag bytes at 0x48 (legacy ObScriptCommand::flags = 0).
		cmd.editorFilter = false;
		cmd.invalidatesCellList = false;
		cmd.pad4A = 0;
		REL::safe_write((uintptr_t)hijackedCommand, &cmd, sizeof(cmd));
	}

	return true;
}

namespace
{
	constexpr std::string_view HookStatusName(SKEEHookStatus a_status)
	{
		switch (a_status) {
		case SKEEHookStatus::kEnabled:
			return "enabled";
		case SKEEHookStatus::kDisabledByPolicy:
			return "disabled-by-policy";
		case SKEEHookStatus::kUnavailableAddress:
			return "unavailable-address";
		case SKEEHookStatus::kSignatureMismatch:
			return "signature-mismatch";
		case SKEEHookStatus::kInitializationFailed:
			return "initialization-failed";
		}

		return "initialization-failed";
	}

	SKEEHookInstallResult MakeVRHookResult(
		const SKEEHookGroupResult& a_bodySystems,
		const SKEEHookGroupResult& a_nativeSliders,
		const SKEEHookGroupResult& a_morphs,
		const SKEEHookGroupResult& a_sculpt,
		const SKEEHookGroupResult& a_headPresets,
		const SKEEHookGroupResult& a_tintInventory,
		const SKEEHookGroupResult& a_faceOverlays,
		const SKEEHookGroupResult& a_faceGenCache)
	{
		return {
			true,
			{ {
				{ "core-services", SKEEHookStatus::kEnabled, "SKSE lifecycle, serialization, Papyrus, and plugin interfaces remain available" },
				a_bodySystems,
				a_nativeSliders,
				a_morphs,
				a_sculpt,
				a_headPresets,
				a_tintInventory,
				a_faceOverlays,
				a_faceGenCache
			} }
		};
	}

	SKEEHookInstallResult MakeFlatHookResult(bool a_success)
	{
		const auto activeStatus = a_success ? SKEEHookStatus::kEnabled : SKEEHookStatus::kInitializationFailed;
		return {
			a_success,
			{ {
				{ "core-services", SKEEHookStatus::kEnabled, "SKSE lifecycle, serialization, Papyrus, and plugin interfaces remain available" },
				{ "body-systems", activeStatus, "flat-runtime body behavior is preserved" },
				{ "native-face-sliders", activeStatus, "flat-runtime slider behavior is preserved" },
				{ "face-morph-application", activeStatus, "flat-runtime morph behavior is preserved" },
				{ "sculpt-rendering", activeStatus, "flat-runtime sculpt behavior is preserved" },
				{ "head-preset-compatibility", activeStatus, "flat-runtime head and preset behavior is preserved" },
				{ "tint-and-inventory", activeStatus, "flat-runtime tint and inventory behavior is preserved" },
				{ "face-overlays", activeStatus, "flat-runtime face overlay behavior is preserved" },
				{ "facegen-cache-bypass", activeStatus, "flat-runtime FaceGen cache policy is preserved" }
			} }
		};
	}

	void LogHookResult(const SKEEHookInstallResult& a_result)
	{
		SKSE::log::info(
			"RaceMenu VR 2 hook qualification: runtime={}, CommonLibSSE-NG={}, Skyrim VR Address Library baseline={}",
			REL::Module::get().version().string("."),
			SKEE_COMMONLIB_VERSION,
			SKEE_VR_ADDRESS_LIBRARY_VERSION);

		for (const auto& group : a_result.groups) {
			SKSE::log::info("hook-group {}: {} - {}", group.group, HookStatusName(group.status), group.consequence);
		}
	}
}

SKEEHookInstallResult InstallSKEEHooks()
{
	static std::mutex installLock;
	static std::optional<SKEEHookInstallResult> installedResult;
	std::lock_guard lock{ installLock };
	if (installedResult) {
		SKSE::log::info("SKEE hooks were already evaluated; returning the original install result without patching twice");
		return *installedResult;
	}

	if (REL::Module::IsVR()) {
#if defined(ENABLE_SKYRIM_VR)
		if (!SKEE::HasQualifiedCustomAddresses()) {
			const SKEEHookGroupResult bodySystems{ "body-systems", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult nativeSliders{ "native-face-sliders", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult morphs{ "face-morph-application", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult sculpt{ "sculpt-rendering", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult headPresets{ "head-preset-compatibility", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult tintInventory{ "tint-and-inventory", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult faceOverlays{ "face-overlays", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			const SKEEHookGroupResult faceGenCache{ "facegen-cache-bypass", SKEEHookStatus::kSignatureMismatch, "the exact SkyrimVR 1.4.15 helper-entry contract did not match; no SKEE hooks were written" };
			auto result = MakeVRHookResult(bodySystems, nativeSliders, morphs, sculpt, headPresets, tintInventory, faceOverlays, faceGenCache);
			result.success = false;
			LogHookResult(result);
			installedResult = result;
			return *installedResult;
		}
		const auto bodySystems = InstallVRBodySystemHooks();
		const auto nativeSliders = InstallVRNativeSliderHooks();
		const auto morphs = InstallVRMorphHooks();
		const auto sculpt = InstallVRSculptRenderHook();
		const auto headPresets = InstallVRHeadPresetHooks();
		const auto tintInventory = InstallVRTintInventoryHooks();
		const auto faceOverlays = InstallVRFaceOverlayHooks();
		const auto faceGenCache = InstallVRFaceGenCacheBypass();
#else
		const SKEEHookGroupResult bodySystems{
			"body-systems",
			SKEEHookStatus::kUnavailableAddress,
			"VR armor attachment callbacks are unavailable in this build"
		};
		const SKEEHookGroupResult nativeSliders{ "native-face-sliders", SKEEHookStatus::kUnavailableAddress, "VR slider hooks are unavailable in this build" };
		const SKEEHookGroupResult morphs{ "face-morph-application", SKEEHookStatus::kUnavailableAddress, "VR morph hooks are unavailable in this build" };
		const SKEEHookGroupResult sculpt{ "sculpt-rendering", SKEEHookStatus::kUnavailableAddress, "VR sculpt hooks are unavailable in this build" };
		const SKEEHookGroupResult headPresets{ "head-preset-compatibility", SKEEHookStatus::kUnavailableAddress, "VR head hooks are unavailable in this build" };
		const SKEEHookGroupResult tintInventory{ "tint-and-inventory", SKEEHookStatus::kUnavailableAddress, "VR tint hooks are unavailable in this build" };
		const SKEEHookGroupResult faceOverlays{ "face-overlays", SKEEHookStatus::kUnavailableAddress, "VR face overlay hooks are unavailable in this build" };
		const SKEEHookGroupResult faceGenCache{ "facegen-cache-bypass", SKEEHookStatus::kUnavailableAddress, "VR FaceGen cache hooks are unavailable in this build" };
#endif
		auto result = MakeVRHookResult(bodySystems, nativeSliders, morphs, sculpt, headPresets, tintInventory, faceOverlays, faceGenCache);
		LogHookResult(result);
		installedResult = result;
		return *installedResult;
	}

	auto result = MakeFlatHookResult(InstallFlatSKEEHooks());
	LogHookResult(result);
	installedResult = result;
	return *installedResult;
}
