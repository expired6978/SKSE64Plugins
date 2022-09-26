#include "CommandInterface.h"

extern CommandInterface g_commandInterface;

bool CommandInterface::RegisterCommand(const char* command, const char* desc, CommandCallback cb)
{
    std::lock_guard<std::mutex> locker(m_lock);
	return m_commandMap[command].emplace(CommandData{ desc, cb }).second;
}

bool CommandInterface::ExecuteCommand(const char* command, TESObjectREFR* ref, const char* argumentString)
{
	std::lock_guard<std::mutex> locker(m_lock);
    auto it = m_commandMap.find(command);
    if (it != m_commandMap.end())
    {
        for (auto data : it->second)
        {
            if (data.cb(ref, argumentString))
                return true;
        }
    }
    return false;
}

#include "skse64/PluginAPI.h"
#include "skse64/GameRTTI.h"
#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "ActorUpdateManager.h"
#include "NiTransformInterface.h"
#include "FaceMorphInterface.h"

#include "AttachmentInterface.h"
#include "NifUtils.h"
#include "ShaderUtilities.h"
#include "common/IFileStream.h"

extern SKSETaskInterface*	g_task;
extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern OverrideInterface	g_overrideInterface;
extern ActorUpdateManager	g_actorUpdateManager;
extern NiTransformInterface	g_transformInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern FaceMorphInterface	g_morphInterface;

void CommandInterface::RegisterCommands()
{
    RegisterCommand("reload", "<tints>", [](TESObjectREFR* thisObj, const char* argument) -> bool
    {
        if (_strnicmp(argument, "tints", MAX_PATH) == 0)
        {
            g_tintMaskInterface.LoadMods();
            Console_Print("Tint XMLs reloaded");
            return true;
        }
        return false;
    });

	RegisterCommand("erase", "<bodymorph|transforms|sculpt|overlays|bodymorph-cache>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (_strnicmp(argument, "bodymorph", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing BodyMorphs requires a console target");
				return true;
			}

			g_bodyMorphInterface.ClearMorphs(thisObj);
			g_bodyMorphInterface.UpdateModelWeight(thisObj);
			Console_Print("Erased all bodymorphs");
			return true;
		}
		else if (_strnicmp(argument, "transforms", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing transforms requires a console target");
				return true;
			}

			g_transformInterface.Impl_RemoveAllReferenceTransforms(thisObj);
			g_transformInterface.Impl_UpdateNodeAllTransforms(thisObj);
			Console_Print("Erased all transforms");
			return true;
		}
		else if (_strnicmp(argument, "sculpt", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing sculpt requires a console target");
				return true;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return true;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC* npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return true;
			}

			g_morphInterface.EraseSculptData(npc);
			g_task->AddTask(new SKSEUpdateFaceModel(actor));

			Console_Print("Erased all sculpting");
			return true;
		}
		else if (_strnicmp(argument, "overlays", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Erasing overlays requires a console target");
				return true;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return true;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC* npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return true;
			}

			g_overlayInterface.EraseOverlays(actor);

			Console_Print("Erased and reverted all overlays");
			return true;
		}
		else if (_strnicmp(argument, "bodymorph-cache", MAX_PATH) == 0)
		{
			size_t freedMem = g_bodyMorphInterface.ClearMorphCache();
			Console_Print("Erased %I64u bytes from BodyMorph Cache", freedMem);
			return true;
		}
		return false;
	});
	RegisterCommand("preset-save", "<name>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "Data\\SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", argument);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Data\\Textures\\CharGen\\Exported\\");

		g_morphInterface.SaveJsonPreset(slotPath);

		g_task->AddTask(new SKSETaskExportTintMask(tintPath, argument));
		Console_Print("Preset saved");
		return true;
	});
	RegisterCommand("preset-load", "<name>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (!thisObj) {
			Console_Print("Applying a preset requires a console target");
			return true;
		}
		if (thisObj->formType != Character::kTypeID) {
			Console_Print("Console target must be an actor");
			return true;
		}
		Actor* actor = static_cast<Actor*>(thisObj);
		TESNPC* npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
		if (!npc) {
			Console_Print("Failed to acquire ActorBase for specified reference");
			return true;
		}

		char slotPath[MAX_PATH];
		sprintf_s(slotPath, "SKSE\\Plugins\\CharGen\\Exported\\%s.jslot", argument);
		char tintPath[MAX_PATH];
		sprintf_s(tintPath, "Textures\\CharGen\\Exported\\%s.dds", argument);

		auto presetData = std::make_shared<PresetData>();
		bool loadError = g_morphInterface.LoadJsonPreset(slotPath, presetData);
		if (loadError) {
			Console_Print("Failed to load preset at %s", slotPath);
			return true;
		}

		presetData->tintTexture = tintPath;
		g_morphInterface.AssignPreset(npc, presetData);
		g_morphInterface.ApplyPresetData(actor, presetData, true, FaceMorphInterface::ApplyTypes::kPresetApplyAll);

		// Queue a node update
		CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
		Console_Print("Preset loaded");
		return true;
	});
#if _DEBUG
	RegisterCommand("attach", "<object>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (!thisObj) {
			Console_Print("Attaching mesh requires object");
			return true;
		}

		g_task->AddTask(new SKSEAttachSkinnedMesh(static_cast<Actor*>(thisObj), argument, "TestRoot", false, true, std::vector<BSFixedString>()));
		return true;
	});
#endif
	RegisterCommand("diagnostics", "<bodymorph|transforms|strings|updates>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (_strnicmp(argument, "bodymorph", MAX_PATH) == 0)
		{
			g_bodyMorphInterface.PrintDiagnostics();
			return true;
		}
		else if (_strnicmp(argument, "transforms", MAX_PATH) == 0)
		{
			g_transformInterface.PrintDiagnostics();
			return true;
		}
		else if (_strnicmp(argument, "strings", MAX_PATH) == 0)
		{
			g_stringTable.PrintDiagnostics();
			return true;
		}
		else if (_strnicmp(argument, "updates", MAX_PATH) == 0)
		{
			g_actorUpdateManager.PrintDiagnostics();
			return true;
		}
		return false;
	});
	RegisterCommand("dump", "<bodymorph|morphnames|transforms|tints|overrides|itemdata|itembinding|skeleton_3p|skeleton_1p|equipped>", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (_strnicmp(argument, "bodymorph", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping transforms requires a console target");
				return true;
			}

			Console_Print("Dumping body morphs for %08X", thisObj->formID);

			class Visitor : public IBodyMorphInterface::MorphValueVisitor
			{
			public:
				Visitor() { }

				virtual void Visit(TESObjectREFR* ref, const char* morphKey, const char* key, float value)
				{
					m_mapping[key][morphKey] = value;
				}
				std::map<SKEEFixedString, std::map<SKEEFixedString, float>> m_mapping;
			};
			Visitor visitor;
			g_bodyMorphInterface.VisitMorphValues(thisObj, visitor);

			UInt32 totalMorphs = 0;
			for (auto& key : visitor.m_mapping)
			{
				Console_Print("Key: %s", key.first.c_str());
				for (auto& morph : key.second)
				{
					Console_Print("\tMorph: %s\t\tValue: %f", morph.first.c_str(), morph.second);
				}
				Console_Print("Dumped %d morphs for key %s", key.second.size(), key.first.c_str());
				totalMorphs += key.second.size();
			}
			Console_Print("Dumped %d total morphs", totalMorphs);
			return true;
		}
		else if (_strnicmp(argument, "morphnames", MAX_PATH) == 0)
		{
			Console_Print("Dumping morph names");
			auto morphNames = g_bodyMorphInterface.GetCachedMorphNames();
			for (auto& name : morphNames)
			{
				Console_Print("\t%s", name.c_str());
			}
			Console_Print("%d total morphs", morphNames.size());
			return true;
		}
		else if (_strnicmp(argument, "transforms", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping transforms requires a console target");
				return true;
			}
			if (thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return true;
			}
			Actor* actor = static_cast<Actor*>(thisObj);
			TESNPC* npc = DYNAMIC_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc) {
				Console_Print("Failed to acquire ActorBase for specified reference");
				return true;
			}

			Console_Print("Dumping transforms for %08X", thisObj->formID);

			UInt32 totalTransforms = 0;
			g_transformInterface.Impl_VisitNodes(thisObj, false, CALL_MEMBER_FN(npc, GetSex)() == 1, [&](const SKEEFixedString& node, OverrideRegistration<StringTableItem>* keys)
			{
				Console_Print("Node: %s", node.c_str());
				for (auto& item : *keys)
				{
					Console_Print("\tKey: %s\t\tProperties %d", item.first ? item.first->c_str() : "", item.second.size());
					totalTransforms++;
				}
				return true;
			});
			Console_Print("Dumped %d total transforms", totalTransforms);
			return true;
		}
		else if (_strnicmp(argument, "tints", MAX_PATH) == 0)
		{
			if (thisObj && thisObj->formType != Character::kTypeID) {
				Console_Print("Console target must be an actor");
				return true;
			}

			UInt32 mask = 1;
			for (UInt32 i = 0; i < 32; ++i)
			{
				IItemDataInterface::Identifier identifier;
				identifier.SetSlotMask(mask);
				g_task->AddTask(new NIOVTaskUpdateItemDye(thisObj ? static_cast<Actor*>(thisObj) : (*g_thePlayer), identifier, TintMaskInterface::kUpdate_All, true, [mask](TESObjectARMO* armo, TESObjectARMA* arma, const char* path, NiTexturePtr texture, LayerTarget& layer)
				{
					char texturePath[MAX_PATH];
					_snprintf_s(texturePath, MAX_PATH, "Data\\SKSE\\Plugins\\NiOverride\\Exported\\TintMasks\\%s", path);

					IFileStream::MakeAllDirs(texturePath);

					SaveRenderedDDS(texture, texturePath);

					Console_Print("Dumped result for slot %08X at %s on shape", mask, texturePath, layer.object->m_name);
				}));
				mask <<= 1;
			}
			return true;
		}
		else if (_strnicmp(argument, "overrides", MAX_PATH) == 0)
		{
			Console_Print("Dumping node overrides...");
			g_overrideInterface.Dump();
			Console_Print("Dump complete. See log file for details.");
			return true;
		}
		else if (_strnicmp(argument, "itemdata", MAX_PATH) == 0)
		{
			g_itemDataInterface.ForEachItemAttribute([](const ItemAttribute& item)
			{
				_MESSAGE("Item UID: %d ID: %d Owner: %08X Form: %08X", item.uid, item.rank, item.ownerForm, item.formId);
				gLog.Indent();
				item.data->ForEachLayer([&](SInt32 layerIndex, auto tintData) -> bool
				{
					_MESSAGE("Tint Index: %d", layerIndex);
					gLog.Indent();
					for (auto& color : tintData.m_colorMap)
					{
						_MESSAGE("ColorIndex: %d Color: %08X", color.first, color.second);
					}
					for (auto& blend : tintData.m_blendMap)
					{
						_MESSAGE("BlendIndex: %d Blend: %s", blend.first, blend.second->c_str());
					}
					for (auto& texture : tintData.m_textureMap)
					{
						_MESSAGE("TextureIndex: %d Texture: %s", texture.first, texture.second->c_str());
					}
					for (auto& type : tintData.m_typeMap)
					{
						_MESSAGE("TypeIndex: %d Type: %d", type.first, type.second);
					}
					gLog.Outdent();
					return false;
				});
				gLog.Outdent();
				Console_Print("Item UID: %d ID: %d Owner: %08X Form: %08X", item.uid, item.rank, item.ownerForm, item.formId);
			});
			return true;
		}
		else if (_strnicmp(argument, "itembinding", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping itembinding requires a console target");
				return true;
			}

			class RankItemFinder
			{
			public:
				bool Accept(InventoryEntryData* pEntryData)
				{
					if (!pEntryData)
						return true;

					ExtendDataList* pExtendList = pEntryData->extendDataList;
					if (!pExtendList)
						return true;

					SInt32 n = 0;
					BaseExtraList* pExtraDataList = pExtendList->GetNthItem(n);
					while (pExtraDataList)
					{
						if (pExtraDataList->HasType(kExtraData_Rank))
						{
							ExtraRank* extraRank = static_cast<ExtraRank*>(pExtraDataList->GetByType(kExtraData_Rank));
							Console_Print("\tItem ID: %d Form: %08X", extraRank->rank, pEntryData->type->formID);
							auto itemData = g_itemDataInterface.GetData(extraRank->rank);
							if (itemData)
							{
								itemData->ForEachLayer([&](SInt32 layerIndex, auto tintData) -> bool
								{
									Console_Print("\t\tTint Index: %d", layerIndex);
									for (auto& color : tintData.m_colorMap)
									{
										Console_Print("\t\t\tColorIndex: %d Color: %08X", color.first, color.second);
									}
									for (auto& blend : tintData.m_blendMap)
									{
										Console_Print("\t\t\tBlendIndex: %d Blend: %s", blend.first, blend.second->c_str());
									}
									for (auto& texture : tintData.m_textureMap)
									{
										Console_Print("\t\t\tTextureIndex: %d Texture: %s", texture.first, texture.second->c_str());
									}
									for (auto& type : tintData.m_typeMap)
									{
										Console_Print("\t\t\tTypeIndex: %d Type: %d", type.first, type.second);
									}
									return false;
								});
							}
							foundItems++;
						}

						n++;
						pExtraDataList = pExtendList->GetNthItem(n);
					}
					return true;
				}

				UInt32 foundItems = 0;
			};

			Console_Print("Finding items with extended data inside %08X", thisObj->formID);

			ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(thisObj->extraData.GetByType(kExtraData_ContainerChanges));
			if (pContainerChanges) {
				RankItemFinder itemFinder;
				auto data = pContainerChanges->data;
				if (data) {
					auto objList = data->objList;
					if (objList) {
						objList->Visit(itemFinder);

						Console_Print("Found %d items with extended data inside %08X", itemFinder.foundItems, thisObj->formID);
					}
				}
			}
			return true;
		}
		else if (_strnicmp(argument, "skeleton_3p", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping nodes requires a reference");
				return true;
			}

			Console_Print("Dumping reference third person skeleton...");
			DumpNodeChildren(thisObj->GetNiRootNode(0));
			Console_Print("Dumped reference. See log for more details.");
			return true;
		}
		else if (_strnicmp(argument, "skeleton_1p", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping nodes requires a reference");
				return true;
			}

			Console_Print("Dumping reference first person skeleton...");
			DumpNodeChildren(thisObj->GetNiRootNode(1));
			Console_Print("Dumped reference. See log for more details.");
			return true;
		}
		else if (_strnicmp(argument, "equipped", MAX_PATH) == 0)
		{
			if (!thisObj) {
				Console_Print("Dumping biped nodes requires a reference");
				return false;
			}
			for (int k = 0; k <= 1; ++k)
			{
				auto weightModel = thisObj->GetBiped(k);
				_MESSAGE("Biped Set %d", k);
				if (weightModel && weightModel->bipedData)
				{
					for (int i = 0; i < 42; ++i)
					{
						_MESSAGE("Biped 1 Slot: %d Armor: %08X Arma: %08X", i, weightModel->bipedData->unk10[i].armor ? weightModel->bipedData->unk10[i].armor->formID : 0, weightModel->bipedData->unk10[i].addon ? weightModel->bipedData->unk10[i].addon->formID : 0);
						TESForm* armor = weightModel->bipedData->unk10[i].armor;
						NiAVObject* node = weightModel->bipedData->unk10[i].object;
						if (armor && armor->formType == TESObjectARMO::kTypeID)
						{
							_MESSAGE("Armor: %s Shape: %s {%X}", static_cast<TESObjectARMO*>(armor)->fullName.GetName(), node ? node->m_name : "", node);
						}
					}
					for (int i = 0; i < 42; ++i)
					{
						_MESSAGE("Biped 2 Slot: %d Armor: %08X Arma: %08X", i, weightModel->bipedData->unk13C0[i].armor ? weightModel->bipedData->unk13C0[i].armor->formID : 0, weightModel->bipedData->unk13C0[i].addon ? weightModel->bipedData->unk13C0[i].addon->formID : 0);
						TESForm* armor = weightModel->bipedData->unk13C0[i].armor;
						NiAVObject* node = weightModel->bipedData->unk13C0[i].object;
						if (armor && armor->formType == TESObjectARMO::kTypeID)
						{
							_MESSAGE("Armor: %s Shape: %s {%X}", static_cast<TESObjectARMO*>(armor)->fullName.GetName(), node ? node->m_name : "", node);
						}
					}
				}
			}
			return true;
		}
		return false;
	});
	RegisterCommand("help", "Displays all the registered commands and their description", [](TESObjectREFR* thisObj, const char* argument) -> bool
	{
		if (argument == nullptr || argument[0] == 0)
		{
			std::scoped_lock<std::mutex> locker(g_commandInterface.m_lock);
			for (auto& cmdItem : g_commandInterface.m_commandMap)
			{
				if (_stricmp(cmdItem.first.c_str(), "help") == 0)
				{
					continue;
				}

				for (auto& cmd : cmdItem.second)
				{
					Console_Print("\t%s %s", cmdItem.first.c_str(), cmd.desc.c_str());
				}
			}
			return true;
		}

		return false;
	});
}