#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <RE/B/BSFixedString.h>
#include <REL/Relocation.h>

#include "RaceMenuTypes.h"

// Game types used only by pointer/reference in the signatures below. The
// project PCH (RE/Skyrim.h) provides full definitions to every TU.
namespace RE
{
	class Actor;
	class BGSHeadPart;
	class BSFaceGenManager;
	class BSFaceGenModel;
	class BSFaceGenNiNode;
	class BSFadeNode;
	class BSGeometry;
	class BSLightingShaderMaterial;
	class BSLightingShaderProperty;
	class BSTextureSet;
	struct  BIPOBJECT;
	class BipedAnim;
	class BSTriShape;
	class BSDynamicTriShape;
	class ExtraDataList;
	class FxResponseArgsBase;
	class GFxMovieView;
	class Inventory3DManager;
	class InventoryChanges;
	class NiAVObject;
	class NiColorA;
	class NiGeometryData;
	class NiNode;
	class NiObject;
	class NiSourceTexture;
	class NiStream;
	class TESForm;
	class TESModelTri;
	class TESNPC;
	class TESObjectREFR;
	class TaskQueueInterface;
	template <class T> class NiPointer;
}

// ============================================================================
// Relocation IDs — single source of truth for every qualified flat-runtime
// game address SKEE uses, sorted by AE ID. Skyrim VR uses fixed 1.4.15 RVAs
// independently qualified against the exact executable, full-memory dump,
// CommonLib contracts, and VR Address Library mappings.
// ============================================================================
inline constexpr std::uint32_t kID_LookupREFRByHandle              = 12331;
inline constexpr std::uint32_t kID_AttachBipedObject               = 15711;
inline constexpr std::uint32_t kID_InventoryChanges_SetUniqueID    = 16147;
inline constexpr std::uint32_t kID_TransferItemUID                 = 16149;
inline constexpr std::uint32_t kID_UpdateNPCMorphs                 = 24713;
inline constexpr std::uint32_t kID_UpdateNPCMorph                  = 24714;
inline constexpr std::uint32_t kID_UpdateHeadState                 = 24724;
inline constexpr std::uint32_t kID_UpdateHeadState_Target2         = 24725;
inline constexpr std::uint32_t kID_PreprocessedHeads               = 24731;
inline constexpr std::uint32_t kID_UpdateHeadState_Target1         = 24732;
inline constexpr std::uint32_t kID_FaceGen_ApplyMorph              = 26831;
inline constexpr std::uint32_t kID_FaceGen_ApplyMorphByPart        = 26833;
inline constexpr std::uint32_t kID_UpdateMorphs_Target             = 26835;
inline constexpr std::uint32_t kID_ApplyRaceMorph_Target           = 26836;
inline constexpr std::uint32_t kID_RegenerateHead                  = 26838;
inline constexpr std::uint32_t kID_BSFaceGenModel_ApplyRaceMorph   = 26882;
inline constexpr std::uint32_t kID_BSFaceGenModel_ApplyMorph       = 26883;
inline constexpr std::uint32_t kID_UpdateModelFace                 = 27044;
inline constexpr std::uint32_t kID_UpdateMorph_Target              = 27061;
inline constexpr std::uint32_t kID_ChangeActorHeadPart             = 27063;
inline constexpr std::uint32_t kID_UpdateModelSkin                 = 27066;
inline constexpr std::uint32_t kID_UpdateModelHair                 = 27067;
inline constexpr std::uint32_t kID_SetNiGeometryTexture            = 36987;
inline constexpr std::uint32_t kID_SetNewInventoryItemModel        = 51772;
inline constexpr std::uint32_t kID_SetInventoryItemModel           = 51775;
inline constexpr std::uint32_t kID_SetNewInventoryItemModel_Target = 51776;
inline constexpr std::uint32_t kID_DoubleMorphCallback2_Target     = 52356;
inline constexpr std::uint32_t kID_CachePartsTarget_Target         = 52369;
inline constexpr std::uint32_t kID_DoubleMorphCallback             = 52401;
inline constexpr std::uint32_t kID_InvokeCategoriesList_Target     = 52407;
inline constexpr std::uint32_t kID_LoadSliders                     = 52409;
inline constexpr std::uint32_t kID_AddRaceMenuSlider               = 52453;
inline constexpr std::uint32_t kID_NiStreamCtor                    = 70324;
inline constexpr std::uint32_t kID_NiStreamDtor                    = 70325;
inline constexpr std::uint32_t kID_NiStreamAddObject               = 70326;
inline constexpr std::uint32_t kID_CreateBSTriShape                = 70655;
inline constexpr std::uint32_t kID_CreateSourceTexture             = 70717;
inline constexpr std::uint32_t kID_NiAllocate_Geom                 = 70939;
inline constexpr std::uint32_t kID_CreateBSDynamicTriShape         = 70946;
inline constexpr std::uint32_t kID_NiAllocate_Geom2_Target         = 70947;
inline constexpr std::uint32_t kID_NiFree_Geom2_Target             = 70958;
inline constexpr std::uint32_t kID_NiTriBasedGeomCtor              = 71852;
inline constexpr std::uint32_t kID_GFxInvokeFunction               = 82640;
inline constexpr std::uint32_t kID_BSFadeNodeCtor                  = 105481;
inline constexpr std::uint32_t kID_InitializeShader                = 106507;
inline constexpr std::uint32_t kID_InvalidateTextures              = 106510;
inline constexpr std::uint32_t kID_CopyFrom                        = 106713;
inline constexpr std::uint32_t kID_RaceSexMenu_Vtable              = 215885; // ??_7RaceSexMenu@@6B@
inline constexpr std::uint32_t kID_useFaceGenPreProcessedHeads     = 378620; // ini setting (data, not a function)

// The Skyrim VR Address Library identifies these SSE IDs as bit-for-bit
// identical in Skyrim VR 1.4.15. They deliberately do not use the kID_ prefix:
// that prefix denotes the AE-keyed inventory above.
inline constexpr std::uint32_t kReloc_SetUniqueIDSEVR = 15907;
inline constexpr std::uint32_t kReloc_UpdateHeadStateSEVR = 24220;
inline constexpr std::uint32_t kReloc_NiStreamCtorSEVR = 68971;
inline constexpr std::uint32_t kReloc_NiStreamDtorSEVR = 68972;

// Function-pointer types for the hooked functions whose unpatched originals
// are kept in code-cave trampolines (see the *_Original holders below).
using AttachBipedObjectFn        = RE::NiNode* (*)(RE::BipedAnim*, RE::NiNode*, std::uint32_t, std::uint8_t, std::uint8_t, std::uint64_t);
using RegenerateHeadFn           = void (*)(RE::BSFaceGenManager*, RE::BSFaceGenNiNode*, RE::BGSHeadPart*, RE::TESNPC*);
using BSFaceGenModelApplyMorphFn = std::uint8_t (*)(RE::BSFaceGenModel*, RE::BSFixedString*, RE::TESModelTri*, RE::NiAVObject**, float, std::uint8_t);
using SetInventoryItemModelFn    = void (*)(RE::Inventory3DManager*, RE::TESForm*, RE::ExtraDataList*);
using TransferItemUIDFn          = void (*)(RE::InventoryChanges*, RE::ExtraDataList*, RE::TESForm*, RE::TESForm*, std::uint32_t);

// Originals of the hooked functions. InstallSKEEHooks() sets each to the entry
// of a code-cave trampoline that calls through to the unpatched game function;
// nullptr until the corresponding hook is installed.
extern AttachBipedObjectFn        AttachBipedObject_Original;
extern RegenerateHeadFn           RegenerateHead_Original;
extern BSFaceGenModelApplyMorphFn BSFaceGenModel_ApplyMorph_Original;
extern SetInventoryItemModelFn    SetInventoryItemModel_Original;
extern TransferItemUIDFn          TransferItemUID_Original;

// Retain a RaceMenu-adjusted BSDynamicTriShape data buffer.  The lookup and
// reference-count increment are one locked operation because Skyrim may
// destroy dynamic geometry on a BSJobs worker while overlays are cloned on
// another thread.
bool RetainAdjustedDynamicData(void* a_data);

// Game functions SKEE calls directly (not exposed by CommonLibSSE-NG). Each
// wrapper lazily resolves its own Relocation on first use.
namespace SKEE
{
	[[nodiscard]] bool HasQualifiedVRHelperAddresses() noexcept;

	template <class Fn>
	[[nodiscard]] inline Fn VRFunction(std::uintptr_t a_rva) noexcept
	{
		return reinterpret_cast<Fn>(REL::Offset(a_rva).address());
	}

	[[nodiscard]] inline bool HasQualifiedCustomAddresses() noexcept
	{
		return REL::Module::IsVR() ?
			REL::Module::get().version() == REL::Version{ 1, 4, 15, 0 } && HasQualifiedVRHelperAddresses() :
			REL::Module::GetRuntime() != REL::Module::Runtime::Unknown;
	}

	[[nodiscard]] inline bool HasQualifiedNiStreamLifecycleAddresses() noexcept
	{
		return HasQualifiedCustomAddresses();
	}

	// --- BSLightingShaderProperty / material helpers --------------------------

	inline std::uint32_t InitializeShader(RE::BSLightingShaderProperty* a_this, RE::BSGeometry* a_geometry)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint32_t (*)(RE::BSLightingShaderProperty*, RE::BSGeometry*)>(0x01303AC0)(a_this, a_geometry);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint32_t (*)(RE::BSLightingShaderProperty*, RE::BSGeometry*)> func{ REL::RelocationID(0, kID_InitializeShader) };
		return func(a_this, a_geometry);
	}

	void InvalidateTextures(RE::BSLightingShaderProperty* a_this, std::uint32_t a_unk1);

	inline void CopyFrom(RE::BSLightingShaderMaterial* a_this, RE::BSLightingShaderMaterial* a_other)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::BSLightingShaderMaterial*, RE::BSLightingShaderMaterial*)>(0x0130DE50)(a_this, a_other);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::BSLightingShaderMaterial*, RE::BSLightingShaderMaterial*)> func{ REL::RelocationID(0, kID_CopyFrom) };
		func(a_this, a_other);
	}

	inline std::uint32_t SetNiGeometryTexture(RE::TaskQueueInterface* a_this, RE::NiAVObject* a_geometry, RE::BSTextureSet* a_textureSet)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint32_t (*)(RE::TaskQueueInterface*, RE::NiAVObject*, RE::BSTextureSet*)>(0x005CF190)(a_this, a_geometry, a_textureSet);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint32_t (*)(RE::TaskQueueInterface*, RE::NiAVObject*, RE::BSTextureSet*)> func{ REL::RelocationID(0, kID_SetNiGeometryTexture) };
		return func(a_this, a_geometry, a_textureSet);
	}

	// --- FaceGen / head morphs --------------------------------------------------

	inline void FaceGen_ApplyMorph(RE::BSFaceGenManager* a_this, RE::BSFaceGenNiNode* a_faceNode, RE::TESNPC* a_npc, const RE::BSFixedString& a_morphName, float a_relative)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::BSFaceGenManager*, RE::BSFaceGenNiNode*, RE::TESNPC*, const RE::BSFixedString&, float)>(0x003E1B80)(a_this, a_faceNode, a_npc, a_morphName, a_relative);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::BSFaceGenManager*, RE::BSFaceGenNiNode*, RE::TESNPC*, const RE::BSFixedString&, float)> func{ REL::RelocationID(0, kID_FaceGen_ApplyMorph) };
		func(a_this, a_faceNode, a_npc, a_morphName, a_relative);
	}

	inline void FaceGen_ApplyMorphByPart(RE::BSFaceGenManager* a_this, RE::BSFaceGenNiNode* a_faceNode, RE::BGSHeadPart* a_headPart, const RE::BSFixedString& a_morphName, float a_relative)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::BSFaceGenManager*, RE::BSFaceGenNiNode*, RE::BGSHeadPart*, const RE::BSFixedString&, float)>(0x003E1CE0)(a_this, a_faceNode, a_headPart, a_morphName, a_relative);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::BSFaceGenManager*, RE::BSFaceGenNiNode*, RE::BGSHeadPart*, const RE::BSFixedString&, float)> func{ REL::RelocationID(0, kID_FaceGen_ApplyMorphByPart) };
		func(a_this, a_faceNode, a_headPart, a_morphName, a_relative);
	}

	inline std::uint8_t BSFaceGenModel_ApplyRaceMorph(RE::BSFaceGenModel* a_this, RE::BSFixedString* a_morphName, RE::TESModelTri* a_modelMorph, RE::NiAVObject** a_headNode, float a_relative, std::uint8_t a_unk1)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint8_t (*)(RE::BSFaceGenModel*, RE::BSFixedString*, RE::TESModelTri*, RE::NiAVObject**, float, std::uint8_t)>(0x003E3FA0)(a_this, a_morphName, a_modelMorph, a_headNode, a_relative, a_unk1);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint8_t (*)(RE::BSFaceGenModel*, RE::BSFixedString*, RE::TESModelTri*, RE::NiAVObject**, float, std::uint8_t)> func{ REL::RelocationID(0, kID_BSFaceGenModel_ApplyRaceMorph) };
		return func(a_this, a_morphName, a_modelMorph, a_headNode, a_relative, a_unk1);
	}

	inline void UpdateNPCMorphs(RE::TESNPC* a_npc, void* a_unk1, RE::BSFaceGenNiNode* a_faceNode)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::TESNPC*, void*, RE::BSFaceGenNiNode*)>(0x00370140)(a_npc, a_unk1, a_faceNode);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::TESNPC*, void*, RE::BSFaceGenNiNode*)> func{ REL::RelocationID(0, kID_UpdateNPCMorphs) };
		func(a_npc, a_unk1, a_faceNode);
	}

	inline void UpdateNPCMorph(RE::TESNPC* a_npc, RE::BGSHeadPart* a_headPart, RE::BSFaceGenNiNode* a_faceNode)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::TESNPC*, RE::BGSHeadPart*, RE::BSFaceGenNiNode*)>(0x00370330)(a_npc, a_headPart, a_faceNode);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::TESNPC*, RE::BGSHeadPart*, RE::BSFaceGenNiNode*)> func{ REL::RelocationID(0, kID_UpdateNPCMorph) };
		func(a_npc, a_headPart, a_faceNode);
	}

	inline std::int32_t UpdateHeadState(RE::TESNPC* a_npc, RE::Actor* a_actor, std::uint32_t a_unk1)
	{
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::int32_t (*)(RE::TESNPC*, RE::Actor*, std::uint32_t)> func{
			REL::RelocationID(kReloc_UpdateHeadStateSEVR, kID_UpdateHeadState, kReloc_UpdateHeadStateSEVR)
		};
		return func(a_npc, a_actor, a_unk1);
	}

	inline void ChangeActorHeadPart(RE::Actor* a_this, RE::BGSHeadPart* a_oldPart, RE::BGSHeadPart* a_newPart)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::Actor*, RE::BGSHeadPart*, RE::BGSHeadPart*)>(0x003EBD30)(a_this, a_oldPart, a_newPart);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::Actor*, RE::BGSHeadPart*, RE::BGSHeadPart*)> func{ REL::RelocationID(0, kID_ChangeActorHeadPart) };
		func(a_this, a_oldPart, a_newPart);
	}

	inline std::uint32_t UpdateModelFace(RE::NiAVObject* a_object)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint32_t (*)(RE::NiAVObject*)>(0x003EB710)(a_object);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint32_t (*)(RE::NiAVObject*)> func{ REL::RelocationID(0, kID_UpdateModelFace) };
		return func(a_object);
	}

	inline std::uint32_t UpdateModelSkin(RE::NiAVObject* a_object, RE::NiColorA** a_color)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint32_t (*)(RE::NiAVObject*, RE::NiColorA**)>(0x003EC090)(a_object, a_color);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint32_t (*)(RE::NiAVObject*, RE::NiColorA**)> func{ REL::RelocationID(0, kID_UpdateModelSkin) };
		return func(a_object, a_color);
	}

	inline std::uint32_t UpdateModelHair(RE::NiAVObject* a_object, RE::NiColorA** a_color)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::uint32_t (*)(RE::NiAVObject*, RE::NiColorA**)>(0x003EC150)(a_object, a_color);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::uint32_t (*)(RE::NiAVObject*, RE::NiColorA**)> func{ REL::RelocationID(0, kID_UpdateModelHair) };
		return func(a_object, a_color);
	}

	// --- Race menu sliders ------------------------------------------------------

	inline std::int32_t AddRaceMenuSlider(RE::RaceMenuSliderArray* a_sliders, RE::RaceMenuSlider* a_slider)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<std::int32_t (*)(RE::RaceMenuSliderArray*, RE::RaceMenuSlider*)>(0x008E9850)(a_sliders, a_slider);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::int32_t (*)(RE::RaceMenuSliderArray*, RE::RaceMenuSlider*)> func{ REL::RelocationID(0, kID_AddRaceMenuSlider) };
		return func(a_sliders, a_slider);
	}

	inline void DoubleMorphCallback(RE::RaceSexMenu* a_menu, float a_newValue, std::uint32_t a_sliderId)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::RaceSexMenu*, float, std::uint32_t)>(0x008E23B0)(a_menu, a_newValue, a_sliderId);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::RaceSexMenu*, float, std::uint32_t)> func{ REL::RelocationID(0, kID_DoubleMorphCallback) };
		func(a_menu, a_newValue, a_sliderId);
	}

	inline void* LoadSliders(RE::RaceSexMenu* a_menu, std::uint64_t a_unk1, std::uint8_t a_unk2)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<void* (*)(RE::RaceSexMenu*, std::uint64_t, std::uint8_t)>(0x008E39B0)(a_menu, a_unk1, a_unk2);
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<void* (*)(RE::RaceSexMenu*, std::uint64_t, std::uint8_t)> func{ REL::RelocationID(0, kID_LoadSliders) };
		return func(a_menu, a_unk1, a_unk2);
	}

	// --- Geometry / NiStream creation -------------------------------------------

	inline void NiTriBasedGeomCtor(RE::NiAVObject* a_this, RE::NiGeometryData* a_data)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::NiAVObject*, RE::NiGeometryData*)>(0x00CC7420)(a_this, a_data);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::NiAVObject*, RE::NiGeometryData*)> func{ REL::RelocationID(0, kID_NiTriBasedGeomCtor) };
		func(a_this, a_data);
	}

	inline RE::BSTriShape* CreateBSTriShape()
	{
		if (REL::Module::IsVR()) {
			return VRFunction<RE::BSTriShape* (*)()>(0x00CAD6D0)();
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<RE::BSTriShape* (*)()> func{ REL::RelocationID(0, kID_CreateBSTriShape) };
		return func();
	}

	inline RE::BSDynamicTriShape* CreateBSDynamicTriShape()
	{
		if (REL::Module::IsVR()) {
			return VRFunction<RE::BSDynamicTriShape* (*)()>(0x00CB8530)();
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<RE::BSDynamicTriShape* (*)()> func{ REL::RelocationID(0, kID_CreateBSDynamicTriShape) };
		return func();
	}

	inline RE::NiStream* NiStreamCtor(RE::NiStream* a_this)
	{
		if (!a_this || !HasQualifiedNiStreamLifecycleAddresses()) {
			return nullptr;
		}
		static REL::Relocation<RE::NiStream* (*)(RE::NiStream*)> func{
			REL::RelocationID(kReloc_NiStreamCtorSEVR, kID_NiStreamCtor, kReloc_NiStreamCtorSEVR)
		};
		return func(a_this);
	}

	inline void NiStreamDtor(RE::NiStream* a_this)
	{
		if (!a_this || !HasQualifiedNiStreamLifecycleAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::NiStream*)> func{
			REL::RelocationID(kReloc_NiStreamDtorSEVR, kID_NiStreamDtor, kReloc_NiStreamDtorSEVR)
		};
		func(a_this);
	}

	inline bool NiStreamAddObject(RE::NiStream* a_this, RE::NiObject* a_object)
	{
		if (!a_this || !a_object || !HasQualifiedCustomAddresses()) {
			return false;
		}
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::NiStream*, RE::NiObject*)>(0x00C9F090)(a_this, a_object);
			return true;
		}
		static REL::Relocation<void (*)(RE::NiStream*, RE::NiObject*)> func{ REL::RelocationID(0, kID_NiStreamAddObject) };
		func(a_this, a_object);
		return true;
	}

	inline RE::BSFadeNode* BSFadeNodeCtor(RE::BSFadeNode* a_this)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<RE::BSFadeNode* (*)(RE::BSFadeNode*)>(0x012C8230)(a_this);
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<RE::BSFadeNode* (*)(RE::BSFadeNode*)> func{ REL::RelocationID(0, kID_BSFadeNodeCtor) };
		return func(a_this);
	}

	inline RE::NiSourceTexture* CreateSourceTexture(const RE::BSFixedString& a_name)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<RE::NiSourceTexture* (*)(const RE::BSFixedString&)>(0x00CAEF60)(a_name);
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<RE::NiSourceTexture* (*)(const RE::BSFixedString&)> func{ REL::RelocationID(0, kID_CreateSourceTexture) };
		return func(a_name);
	}

	// --- Inventory / tinting ------------------------------------------------------

	inline void SetNewInventoryItemModel(void* a_unk1, RE::TESForm* a_form1, RE::TESForm* a_form2, RE::NiNode** a_node)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(void*, RE::TESForm*, RE::TESForm*, RE::NiNode**)>(0x008B5B40)(a_unk1, a_form1, a_form2, a_node);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(void*, RE::TESForm*, RE::TESForm*, RE::NiNode**)> func{ REL::RelocationID(0, kID_SetNewInventoryItemModel) };
		func(a_unk1, a_form1, a_form2, a_node);
	}

	inline void InventoryChanges_SetUniqueID(RE::InventoryChanges* a_this, RE::ExtraDataList* a_extraList, RE::TESForm* a_oldForm, RE::TESForm* a_newForm)
	{
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::InventoryChanges*, RE::ExtraDataList*, RE::TESForm*, RE::TESForm*)> func{
			REL::RelocationID(kReloc_SetUniqueIDSEVR, kID_InventoryChanges_SetUniqueID, kReloc_SetUniqueIDSEVR)
		};
		func(a_this, a_extraList, a_oldForm, a_newForm);
	}

	// --- Misc ---------------------------------------------------------------------

	inline void* GFxInvokeFunction(RE::GFxMovieView* a_movie, const char* a_fnName, RE::FxResponseArgsBase& a_arguments)
	{
		if (REL::Module::IsVR()) {
			return VRFunction<void* (*)(RE::GFxMovieView*, const char*, RE::FxResponseArgsBase*)>(0x00F342E0)(a_movie, a_fnName, std::addressof(a_arguments));
		}
		if (!HasQualifiedCustomAddresses()) {
			return nullptr;
		}
		static REL::Relocation<void* (*)(RE::GFxMovieView*, const char*, RE::FxResponseArgsBase&)> func{ REL::RelocationID(0, kID_GFxInvokeFunction) };
		return func(a_movie, a_fnName, a_arguments);
	}

	bool LookupREFRByHandle(std::uint32_t& a_handle, RE::NiPointer<RE::TESObjectREFR>& a_refr);
}

enum class SKEEHookStatus
{
	kEnabled,
	kDisabledByPolicy,
	kUnavailableAddress,
	kSignatureMismatch,
	kInitializationFailed
};

struct SKEEHookGroupResult
{
	std::string_view group;
	SKEEHookStatus status;
	std::string_view consequence;
};

struct SKEEHookInstallResult
{
	bool success;
	std::array<SKEEHookGroupResult, 9> groups;
};

SKEEHookInstallResult InstallSKEEHooks();
