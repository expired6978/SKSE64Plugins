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

typedef NiNode * (*_CreateArmorNode)(void * unk1, void * unk2, UInt64 unk3, UInt64 unk4, UInt64 unk5, UInt64 unk6, UInt64 unk7);
extern RelocAddr<_CreateArmorNode> CreateArmorNode;

class BipedParam
{
public:
	UInt64		unk00;					// 00 - ?
	UInt64		unk08;					// 08 - inited to zero
	Biped::Data	data;					// 10
};

void InstallArmorAddonHook(TESObjectREFR * refr, BipedParam * params, NiNode * boneTree, NiAVObject * resultNode);
void InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate);

bool InstallSKEEHooks();