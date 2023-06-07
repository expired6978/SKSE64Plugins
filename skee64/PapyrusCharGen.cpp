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
#include "PresetInterface.h"

#include "FileUtils.h"

extern FaceMorphInterface g_morphInterface;
extern bool			g_externalHeads;
extern bool			g_enableHeadExport;

extern SKSETaskInterface				* g_task;
extern OverrideInterface				g_overrideInterface;
extern NiTransformInterface				g_transformInterface;
extern BodyMorphInterface				g_bodyMorphInterface;
extern OverlayInterface					g_overlayInterface;
extern PresetInterface					g_presetInterface;

namespace papyrusCharGen
{
	void SaveCharacter(StaticFunctionTag*, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Data\\Textures\\CharGen\\Exported\\");

		g_presetInterface.SaveJsonPreset(slotPath);

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
		bool loadError = g_presetInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.slot", fileName.data);
			loadError = g_presetInterface.LoadBinaryPreset(slotPath, presetData);
		}

		if (loadError) {
			_ERROR("%s - failed to load preset.", __FUNCTION__);
			return false;
		}

		presetData->tintTexture = tintPath;
		g_presetInterface.AssignMappedPreset(npc, presetData);

		g_presetInterface.ApplyPreset(actor, race, npc, presetData, (PresetInterface::ApplyTypes)flags);
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

		g_presetInterface.SaveJsonPreset(slotPath);

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

		g_presetInterface.EraseMappedPreset(npc);

		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", fileName.data);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_presetInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.slot", fileName.data);
			loadError = g_presetInterface.LoadBinaryPreset(slotPath, presetData);
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

		g_presetInterface.ApplyPreset(actor, race, npc, presetData, (PresetInterface::ApplyTypes)flags);
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
		bool loadError = g_presetInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Presets\\%s.slot", fileName.data);
			loadError = g_presetInterface.LoadBinaryPreset(slotPath, presetData);
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

		g_presetInterface.ApplyPresetData(actor, presetData, true, (PresetInterface::ApplyTypes)flags);

		// Queue a node update
		CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
		return true;
	}

	void SaveCharacterPreset(StaticFunctionTag*, Actor * actor, BSFixedString fileName)
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Presets\\%s.jslot", fileName.data);
		g_presetInterface.SaveJsonPreset(slotPath);
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

		return g_presetInterface.EraseMappedPreset(npc);
	}

	void ClearPresets(StaticFunctionTag*)
	{
		g_presetInterface.ClearMappedPresets();
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
		g_presetInterface.SaveJsonPreset(slotPath);
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
