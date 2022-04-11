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

typedef void(*_FaceGenApplyMorph)(FaceGen* faceGen, BSFaceGenNiNode* node, TESNPC* npc, BSFixedString* morph, float relative);
extern RelocAddr<_FaceGenApplyMorph> FaceGenApplyMorph;

typedef void(*_AddGFXArgument)(GArray<GFxValue> * arr, GArray<GFxValue> * arr2, UInt64 idx);
extern RelocAddr<_AddGFXArgument> AddGFXArgument;

typedef void(*_UpdateNPCMorphs)(TESNPC * npc, void * unk1, BSFaceGenNiNode * faceNode);
extern RelocAddr<_UpdateNPCMorphs> UpdateNPCMorphs;

typedef void(*_UpdateNPCMorph)(TESNPC * npc, BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);
extern RelocAddr<_UpdateNPCMorph> UpdateNPCMorph;

typedef NiNode * (*_AttachBipedObject)(Biped* bipedInfo, NiNode* objectRoot, UInt32 bipedIndex, UInt32 unkIndex, UInt64 unk5, UInt64 unk6, UInt64 unk7);
extern RelocAddr<_AttachBipedObject> AttachBipedObject;

void InstallArmorAddonHook(TESObjectREFR * refr, Biped::Data& params, NiNode * boneTree, NiAVObject * resultNode);
void InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate);

bool InstallSKEEHooks();