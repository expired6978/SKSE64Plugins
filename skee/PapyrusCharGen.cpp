#include "PapyrusCharGen.h"
#include "FaceMorphInterface.h"
#include "NifUtils.h"

#include "common/IFileStream.h"

#include "skse64/PluginAPI.h"

#include "skse64/GameFormComponents.h"
#include "skse64/GameData.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameForms.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiNodes.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiProperties.h"

#include "skse64/PapyrusVM.h"
#include "skse64/PapyrusNativeFunctions.h"

#include "common/ICriticalSection.h"

#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"

#include "FileUtils.h"

extern FaceMorphInterface g_morphInterface;
extern bool			g_externalHeads;
extern bool			g_enableHeadExport;

extern SKSETaskInterface				* g_task;
extern OverrideInterface				g_overrideInterface;
extern NiTransformInterface				g_transformInterface;
extern BodyMorphInterface				g_bodyMorphInterface;
extern OverlayInterface					g_overlayInterface;

void ApplyPreset(Actor * actor, TESRace * race, TESNPC * npc, PresetDataPtr presetData, FaceMorphInterface::ApplyTypes applyType)
{
	CALL_MEMBER_FN(actor, SetRace)(race, actor == (*g_thePlayer));

	npc->overlayRace = NULL;
	npc->weight = presetData->weight;

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

	// Sculpt data loaded here
	g_morphInterface.EraseSculptData(npc);
	if (presetData->sculptData) {
		if (presetData->sculptData->size() > 0)
			g_morphInterface.SetSculptTarget(npc, presetData->sculptData);
	}

	// Assign custom morphs here (values only)
	g_morphInterface.EraseMorphData(npc);
	for (auto & it : presetData->customMorphs)
		g_morphInterface.SetMorphValue(npc, it.name, it.value);

	// Wipe the HeadPart list and replace it with the default race list
	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();
	TESRace::CharGenData * chargenData = race->chargenData[gender];
	if (chargenData) {
		BGSHeadPart ** headParts = npc->headparts;
		tArray<BGSHeadPart*> * headPartList = race->chargenData[gender]->headParts;
		if (headParts && headPartList) {
			Heap_Free(headParts);
			npc->numHeadParts = headPartList->count;
			headParts = (BGSHeadPart **)Heap_Allocate(npc->numHeadParts * sizeof(BGSHeadPart*));
			for (UInt32 i = 0; i < headPartList->count; i++)
				headPartList->GetNthItem(i, headParts[i]);
			npc->headparts = headParts;
		}
	}

	// Force the remaining parts to change to that of the preset
	for (auto part : presetData->headParts)
		CALL_MEMBER_FN(npc, ChangeHeadPart)(part);

	//npc->MarkChanged(0x2000800); // Save FaceData and Race
	npc->MarkChanged(0x800); // Save FaceData

	// Grab the skin tint and convert it from RGBA to RGB on NPC
	if (presetData->tints.size() > 0) {
		PresetData::Tint & tint = presetData->tints.at(0);
		float alpha = (tint.color >> 24) / 255.0;
		TintMask tintMask;
		tintMask.color.red = (tint.color >> 16) & 0xFF;
		tintMask.color.green = (tint.color >> 8) & 0xFF;
		tintMask.color.blue = tint.color & 0xFF;
		tintMask.alpha = alpha;
		tintMask.tintType = TintMask::kMaskType_SkinTone;

		NiColorA colorResult;
		CALL_MEMBER_FN(npc, SetSkinFromTint)(&colorResult, &tintMask, true);
	}

	// Queue a node update
	CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);

	if ((applyType & FaceMorphInterface::ApplyTypes::kPresetApplyOverrides) == FaceMorphInterface::ApplyTypes::kPresetApplyOverrides) {
		g_overrideInterface.RemoveAllReferenceNodeOverrides(actor);

		g_overlayInterface.RevertOverlays(actor, true);

		if (!g_overlayInterface.HasOverlays(actor) && presetData->overrideData.size() > 0)
			g_overlayInterface.AddOverlays(actor);

		for (auto & nodes : presetData->overrideData) {
			for (auto & value : nodes.second) {
				g_overrideInterface.AddNodeOverride(actor, gender == 1 ? true : false, nodes.first, value);
			}
		}
	}

	if ((applyType & FaceMorphInterface::ApplyTypes::kPresetApplySkinOverrides) == FaceMorphInterface::ApplyTypes::kPresetApplySkinOverrides)
	{
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto & slot : presetData->skinData[i]) {
				for (auto & value : slot.second) {
					g_overrideInterface.AddSkinOverride(actor, gender == 1 ? true : false, i == 1 ? true : false, slot.first, value);
				}
			}
		}
	}

	if ((applyType & FaceMorphInterface::ApplyTypes::kPresetApplyTransforms) == FaceMorphInterface::ApplyTypes::kPresetApplyTransforms) {
		g_transformInterface.Impl_RemoveAllReferenceTransforms(actor);
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto & xForms : presetData->transformData[i]) {
				for (auto & key : xForms.second) {
					for (auto & value : key.second) {
						g_transformInterface.Impl_AddNodeTransform(actor, i == 1 ? true : false, gender == 1 ? true : false, xForms.first, key.first, value);
					}
				}
			}
		}
		g_transformInterface.Impl_UpdateNodeAllTransforms(actor);
	}

	if ((applyType & FaceMorphInterface::ApplyTypes::kPresetApplyBodyMorphs) == FaceMorphInterface::ApplyTypes::kPresetApplyBodyMorphs) {
		g_bodyMorphInterface.ClearMorphs(actor);

		for (auto & morph : presetData->bodyMorphData) {
			for (auto & keys : morph.second)
				g_bodyMorphInterface.SetMorph(actor, morph.first, keys.first, keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}

namespace papyrusCharGen
{
	void SaveCharacter(StaticFunctionTag*, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Data\\Textures\\CharGen\\Exported\\");

		g_morphInterface.SaveJsonPreset(slotPath);

		if(g_enableHeadExport)
			g_task->AddTask(new SKSETaskExportTintMask(tintPath, fileName.data));
	}

	void DeleteCharacter(StaticFunctionTag*, BSFixedString fileName)
	{
		char tempPath[MAX_PATH];
		sprintf_s(tempPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.slot", fileName.data);
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
				case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			}
		}
		sprintf_s(tempPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
			case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			}
		}
		sprintf_s(tempPath, "Data\\Textures\\CharGen\\Exported\\%s.dds", fileName.data);
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
			case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			}
		}
		sprintf_s(tempPath, "Data\\Meshes\\CharGen\\Exported\\%s.nif", fileName.data);
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
			case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			}
		}
	}
	
	SInt32 DeleteFaceGenData(StaticFunctionTag*, TESNPC * npc)
	{
		SInt32 ret = 0;
		if (!npc) {
			_ERROR("%s - invalid actorbase.", __FUNCTION__);
			return -1;
		}

		ModInfo * modInfo = GetModInfoByFormID(npc->formID);
		if (!modInfo) {
			_ERROR("%s - failed to find mod for %08X.", __FUNCTION__, npc->formID);
			return false;
		}

		enum
		{
			kReturnDeletedNif = 1,
			kReturnDeletedDDS = 2
		};
		
		char tempPath[MAX_PATH];
		sprintf_s(tempPath, "Data\\Meshes\\Actors\\Character\\FaceGenData\\FaceGeom\\%s\\%08X.nif", modInfo->name, modInfo->IsLight() ? (npc->formID & 0xFFF) : (npc->formID & 0xFFFFFF));
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
			case ERROR_FILE_NOT_FOUND: // We don't need to display a message for this
				break;
			case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			default:
				_ERROR("%s - error deleting file %s (Error %d)", __FUNCTION__, tempPath, lastError);
				break;
			}
		}
		else
			ret |= kReturnDeletedNif;

		sprintf_s(tempPath, "Data\\Textures\\Actors\\Character\\FaceGenData\\FaceTint\\%s\\%08X.dds", modInfo->name, modInfo->IsLight() ? (npc->formID & 0xFFF) : (npc->formID & 0xFFFFFF));
		if (!DeleteFile(tempPath)) {
			UInt32 lastError = GetLastError();
			switch (lastError) {
			case ERROR_FILE_NOT_FOUND: // We don't need to display a message for this
				break;
			case ERROR_ACCESS_DENIED:
				_ERROR("%s - access denied could not delete %s", __FUNCTION__, tempPath);
				break;
			default:
				_ERROR("%s - error deleting file %s (Error %d)", __FUNCTION__, tempPath, lastError);
				break;
			}
		}
		else
			ret |= kReturnDeletedDDS;

		return ret;
	}
	
	bool LoadCharacterEx(StaticFunctionTag*, Actor * actor, TESRace * race, BSFixedString fileName, UInt32 flags)
	{
		if (!actor) {
			_ERROR("%s - No actor found.", __FUNCTION__);
			return false;
		}
		if (!race) {
			_ERROR("%s - No race found.", __FUNCTION__);
			return false;
		}
		TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
		if (!npc) {
			_ERROR("%s - failed acquire ActorBase.", __FUNCTION__);
			return false;
		}
		
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Textures\\CharGen\\Exported\\%s.dds", fileName.data);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_morphInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.slot", fileName.data);
			loadError = g_morphInterface.LoadBinaryPreset(slotPath, presetData);
		}

		if (loadError) {
			_ERROR("%s - failed to load preset.", __FUNCTION__);
			return false;
		}

		presetData->tintTexture = tintPath;
		g_morphInterface.AssignPreset(npc, presetData);

		ApplyPreset(actor, race, npc, presetData, (FaceMorphInterface::ApplyTypes)flags);
		return true;
	}

	void SaveExternalCharacter(StaticFunctionTag*, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		char nifPath[MAX_PATH];
		sprintf_s(nifPath, "Data\\Meshes\\CharGen\\Exported\\%s.nif", fileName.data);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Data\\Textures\\CharGen\\Exported\\%s.dds", fileName.data);

		g_morphInterface.SaveJsonPreset(slotPath);

		if(g_enableHeadExport)
			g_task->AddTask(new SKSETaskExportHead((*g_thePlayer), nifPath, tintPath));
	}

	bool LoadExternalCharacterEx(StaticFunctionTag*, Actor * actor, TESRace * race, BSFixedString fileName, UInt32 flags)
	{
		if (!actor) {
			_ERROR("%s - No actor found.", __FUNCTION__);
			return false;
		}
		if (!race) {
			_ERROR("%s - No race found.", __FUNCTION__);
			return false;
		}
		TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
		if (!npc) {
			_ERROR("%s - failed acquire ActorBase.", __FUNCTION__);
			return false;
		}

		g_morphInterface.ErasePreset(npc);

		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_morphInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.slot", fileName.data);
			loadError = g_morphInterface.LoadBinaryPreset(slotPath, presetData);
		}

		if (loadError) {
			_ERROR("%s - failed to load preset.", __FUNCTION__);
			return false;
		}

		ModInfo * modInfo = GetModInfoByFormID(npc->formID);
		if (!modInfo) {
			_ERROR("%s - failed to find mod for %08X.", __FUNCTION__, npc->formID);
			return false;
		}

		char sourcePath[MAX_PATH];
		char destPath[MAX_PATH];
		sprintf_s(sourcePath, "Data\\Meshes\\CharGen\\Exported\\%s.nif", fileName.data);
		sprintf_s(destPath, "Data\\Meshes\\Actors\\Character\\FaceGenData\\FaceGeom\\%s\\%08X.nif", modInfo->name, modInfo->IsLight() ? (npc->formID & 0xFFF) : (npc->formID & 0xFFFFFF));
		CopyFile(sourcePath, destPath, false);
		sprintf_s(sourcePath, "Data\\Textures\\CharGen\\Exported\\%s.dds", fileName.data);
		sprintf_s(destPath, "Data\\Textures\\Actors\\Character\\FaceGenData\\FaceTint\\%s\\%08X.dds", modInfo->name, modInfo->IsLight() ? (npc->formID & 0xFFF) : (npc->formID & 0xFFFFFF));
		CopyFile(sourcePath, destPath, false);

		ApplyPreset(actor, race, npc, presetData, (FaceMorphInterface::ApplyTypes)flags);
		g_task->AddTask(new SKSETaskRefreshTintMask(actor, sourcePath));
		return true;
	}

	bool LoadCharacterPresetEx(StaticFunctionTag*, Actor * actor, BSFixedString fileName, BGSColorForm * hairColor, UInt32 flags)
	{
		if (!actor) {
			_ERROR("%s - No actor found.", __FUNCTION__);
			return false;
		}
		TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
		if (!npc) {
			_ERROR("%s - failed acquire ActorBase.", __FUNCTION__);
			return false;
		}

		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Presets\\%s.jslot", fileName.data);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_morphInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Presets\\%s.slot", fileName.data);
			loadError = g_morphInterface.LoadBinaryPreset(slotPath, presetData);
		}

		if (loadError) {
			_ERROR("%s - failed to load preset.", __FUNCTION__);
			return false;
		}

		if (hairColor)
		{
			hairColor->color.red = (presetData->hairColor >> 16) & 0xFF;
			hairColor->color.green = (presetData->hairColor >> 8) & 0xFF;
			hairColor->color.blue = presetData->hairColor & 0xFF;

			npc->SetHairColor(hairColor);
		}

		g_morphInterface.ApplyPresetData(actor, presetData, true, (FaceMorphInterface::ApplyTypes)flags);

		// Queue a node update
		CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
		return true;
	}

	void SaveCharacterPreset(StaticFunctionTag*, Actor * actor, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Presets\\%s.jslot", fileName.data);
		g_morphInterface.SaveJsonPreset(slotPath);
	}

	bool IsExternalEnabled(StaticFunctionTag*)
	{
		return g_externalHeads;
	}

	bool ClearPreset(StaticFunctionTag*, TESNPC * npc)
	{
		if (!npc) {
			_ERROR("%s - failed acquire ActorBase.", __FUNCTION__);
			return false;
		}

		return g_morphInterface.ErasePreset(npc);
	}

	void ClearPresets(StaticFunctionTag*)
	{
		g_morphInterface.ClearPresets();
	}

	void ExportHead(StaticFunctionTag*, BSFixedString fileName)
	{
		if (g_enableHeadExport)
		{
			char nifPath[MAX_PATH];
			sprintf_s(nifPath, "Data\\SKSE\\Plugins\\CharGen\\%s.nif", fileName.data);
			char tintPath[MAX_PATH];
			sprintf_s(tintPath, "Data\\SKSE\\Plugins\\CharGen\\%s.dds", fileName.data);

			g_task->AddTask(new SKSETaskExportHead((*g_thePlayer), nifPath, tintPath));
		}
	}

	void ExportSlot(StaticFunctionTag*, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		g_morphInterface.SaveJsonPreset(slotPath);
	}
};

void papyrusCharGen::RegisterFuncs(VMClassRegistry* registry)
{
	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, BSFixedString>("SaveCharacter", "CharGen", papyrusCharGen::SaveCharacter, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, bool, Actor*, TESRace*, BSFixedString, UInt32>("LoadCharacterEx", "CharGen", papyrusCharGen::LoadCharacterEx, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, BSFixedString>("DeleteCharacter", "CharGen", papyrusCharGen::DeleteCharacter, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, SInt32, TESNPC*>("DeleteFaceGenData", "CharGen", papyrusCharGen::DeleteFaceGenData, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, BSFixedString>("SaveExternalCharacter", "CharGen", papyrusCharGen::SaveExternalCharacter, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, bool, Actor*, TESRace*, BSFixedString, UInt32>("LoadExternalCharacterEx", "CharGen", papyrusCharGen::LoadExternalCharacterEx, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, bool, TESNPC*>("ClearPreset", "CharGen", papyrusCharGen::ClearPreset, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, void>("ClearPresets", "CharGen", papyrusCharGen::ClearPresets, registry));

	registry->RegisterFunction(
		new NativeFunction0<StaticFunctionTag, bool>("IsExternalEnabled", "CharGen", papyrusCharGen::IsExternalEnabled, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, BSFixedString>("ExportHead", "CharGen", papyrusCharGen::ExportHead, registry));

	registry->RegisterFunction(
		new NativeFunction1<StaticFunctionTag, void, BSFixedString>("ExportSlot", "CharGen", papyrusCharGen::ExportSlot, registry));

	registry->RegisterFunction(
		new NativeFunction4<StaticFunctionTag, bool, Actor*, BSFixedString, BGSColorForm*, UInt32>("LoadCharacterPresetEx", "CharGen", papyrusCharGen::LoadCharacterPresetEx, registry));

	registry->RegisterFunction(
		new NativeFunction2<StaticFunctionTag, void, Actor*, BSFixedString>("SaveCharacterPreset", "CharGen", papyrusCharGen::SaveCharacterPreset, registry));
}
