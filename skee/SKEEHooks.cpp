#include "common/IFileStream.h"
#include "skse64_common/SafeWrite.h"
#include "skse64/PluginAPI.h"
#include "skse64/PapyrusVM.h"

#include "skse64/GameData.h"
#include "skse64/GameRTTI.h"
#include "skse64/ObScript.h"

#include "skse64/NiExtraData.h"
#include "skse64/NiNodes.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiNodes.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiProperties.h"

#include "SKEEHooks.h"
#include "PapyrusNiOverride.h"

#include "ActorUpdateManager.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "TintMaskInterface.h"
#include "ItemDataInterface.h"
#include "NiTransformInterface.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"

#include "SkeletonExtender.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"

#include <vector>

#include "skse64_common/Relocation.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/SafeWrite.h"
#include "xbyak/xbyak.h"

#include "common/ICriticalSection.h"
#include <queue>

extern SKSETaskInterface	* g_task;
extern SKSETrampolineInterface	* g_trampoline;

extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern OverrideInterface	g_overrideInterface;
extern ActorUpdateManager	g_actorUpdateManager;
extern NiTransformInterface	g_transformInterface;

extern bool					g_enableFaceOverlays;
extern bool					g_enableTintSync;
extern bool					g_enableTintInventory;
extern UInt32				g_numFaceOverlays;

extern bool					g_playerOnly;
extern UInt32				g_numSpellFaceOverlays;

extern bool					g_immediateArmor;
extern bool					g_immediateFace;

extern bool					g_enableEquippableTransforms;
extern bool					g_disableFaceGenCache;

Actor						* g_weaponHookActor = NULL;
TESObjectWEAP				* g_weaponHookWeapon = NULL;
UInt32						g_firstPerson = 0;

extern FaceMorphInterface			g_morphInterface;
extern PartSet				g_partSet;
extern UInt32				g_customDataMax;
extern bool					g_externalHeads;
extern bool					g_extendedMorphs;
extern bool					g_allowAllMorphs;
extern bool					g_allowAnyRacePart;
extern bool					g_allowAnyGenderPart;

RelocAddr<_CreateArmorNode> CreateArmorNode(0x001CAC70);

typedef void(*_RegenerateHead)(FaceGen * faceGen, BSFaceGenNiNode * headNode, BGSHeadPart * headPart, TESNPC * npc);
RelocAddr <_RegenerateHead> RegenerateHead(0x003D2A60);
_RegenerateHead RegenerateHead_Original = nullptr;

RelocPtr<bool> g_useFaceGenPreProcessedHeads(0x01DEA030);

// ??_7TESModelTri@@6B@
RelocAddr<uintptr_t> TESModelTri_vtbl(0x01596840);

// DB0F3961824CB053B91AC8B9D2FE917ACE7DD265+84
RelocAddr<_AddGFXArgument> AddGFXArgument(0x00856E10);

// 57F6EC6339F20ED6A0882786A452BA66A046BDE8+1AE
RelocAddr<_FaceGenApplyMorph> FaceGenApplyMorph(0x003D2220);
RelocAddr<_AddRaceMenuSlider> AddRaceMenuSlider(0x008BC760);
RelocAddr<_DoubleMorphCallback> DoubleMorphCallback(0x008B4820);

RelocAddr<_UpdateNPCMorphs> UpdateNPCMorphs(0x00360840);
RelocAddr<_UpdateNPCMorph> UpdateNPCMorph(0x00360A30);

typedef void(*_RaceSexMenu_Render)(RaceSexMenu * menu);
RelocAddr<_RaceSexMenu_Render> RaceSexMenu_Render(0x0051E7F0);

typedef SInt32(*_UpdateHeadState)(TESNPC * npc, Actor * actor, UInt32 unk1);
RelocAddr<_UpdateHeadState> UpdateHeadState(0x00362E90);

typedef void(*_SetInventoryItemModel)(void * unk1, void * unk2, void * unk3);
RelocAddr <_SetInventoryItemModel> SetInventoryItemModel(0x00888730);
_SetInventoryItemModel SetInventoryItemModel_Original = nullptr;

typedef void(*_SetNewInventoryItemModel)(void * unk1, TESForm * form1, TESForm * form2, NiNode ** node);
RelocAddr <_SetNewInventoryItemModel> SetNewInventoryItemModel(0x00888290);

typedef void*(*_GetHeadParts)(void * unk1, void * unk2);
RelocAddr <_GetHeadParts> GetHeadParts(0x00165660);

void __stdcall InstallWeaponHook(Actor * actor, TESObjectWEAP * weapon, NiAVObject * resultNode1, NiAVObject * resultNode2, UInt32 firstPerson)
{
	if (!actor) {
#ifdef _DEBUG
		_MESSAGE("%s - Error no reference found skipping overrides.", __FUNCTION__);
#endif
		return;
	}
	if (!weapon) {
#ifdef _DEBUG
		_MESSAGE("%s - Error no weapon found skipping overrides.", __FUNCTION__);
#endif
		return;
	}

	std::vector<TESObjectWEAP*> flattenedWeapons;
	flattenedWeapons.push_back(weapon);
	TESObjectWEAP * templateWeapon = weapon->templateForm;
	while (templateWeapon) {
		flattenedWeapons.push_back(templateWeapon);
		templateWeapon = templateWeapon->templateForm;
	}

	// Apply top-most parent properties first
	for (std::vector<TESObjectWEAP*>::reverse_iterator it = flattenedWeapons.rbegin(); it != flattenedWeapons.rend(); ++it)
	{
		if (resultNode1)
			g_overrideInterface.ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode1, true);
		if (resultNode2)
			g_overrideInterface.ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode2, true);
	}
}

#ifdef FIXME
typedef NiAVObject * (*_CreateWeaponNode)(UInt32 * unk1, UInt32 unk2, Actor * actor, UInt32 ** unk4, UInt32 * unk5);
extern const _CreateWeaponNode CreateWeaponNode = (_CreateWeaponNode)0x0046F530;

// Store stack values here, they would otherwise be lost
enum
{
	kWeaponHook_EntryStackOffset1 = 0x40,
	kWeaponHook_EntryStackOffset2 = 0x20,
	kWeaponHook_VarObj = 0x04
};

static const UInt32 kInstallWeaponFPHook_Base = 0x0046F870 + 0x143;
static const UInt32 kInstallWeaponFPHook_Entry_retn = kInstallWeaponFPHook_Base + 0x5;

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

static const UInt32 kInstallWeapon3PHook_Base = 0x0046F870 + 0x17E;
static const UInt32 kInstallWeapon3PHook_Entry_retn = kInstallWeapon3PHook_Base + 0x5;

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
static const UInt32 kInstallWeaponHook_Base = 0x0046F530 + 0x28A;
static const UInt32 kInstallWeaponHook_Entry_retn = kInstallWeaponHook_Base + 0x5;

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

UInt8 * g_unk1 = (UInt8*)0x001240DE8;
UInt32 * g_unk2 = (UInt32*)0x001310588;

#endif

NiAVObject * CreateArmorNode_Hooked(Biped * bipedInfo, NiNode * objectRoot, UInt64 unk3_4, BipedParam * params, UInt8 unk5, UInt8 unk6, UInt64 unk7)
{
	NiAVObject * retVal = CreateArmorNode(bipedInfo, objectRoot, unk3_4 >> 32, unk3_4 & 0xFFFFFFFF, unk5, unk6, unk7);

	NiPointer<TESObjectREFR> reference;
	UInt32 handle = bipedInfo->handle;
	LookupREFRByHandle(handle, reference);
	if (reference)
		InstallArmorAddonHook(reference, params, bipedInfo->root, retVal);

	return retVal;
}

void InstallArmorAddonHook(TESObjectREFR * refr, BipedParam * params, NiNode * boneTree, NiAVObject * resultNode)
{
	if (!refr) {
#ifdef _DEBUG
		_ERROR("%s - Error no reference found skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!params) {
#ifdef _DEBUG
		_ERROR("%s - Error no armor parameters found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!params->data.armor || !params->data.addon) {
#ifdef _DEBUG
		_ERROR("%s - Armor or ArmorAddon found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!boneTree) {
#ifdef _DEBUG
		_ERROR("%s - Error no bone tree found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!resultNode) {
#ifdef _DEBUG
		UInt32 addonFormid = params->data.addon ? params->data.addon->formID : 0;
		UInt32 armorFormid = params->data.armor ? params->data.armor->formID : 0;
		_ERROR("%s - Error no node found on Reference (%08X) while attaching ArmorAddon (%08X) of Armor (%08X)", __FUNCTION__, refr->formID, addonFormid, armorFormid);
#endif
		return;
	}

	NiNode * node3P = refr->GetNiRootNode(0);
	NiNode * node1P = refr->GetNiRootNode(1);

	// Go up to the root and see which one it is
	NiNode * rootNode = nullptr;
	NiNode * parent = boneTree->m_parent;
	do
	{
		if (parent == node1P)
			rootNode = node1P;
		if (parent == node3P)
			rootNode = node3P;
		parent = parent->m_parent;
	} while (parent);

	bool isFirstPerson = (rootNode == node1P);
	if (node1P == node3P) { // Theres only one node, theyre the same, no 1st person
		isFirstPerson = false;
	}

	if (rootNode != node1P && rootNode != node3P) {
#ifdef _DEBUG
		_DMESSAGE("%s - Mismatching root nodes, bone tree not for this reference (%08X)", __FUNCTION__, refr->formID);
#endif
		return;
	}
	if (params->data.armor->formType == TESObjectARMO::kTypeID && params->data.addon->formType == TESObjectARMA::kTypeID)
	{
		NiPointer<NiAVObject> node = resultNode;
		g_actorUpdateManager.OnAttach(refr, static_cast<TESObjectARMO*>(params->data.armor), static_cast<TESObjectARMA*>(params->data.addon), node, isFirstPerson, isFirstPerson ? node1P : node3P, boneTree);
	}

	
}

void __stdcall InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate)
{
	if (!refr) {
#ifdef _DEBUG
		_DMESSAGE("%s - Warning no reference found skipping overlay", __FUNCTION__);
#endif
		return;
	}

	if (!refr->GetFaceGenNiNode()) {
#ifdef _DEBUG
		_DMESSAGE("%s - Warning no head node for %08X skipping overlay", __FUNCTION__, refr->formID);
#endif
		return;
	}

#ifdef _DEBUG
	_DMESSAGE("%s - Attempting to install face overlay to %08X - Flags %08X", __FUNCTION__, refr->formID, refr->GetFaceGenNiNode()->m_flags);
#endif

	if ((refr == (*g_thePlayer) && g_playerOnly) || !g_playerOnly || g_overlayInterface.HasOverlays(refr))
	{
		char buff[MAX_PATH];
		// Face
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, buff);
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					g_task->AddTask(task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, buff, FACE_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				g_task->AddTask(task);
			}
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, buff);
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					g_task->AddTask(task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, buff, FACE_MAGIC_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				g_task->AddTask(task);
			}
		}
	}
}

SInt32 UpdateHeadState_Enable_Hooked(TESNPC * npc, Actor * actor, UInt32 unk1)
{
	SInt32 ret = UpdateHeadState(npc, actor, unk1);
	InstallFaceOverlayHook(actor, true, g_immediateFace);
	return ret;
}

SInt32 UpdateHeadState_Disabled_Hooked(TESNPC * npc, Actor * actor, UInt32 unk1)
{
	SInt32 ret = UpdateHeadState(npc, actor, unk1);
	InstallFaceOverlayHook(actor, false, g_immediateFace);
	return ret;
}

#include <d3d11_4.h>
#include "CDXD3DDevice.h"
#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXCamera.h"
#include "Utilities.h"

#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"

extern CDXD3DDevice					* g_Device;
extern CDXNifScene					g_World;
extern CDXModelViewerCamera			g_Camera;

void RaceSexMenu_Render_Hooked(RaceSexMenu * rsm)
{
	if (g_Device && g_World.IsVisible() && g_World.GetRenderTargetView()) {
		ScopedCriticalSection cs(&g_renderManager->lock);
		g_World.Begin(&g_Camera, g_Device);
		g_World.Render(&g_Camera, g_Device);
		g_World.End(&g_Camera, g_Device);
	}

	RaceSexMenu_Render(rsm);
}

void RegenerateHead_Hooked(FaceGen * faceGen, BSFaceGenNiNode * headNode, BGSHeadPart * headPart, TESNPC * npc)
{
	RegenerateHead_Original(faceGen, headNode, headPart, npc);
	g_morphInterface.ApplyPreset(npc, headNode, headPart);
}

bool UsePreprocessedHead(TESNPC * npc)
{
	// For some reason the NPC vanilla preset data is reset when the actor is disable/enabled
	auto presetData = g_morphInterface.GetPreset(npc);
	if (presetData) {
		if (!npc->faceMorph)
			npc->faceMorph = (TESNPC::FaceMorphs*)Heap_Allocate(sizeof(TESNPC::FaceMorphs));

		UInt32 i = 0;
		for (auto & preset : presetData->presets) {
			npc->faceMorph->presets[i] = preset;
			i++;
		}

		i = 0;
		for (auto & morph : presetData->morphs) {
			npc->faceMorph->option[i] = morph;
			i++;
		}
	}
	return presetData == nullptr && (*g_useFaceGenPreProcessedHeads);
}

void _cdecl ClearFaceGenCache_Hooked()
{
	g_morphInterface.RevertInternals();
	g_partSet.Revert(); // Cleanup HeadPart List before loading new ones
}

void UpdateMorphs_Hooked(TESNPC * npc, void * unk1, BSFaceGenNiNode * faceNode)
{
	UpdateNPCMorphs(npc, unk1, faceNode);
#ifdef _DEBUG_HOOK
	_DMESSAGE("UpdateMorphs_Hooked - Applying custom morphs");
#endif
	try
	{
		g_morphInterface.ApplyMorphs(npc, faceNode);
	}
	catch (...)
	{
		_DMESSAGE("%s - Fatal error", __FUNCTION__);
	}
}

void UpdateMorph_Hooked(TESNPC * npc, BGSHeadPart * headPart, BSFaceGenNiNode * faceNode)
{
	UpdateNPCMorph(npc, headPart, faceNode);
#ifdef _DEBUG_HOOK
	_DMESSAGE("UpdateMorph_Hooked - Applying single custom morph");
#endif
	try
	{
		g_morphInterface.ApplyMorph(npc, headPart, faceNode);
	}
	catch (...)
	{
		_DMESSAGE("%s - Fatal error", __FUNCTION__);
	}
}

#ifdef _DEBUG_HOOK
class DumpPartVisitor : public PartSet::Visitor
{
public:
	bool Accept(UInt32 key, BGSHeadPart * headPart)
	{
		_DMESSAGE("DumpPartVisitor - Key: %d Part: %s", key, headPart->partName.data);
		return false;
	}
};
#endif

void * GetHeadParts_Hooked(void * unk1, void * unk2)
{
	class RaceVisitor : public BGSListForm::Visitor
	{
	public:
		explicit RaceVisitor(TESRace * race) : m_race(race) { }
		virtual bool Accept(TESForm * form)
		{
			return form == m_race;
		};

	protected:
		TESRace * m_race;
	};

	void * ret = GetHeadParts(unk1, unk2);
	g_partSet.Revert();

	TESNPC * npc = DYNAMIC_CAST((*g_thePlayer)->baseForm, TESForm, TESNPC);
	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();
	bool isFemale = gender == 1;

	for (SInt32 i = 0; i < (*g_dataHandler)->headParts.count; ++i)
	{
		BGSHeadPart* headPart = nullptr;
		(*g_dataHandler)->headParts.GetNthItem(i, headPart);

		bool isPlayable = headPart->partFlags & BGSHeadPart::kFlagPlayable;
		bool isValidForRace = g_allowAnyRacePart || (headPart->validRaces ? headPart->validRaces->Visit(RaceVisitor((*g_thePlayer)->race)) : false);
		bool isValidForGender = g_allowAnyGenderPart || (isFemale ? (headPart->partFlags & BGSHeadPart::kFlagFemale) == BGSHeadPart::kFlagFemale : (headPart->partFlags & BGSHeadPart::kFlagMale) == BGSHeadPart::kFlagMale);

		if (isPlayable && isValidForRace && isValidForGender)
		{
			if (headPart->type >= BGSHeadPart::kNumTypes) {
				if ((headPart->partFlags & BGSHeadPart::kFlagExtraPart) == 0) { // Skip Extra Parts
					if (strcmp(headPart->model.GetModelName(), "") == 0)
						g_partSet.SetDefaultPart(headPart->type, headPart);
					else
						g_partSet.AddPart(headPart->type, headPart);
				}
			}
			else if ((headPart->partFlags & BGSHeadPart::kFlagExtraPart) == 0 && isPlayable)
			{
				// maps the pre-existing part to this type
				g_partSet.AddPart(headPart->type, headPart);

				if (g_partSet.GetDefaultPart(headPart->type) == nullptr) {
					auto playerRace = (*g_thePlayer)->race;
					if (playerRace) {
						auto chargenData = playerRace->chargenData[gender];
						if (chargenData) {
							auto headParts = chargenData->headParts;
							if (headParts) {
								for (UInt32 i = 0; i < headParts->count; i++) {
									BGSHeadPart * part;
									headParts->GetNthItem(i, part);
									if (part->type == headPart->type)
										g_partSet.SetDefaultPart(part->type, part);
								}
							}
						}
					}
				}
			}
		}
	}

	return ret;
}

class MorphVisitor : public MorphMap::Visitor
{
public:
	MorphVisitor::MorphVisitor(BSFaceGenModel * model, SKEEFixedString morphName, NiAVObject ** headNode, float relative, UInt8 unk1)
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
			BSGeometry * geometry = NULL;
			if (m_headNode && (*m_headNode))
				geometry = (*m_headNode)->GetAsBSGeometry();

			if (geometry)
				morphData.triFile->Apply(geometry, m_morphName, m_relative);
		}

		return false;
	}
private:
	BSFaceGenModel	* m_model;
	SKEEFixedString	m_morphName;
	NiAVObject		** m_headNode;
	float			m_relative;
	UInt8			m_unk1;
};

UInt8 ApplyRaceMorph_Hooked(BSFaceGenModel * model, BSFixedString * morphName, TESModelTri * modelMorph, NiAVObject ** headNode, float relative, UInt8 unk1)
{
	//BGSHeadPart * headPart = (BGSHeadPart *)((UInt32)modelMorph - offsetof(BGSHeadPart, raceMorph));
	UInt8 ret = CALL_MEMBER_FN(model, ApplyMorph)(morphName, modelMorph, headNode, relative, unk1);
#ifdef _DEBUG
	//_MESSAGE("%08X - Applying %s from %s : %s", this, morphName[0], modelMorph->name.data, headPart->partName.data);
#endif

	try
	{
		MorphVisitor morphVisitor(model, *morphName, headNode, relative, unk1);
		g_morphInterface.VisitMorphMap(modelMorph->GetModelName(), morphVisitor);
	}
	catch (...)
	{
		_ERROR("%s - fatal error while applying morph (%s)", __FUNCTION__, *morphName);
	}

	return ret;
}

UInt8 ApplyChargenMorph_Hooked(BSFaceGenModel * model, BSFixedString * morphName, TESModelTri * modelMorph, NiAVObject ** headNode, float relative, UInt8 unk1)
{
#ifdef _DEBUG
	//_MESSAGE("%08X - Applying %s from %s : %s - %f", this, morphName[0], modelMorph->name.data, headPart->partName.data, relative);
#endif

	UInt8 ret = CALL_MEMBER_FN(model, ApplyMorph)(morphName, modelMorph, headNode, relative, unk1);

	try
	{
		MorphVisitor morphVisitor(model, *morphName, headNode, relative, unk1);
		g_morphInterface.VisitMorphMap(modelMorph->GetModelName(), morphVisitor);
	}
	catch (...)
	{
		_ERROR("%s - fatal error while applying morph (%s)", __FUNCTION__, *morphName);
	}

	return ret;
}

void SetRelativeMorph(TESNPC * npc, BSFaceGenNiNode * faceNode, BSFixedString name, float relative)
{
	float absRel = abs(relative);
	if (absRel > 1.0) {
		float max = 0.0;
		if (relative < 0.0)
			max = -1.0;
		if (relative > 0.0)
			max = 1.0;
		UInt32 count = (UInt32)absRel;
		for (UInt32 i = 0; i < count; i++) {
			g_morphInterface.SetMorph(npc, faceNode, name.data, max);
			relative -= max;
		}
	}
	g_morphInterface.SetMorph(npc, faceNode, name.data, relative);
}


void InvokeCategoryList_Hook(GFxMovieView * movie, const char * fnName, FxResponseArgsList * arguments)
{
	UInt64 idx = arguments->args.size;
	AddGFXArgument(&arguments->args, &arguments->args, idx + 1); // 17 elements
	memset(&arguments->args.values[idx], 0, sizeof(GFxValue));
	arguments->args.values[idx].SetString("$EXTRA"); idx++;
	AddGFXArgument(&arguments->args, &arguments->args, idx + 1);
	memset(&arguments->args.values[idx], 0, sizeof(GFxValue));
	arguments->args.values[idx].SetNumber(SLIDER_CATEGORY_EXTRA); idx++;
	AddGFXArgument(&arguments->args, &arguments->args, idx + 1);
	memset(&arguments->args.values[idx], 0, sizeof(GFxValue));
	arguments->args.values[idx].SetString("$EXPRESSIONS"); idx++;
	AddGFXArgument(&arguments->args, &arguments->args, idx + 1);
	memset(&arguments->args.values[idx], 0, sizeof(GFxValue));
	arguments->args.values[idx].SetNumber(SLIDER_CATEGORY_EXPRESSIONS);
	InvokeFunction(movie, fnName, arguments);
}


SInt32 AddSlider_Hook(tArray<RaceMenuSlider> * sliders, RaceMenuSlider * slider)
{
	SInt32 totalSliders = AddRaceMenuSlider(sliders, slider);
	totalSliders = g_morphInterface.LoadSliders(sliders, slider);
	return totalSliders;
}

float SliderLookup_Hooked(RaceMenuSlider * slider)
{
	return slider->value;
}

void DoubleMorphCallback_Hook(RaceSexMenu * menu, float newValue, UInt32 sliderId)
{
	RaceMenuSlider * slider = NULL;
	RaceSexMenu::RaceComponent * raceData = NULL;

	UInt8 gender = 0;
	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();
	BSFaceGenNiNode * faceNode = player->GetFaceGenNiNode();

	if (menu->raceIndex < menu->sliderData[gender].count)
		raceData = &menu->sliderData[gender][menu->raceIndex];
	if (raceData && sliderId < raceData->sliders.count)
		slider = &raceData->sliders[sliderId];

	if (raceData && slider) {
#ifdef _DEBUG_HOOK
		_DMESSAGE("Name: %s Value: %f Callback: %s Index: %d", slider->name, slider->value, slider->callback, slider->index);
#endif
		if (slider->index >= SLIDER_OFFSET) {
			UInt32 sliderIndex = slider->index - SLIDER_OFFSET;
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
				_DMESSAGE("Skipping Morph %s", sliderInternal->name.data);
#endif
				return;
			}

			if (sliderInternal->type == SliderInternal::kTypePreset)
			{
				slider->value = newValue;

				char buffer[MAX_PATH];
				slider->value = newValue;
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.c_str(), (UInt32)currentValue);
				g_morphInterface.SetMorph(actorBase, faceNode, buffer, -1.0);
				memset(buffer, 0, MAX_PATH);
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.c_str(), (UInt32)newValue);
				g_morphInterface.SetMorph(actorBase, faceNode, buffer, 1.0);

				g_morphInterface.SetMorphValue(actorBase, sliderInternal->name, newValue);
				return;
			}

			if (sliderInternal->type == SliderInternal::kTypeHeadPart)
			{
				slider->value = newValue;

				UInt8 partType = sliderInternal->presetCount;

				HeadPartList * partList = g_partSet.GetPartList(partType);
				if (partList)
				{
					if (newValue == 0.0) {
						BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(partType);
						if (oldPart) {
							BGSHeadPart * defaultPart = g_partSet.GetDefaultPart(partType);
							if (defaultPart && oldPart != defaultPart) {
								CALL_MEMBER_FN(actorBase, ChangeHeadPart)(defaultPart);
								ChangeActorHeadPart(player, oldPart, defaultPart);
							}
						}
						return;
					}
					BGSHeadPart * targetPart = g_partSet.GetPartByIndex(partList, (UInt32)newValue - 1);
					if (targetPart) {
						BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(partType);
						if (oldPart != targetPart) {
							CALL_MEMBER_FN(actorBase, ChangeHeadPart)(targetPart);
							ChangeActorHeadPart(player, oldPart, targetPart);
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
				_DMESSAGE("Undoing Upper Morph: New: %f Old: %f Relative %f Remaining %f", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

			// Cross from negative to positive
			if (newValue > 0.0 && currentValue < 0.0) {
				// Undo the lower morph
				SetRelativeMorph(actorBase, faceNode, sliderInternal->lowerBound, -abs(currentValue));
#ifdef _DEBUG_HOOK
				_DMESSAGE("Undoing Lower Morph: New: %f Old: %f Relative %f Remaining %f", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

#ifdef _DEBUG_HOOK
			_DMESSAGE("CurrentValue: %f Relative: %f SavedValue: %f", currentValue, relative, slider->value);
#endif
			slider->value = newValue;

			BSFixedString bound = sliderInternal->lowerBound;
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
			_DMESSAGE("Morphing %d - %s Relative: %f", sliderIndex, bound.data, relative);
#endif

			SetRelativeMorph(actorBase, faceNode, bound, relative);
			g_morphInterface.SetMorphValue(actorBase, sliderInternal->name, newValue);
			return;
		}
	}

	DoubleMorphCallback(menu, newValue, sliderId);
}

// This tracking container is because I only verified two locations of allocation
// in the case somehow it is allocated elsewhere without the hook the destructor wont
// crash the game and instead free the original pointer
ICriticalSection g_cs;
std::unordered_set<void*> g_adjustedBlocks;

void * NiAllocate_Hooked(size_t size)
{
	IScopedCriticalSection scs(&g_cs);
	void* ptr = NiAllocate(size + 0x10);
	*((uintptr_t*)ptr) = 1;
	*((uintptr_t*)ptr+1) = 0;
	void* adjusted = reinterpret_cast<void*>((uintptr_t)ptr + 0x10);
	g_adjustedBlocks.emplace(adjusted);
	return adjusted;
}

void NiFree_Hooked(void* ptr)
{
	IScopedCriticalSection scs(&g_cs);
	auto it = g_adjustedBlocks.find(ptr);
	if (it != g_adjustedBlocks.end())
	{
		ptr = reinterpret_cast<void*>((uintptr_t)ptr - 0x10);
		if (InterlockedDecrement((uintptr_t*)ptr) == 0)
		{
			g_adjustedBlocks.erase(it);
			NiFree(ptr);
		}
	}
	else
	{
		NiFree(ptr);
	}
}

void UpdateModelColor_Recursive(NiAVObject * object, NiColor *& color, UInt32 shaderType)
{
	BSGeometry* geometry = object->GetAsBSGeometry();
	if (geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
		{
			BSLightingShaderMaterial * material = static_cast<BSLightingShaderMaterial*>(shaderProperty->material);
			if (material && material->GetShaderType() == shaderType)
			{
				NiExtraData* extraData = shaderProperty->GetExtraData("NO_TINT");
				if (extraData) {
					NiBooleanExtraData* booleanData = static_cast<NiBooleanExtraData*>(extraData);
					if (booleanData->m_data)
					{
						return;
					}
				}
				if (material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
				{
					BSLightingShaderMaterialFacegenTint* tintMaterial = (BSLightingShaderMaterialFacegenTint *)shaderProperty->material;
					tintMaterial->tintColor.r = color->r;
					tintMaterial->tintColor.g = color->g;
					tintMaterial->tintColor.b = color->b;
				}
				else if (material->GetShaderType() == BSLightingShaderMaterial::kShaderType_HairTint)
				{
					BSLightingShaderMaterialHairTint* tintMaterial = (BSLightingShaderMaterialHairTint *)shaderProperty->material;
					tintMaterial->tintColor.r = color->r;
					tintMaterial->tintColor.g = color->g;
					tintMaterial->tintColor.b = color->b;
				}
			}
		}
	}
	else
	{
		NiNode * node = object->GetAsNiNode();
		if (node)
		{
			for (UInt32 i = 0; i < node->m_children.m_emptyRunStart; i++)
			{
				NiAVObject * object = node->m_children.m_data[i];
				if (object) {
					UpdateModelColor_Recursive(object, color, shaderType);
				}
			}
		}
	}
}

void UpdateModelSkin_Hooked(NiAVObject * object, NiColor *& color)
{
	NiAVObjectPtr rootNode = GetRootNode(object, true);
	if (rootNode && rootNode->m_owner && rootNode->m_owner->formType == Actor::kTypeID)
	{
		UInt32 mask = 1;
		for (UInt32 i = 0; i < 32; ++i)
		{
			ModifiedItemIdentifier identifier;
			identifier.SetSlotMask(mask);
			g_task->AddTask(new NIOVTaskUpdateItemDye(static_cast<Actor*>(rootNode->m_owner), identifier, TintMaskInterface::kUpdate_Skin, true));
			mask <<= 1;
		}
	}

	UpdateModelColor_Recursive(object, color, BSLightingShaderMaterial::kShaderType_FaceGenRGBTint);
}

void UpdateModelHair_Hooked(NiAVObject * object, NiColor *& color)
{
	NiAVObjectPtr rootNode = GetRootNode(object, true);
	if (rootNode && rootNode->m_owner && rootNode->m_owner->formType == Actor::kTypeID)
	{
		UInt32 mask = 1;
		for (UInt32 i = 0; i < 32; ++i)
		{
			ModifiedItemIdentifier identifier;
			identifier.SetSlotMask(mask);
			g_task->AddTask(new NIOVTaskUpdateItemDye(static_cast<Actor*>(rootNode->m_owner), identifier, TintMaskInterface::kUpdate_Hair, true));
			mask <<= 1;
		}
	}

	UpdateModelColor_Recursive(object, color, BSLightingShaderMaterial::kShaderType_HairTint);
}

void SetInventoryItemModel_Hooked(Inventory3DManager * inventoryManager, TESForm * baseForm, BaseExtraList * baseExtraList)
{
	SetInventoryItemModel_Original(inventoryManager, baseForm, baseExtraList);

	if (baseForm && baseForm->formType == TESObjectARMO::kTypeID) {
		TESObjectARMO* armor = DYNAMIC_CAST(baseForm, TESForm, TESObjectARMO);
		if (armor) {
			UInt32 rankId = 0; // Rank 0 will reset if applicable
			if (baseExtraList) {
				BSReadLocker locker(&baseExtraList->m_lock);
				auto rankData = static_cast<ExtraRank*>(baseExtraList->GetByType(kExtraData_Rank));
				if (rankData) {
					rankId = rankData->rank;
				}
			}

			NiNode * rootNode = nullptr;
			for (UInt32 i = 0; i < inventoryManager->meshCount; ++i)
			{
				if (inventoryManager->itemData[i].form1 == baseForm)
				{
					rootNode = inventoryManager->itemData[i].node;
					break;
				}
			}

			if (rootNode) {
				g_itemDataInterface.UpdateInventoryItemDye(rankId, armor, rootNode);
			}
		}
	}
}

void SetNewInventoryItemModel_Hooked(Inventory3DManager * inventoryManager, TESForm * form1, TESForm * form2, NiNode ** node)
{
	SetNewInventoryItemModel(inventoryManager, form1, form2, node);

	if (form1 && form1->formID == TESObjectARMO::kTypeID && *node) {
		TESObjectARMO* armor = DYNAMIC_CAST(form1, TESForm, TESObjectARMO);
		if (armor) {
			BaseExtraList& baseExtraList = inventoryManager->baseExtraList;
			BSReadLocker locker(&baseExtraList.m_lock);

			UInt32 rankId = 0; // Rank 0 will reset if applicable
			auto rankData = static_cast<ExtraRank*>(baseExtraList.GetByType(kExtraData_Rank));
			if (rankData) {
				rankId = rankData->rank;
			}

			g_itemDataInterface.UpdateInventoryItemDye(rankId, armor, *node);
		}
	}
}

bool SKEE_Execute(const ObScriptParam * paramInfo, ScriptData * scriptData, TESObjectREFR * thisObj, TESObjectREFR* containingObj, Script* scriptObj, ScriptLocals* locals, double& result, UInt32& opcodeOffsetPtr)
{
	char buffer[MAX_PATH];
	memset(buffer, 0, MAX_PATH);
	char buffer2[MAX_PATH];
	memset(buffer2, 0, MAX_PATH);

	if (!ObjScript_ExtractArgs(paramInfo, scriptData, opcodeOffsetPtr, thisObj, containingObj, scriptObj, locals, buffer, buffer2))
	{
		return false;
	}

	if (_strnicmp(buffer, "reload", MAX_PATH) == 0)
	{
		if (_strnicmp(buffer2, "tints", MAX_PATH) == 0)
		{
			g_tintMaskInterface.LoadMods();
			Console_Print("Tint XMLs reloaded");
		}
	}
	else if (_strnicmp(buffer, "erase", MAX_PATH) == 0)
	{
		if (_strnicmp(buffer2, "bodymorph", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing BodyMorphs requires a console target");
				return false;
			}

			g_bodyMorphInterface.ClearMorphs(thisObj);
			g_bodyMorphInterface.UpdateModelWeight(thisObj);
			Console_Print("Erased all bodymorphs");
		}
		else if (_strnicmp(buffer2, "transforms", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing transforms requires a console target");
				return false;
			}

			g_transformInterface.RemoveAllReferenceTransforms(thisObj);
			g_transformInterface.UpdateNodeAllTransforms(thisObj);
			Console_Print("Erased all transforms");
		}
		else if (_strnicmp(buffer2, "sculpt", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing sculpt requires a console target");
				return false;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return false;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC * npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return false;
			}

			g_morphInterface.EraseSculptData(npc);
			g_task->AddTask(new SKSEUpdateFaceModel(actor));

			Console_Print("Erased all sculpting");
		}
		else if (_strnicmp(buffer2, "overlays", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing overlays requires a console target");
				return false;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return false;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC * npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return false;
			}

			g_overlayInterface.EraseOverlays(actor);

			Console_Print("Erased and reverted all overlays");
		}
	}
	else if (_strnicmp(buffer, "preset-save", MAX_PATH) == 0)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", buffer2);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Data\\Textures\\CharGen\\Exported\\");

		g_morphInterface.SaveJsonPreset(slotPath);

		g_task->AddTask(new SKSETaskExportTintMask(tintPath, buffer2));
		Console_Print("Preset saved");
	}
	else if (_strnicmp(buffer, "preset-load", MAX_PATH) == 0)
	{
		if (!thisObj) {
			Console_Print("Applying a preset requires a console target");
			return false;
		}
		if (thisObj->formType != Character::kTypeID) {
			Console_Print("Console target must be an actor");
			return false;
		}
		Actor* actor = static_cast<Actor*>(thisObj);
		TESNPC * npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
		if (!npc) {
			Console_Print("Failed to acquire ActorBase for specified reference");
			return false;
		}

		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", buffer2);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Textures\\CharGen\\Exported\\%s.dds", buffer2);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_morphInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			Console_Print("Failed to load preset at %s", slotPath);
			return false;
		}

		presetData->tintTexture = tintPath;
		g_morphInterface.AssignPreset(npc, presetData);
		g_morphInterface.ApplyPresetData(actor, presetData, true, FaceMorphInterface::ApplyTypes::kPresetApplyAll);

		// Queue a node update
		CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
		Console_Print("Preset loaded");
	}

	else if (_strnicmp(buffer, "dump", MAX_PATH) == 0)
	{
		if (_strnicmp(buffer2, "bodymorph", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping transforms requires a console target");
				return false;
			}

			Console_Print("Dumping body morphs for %08X", thisObj->formID);

			class Visitor : public IBodyMorphInterface::MorphValueVisitor
			{
			public:
				Visitor() { }

				virtual void Visit(TESObjectREFR * ref, const char* morphKey, const char* key, float value)
				{
					m_mapping[key][morphKey] = value;
				}
				std::map<SKEEFixedString, std::map<SKEEFixedString, float>> m_mapping;
			};
			Visitor visitor;
			g_bodyMorphInterface.VisitMorphValues(thisObj, visitor);
				
			UInt32 totalMorphs = 0;
			for (auto & key : visitor.m_mapping)
			{
				Console_Print("Key: %s", key.first.c_str());
				for (auto & morph : key.second)
				{
					Console_Print("\tMorph: %s\t\tValue: %f", morph.first.c_str(), morph.second);
				}
				Console_Print("Dumped %d morphs for key %s", key.second.size(), key.first.c_str());
				totalMorphs += key.second.size();
			}
			Console_Print("Dumped %d total morphs", totalMorphs);

		}
		else if (_strnicmp(buffer2, "transforms", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping transforms requires a console target");
				return false;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return false;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC * npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return false;
			}

			Console_Print("Dumping transforms for %08X", thisObj->formID);

			UInt32 totalTransforms = 0;
			g_transformInterface.VisitNodes(thisObj, false, CALL_MEMBER_FN(npc, GetSex)() == 1, [&](const SKEEFixedString& node, OverrideRegistration<StringTableItem> * keys)
			{
				Console_Print("Node: %s", node.c_str());
				for (auto& item : *keys)
				{
					Console_Print("\tKey: %s\t\tProperties %d", item.first ? item.first->c_str() : "", item.second.size());
					totalTransforms++;
				}
				return false;
			});
			Console_Print("Dumped %d total transforms", totalTransforms);
		}
		else if (_strnicmp(buffer2, "tints", MAX_PATH) == 0)
		{
			UInt32 mask = 1;
			for (UInt32 i = 0; i < 32; ++i)
			{
				ModifiedItemIdentifier identifier;
				identifier.SetSlotMask(mask);
				g_task->AddTask(new NIOVTaskUpdateItemDye((*g_thePlayer), identifier, TintMaskInterface::kUpdate_All, true, [mask](TESObjectARMO* armo, TESObjectARMA* arma, const char* path, NiTexturePtr texture, LayerTarget& layer)
				{
					char texturePath[MAX_PATH];
					_snprintf_s(texturePath, MAX_PATH, "Data\\SKSE\\Plugins\\NiOverride\\Exported\\TintMasks\\%s", path);

					IFileStream::MakeAllDirs(texturePath);

					SaveRenderedDDS(texture, texturePath);

					Console_Print("Dumped result for slot %08X at %s on shape", mask, texturePath, layer.object->m_name);
				}));
				mask <<= 1;
			}
		}
#if _DEBUG
		else if (_strnicmp(buffer2, "nodes", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping nodes requires a reference");
				return false;
			}

			if (_strnicmp(buffer2, "fp", MAX_PATH) == 0)
			{
				DumpNodeChildren(thisObj->GetNiRootNode(1));
			}
			else
			{
				DumpNodeChildren(thisObj->GetNiRootNode(0));
			}

			Console_Print("Dumped actor");
		}
		
#endif
	}
	
	return true;
}



bool InstallSKEEHooks()
{
	BranchTrampoline& branchTrampoline = g_trampoline ? g_trampoline->GetBranchTrampoline() : g_branchTrampoline;
	BranchTrampoline& localTrampoline = g_trampoline ? g_trampoline->GetLocalTrampoline() : g_localTrampoline;

	if (!branchTrampoline.IsValid())
	{
		if (!branchTrampoline.Create(1024))
		{
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}
	}

	if (!localTrampoline.IsValid())
	{
		if (!localTrampoline.Create(1024, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}
	}

	if (!branchTrampoline.IsValid() || !localTrampoline.IsValid())
	{
		_ERROR("Failed to initialize branch trampolines");
		return false;
	}

	static const uintptr_t LoadRaceMenuSliders = 0x008B5E20;

	RelocAddr <uintptr_t> GetHeadParts_Target(LoadRaceMenuSliders + 0x212);
	branchTrampoline.Write5Call(GetHeadParts_Target.GetUIntPtr(), (uintptr_t)GetHeadParts_Hooked);
	
	RelocAddr <uintptr_t> InvokeCategoriesList_Target(0x008B5230 + 0x9FB);
	branchTrampoline.Write5Call(InvokeCategoriesList_Target.GetUIntPtr(), (uintptr_t)InvokeCategoryList_Hook);

	RelocAddr <uintptr_t> AddSlider_Target(LoadRaceMenuSliders + 0x37E4);
	branchTrampoline.Write5Call(AddSlider_Target.GetUIntPtr(), (uintptr_t)AddSlider_Hook);

	RelocAddr <uintptr_t> DoubleMorphCallback1_Target(LoadRaceMenuSliders + 0x3CD5);
	branchTrampoline.Write5Call(DoubleMorphCallback1_Target.GetUIntPtr(), (uintptr_t)DoubleMorphCallback_Hook);

	RelocAddr <uintptr_t> DoubleMorphCallback2_Target(0x008B1BC0 + 0x4F); // ChangeDoubleMorph callback
	branchTrampoline.Write5Call(DoubleMorphCallback2_Target.GetUIntPtr(), (uintptr_t)DoubleMorphCallback_Hook);
	
	RelocAddr<uintptr_t> SliderLookup_Target(LoadRaceMenuSliders + 0x3895);
	{
		struct SliderLookup_Entry_Code : Xbyak::CodeGenerator {
			SliderLookup_Entry_Code(void * buf, UInt64 funcAddr, UInt64 targetAddr) : Xbyak::CodeGenerator(4096, buf)
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

		void * codeBuf = localTrampoline.StartAlloc();
		SliderLookup_Entry_Code code(codeBuf, uintptr_t(SliderLookup_Hooked), SliderLookup_Target.GetUIntPtr());
		localTrampoline.EndAlloc(code.getCurr());

		branchTrampoline.Write5Branch(SliderLookup_Target.GetUIntPtr(), uintptr_t(code.getCode()));
	}

	if (!g_externalHeads)
	{
		static const uintptr_t PreprocessedHeads = 0x00363DE0;

		RelocAddr<uintptr_t> PreprocessedHeads1_Target(PreprocessedHeads + 0x58);
		RelocAddr<uintptr_t> PreprocessedHeads2_Target(PreprocessedHeads + 0x81);
		RelocAddr<uintptr_t> PreprocessedHeads3_Target(PreprocessedHeads + 0x67);
		{
			struct UsePreprocessedHeads_Entry_Code : Xbyak::CodeGenerator {
				UsePreprocessedHeads_Entry_Code(void * buf, UInt64 funcAddr, UInt64 targetAddr) : Xbyak::CodeGenerator(4096, buf)
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

			void * codeBuf = localTrampoline.StartAlloc();
			UsePreprocessedHeads_Entry_Code code1(codeBuf, uintptr_t(UsePreprocessedHead), PreprocessedHeads1_Target.GetUIntPtr());
			localTrampoline.EndAlloc(code1.getCurr());

			codeBuf = localTrampoline.StartAlloc();
			UsePreprocessedHeads_Entry_Code code2(codeBuf, uintptr_t(UsePreprocessedHead), PreprocessedHeads2_Target.GetUIntPtr());
			localTrampoline.EndAlloc(code2.getCurr());

			UInt8 resultFix[] = {
				0x90,		// NOP
				0x84, 0xC0	// TEST al, al
			};
			UInt8 testFix[] = {
				0x85, 0xDB	// TEST ebx, ebx
			};

			branchTrampoline.Write6Branch(PreprocessedHeads1_Target.GetUIntPtr(), uintptr_t(code1.getCode()));
			SafeWriteBuf(PreprocessedHeads1_Target.GetUIntPtr() + 6, resultFix, sizeof(resultFix));
			branchTrampoline.Write6Branch(PreprocessedHeads2_Target.GetUIntPtr(), uintptr_t(code2.getCode()));
			SafeWriteBuf(PreprocessedHeads2_Target.GetUIntPtr() + 6, resultFix, sizeof(resultFix));

			SafeWriteBuf(PreprocessedHeads3_Target.GetUIntPtr(), testFix, sizeof(testFix));
		}

		// Preprocessing heads, used to restore mask and tinting where applicable
		{
			struct PreprocessedHeads_Code : Xbyak::CodeGenerator {
				PreprocessedHeads_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(rax, rsp);
					push(rbp);
					push(r12);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(RegenerateHead.GetUIntPtr() + 6);
				}
			};

			void * codeBuf = localTrampoline.StartAlloc();
			PreprocessedHeads_Code code(codeBuf);
			localTrampoline.EndAlloc(code.getCurr());

			RegenerateHead_Original = (_RegenerateHead)codeBuf;

			branchTrampoline.Write6Branch(RegenerateHead.GetUIntPtr(), (uintptr_t)RegenerateHead_Hooked);
		}
	}


	if (g_extendedMorphs)
	{
		RelocAddr <uintptr_t> ApplyChargenMorph_Target(0x003D2380 + 0xF3);
		branchTrampoline.Write5Call(ApplyChargenMorph_Target.GetUIntPtr(), (uintptr_t)ApplyChargenMorph_Hooked);

		RelocAddr <uintptr_t> ApplyRaceMorph_Target(0x003D45B0 + 0x56);
		branchTrampoline.Write5Call(ApplyRaceMorph_Target.GetUIntPtr(), (uintptr_t)ApplyRaceMorph_Hooked);
	}

	RelocAddr <uintptr_t> UpdateMorphs_Target(0x003D24F0 + 0xC7);
	branchTrampoline.Write5Call(UpdateMorphs_Target.GetUIntPtr(), (uintptr_t)UpdateMorphs_Hooked);

	RelocAddr <uintptr_t> UpdateMorph_Target(0x003DC1C0 + 0x79);
	branchTrampoline.Write5Call(UpdateMorph_Target.GetUIntPtr(), (uintptr_t)UpdateMorph_Hooked);

	// Hooking Dynamic Geometry Alloc/Free to add intrusive refcount
	// This hook is very sad but BSDynamicTriShape render data has no refcount so we need implement it
	if(g_enableFaceOverlays)
	{
		RelocAddr <uintptr_t> NiAllocate_Geom_Target(0x00C71F00 + 0x92);
		branchTrampoline.Write5Call(NiAllocate_Geom_Target.GetUIntPtr(), (uintptr_t)NiAllocate_Hooked);

		RelocAddr <uintptr_t> NiFree_Geom_Target(0x00C71F00 + 0x8B);
		branchTrampoline.Write5Call(NiFree_Geom_Target.GetUIntPtr(), (uintptr_t)NiFree_Hooked);

		RelocAddr <uintptr_t> NiAllocate_Geom2_Target(0x00C72250 + 0x76);
		branchTrampoline.Write5Call(NiAllocate_Geom2_Target.GetUIntPtr(), (uintptr_t)NiAllocate_Hooked);

		RelocAddr <uintptr_t> NiFree_Geom2_Target(0x00C72C60 + 0x2F);
		branchTrampoline.Write5Call(NiFree_Geom2_Target.GetUIntPtr(), (uintptr_t)NiFree_Hooked);

		RelocAddr<uintptr_t> UpdateHeadState_Target1(0x00363F20 + 0x1E0);
		branchTrampoline.Write5Call(UpdateHeadState_Target1.GetUIntPtr(), (uintptr_t)UpdateHeadState_Enable_Hooked);

		// RelocAddr<uintptr_t> UpdateHeadState_Target2(0x00364FF0 + 0x33E);
		// branchTrampoline.Write5Call(UpdateHeadState_Target2.GetUIntPtr(), (uintptr_t)UpdateHeadState_Disabled_Hooked);

		RelocAddr<uintptr_t> UpdateHeadState_Target2(0x00363000 + 0x1DF);
		branchTrampoline.Write5Call(UpdateHeadState_Target2.GetUIntPtr(), (uintptr_t)UpdateHeadState_Disabled_Hooked);
	}

	RelocAddr <uintptr_t> RaceSexMenu_Render_Target(0x016BAC68 + 0x30); // ??_7RaceSexMenu@@6B@
	SafeWrite64(RaceSexMenu_Render_Target.GetUIntPtr(), (uintptr_t)RaceSexMenu_Render_Hooked);

	if (g_disableFaceGenCache)
	{
		/*RelocAddr <uintptr_t> Cache_Target(0x008B2BE0);
		SafeWrite8(Cache_Target.GetUIntPtr(), 0xC3); // Cache immediate retn
		RelocAddr <uintptr_t> CacheClear_Target(0x008B2D60);
		SafeWrite8(CacheClear_Target.GetUIntPtr(), 0xC3); // Cache clear immediate retn*/

		RelocAddr <uintptr_t> CachePartsTarget_Target(0x008BB750);
		SafeWrite8(CachePartsTarget_Target.GetUIntPtr(), 0xC3);
	}

	RelocAddr<uintptr_t> ArmorAddon_Target(0x001C6F90 + 0xB4A);
	{
		struct ArmorAddonHook_Entry_Code : Xbyak::CodeGenerator {
			ArmorAddonHook_Entry_Code(void * buf, UInt64 funcAddr, UInt64 targetAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				mov(r8d, r15d);
				shl(r8, 0x20);
				and (r9, 0xFFFFFFFF);
				or (r8, r9);
				mov(r9, ptr[rsp + 0x78]);
				mov(rdx, rsi);
				mov(rcx, r13);
				call(ptr[rip + funcLabel]);
				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(targetAddr + 0xE);
			}
		};

		void * codeBuf = localTrampoline.StartAlloc();
		ArmorAddonHook_Entry_Code code(codeBuf, uintptr_t(CreateArmorNode_Hooked), ArmorAddon_Target.GetUIntPtr());
		localTrampoline.EndAlloc(code.getCurr());

		branchTrampoline.Write6Branch(ArmorAddon_Target.GetUIntPtr(), uintptr_t(code.getCode()));
	}

	if (g_enableTintSync)
	{
		branchTrampoline.Write6Branch(UpdateModelSkin.GetUIntPtr(), (uintptr_t)UpdateModelSkin_Hooked);
		branchTrampoline.Write6Branch(UpdateModelHair.GetUIntPtr(), (uintptr_t)UpdateModelHair_Hooked);
	}

	if (g_enableTintInventory)
	{
		RelocAddr<uintptr_t> SetNewInventoryItemModel_Target(0x008887E0 + 0x1BA);
		{
			struct TintInventoryItem_Code : Xbyak::CodeGenerator {
				TintInventoryItem_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;
					Xbyak::Label funcLabel;

					push(rbx);
					sub(rsp, 0x20);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(SetInventoryItemModel.GetUIntPtr() + 6);
				}
			};

			void * codeBuf = localTrampoline.StartAlloc();
			TintInventoryItem_Code code(codeBuf);
			localTrampoline.EndAlloc(code.getCurr());

			SetInventoryItemModel_Original = (_SetInventoryItemModel)codeBuf;

			branchTrampoline.Write6Branch(SetInventoryItemModel.GetUIntPtr(), (uintptr_t)SetInventoryItemModel_Hooked);

			branchTrampoline.Write5Call(SetNewInventoryItemModel_Target.GetUIntPtr(), (uintptr_t)SetNewInventoryItemModel_Hooked);
		}
	}

	ObScriptCommand * hijackedCommand = nullptr;
	for (ObScriptCommand * iter = g_firstConsoleCommand; iter->opcode < kObScript_NumConsoleCommands + kObScript_ConsoleOpBase; ++iter)
	{
		if (!strcmp(iter->longName, "JobListTool"))
		{
			hijackedCommand = iter;
			break;
		}
	}
	if (hijackedCommand)
	{
		static ObScriptParam params[2];
		params[0].typeID = ObScriptParam::kType_String;
		params[0].typeStr = "String";
		params[0].isOptional = 0;
		params[1].typeID = ObScriptParam::kType_String;
		params[1].typeStr = "String (optional)";
		params[1].isOptional = 1;

		ObScriptCommand cmd = *hijackedCommand;
		cmd.longName = "skee";
		cmd.shortName = "skee";
		cmd.helpText = "skee <reload|erase> <tints|bodymorph>";
		cmd.needsParent = 0;
		cmd.numParams = 2;
		cmd.params = params;
		cmd.execute = SKEE_Execute;
		cmd.flags = 0;
		SafeWriteBuf((uintptr_t)hijackedCommand, &cmd, sizeof(cmd));
	}

	return true;
}