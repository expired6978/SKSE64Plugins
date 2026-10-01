#pragma once

// Included by SKEEHooks.h after the shared engine types and relocation IDs.
namespace SKEE
{
	[[nodiscard]] bool HasQualifiedVRHelperAddresses() noexcept;
	bool RetainAdjustedDynamicData(void* a_data);

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
		if (REL::Module::IsVR()) {
			return VRFunction<std::int32_t (*)(RE::TESNPC*, RE::Actor*, std::uint32_t)>(0x003727B0)(a_npc, a_actor, a_unk1);
		}
		if (!HasQualifiedCustomAddresses()) {
			return 0;
		}
		static REL::Relocation<std::int32_t (*)(RE::TESNPC*, RE::Actor*, std::uint32_t)> func{ REL::RelocationID(0, kID_UpdateHeadState) };
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

	inline void UpdateModelFace(RE::NiAVObject* a_object)
	{
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::NiAVObject*)> func{ kReloc_UpdateModelFace };
		func(a_object);
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

	inline void LoadSliders(RE::RaceSexMenu* a_menu, RE::TESActorBase* a_actorOverride, std::uint8_t a_unk2)
	{
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::RaceSexMenu*, RE::TESActorBase*, std::uint8_t)> func{ kReloc_LoadSliders };
		func(a_menu, a_actorOverride, a_unk2);
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

	inline RE::BSDismemberSkinInstance* CreateBSDismemberSkinInstance()
	{
		static REL::Relocation<RE::BSDismemberSkinInstance* (*)()> func{ REL::RelocationID(0, kID_CreateBSDismemberSkinInstance) };
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
		static REL::Relocation<void (*)(void*, RE::TESForm*, RE::TESForm*, RE::NiNode**)> func{ REL::RelocationID(0, kID_SetNewInventoryItemModel) };
		func(a_unk1, a_form1, a_form2, a_node);
	}

	inline void InitializeDisplayObject(RE::Inventory3DManager* a_manager, RE::TESForm* a_form1, RE::TESForm* a_form2, RE::NiNode* a_node)
	{
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::Inventory3DManager*, RE::TESForm*, RE::TESForm*, RE::NiNode*)> func{
			kReloc_InitializeDisplayObject
		};
		func(a_manager, a_form1, a_form2, a_node);
	}

	inline void InventoryChanges_SetUniqueID(RE::InventoryChanges* a_this, RE::ExtraDataList* a_extraList, RE::TESForm* a_oldForm, RE::TESForm* a_newForm)
	{
		if (REL::Module::IsVR()) {
			VRFunction<void (*)(RE::InventoryChanges*, RE::ExtraDataList*, RE::TESForm*, RE::TESForm*)>(0x001FD7D0)(a_this, a_extraList, a_oldForm, a_newForm);
			return;
		}
		if (!HasQualifiedCustomAddresses()) {
			return;
		}
		static REL::Relocation<void (*)(RE::InventoryChanges*, RE::ExtraDataList*, RE::TESForm*, RE::TESForm*)> func{ REL::RelocationID(0, kID_InventoryChanges_SetUniqueID) };
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
