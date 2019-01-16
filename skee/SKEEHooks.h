#pragma once

class TESObjectARMO;
class TESObjectARMA;
class TESModelTextureSwap;
class BGSTextureSet;
class BSFaceGenNiNode;
class NiNode;

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

typedef NiNode * (*_CreateArmorNode)(void * unk1, void * unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5, UInt64 unk6);
extern RelocAddr<_CreateArmorNode> CreateArmorNode;

class ArmorAddonTree
{
public:
	UInt32	unk00;			// 00 refcount?
	UInt32	unk04;			// 04
	NiNode * boneTree;		// 08 FlattenedBoneTree
	TESObjectARMO * skin;	// 10
	UInt64	unk18[(0x2770 - 0x18) >> 3];	// 18
	UInt32	handle;			// 2770
	UInt32	unk2774;		// 2774
};

class AddonTreeParameters
{
public:
	UInt64	unk00;					// 00 - ?
	UInt64	unk08;					// 08 - inited to zero
	TESObjectARMO * armor;			// 10
	TESObjectARMA * addon;			// 18
	TESModelTextureSwap * model;	// 20
	BGSTextureSet * textureSet;		// 28
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

void InstallArmorAddonHook(TESObjectREFR * refr, AddonTreeParameters * params, NiNode * boneTree, NiAVObject * resultNode);
void InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate);

bool InstallSKEEHooks();