#include "skse64_common/SafeWrite.h"
#include "skse64/PluginAPI.h"
#include "skse64/PapyrusVM.h"

#include "skse64/GameData.h"
#include "skse64/GameRTTI.h"

#include "skse64/NiExtraData.h"
#include "skse64/NiNodes.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiNodes.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiProperties.h"

#include "SKEEHooks.h"
#include "PapyrusNiOverride.h"

#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "TintMaskInterface.h"
#include "ItemDataInterface.h"

#include "MorphHandler.h"
#include "PartHandler.h"

#include "SkeletonExtender.h"
#include "ShaderUtilities.h"

#include <vector>

#include "skse64_common/Relocation.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/SafeWrite.h"
#include "xbyak/xbyak.h"

extern SKSETaskInterface	* g_task;

extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern OverrideInterface	g_overrideInterface;

extern bool					g_enableFaceOverlays;
extern UInt32				g_numFaceOverlays;

extern bool					g_playerOnly;
extern UInt32				g_numSpellFaceOverlays;

extern bool					g_immediateArmor;
extern bool					g_immediateFace;

extern bool					g_enableEquippableTransforms;

Actor						* g_weaponHookActor = NULL;
TESObjectWEAP				* g_weaponHookWeapon = NULL;
UInt32						g_firstPerson = 0;

typedef NiAVObject * (*_CreateWeaponNode)(UInt32 * unk1, UInt32 unk2, Actor * actor, UInt32 ** unk4, UInt32 * unk5);
extern const _CreateWeaponNode CreateWeaponNode = (_CreateWeaponNode)0x0046F530;

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

struct ArmorAddonStack
{
	AddonTreeParameters * params;	// 00
	UInt64			unk08;		// 08
	UInt32			unk10;		// 10
	UInt32			unk14;		// 14
};

RelocAddr<_CreateArmorNode> CreateArmorNode(0x01CAE20);

NiAVObject * CreateArmorNode_Hooked(ArmorAddonTree * addonInfo, NiNode * objectRoot, ArmorAddonStack * stackInfo, UInt32 unk4, UInt64 unk5, UInt64 unk6)
{
	NiAVObject * retVal = CreateArmorNode(addonInfo, objectRoot, stackInfo->unk10, unk4, unk5, unk6);

	TESObjectREFR * reference = nullptr;
	UInt32 handle = addonInfo->handle;
	LookupREFRByHandle(&handle, &reference);
	if (reference)
		InstallArmorAddonHook(reference, stackInfo->params, addonInfo->boneTree, retVal);

	return retVal;
}

#ifdef FIXME
static const UInt32 kCreateArmorNode = GetFnAddr(&ArmorAddonTree::CreateArmorNode);
static const UInt32 kInstallArmorHook_Base = 0x00470050 + 0x902;
static const UInt32 kInstallArmorHook_Entry_retn = kInstallArmorHook_Base + 0x5;

enum
{
	kArmorHook_EntryStackOffset = 0x04,
	kArmorHook_VarObj = 0x14
};

__declspec(naked) void InstallArmorNodeHook_Entry(void)
{
	__asm
	{
		lea		eax, [ebx]
		push	eax
		call[kCreateArmorNode]

		jmp[kInstallArmorHook_Entry_retn]
	}
}
#endif


void __stdcall InstallArmorAddonHook(TESObjectREFR * refr, AddonTreeParameters * params, NiNode * boneTree, NiAVObject * resultNode)
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
	if (!params->armor || !params->addon) {
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
		UInt32 addonFormid = params->addon ? params->addon->formID : 0;
		UInt32 armorFormid = params->armor ? params->armor->formID : 0;
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

	SkeletonExtender::Attach(refr, boneTree, resultNode);

#ifdef _DEBUG
	_ERROR("%s - Applying Vertex Diffs on Reference (%08X) ArmorAddon (%08X) of Armor (%08X)", __FUNCTION__, refr->formID, params->addon->formID, params->armor->formID);
#endif

	// Apply no v-diffs if theres no morphs at all
	if (g_bodyMorphInterface.HasMorphs(refr)) {
		NiAutoRefCounter rf(resultNode);
		g_bodyMorphInterface.ApplyVertexDiff(refr, resultNode, true);
	}

	
	if (g_enableEquippableTransforms)
	{
		NiAutoRefCounter rf(resultNode);
		SkeletonExtender::AddTransforms(refr, isFirstPerson, isFirstPerson ? node1P : node3P, resultNode);
	}

	if ((refr == (*g_thePlayer) && g_playerOnly) || !g_playerOnly || g_overlayInterface.HasOverlays(refr))
	{
		UInt32 armorMask = params->armor->bipedObject.GetSlotMask();
		UInt32 addonMask = params->addon->biped.GetSlotMask();
		g_overlayInterface.BuildOverlays(armorMask, addonMask, refr, boneTree, resultNode);
	}

	{
		NiAutoRefCounter rf(resultNode);
		g_overrideInterface.ApplyOverrides(refr, params->armor, params->addon, resultNode, g_immediateArmor);
	}

	{
		UInt32 armorMask = params->armor->bipedObject.GetSlotMask();
		UInt32 addonMask = params->addon->biped.GetSlotMask();
		NiAutoRefCounter rf(resultNode);
		g_overrideInterface.ApplySkinOverrides(refr, isFirstPerson, params->armor, params->addon, armorMask & addonMask, resultNode, g_immediateArmor);
	}

	UInt32 armorMask = params->armor->bipedObject.GetSlotMask();

	std::function<void(ColorMap*)> overrideFunc = [=](ColorMap* colorMap)
	{
		Actor * actor = DYNAMIC_CAST(refr, TESForm, Actor);
		if (actor) {
			ModifiedItemIdentifier identifier;
			identifier.SetSlotMask(armorMask);
			ItemAttributeData * data = g_itemDataInterface.GetExistingData(actor, identifier);
			if (data) {
				if (data->m_tintData) {
					*colorMap = data->m_tintData->m_colorMap;
				}
			}
		}
	};

	g_task->AddTask(new NIOVTaskDeferredMask(refr, isFirstPerson, params->armor, params->addon, resultNode, overrideFunc));
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

void ExtraContainerChangesData_Hooked::TransferItemUID_Hooked(BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1)
{
	CALL_MEMBER_FN(this, TransferItemUID)(extraList, oldForm, newForm, unk1);

	if (extraList) {
		if (extraList->HasType(kExtraData_Rank) && !extraList->HasType(kExtraData_UniqueID)) {
			CALL_MEMBER_FN(this, SetUniqueID)(extraList, oldForm, newForm);
			ExtraRank * rank = static_cast<ExtraRank*>(extraList->GetByType(kExtraData_Rank));
			ExtraUniqueID * uniqueId = static_cast<ExtraUniqueID*>(extraList->GetByType(kExtraData_UniqueID));
			if (rank && uniqueId) {
				// Re-assign mapping
				g_itemDataInterface.UpdateUIDByRank(rank->rank, uniqueId->uniqueId, uniqueId->ownerFormId);
			}
		}
	}
}

bool BSLightingShaderProperty_Hooked::HasFlags_Hooked(UInt32 flags)
{
	bool ret = HasFlags(flags);
	if (material) {
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen) {
			NiExtraData * tintData = GetExtraData("TINT");
			if (tintData) {
				NiBooleanExtraData * boolData = ni_cast(tintData, NiBooleanExtraData);
				if (boolData) {
					return boolData->m_data;
				}
			}

			return false;
		}
	}

	return ret;
}

#ifdef FIXME
SInt32 TESNPC_Hooked::CreateHeadState_Hooked(Actor * actor, UInt32 unk1)
{
	SInt32 ret = CALL_MEMBER_FN(this, UpdateHeadState)(actor, unk1);
	try {
		__asm {
			pushad
			mov		al, g_immediateFace
			push	eax
			push	1
			mov		eax, actor
			push	eax
			call	InstallFaceOverlayHook
			popad
		}
	}
	catch (...) {
		if (actor)
			_MESSAGE("%s - unexpected error while updating face overlay for actor %08X", __FUNCTION__, actor->formID);
	}
	return ret;
}

SInt32 TESNPC_Hooked::UpdateHeadState_Hooked(Actor * actor, UInt32 unk1)
{
	SInt32 ret = CALL_MEMBER_FN(this, UpdateHeadState)(actor, unk1);
	try {
		__asm {
			pushad
			mov		al, g_immediateFace
			push	eax
			push	0
			mov		eax, actor
			push	eax
			call	InstallFaceOverlayHook
			popad
		}
	}
	catch (...) {
		if (actor)
			_MESSAGE("%s - unexpected error while updating face overlay for actor %08X", __FUNCTION__, actor->formID);
	}
	return ret;
}
#endif

class UniqueIDEventHandler : public BSTEventSink <TESUniqueIDChangeEvent>
{
public:
	virtual	EventResult		ReceiveEvent(TESUniqueIDChangeEvent * evn, EventDispatcher<TESUniqueIDChangeEvent> * dispatcher)
	{
		if (evn->oldOwnerFormId != 0) {
			g_itemDataInterface.UpdateUID(evn->oldUniqueId, evn->oldOwnerFormId, evn->newUniqueId, evn->newOwnerFormId);
		}
		if (evn->newOwnerFormId == 0) {
			g_itemDataInterface.EraseByUID(evn->oldUniqueId, evn->oldOwnerFormId);
		}
		return EventResult::kEvent_Continue;
	}
};

UniqueIDEventHandler			g_uniqueIdEventSink;

#ifdef FIXME
void SkyrimVM_Hooked::RegisterEventSinks_Hooked()
{
	RegisterPapyrusFunctions(GetClassRegistry());

	CALL_MEMBER_FN(this, RegisterEventSinks)();

	if (g_changeUniqueIDEventDispatcher)
		g_changeUniqueIDEventDispatcher->AddEventSink(&g_uniqueIdEventSink);

	g_tintMaskInterface.ReadTintData("Data\\SKSE\\Plugins\\NiOverride\\TintData\\", "*.xml");
}
#endif

#ifdef FIXME
#include "CDXNifScene.h"
#include "CDXNifMesh.h"

#include "CDXCamera.h"

#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"
#include <d3dx9.h>
#pragma comment(lib, "d3dx9.lib")


CDXNifScene						g_World;
extern CDXModelViewerCamera			g_Camera;                // A model viewing camera

void RaceSexMenu_Hooked::RenderMenu_Hooked(void)
{
	CALL_MEMBER_FN(this, RenderMenu)();

	LPDIRECT3DDEVICE9 pDevice = NiDX9Renderer::GetSingleton()->m_pkD3DDevice9;
	if (!pDevice) // This shouldnt happen
		return;

	if (g_World.IsVisible() && g_World.GetTextureGroup()) {
		NiRenderedTexture * renderedTexture = g_World.GetTextureGroup()->renderedTexture[0];
		if (renderedTexture) {
			g_World.Begin(pDevice);
			LPDIRECT3DSURFACE9 oldTarget;
			pDevice->GetRenderTarget(0, &oldTarget);
			LPDIRECT3DSURFACE9 pRenderSurface = NULL;
			LPDIRECT3DTEXTURE9 pRenderTexture = (LPDIRECT3DTEXTURE9)((NiTexture::NiDX9TextureData*)renderedTexture->rendererData)->texture;
			pRenderTexture->GetSurfaceLevel(0, &pRenderSurface);
			pDevice->SetRenderTarget(0, pRenderSurface);
			g_World.Render(pDevice);
			pDevice->SetRenderTarget(0, oldTarget);
			g_World.End(pDevice);
		}
	}
}
#endif

bool CacheTempTRI(UInt32 hash, const char * originalPath);

extern MorphHandler g_morphHandler;
extern PartSet	g_partSet;
extern UInt32	g_customDataMax;
extern bool		g_externalHeads;
extern bool		g_extendedMorphs;
extern bool		g_allowAllMorphs;

static const UInt32 kInstallRegenHeadHook_Base = 0x005A4B80 + 0x49B;
static const UInt32 kInstallRegenHeadHook_Entry_retn = kInstallRegenHeadHook_Base + 0x8;

enum
{
	kRegenHeadHook_EntryStackOffset1 = 0x20,
	kRegenHeadHook_EntryStackOffset2 = 0xA8,

	kRegenHeadHook_VarHeadPart = 0x08,
	kRegenHeadHook_VarFaceGenNode = 0x04,
	kRegenHeadHook_VarNPC = 0x0C
};

void __stdcall ApplyPreset(TESNPC * npc, BSFaceGenNiNode * headNode, BGSHeadPart * headPart)
{
	g_morphHandler.ApplyPreset(npc, headNode, headPart);
}

#ifdef FIXME
__declspec(naked) void InstallRegenHeadHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarHeadPart]
		push	eax
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarFaceGenNode + 0x04]
		push	eax
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarNPC + 0x08]
		push	eax
		call	ApplyPreset
		popad

		pop edi
		pop ebx
		add esp, 0xA0
		jmp[kInstallRegenHeadHook_Entry_retn]
	}
}
#endif

static const UInt32 kInstallForceRegenHeadHook_Base = 0x0056AEB0 + 0x35;
static const UInt32 kInstallForceRegenHeadHook_Entry_retn = kInstallForceRegenHeadHook_Base + 0x7;		// Standard execution

enum
{
	kForceRegenHeadHook_EntryStackOffset = 0x10,
};

bool	* g_useFaceGenPreProcessedHeads = (bool*)0x0125D280;

bool __stdcall IsHeadGenerated(TESNPC * npc)
{
	// For some reason the NPC vanilla preset data is reset when the actor is disable/enabled
	auto presetData = g_morphHandler.GetPreset(npc);
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
	return (presetData != NULL) || !(*g_useFaceGenPreProcessedHeads);
}

#ifdef FIXME
__declspec(naked) void InstallForceRegenHeadHook_Entry(void)
{
	__asm
	{
		push	esi
		call	IsHeadGenerated
		cmp		al, 1
		jmp[kInstallForceRegenHeadHook_Entry_retn]
	}
}
#endif

SInt32 GetGameSettingInt(const char * key)
{
	Setting	* setting = (*g_gameSettingCollection)->Get(key);
	if (setting && setting->GetType() == Setting::kType_Integer)
		return setting->data.s32;

	return 0;
}

typedef void(*_ClearFaceGenCache)();
const _ClearFaceGenCache ClearFaceGenCache = (_ClearFaceGenCache)0x00886B50;

void _cdecl ClearFaceGenCache_Hooked()
{
	ClearFaceGenCache();

	g_morphHandler.RevertInternals();
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
		g_morphHandler.ApplyMorphs(npc, faceNode);
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
		g_morphHandler.ApplyMorph(npc, headPart, faceNode);
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

void DataHandler_Hooked::GetValidPlayableHeadParts_Hooked(UInt32 unk1, void * unk2)
{
#ifdef _DEBUG_HOOK
	_DMESSAGE("Reverting Parts:");
	DumpPartVisitor dumpVisitor;
	g_partSet.Visit(dumpVisitor);
#endif

	g_partSet.Revert(); // Cleanup HeadPart List before loading new ones

	CALL_MEMBER_FN(this, GetValidPlayableHeadParts)(unk1, unk2);
}

// Pre-filtered by ValidRace and Gender
UInt8 BGSHeadPart_Hooked::IsPlayablePart_Hooked()
{
	UInt8 ret = CALL_MEMBER_FN(this, IsPlayablePart)();

#ifdef _DEBUG_HOOK
	_DMESSAGE("IsPlayablePart_Hooked - Reading Part: %08X : %s", this->formID, this->partName.data);
#endif

	if (this->type >= BGSHeadPart::kNumTypes) {
		if ((this->partFlags & BGSHeadPart::kFlagExtraPart) == 0) { // Skip Extra Parts
			if (strcmp(this->model.GetModelName(), "") == 0)
				g_partSet.SetDefaultPart(this->type, this);
			else
				g_partSet.AddPart(this->type, this);
		}
		return false; // Prevents hanging if the HeadPart is marked as Playable
	}
	else if ((this->partFlags & BGSHeadPart::kFlagExtraPart) == 0 && ret)
	{
		// Sets the default part of this type
		if (g_partSet.GetDefaultPart(this->type) == NULL) {
			auto playeRace = (*g_thePlayer)->race;
			if (playeRace) {
				TESNPC * npc = DYNAMIC_CAST((*g_thePlayer)->baseForm, TESForm, TESNPC);
				UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();

				auto chargenData = playeRace->chargenData[gender];
				if (chargenData) {
					auto headParts = chargenData->headParts;
					if (headParts) {
						for (UInt32 i = 0; i < headParts->count; i++) {
							BGSHeadPart * part;
							headParts->GetNthItem(i, part);
							if (part->type == this->type)
								g_partSet.SetDefaultPart(part->type, part);
						}
					}
				}
			}
		}

		// maps the pre-existing part to this type
		g_partSet.AddPart(this->type, this);
	}

	return ret;
}

class MorphVisitor : public MorphMap::Visitor
{
public:
	MorphVisitor::MorphVisitor(BSFaceGenModel * model, BSFixedString * morphName, NiAVObject ** headNode, float relative, UInt8 unk1)
	{
		m_model = model;
		m_morphName = morphName;
		m_headNode = headNode;
		m_relative = relative;
		m_unk1 = unk1;
	}
	bool Accept(BSFixedString morphName)
	{
		TRIModelData & morphData = g_morphHandler.GetExtendedModelTri(morphName, true);
		if (morphData.morphModel && morphData.triFile) {
			BSGeometry * geometry = NULL;
			if (m_headNode && (*m_headNode))
				geometry = (*m_headNode)->GetAsBSGeometry();

			if (geometry)
				morphData.triFile->Apply(geometry, *m_morphName, m_relative);
		}

		return false;
	}
private:
	BSFaceGenModel	* m_model;
	BSFixedString	* m_morphName;
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
		MorphVisitor morphVisitor(model, morphName, headNode, relative, unk1);
		g_morphHandler.VisitMorphMap(modelMorph->GetModelName(), morphVisitor);
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
		MorphVisitor morphVisitor(model, morphName, headNode, relative, unk1);
		g_morphHandler.VisitMorphMap(BSFixedString(modelMorph->GetModelName()), morphVisitor);
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
			g_morphHandler.SetMorph(npc, faceNode, name.data, max);
			relative -= max;
		}
	}
	g_morphHandler.SetMorph(npc, faceNode, name.data, relative);
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
	totalSliders = g_morphHandler.LoadSliders(sliders, slider);
	return totalSliders;
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
			SliderInternalPtr sliderInternal = g_morphHandler.GetSliderByIndex(player->race, sliderIndex);
			if (!sliderInternal)
				return;

			float currentValue = g_morphHandler.GetMorphValueByName(actorBase, sliderInternal->name);
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
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.data, (UInt32)currentValue);
				g_morphHandler.SetMorph(actorBase, faceNode, buffer, -1.0);
				memset(buffer, 0, MAX_PATH);
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.data, (UInt32)newValue);
				g_morphHandler.SetMorph(actorBase, faceNode, buffer, 1.0);

				g_morphHandler.SetMorphValue(actorBase, sliderInternal->name, newValue);
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
			g_morphHandler.SetMorphValue(actorBase, sliderInternal->name, newValue);
			return;
		}
	}

	DoubleMorphCallback(menu, newValue, sliderId);
}

// TESModelTri
RelocAddr<uintptr_t> TESModelTri_vtbl(0x015AF848);

// DB0F3961824CB053B91AC8B9D2FE917ACE7DD265+84
RelocAddr<_AddGFXArgument> AddGFXArgument(0x00856960);

// 57F6EC6339F20ED6A0882786A452BA66A046BDE8+1AE
RelocAddr<_FaceGenApplyMorph> FaceGenApplyMorph(0x003D2450);
RelocAddr<_AddRaceMenuSlider> AddRaceMenuSlider(0x008BC380);
RelocAddr<_DoubleMorphCallback> DoubleMorphCallback(0x008B4440);

RelocAddr<_UpdateNPCMorphs> UpdateNPCMorphs(0x00360990);
RelocAddr<_UpdateNPCMorph> UpdateNPCMorph(0x00360B80);

bool InstallSKEEHooks()
{
#ifdef FIXME
	WriteRelJump(kInstallArmorHook_Base, (UInt32)&InstallArmorNodeHook_Entry);

	if (g_enableFaceOverlays) {
		WriteRelJump(0x00569990 + 0x16E, GetFnAddr(&TESNPC_Hooked::CreateHeadState_Hooked)); // Creates new head
		WriteRelCall(0x0056AFC0 + 0x2E5, GetFnAddr(&TESNPC_Hooked::UpdateHeadState_Hooked)); // Updates head state
	}

	WriteRelCall(0x0046C310 + 0x110, GetFnAddr(&BSLightingShaderProperty_Hooked::HasFlags_Hooked));

	// Make the ExtraRank item unique
	WriteRelCall(0x00482120 + 0x5DD, GetFnAddr(&ExtraContainerChangesData_Hooked::TransferItemUID_Hooked));

	// Closest and easiest hook to Payprus Native Function Registry without a detour
	WriteRelCall(0x008D7A40 + 0x995, GetFnAddr(&SkyrimVM_Hooked::RegisterEventSinks_Hooked));

	WriteRelJump(kInstallWeaponFPHook_Base, (UInt32)&CreateWeaponNodeFPHook_Entry);
	WriteRelJump(kInstallWeapon3PHook_Base, (UInt32)&CreateWeaponNode3PHook_Entry);
	WriteRelJump(kInstallWeaponHook_Base, (UInt32)&InstallWeaponNodeHook_Entry);



	WriteRelCall(DATA_ADDR(0x00699100, 0x275), (UInt32)&LoadActorValues_Hook); // Hook for loading initial data on startup

	WriteRelCall(DATA_ADDR(0x00882290, 0x185), GetFnAddr(&DataHandler_Hooked::GetValidPlayableHeadParts_Hooked)); // Cleans up HeadPart List
	WriteRelCall(DATA_ADDR(0x00886C70, 0x97), (UInt32)&ClearFaceGenCache_Hooked); // RaceMenu dtor Cleans up HeadPart List
	WriteRelCall(DATA_ADDR(0x00881320, 0x67), GetFnAddr(&BGSHeadPart_Hooked::IsPlayablePart_Hooked)); // Custom head part insertion

	if (g_extendedMorphs) {
		WriteRelCall(DATA_ADDR(0x005A4070, 0xBE), GetFnAddr(&BSFaceGenModel_Hooked::ApplyChargenMorph_Hooked)); // Load and apply extended morphs
		WriteRelCall(DATA_ADDR(0x005A5CE0, 0x39), GetFnAddr(&BSFaceGenModel_Hooked::ApplyRaceMorph_Hooked)); // Load and apply extended morphs
	}
	WriteRelCall(DATA_ADDR(0x005A4AC0, 0x67), GetFnAddr(&TESNPC_Hooked::UpdateMorphs_Hooked)); // Updating all morphs when head is regenerated
	WriteRelCall(DATA_ADDR(0x005AA230, 0x5C), GetFnAddr(&TESNPC_Hooked::UpdateMorph_Hooked)); // ChangeHeadPart to update morph on single part
	WriteRelCall(DATA_ADDR(0x00882290, 0x2E26), GetFnAddr(&SliderArray::AddSlider_Hooked)); // Create Slider 
	WriteRelCall(DATA_ADDR(0x00881DD0, 0x436), GetFnAddr(&FxResponseArgsList_Hooked::AddArgument_Hooked)); // Add Slider

	WriteRelCall(DATA_ADDR(0x00882290, 0x337C), GetFnAddr(&RaceSexMenu_Hooked::DoubleMorphCallback_Hooked)); // Change Slider OnLoad
	WriteRelCall(DATA_ADDR(0x0087FA50, 0x93), GetFnAddr(&RaceSexMenu_Hooked::DoubleMorphCallback_Hooked)); // Change Slider OnCallback

	SafeWrite32(DATA_ADDR(0x010E7404, 0x18), GetFnAddr(&RaceSexMenu_Hooked::RenderMenu_Hooked)); // Hooks RaceMenu renderer

	if (!g_externalHeads) {
		WriteRelJump(kInstallRegenHeadHook_Base, (UInt32)&InstallRegenHeadHook_Entry); // FaceGen Regenerate HeadPart Hook
		WriteRelJump(kInstallForceRegenHeadHook_Base, (UInt32)&InstallForceRegenHeadHook_Entry); // Preprocessed Head Hook
	}
#endif
	// ApplyChargenMorph - 1403D2530+F3
	// ApplyRaceMorph - 1403D4760+56

	// UpdateMorphs Hook - 1403D26A0+C7
	// UpdateMorph - 1403DC370+79

	// AddSlider - 1408B4590+37E4 - call 1408BAED0
	// AddArgument - 1408B39A0+97A - call 1408554B0

	// DoubleMorphCallback - 1408B4590+3CD5

	if (!g_branchTrampoline.Create(1024 * 64))
	{
		_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
		return false;
	}

	if (!g_localTrampoline.Create(1024 * 64, nullptr))
	{
		_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
		return false;
	}

	RelocAddr <uintptr_t> InvokeCategoriesList_Target(0x008B4E50 + 0x9FB);
	g_branchTrampoline.Write5Call(InvokeCategoriesList_Target.GetUIntPtr(), (uintptr_t)InvokeCategoryList_Hook);

	RelocAddr <uintptr_t> AddSlider_Target(0x008B5A40 + 0x37E4);
	g_branchTrampoline.Write5Call(AddSlider_Target.GetUIntPtr(), (uintptr_t)AddSlider_Hook);

	RelocAddr <uintptr_t> DoubleMorphCallback1_Target(0x008B5A40 + 0x3CD5);
	g_branchTrampoline.Write5Call(DoubleMorphCallback1_Target.GetUIntPtr(), (uintptr_t)DoubleMorphCallback_Hook);

	RelocAddr <uintptr_t> DoubleMorphCallback2_Target(0x008B17E0 + 0x4F);
	g_branchTrampoline.Write5Call(DoubleMorphCallback2_Target.GetUIntPtr(), (uintptr_t)DoubleMorphCallback_Hook);

	RelocAddr <uintptr_t> ApplyChargenMorph_Target(0x003D25B0 + 0xF3);
	g_branchTrampoline.Write5Call(ApplyChargenMorph_Target.GetUIntPtr(), (uintptr_t)ApplyChargenMorph_Hooked);

	RelocAddr <uintptr_t> ApplyRaceMorph_Target(0x003D47E0 + 0x56);
	g_branchTrampoline.Write5Call(ApplyRaceMorph_Target.GetUIntPtr(), (uintptr_t)ApplyRaceMorph_Hooked);

	RelocAddr <uintptr_t> UpdateMorphs_Target(0x003D2720 + 0xC7);
	g_branchTrampoline.Write5Call(UpdateMorphs_Target.GetUIntPtr(), (uintptr_t)UpdateMorphs_Hooked);

	RelocAddr <uintptr_t> UpdateMorph_Target(0x003DC3F0 + 0x79);
	g_branchTrampoline.Write5Call(UpdateMorph_Target.GetUIntPtr(), (uintptr_t)UpdateMorph_Hooked);


	RelocAddr<uintptr_t> ArmorAddon_Target(0x001C7140 + 0xB4A);
	{
		struct ArmorAddonHook_Entry_Code : Xbyak::CodeGenerator {
			ArmorAddonHook_Entry_Code(void * buf, UInt64 funcAddr, UInt64 targetAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				lea(r8, ptr[rsp + 0x78]);
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

		void * codeBuf = g_localTrampoline.StartAlloc();
		ArmorAddonHook_Entry_Code code(codeBuf, uintptr_t(CreateArmorNode_Hooked), ArmorAddon_Target.GetUIntPtr());
		g_localTrampoline.EndAlloc(code.getCurr());

		g_branchTrampoline.Write6Branch(ArmorAddon_Target.GetUIntPtr(), uintptr_t(code.getCode()));
	}

	return true;
}