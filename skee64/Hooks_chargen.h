#pragma once

#include "skse/GameObjects.h"
#include "skse/GameMenus.h"
#include "skse/GameData.h"

#define CODE_ADDR(x) x
#define DATA_ADDR(x, off) (x + off)

void InstallHooks();

void _cdecl LoadActorValues_Hook();
void _cdecl ClearFaceGenCache_Hooked();
bool _cdecl CacheTRIFile_Hook(const char * filePath, BSFaceGenDB::TRI::DBTraits::MorphSet ** morphSet, UInt32 * unk1);

class TESNPC_Hooked : public TESNPC
{
public:
	MEMBER_FN_PREFIX(TESNPC_Hooked);
	DEFINE_MEMBER_FN(UpdateMorphs, void, CODE_ADDR(0x005640C0), void * unk1, BSFaceGenNiNode * faceNode);
	DEFINE_MEMBER_FN(UpdateMorph, void, CODE_ADDR(0x00564240), BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);

	void UpdateMorphs_Hooked(void * unk1, BSFaceGenNiNode * faceNode);
	void UpdateMorph_Hooked(BGSHeadPart * headPart, BSFaceGenNiNode * faceNode);
};

class BGSHeadPart_Hooked : public BGSHeadPart
{
public:
	MEMBER_FN_PREFIX(BGSHeadPart_Hooked);
	DEFINE_MEMBER_FN(IsPlayablePart, UInt8, CODE_ADDR(0x0054CD80));

	UInt8 IsPlayablePart_Hooked();
};

class BSFaceGenModel_Hooked : public BSFaceGenModel
{
public:
	UInt8 ApplyRaceMorph_Hooked(const char ** morphName, TESModelTri * chargenTri, NiAVObject ** headNode, float relative, UInt8 unk1);
	UInt8 ApplyChargenMorph_Hooked(const char ** morphName, TESModelTri * chargenTri, NiAVObject ** headNode, float relative, UInt8 unk1);
};

class SliderArray : public tArray<RaceMenuSlider>
{
public:
	MEMBER_FN_PREFIX(SliderArray);
	DEFINE_MEMBER_FN(AddSlider, UInt32, CODE_ADDR(0x00880A70), RaceMenuSlider * slider);

	UInt32 AddSlider_Hooked(RaceMenuSlider * slider);
};

class FxResponseArgsList_Hooked : public FxResponseArgsList
{
public:
	MEMBER_FN_PREFIX(FxResponseArgsList_Hooked);
	DEFINE_MEMBER_FN(AddArgument, void, CODE_ADDR(0x00843150), GFxValue * value);

	void AddArgument_Hooked(GFxValue * value);
};

class DataHandler_Hooked : public DataHandler
{
public:
	MEMBER_FN_PREFIX(DataHandler_Hooked);
	DEFINE_MEMBER_FN(GetValidPlayableHeadParts, void, CODE_ADDR(0x00881320), UInt32 unk1, void * unk2);

	void GetValidPlayableHeadParts_Hooked(UInt32 unk1, void * unk2);
};

class RaceSexMenu_Hooked : public RaceSexMenu
{
public:
	MEMBER_FN_PREFIX(RaceSexMenu_Hooked);
	DEFINE_MEMBER_FN(DoubleMorphCallback, void, CODE_ADDR(0x0087F6E0), float value, UInt32 sliderId);
	DEFINE_MEMBER_FN(RenderMenu, void, CODE_ADDR(0x00847000));

	DEFINE_MEMBER_FN(CloseMenu, UInt32, CODE_ADDR(0x00885980), UInt32 unk1, UInt32 unk2);
	UInt32 CloseMenu_Hooked(UInt32 unk1, UInt32 unk2);
	void RenderMenu_Hooked(void);

	void DoubleMorphCallback_Hooked(float value, UInt32 sliderId);
	void SetRelativeMorph(TESNPC * npc, BSFaceGenNiNode * faceNode, BSFixedString name, float relative);
	//void SetRelativeMorph(TESNPC * npc, BSFaceGenNiNode * faceNode, UInt32 sliderIndex, UInt32 sliderType, std::string sliderName, float relative);
};