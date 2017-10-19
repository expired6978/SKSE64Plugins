#pragma once

class TESObjectARMO;
class TESObjectARMA;
class TESModelTextureSwap;
class BGSTextureSet;
class BSFaceGenNiNode;

#include "common/IErrors.h"
#include "skse64/NiProperties.h"
#include "skse64/GameTypes.h"
#include "skse64/GameData.h"
#include "skse64/GameExtraData.h"
#include "skse64/GameMenus.h"

extern RelocAddr<uintptr_t> TESModelTri_vtbl;
typedef SInt32 (*_AddRaceMenuSlider)(tArray<RaceMenuSlider> * sliders, RaceMenuSlider * slider);
extern RelocAddr<_AddRaceMenuSlider> AddRaceMenuSlider;

typedef void (*_DoubleMorphCallback)(RaceSexMenu * menu, float value, UInt32 sliderId);
extern RelocAddr<_DoubleMorphCallback> DoubleMorphCallback;

typedef void(*_FaceGenApplyMorph)(FaceGen * faceGen, BSFaceGenNiNode * node, TESNPC * npc, BSFixedString * morph, float relative);
extern RelocAddr<_FaceGenApplyMorph> FaceGenApplyMorph;

typedef void(*_AddGFXArgument)(GArray<GFxValue> * arr, GArray<GFxValue> * arr2, UInt64 idx);
extern RelocAddr<_AddGFXArgument> AddGFXArgument;

typedef void(*_UpdateNPCMorphs)(TESNPC * npc, void * unk1, BSFaceGenNiNode * faceNode);
extern RelocAddr<_UpdateNPCMorphs> UpdateNPCMorphs;

typedef void(*_UpdateNPCMorph)(TESNPC * npc, BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);
extern RelocAddr<_UpdateNPCMorph> UpdateNPCMorph;


class AddonTreeParameters
{
public:
	UInt32	unk00;					// 00 - ?
	UInt32	unk04;					// 04 - inited to zero
	TESObjectARMO * armor;			// 08
	TESObjectARMA * addon;			// 0C
	TESModelTextureSwap * model;	// 10
	BGSTextureSet * textureSet;		// 14
};

class ArmorAddonTree
{
public:
	MEMBER_FN_PREFIX(ArmorAddonTree);
	DEFINE_MEMBER_FN(CreateArmorNode, NiAVObject *, 0x0046F0B0, NiNode * unk1, UInt32 unk2, UInt8 unk3, UInt32 unk4, UInt32 unk5);

	UInt32	unk00;			// 00 refcount?
	NiNode * unk04;			// 04 FlattenedBoneTree
	TESObjectARMO * skin;	// 08
	UInt32	unk0C[(0xA88 - 0x0C) >> 2];	// 0C
	UInt32	unkA88;			// A88

	//NiAVObject * CreateArmorNode_Hooked(NiNode * unk1, UInt32 unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5);
	NiAVObject * CreateArmorNode(AddonTreeParameters * params, NiNode * unk1, UInt32 unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5);
};

#ifdef FIXME
class SkyrimVM_Hooked : public SkyrimVM
{
public:
	MEMBER_FN_PREFIX(SkyrimVM_Hooked);
	DEFINE_MEMBER_FN(RegisterEventSinks, void, 0x008D2120);

	void RegisterEventSinks_Hooked();
};
#endif

class TESNPC_Hooked : public TESNPC
{
public:
	MEMBER_FN_PREFIX(TESNPC_Hooked);
	DEFINE_MEMBER_FN(BindHead, void, 0x00000000, Actor*, BSFaceGenNiNode**);
	//DEFINE_MEMBER_FN(GetFaceGenHead, UInt32, 0x0056AEB0, UInt32 unk1, UInt32 unk2);
	DEFINE_MEMBER_FN(UpdateHeadState, SInt32, 0x00000000, Actor *, UInt32 unk1);
	DEFINE_MEMBER_FN(UpdateMorphs, void, 0x00000000, void * unk1, BSFaceGenNiNode * faceNode);
	DEFINE_MEMBER_FN(UpdateMorph, void, 0x00000000, BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);

	void BindHead_Hooked(Actor* actor, BSFaceGenNiNode** headNode);
	//UInt32 GetFaceGenHead_Hooked(TESObjectREFR* refr, UInt32 unk1, UInt32 unk2);
	SInt32 CreateHeadState_Hooked(Actor *, UInt32 unk1);
	SInt32 UpdateHeadState_Hooked(Actor *, UInt32 unk1);

	void UpdateMorphs_Hooked(void * unk1, BSFaceGenNiNode * faceNode);
	void UpdateMorph_Hooked(BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);
};

class BGSHeadPart_Hooked : public BGSHeadPart
{
public:
	MEMBER_FN_PREFIX(BGSHeadPart_Hooked);
	DEFINE_MEMBER_FN(IsPlayablePart, UInt8, 0x00000000);

	UInt8 IsPlayablePart_Hooked();
};

class ExtraContainerChangesData_Hooked : public ExtraContainerChanges::Data
{
public:
	MEMBER_FN_PREFIX(ExtraContainerChangesData_Hooked);
	DEFINE_MEMBER_FN(TransferItemUID, void, 0x00000000, BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1);

	void TransferItemUID_Hooked(BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1);
};

class BSLightingShaderProperty_Hooked : public BSLightingShaderProperty
{
public:
	MEMBER_FN_PREFIX(BSLightingShaderProperty_Hooked);

	bool HasFlags_Hooked(UInt32 flags);
};

class DataHandler_Hooked : public DataHandler
{
public:
	MEMBER_FN_PREFIX(DataHandler_Hooked);
	DEFINE_MEMBER_FN(GetValidPlayableHeadParts, void, 0x00000000, UInt32 unk1, void * unk2);

	void GetValidPlayableHeadParts_Hooked(UInt32 unk1, void * unk2);
};

void __stdcall InstallArmorAddonHook(TESObjectREFR * refr, AddonTreeParameters * params, NiNode * boneTree, NiAVObject * resultNode);
void __stdcall InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate);

bool InstallSKEEHooks();