#include "common/IFileStream.h"

#include "skse/PluginAPI.h"
#include "skse/skse_version.h"

#include "ScaleformFunctions.h"
#include "MorphHandler.h"
#include "PartHandler.h"

#include "skse/GameAPI.h"
#include "skse/GameData.h"
#include "skse/GameObjects.h"
#include "skse/GameRTTI.h"
#include "skse/GameStreams.h"
#include "skse/GameMenus.h"

#include "NifUtils.h"

#include "skse/NiRTTI.h"
#include "skse/NiObjects.h"
#include "skse/NiNodes.h"
#include "skse/NiGeometry.h"
#include "skse/NiSerialization.h"

#include "skse/ScaleformMovie.h"
#include "skse/ScaleformLoader.h"

#include "interfaces/OverrideVariant.h"
#include "interfaces/OverrideInterface.h"
#include "interfaces/OverlayInterface.h"
#include "interfaces/NiTransformInterface.h"
#include "interfaces/BodyMorphInterface.h"

extern OverrideInterface	* g_overrideInterface;
extern NiTransformInterface	* g_transformInterface;
extern OverlayInterface		* g_overlayInterface;
extern BodyMorphInterface	* g_bodyMorphInterface;

extern MorphHandler g_morphHandler;
extern PartSet	g_partSet;

extern SKSETaskInterface * g_task;

#include "CDXCamera.h"
#include "skse/NiRenderer.h"

#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXBrush.h"
#include "CDXUndo.h"
#include "CDXNifCommands.h"

#include "skse/NiExtraData.h"

UInt32 colors[] = {
	0xffffff, 0xff0000, 0x0000ff, 0x00ff00,
	0xff00ff, 0xffff00, 0x00ffff, 0x79f2f2,
	0xe58473, 0xe673da, 0x57d936, 0xcc3d00,
	0x5233cc, 0xcc9466, 0xbf001d, 0xb8bf30,
	0x8c007e, 0x466d8c, 0x287300, 0x397359,
	0x453973, 0x662e00, 0x050066, 0x665e1a,
	0x663342, 0x59332d, 0x4c000b, 0x40103b,
	0x33240d, 0x20330d, 0x0d1633, 0x1a332f
};

extern CDXModelViewerCamera			g_Camera;
extern CDXNifScene					g_World;
extern float						g_panSpeed;
extern float						g_cameraFOV;

void RegisterNumber(GFxValue * dst, const char * name, double value)
{
	GFxValue	fxValue;
	fxValue.SetNumber(value);
	dst->SetMember(name, &fxValue);
}

void RegisterBool(GFxValue * dst, const char * name, bool value)
{
	GFxValue	fxValue;
	fxValue.SetBool(value);
	dst->SetMember(name, &fxValue);
}

void RegisterUnmanagedString(GFxValue * dst, const char * name, const char * str)
{
	GFxValue	fxValue;
	fxValue.SetString(str);
	dst->SetMember(name, &fxValue);
}

void RegisterString(GFxValue * dst,  GFxMovieView * view, const char * name, const char * str)
{
	GFxValue	fxValue;
	view->CreateString(&fxValue, str);
	dst->SetMember(name, &fxValue);
}

void SKSEScaleform_SavePreset::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Bool);

	bool saveJson = args->args[1].GetBool();
	const char	* strData = args->args[0].GetString();

	if (saveJson)
		args->result->SetBool(g_morphHandler.SaveJsonPreset(strData));
	else
		args->result->SetBool(g_morphHandler.SaveBinaryPreset(strData));
}

void SKSEScaleform_LoadPreset::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);
	ASSERT(args->args[2].GetType() == GFxValue::kType_Bool);

	bool loadJson = args->args[2].GetBool();

	const char	* strData = args->args[0].GetString();

	GFxValue	* object = NULL;
	if (args->numArgs >= 2)
		object = &args->args[1];

	auto presetData = std::make_shared<PresetData>();
	bool loadError = loadJson ? g_morphHandler.LoadJsonPreset(strData, presetData) : g_morphHandler.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, args->movie, object);
	if (!loadError) {
		g_morphHandler.ApplyPresetData(*g_thePlayer, presetData);

		RegisterNumber(object, "hairColor", presetData->hairColor);

		GFxValue tintArray;
		args->movie->CreateArray(&tintArray);

		for(auto & tint : presetData->tints) {
			GFxValue tintObject;
			args->movie->CreateObject(&tintObject);
			RegisterNumber(&tintObject, "color", tint.color);
			RegisterNumber(&tintObject, "index", tint.index);
			RegisterString(&tintObject, args->movie, "texture", tint.name.data);
			tintArray.PushBack(&tintObject);
		}

		object->SetMember("tints", &tintArray);
	}

	args->result->SetBool(loadError);
}

const char * GetGameSettingString(const char * key)
{
	Setting	* setting = (*g_gameSettingCollection)->Get(key);
	if(setting && setting->GetType() == Setting::kType_String)
		return setting->data.s;

	return NULL;
}

void SKSEScaleform_ReadPreset::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);
	ASSERT(args->args[2].GetType() == GFxValue::kType_Bool);

	bool loadJson = args->args[2].GetBool();
	const char	* strData = args->args[0].GetString();
	

	GFxValue	* object = NULL;
	if (args->numArgs >= 2)
		object = &args->args[1];

	DataHandler * dataHandler = DataHandler::GetSingleton();
	auto presetData = std::make_shared<PresetData>();
	bool loadError = loadJson ? g_morphHandler.LoadJsonPreset(strData, presetData) : g_morphHandler.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, args->movie, object);
	if(!loadError) {
		PlayerCharacter * player = (*g_thePlayer);
		TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

		GFxValue modArray;
		args->movie->CreateArray(&modArray);
		for(std::vector<std::string>::iterator it = presetData->modList.begin(); it != presetData->modList.end(); ++it) {
			GFxValue modObject;
			args->movie->CreateObject(&modObject);
			RegisterString(&modObject, args->movie, "name", (*it).c_str());
			RegisterNumber(&modObject, "loadedIndex", dataHandler->GetModIndex((*it).c_str()));
			modArray.PushBack(&modObject);
		}
		object->SetMember("mods", &modArray);

		GFxValue partArray;
		args->movie->CreateArray(&partArray);
		for(std::vector<BGSHeadPart*>::iterator it = presetData->headParts.begin(); it != presetData->headParts.end(); ++it) {
			GFxValue partObject;
			args->movie->CreateString(&partObject, (*it)->partName.data);
			partArray.PushBack(&partObject);
		}
		object->SetMember("headParts", &partArray);

		GFxValue weightObject;
		args->movie->CreateObject(&weightObject);
		RegisterUnmanagedString(&weightObject, "name", GetGameSettingString("sRSMWeight"));
		RegisterNumber(&weightObject, "value", presetData->weight);
		object->SetMember("weight", &weightObject);

		GFxValue hairObject;
		args->movie->CreateObject(&hairObject);
		RegisterUnmanagedString(&hairObject, "name", GetGameSettingString("sRSMHairColorPresets"));
		RegisterNumber(&hairObject, "value", presetData->hairColor);
		object->SetMember("hair", &hairObject);

		GFxValue tintArray;
		args->movie->CreateArray(&tintArray);
		for(std::vector<PresetData::Tint>::iterator it = presetData->tints.begin(); it != presetData->tints.end(); ++it) {
			PresetData::Tint & tint = (*it);
			GFxValue tintObject;
			args->movie->CreateObject(&tintObject);
			RegisterNumber(&tintObject, "color", tint.color);
			RegisterNumber(&tintObject, "index", tint.index);
			RegisterString(&tintObject, args->movie, "texture", tint.name.data);
			tintArray.PushBack(&tintObject);
		}
		object->SetMember("tints", &tintArray);

		GFxValue morphArray;
		args->movie->CreateArray(&morphArray);

		const char * presetNames[FacePresetList::kNumPresets];
		presetNames[FacePresetList::kPreset_NoseType] = GetGameSettingString("sRSMNoseTypes");
		presetNames[FacePresetList::kPreset_BrowType] = GetGameSettingString("sRSMBrowTypes");
		presetNames[FacePresetList::kPreset_EyesType] = GetGameSettingString("sRSMEyeTypes");
		presetNames[FacePresetList::kPreset_LipType] = GetGameSettingString("sRSMMouthTypes");

		const char * morphNames[FaceMorphList::kNumMorphs];
		morphNames[FaceMorphList::kMorph_NoseShortLong] = GetGameSettingString("sRSMNoseLength");
		morphNames[FaceMorphList::kMorph_NoseDownUp] = GetGameSettingString("sRSMNoseHeight");
		morphNames[FaceMorphList::kMorph_JawUpDown] = GetGameSettingString("sRSMJawHeight");
		morphNames[FaceMorphList::kMorph_JawNarrowWide] = GetGameSettingString("sRSMJawWidth");
		morphNames[FaceMorphList::kMorph_JawBackForward] = GetGameSettingString("sRSMJawForward");
		morphNames[FaceMorphList::kMorph_CheeksDownUp] = GetGameSettingString("sRSMCheekboneHeight");
		morphNames[FaceMorphList::kMorph_CheeksInOut] = GetGameSettingString("sRSMCheekboneWidth");
		morphNames[FaceMorphList::kMorph_EyesMoveDownUp] = GetGameSettingString("sRSMEyeHeight");
		morphNames[FaceMorphList::kMorph_EyesMoveInOut] = GetGameSettingString("sRSMEyeDepth");
		morphNames[FaceMorphList::kMorph_BrowDownUp] = GetGameSettingString("sRSMBrowHeight");
		morphNames[FaceMorphList::kMorph_BrowInOut] = GetGameSettingString("sRSMBrowWidth");
		morphNames[FaceMorphList::kMorph_BrowBackForward] = GetGameSettingString("sRSMBrowForward");
		morphNames[FaceMorphList::kMorph_LipMoveDownUp] = GetGameSettingString("sRSMMouthHeight");
		morphNames[FaceMorphList::kMorph_LipMoveInOut] = GetGameSettingString("sRSMMouthForward");
		morphNames[FaceMorphList::kMorph_ChinThinWide] = GetGameSettingString("sRSMChinWidth");
		morphNames[FaceMorphList::kMorph_ChinMoveUpDown] = GetGameSettingString("sRSMChinLength");
		morphNames[FaceMorphList::kMorph_OverbiteUnderbite] = GetGameSettingString("sRSMChinForward");
		morphNames[FaceMorphList::kMorph_EyesBackForward] = GetGameSettingString("sRSMEyeDepth");
		morphNames[FaceMorphList::kMorph_Vampire] = NULL;

		UInt32 i = 0;
		for(std::vector<SInt32>::iterator it = presetData->presets.begin(); it != presetData->presets.end(); ++it) {
			GFxValue presetObject;
			args->movie->CreateObject(&presetObject);
			if(presetNames[i])
				RegisterUnmanagedString(&presetObject, "name", presetNames[i]);
			RegisterNumber(&presetObject, "value", *it);
			RegisterNumber(&presetObject, "type", 0);
			RegisterNumber(&presetObject, "index", i);
			morphArray.PushBack(&presetObject);
			i++;
		}

		i = 0;
		for(auto & it : presetData->morphs) {
			GFxValue presetObject;
			args->movie->CreateObject(&presetObject);
			if (i < FaceMorphList::kNumMorphs && morphNames[i])
				RegisterUnmanagedString(&presetObject, "name", morphNames[i]);
			RegisterNumber(&presetObject, "value", it);
			RegisterNumber(&presetObject, "type", 1);
			RegisterNumber(&presetObject, "index", i);
			morphArray.PushBack(&presetObject);
			i++;
		}

		i = 0;
		for(auto & it : presetData->customMorphs) {
			std::string morphName = "$";
			morphName.append(it.name.data);
			GFxValue customObject;
			args->movie->CreateObject(&customObject);
			RegisterString(&customObject, args->movie, "name", morphName.c_str());
			RegisterNumber(&customObject, "value", it.value);
			RegisterNumber(&customObject, "type", 2);
			RegisterNumber(&customObject, "index", i);
			morphArray.PushBack(&customObject);
			i++;
		}
		i = 0;
		for (auto & it : presetData->bodyMorphData) {
			GFxValue customObject;
			args->movie->CreateObject(&customObject);
			RegisterString(&customObject, args->movie, "name", it.first.data);

			float morphSum = 0;
			for (auto & keys : it.second)
				morphSum += keys.second;

			RegisterNumber(&customObject, "value", morphSum);
			RegisterNumber(&customObject, "type", 3);
			RegisterNumber(&customObject, "index", i);
			morphArray.PushBack(&customObject);
			i++;
		}
		object->SetMember("morphs", &morphArray);
	}

	args->result->SetBool(loadError);
}

void SKSEScaleform_ReloadSliders::Invoke(Args * args)
{
	MenuManager * mm = MenuManager::GetSingleton();
	if (mm) {
		BSFixedString t("RaceSex Menu");
		RaceSexMenu* raceMenu = (RaceSexMenu*)mm->GetMenu(&t);
		if(raceMenu) {
			PlayerCharacter * player = (*g_thePlayer);
			CALL_MEMBER_FN(raceMenu, LoadSliders)((UInt32)player->baseForm, 0);
			//CALL_MEMBER_FN(raceMenu, UpdatePlayer)();
			CALL_MEMBER_FN((*g_thePlayer), QueueNiNodeUpdate)(true);
		}
	}
}

void SKSEScaleform_GetSliderData::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	UInt32 sliderId = (UInt32)args->args[0].GetNumber();
	double value = args->args[1].GetNumber();

	MenuManager * mm = MenuManager::GetSingleton();
	if(mm)
	{
		BSFixedString t("RaceSex Menu");
		RaceSexMenu * raceMenu = (RaceSexMenu *)mm->GetMenu(&t);
		if(raceMenu)
		{
			RaceMenuSlider * slider = NULL;
			RaceSexMenu::RaceComponent * raceData = NULL;

			UInt8 gender = 0;
			PlayerCharacter * player = (*g_thePlayer);
			TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
			if(actorBase)
				gender = CALL_MEMBER_FN(actorBase, GetSex)();

			if(raceMenu->raceIndex < raceMenu->sliderData[gender].count)
				raceData = &raceMenu->sliderData[gender][raceMenu->raceIndex];
			if(raceData && sliderId < raceData->sliders.count)
				slider = &raceData->sliders[sliderId];

			if(raceData && slider)
			{
				args->movie->CreateObject(args->result);
				RegisterNumber(args->result, "type", slider->type);
				RegisterNumber(args->result, "index", slider->index);

				switch(slider->type)
				{
				case RaceMenuSlider::kTypeHeadPart:
					{
						if(slider->index < RaceSexMenu::kNumHeadPartLists)
						{
							BGSHeadPart * headPart = NULL;
							raceMenu->headParts[slider->index].GetNthItem((UInt32)value, headPart);
							if(headPart) {
								RegisterNumber(args->result, "formId", headPart->formID);
								RegisterString(args->result, args->movie, "partName", headPart->partName.data);
							}
						}
					}
					break;
				case RaceMenuSlider::kTypeDoubleMorph:
					{
						// Provide case for custom parts
						if(slider->index >= SLIDER_OFFSET) {
							UInt32 sliderIndex = slider->index - SLIDER_OFFSET;
							SliderInternalPtr sliderInternal = g_morphHandler.GetSliderByIndex(player->race, sliderIndex);
							if(sliderInternal) {
								RegisterNumber(args->result, "subType", sliderInternal->type);
								switch (sliderInternal->type)
								{
									// Only acquire part information for actual part sliders
									case SliderInternal::kTypeHeadPart:
									{
										UInt8 partType = sliderInternal->presetCount;
										HeadPartList * partList = g_partSet.GetPartList(partType);
										if (partList)
										{
											BGSHeadPart * targetPart = g_partSet.GetPartByIndex(partList, (UInt32)value - 1);
											if (targetPart) {
												RegisterNumber(args->result, "formId", targetPart->formID);
												RegisterString(args->result, args->movie, "partName", targetPart->partName.data);
											}
										}
									}
									break;
								}
							}
						}
					}
					break;
				}
			}
		}
	}
}


void SKSEScaleform_GetModName::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	UInt32 modIndex = (UInt32)args->args[0].GetNumber();

	DataHandler* pDataHandler = DataHandler::GetSingleton();
	ModInfo* modInfo = pDataHandler->modList.modInfoList.GetNthItem(modIndex);
	if(modInfo) {
		args->movie->CreateString(args->result, modInfo->name);
	}
}

void SKSEScaleform_ExportHead::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);

	const char	* strData = args->args[0].GetString();

	// Get the Editor's working actor
	Actor * actor = g_World.GetWorkingActor();
	if (!actor)
		return;

	std::string nifPath = strData;
	nifPath.append(".nif");
	std::string ddsPath = strData;
	ddsPath.append(".dds");

	g_task->AddTask(new SKSETaskExportHead(actor, nifPath.c_str(), ddsPath.c_str()));
}

void SKSEScaleform_ImportHead::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);

	const char	* strData = args->args[0].GetString();

	// Release the previous import just in case
	g_World.ReleaseImport();

	// Get the Editor's working actor
	Actor * actor = g_World.GetWorkingActor();
	if (!actor)
		return;

	BSFaceGenNiNode * faceNode = actor->GetFaceGenNiNode();
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (!actorBase || !faceNode)
		return;

	UInt8 niStreamMemory[0x5B4];
	memset(niStreamMemory, 0, 0x5B4);
	NiStream * niStream = (NiStream *)niStreamMemory;
	CALL_MEMBER_FN(niStream, ctor)();

	NiNode * rootNode = NULL;
	BSResourceNiBinaryStream binaryStream(strData);
	if (binaryStream.IsValid())
	{		
		niStream->LoadStream(&binaryStream);
		if (niStream->m_rootObjects.m_data)
		{
			if (niStream->m_rootObjects.m_data[0]) // Get the root node
				rootNode = niStream->m_rootObjects.m_data[0]->GetAsNiNode();
			if (rootNode)
			{
				args->movie->CreateArray(args->result);
				/*args->movie->CreateObject(args->result);

				GFxValue source;
				args->movie->CreateArray(&source);

				

				GFxValue destination;
				args->movie->CreateArray(&destination);

				UInt32 numParts = actorBase->numHeadParts;
				BGSHeadPart ** headParts = actorBase->headparts;
				if (CALL_MEMBER_FN(actorBase, HasOverlays)()) {
					numParts = GetNumActorBaseOverlays(actorBase);
					headParts = GetActorBaseOverlays(actorBase);
				}

				for (UInt32 i = 0; i < numParts; i++)
				{
					BGSHeadPart * headPart = headParts[i];
					if (!headPart)
						continue;

					NiTriBasedGeom * geometry = GetTriBasedGeomByHeadPart(faceNode, headPart);
					if (!geometry)
						continue;
					
					NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
					if (!geometryData)
						continue;

					bool morphable = geometry->GetExtraData("FOD") != NULL;

					GFxValue gfxPart;
					args->movie->CreateObject(&gfxPart);
					RegisterString(&gfxPart, args->movie, "name", geometry->m_name);
					RegisterNumber(&gfxPart, "vertices", geometryData->m_usVertices);
					RegisterBool(&gfxPart, "morphable", morphable);
					destination.PushBack(&gfxPart);
				}

				*/

				SInt32 gIndex = 0;
				VisitObjects(rootNode, [&gIndex, &args](NiAVObject* trishape)
				{
					NiNode * parent = trishape->m_parent;
					if (parent && BSFixedString(parent->m_name) == BSFixedString("BSFaceGenNiNodeSkinned")) {
						NiTriBasedGeom * geometry = trishape->GetAsNiTriBasedGeom();
						if (!geometry)
							return false;

						NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
						if (!geometryData)
							return false;

						GFxValue gfxGeom;
						args->movie->CreateObject(&gfxGeom);
						RegisterString(&gfxGeom, args->movie, "name", geometry->m_name);
						RegisterNumber(&gfxGeom, "vertices", geometryData->m_usVertices);
						RegisterNumber(&gfxGeom, "gIndex", gIndex);
						args->result->PushBack(&gfxGeom);
						gIndex++;
					}

					return false;
				});

				//args->result->SetMember("source", &source);
				//args->result->SetMember("destination", &destination);
			}
		}
	}

	// Add the Root node to the Editor
	if (rootNode) {
		rootNode->IncRef();
		g_World.SetImportRoot(rootNode);
	}

	// Release the created NiStream
	CALL_MEMBER_FN(niStream, dtor)();
}

void SKSEScaleform_ReleaseImportedHead::Invoke(Args * args)
{
	g_World.ReleaseImport();
}

void SKSEScaleform_LoadImportedHead::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Array);

	UInt32 meshLength = min(g_World.GetNumMeshes(), args->args[0].GetArraySize());

	for (UInt32 i = 0; i < meshLength; i++)
	{
		CDXNifMesh * mesh = static_cast<CDXNifMesh*>(g_World.GetNthMesh(i));
		if (mesh) {
			NiNode * importRoot = g_World.GetImportRoot();
			if (importRoot) {

				SInt32 searchIndex = -1;

				GFxValue gfxIndex;
				args->args[0].GetElement(i, &gfxIndex);
				searchIndex = gfxIndex.GetNumber();

				NiGeometry * sourceGeometry = NULL;
				SInt32 gIndex = 0;
				VisitObjects(importRoot, [&gIndex, &args, &searchIndex, &sourceGeometry](NiAVObject* trishape)
				{
					NiNode * parent = trishape->m_parent;
					if (parent && BSFixedString(parent->m_name) == BSFixedString("BSFaceGenNiNodeSkinned")) {
						NiTriBasedGeom * geometry = trishape->GetAsNiTriBasedGeom();
						if (!geometry)
							return false;

						NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
						if (!geometryData)
							return false;

						if (searchIndex == gIndex) {
							sourceGeometry = geometry;
							return true;
						}
						
						gIndex++;
					}

					return false;
				});

				if (sourceGeometry) {
					std::shared_ptr<CDXNifImportGeometry> importGeometry = std::make_shared<CDXNifImportGeometry>(mesh, sourceGeometry);
					if (importGeometry->Length() > 0)
						importGeometry->Apply(g_undoStack.Push(importGeometry));
				}
			}
		}
	}
}


void SKSEScaleform_ClearSculptData::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Array);

	UInt32 meshLength = args->args[0].GetArraySize();

	for (UInt32 i = 0; i < meshLength; i++)
	{
		GFxValue gfxIndex;
		args->args[0].GetElement(i, &gfxIndex);
		SInt32 meshIndex = gfxIndex.GetNumber();

		CDXNifMesh * mesh = static_cast<CDXNifMesh*>(g_World.GetNthMesh(meshIndex));
		if (mesh) {	
			std::shared_ptr<CDXNifResetSculpt> resetGeometry = std::make_shared<CDXNifResetSculpt>(mesh);
			if (resetGeometry->Length() > 0)
				resetGeometry->Apply(g_undoStack.Push(resetGeometry));
		}
	}
}


void SKSEScaleform_GetHeadParts::Invoke(Args * args)
{
	args->movie->CreateObject(args->result);

	GFxValue partList;
	args->movie->CreateArray(&partList);

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	for(UInt32 i = 0; i < actorBase->numHeadParts; i++)
	{
		GFxValue partData;
		args->movie->CreateObject(&partData);

		BGSHeadPart * headPart = actorBase->headparts[i];

		GFxValue headPartData;
		args->movie->CreateObject(&headPartData);
		RegisterString(&headPartData, args->movie, "partName", headPart->partName.data);
		RegisterNumber(&headPartData, "partFlags", headPart->partFlags);
		RegisterNumber(&headPartData, "partType", headPart->type);
		RegisterString(&headPartData, args->movie, "modelPath", headPart->model.GetModelName());
		RegisterString(&headPartData, args->movie, "chargenMorphPath", headPart->chargenMorph.GetModelName());
		RegisterString(&headPartData, args->movie, "raceMorphPath", headPart->raceMorph.GetModelName());
		partData.SetMember("base", &headPartData);

		// Get the overlay, if there is one
		if(CALL_MEMBER_FN(actorBase, HasOverlays)()) {
			BGSHeadPart * overlayPart = actorBase->GetHeadPartOverlayByType(headPart->type);
			if(overlayPart) {
				GFxValue overlayPartData;
				args->movie->CreateObject(&overlayPartData);
				RegisterString(&overlayPartData, args->movie, "partName", overlayPart->partName.data);
				RegisterNumber(&overlayPartData, "partFlags", overlayPart->partFlags);
				RegisterNumber(&overlayPartData, "partType", overlayPart->type);
				RegisterString(&overlayPartData, args->movie, "modelPath", overlayPart->model.GetModelName());
				RegisterString(&overlayPartData, args->movie, "chargenMorphPath", overlayPart->chargenMorph.GetModelName());
				RegisterString(&overlayPartData, args->movie, "raceMorphPath", overlayPart->raceMorph.GetModelName());
				partData.SetMember("overlay", &overlayPartData);
			}
		}

		partList.PushBack(&partData);
	}

	args->result->SetMember("parts", &partList);
}

void SKSEScaleform_GetPlayerPosition::Invoke(Args * args)
{
	PlayerCharacter * player = (*g_thePlayer);
	NiNode * root = player->GetNiRootNode(0);
	if(root) {
		args->movie->CreateObject(args->result);
		GFxValue x;
		x.SetNumber(root->m_localTransform.pos.x);
		args->result->SetMember("x", &x);
		GFxValue y;
		y.SetNumber(root->m_localTransform.pos.y);
		args->result->SetMember("y", &y);
		GFxValue z;
		z.SetNumber(root->m_localTransform.pos.z);
		args->result->SetMember("z", &z);
	}
}

void SKSEScaleform_GetPlayerRotation::Invoke(Args * args)
{
	PlayerCharacter * player = (*g_thePlayer);
	NiNode * root = player->GetNiRootNode(0);

	args->movie->CreateArray(args->result);
	for(UInt32 i = 0; i < 3 * 3; i++)
	{
		GFxValue index;
		index.SetNumber(((float*)(root->m_localTransform.rot.data))[i]);
		args->result->PushBack(&index);
	}
}

void SKSEScaleform_SetPlayerRotation::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Array);
	ASSERT(args->args[0].GetArraySize() == 9);

	PlayerCharacter * player = (*g_thePlayer);
	NiNode * root = player->GetNiRootNode(0);

	for(UInt32 i = 0; i < 3 * 3; i++)
	{
		GFxValue val;
		args->args[0].GetElement(i, &val);
		if(val.GetType() != GFxValue::kType_Number)
			break;

		((float*)root->m_localTransform.rot.data)[i] = val.GetNumber();
	}

	NiAVObject::ControllerUpdateContext ctx;
	root->UpdateWorldData(&ctx);
}

void SKSEScaleform_GetRaceSexCameraRot::Invoke(Args * args)
{
	RaceSexMenu * raceMenu = DYNAMIC_CAST(MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu), IMenu, RaceSexMenu);
	if(raceMenu) {
		NiNode * raceCamera = raceMenu->camera.cameraNode;
		args->movie->CreateArray(args->result);
		for(UInt32 i = 0; i < 3 * 3; i++)
		{
			GFxValue index;
			index.SetNumber(((float*)raceCamera->m_localTransform.rot.data)[i]);
			args->result->PushBack(&index);
		}
	}
}

void SKSEScaleform_GetRaceSexCameraPos::Invoke(Args * args)
{
	RaceSexMenu * raceMenu = DYNAMIC_CAST(MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu), IMenu, RaceSexMenu);
	if(raceMenu) {
		NiNode * raceCamera = raceMenu->camera.cameraNode;
		args->movie->CreateObject(args->result);
		GFxValue x;
		x.SetNumber(raceCamera->m_localTransform.pos.x);
		args->result->SetMember("x", &x);
		GFxValue y;
		y.SetNumber(raceCamera->m_localTransform.pos.y);
		args->result->SetMember("y", &y);
		GFxValue z;
		z.SetNumber(raceCamera->m_localTransform.pos.z);
		args->result->SetMember("z", &z);
	}
}

void SKSEScaleform_SetRaceSexCameraPos::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Object);

	RaceSexMenu * raceMenu = DYNAMIC_CAST(MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu), IMenu, RaceSexMenu);
	if(raceMenu) {
		NiNode * raceCamera = raceMenu->camera.cameraNode;

		GFxValue val;
		args->args[0].GetMember("x", &val);
		if(val.GetType() == GFxValue::kType_Number)
			raceCamera->m_localTransform.pos.x = val.GetNumber();

		args->args[0].GetMember("y", &val);
		if(val.GetType() == GFxValue::kType_Number)
			raceCamera->m_localTransform.pos.y = val.GetNumber();

		args->args[0].GetMember("z", &val);
		if(val.GetType() == GFxValue::kType_Number)
			raceCamera->m_localTransform.pos.z = val.GetNumber();

		NiAVObject::ControllerUpdateContext ctx;
		raceCamera->UpdateWorldData(&ctx);
	}
}

void SKSEScaleform_CreateMorphEditor::Invoke(Args * args)
{
	LPDIRECT3DDEVICE9 pDevice = NiDX9Renderer::GetSingleton()->m_pkD3DDevice9;
	if (!pDevice) {
		_ERROR("%s - Failed to acquire DirectX device.", __FUNCTION__);
		return;
	}

	PlayerCharacter * player = (*g_thePlayer);
	g_Camera.SetProjParams(g_cameraFOV * (D3DX_PI / 180.0f), 1.0f, 1.0f, 1000.0f);
	g_Camera.SetPanSpeed(g_panSpeed);
	g_World.SetWorkingActor(player);
	g_World.Setup(pDevice);

	Actor * actor = g_World.GetWorkingActor();
	if (!actor) {
		_ERROR("%s - Invalid working actor.", __FUNCTION__);
		return;
	}

	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (!actorBase) {
		_ERROR("%s - No actor base.", __FUNCTION__);
		return;
	}

	NiNode * rootFaceGen = actor->GetFaceGenNiNode();
	if (!rootFaceGen) {
		_ERROR("%s - No FaceGen node.", __FUNCTION__);
		return;
	}

	BSFaceGenAnimationData * animationData = actor->GetFaceGenAnimationData();
	if (animationData) {
		FaceGen::GetSingleton()->isReset = 0;
		for (UInt32 t = BSFaceGenAnimationData::kKeyframeType_Expression; t <= BSFaceGenAnimationData::kKeyframeType_Phoneme; t++)
		{
			BSFaceGenKeyframeMultiple * keyframe = &animationData->keyFrames[t];
			for (UInt32 i = 0; i < keyframe->count; i++)
				keyframe->values[i] = 0.0;
			keyframe->isUpdated = 0;
		}
		UpdateModelFace(rootFaceGen);
	}

	UInt32 numHeadParts = actorBase->numHeadParts;
	BGSHeadPart ** headParts = actorBase->headparts;
	if (CALL_MEMBER_FN(actorBase, HasOverlays)()) {
		numHeadParts = GetNumActorBaseOverlays(actorBase);
		headParts = GetActorBaseOverlays(actorBase);
	}

	// What??
	if (!headParts) {
		_ERROR("%s - No head parts found.", __FUNCTION__);
		return;
	}

	for(UInt32 i = 0; i < rootFaceGen->m_children.m_emptyRunStart; i++)
	{
		std::set<BGSHeadPart*> extraParts; // Collect extra hair parts
		BGSHeadPart * hairPart = actorBase->GetCurrentHeadPartByType(BGSHeadPart::kTypeHair);
		if(hairPart) {
			BGSHeadPart * extraPart = NULL;
			for(UInt32 p = 0; p < hairPart->extraParts.count; p++) {
				if(hairPart->extraParts.GetNthItem(p, extraPart))
					extraParts.insert(extraPart);
			}
		}

		for(UInt32 h = 0; h < actorBase->numHeadParts; h++) {
			BGSHeadPart * headPart = headParts[h];
			if (!headPart)
				continue;

			NiAVObject * object = rootFaceGen->m_children.m_data[i];
			if (!object)
				continue;

			if(headPart->partName == BSFixedString(object->m_name)) {
				CDXNifMesh * mesh = CDXNifMesh::Create(pDevice, object->GetAsNiGeometry());
				if (!mesh)
					continue;

				if (extraParts.find(headPart) != extraParts.end()) // Is one of the hair parts toggle visibility
					mesh->SetVisible(false);
					
				CDXMaterial * material = mesh->GetMaterial();
				if (material)
					material->SetWireframeColor(CDXVec3(((colors[i] >> 16) & 0xFF) / 255.0, ((colors[i] >> 8) & 0xFF) / 255.0, (colors[i] & 0xFF) / 255.0));

				if (headPart->type != BGSHeadPart::kTypeFace)
					mesh->SetLocked(true);

				g_World.AddMesh(mesh);
				break;
			}
		}
	}

	if (animationData) {
		animationData->overrideFlag = 0;
		CALL_MEMBER_FN(animationData, Reset)(1.0, 1, 1, 0, 0);
		FaceGen::GetSingleton()->isReset = 1;
		UpdateModelFace(rootFaceGen);
	}

	args->movie->CreateObject(args->result);
	RegisterNumber(args->result, "width", g_World.GetWidth());
	RegisterNumber(args->result, "height", g_World.GetHeight());
}

void SKSEScaleform_ReleaseMorphEditor::Invoke(Args * args)
{
	g_World.Release();
}

void SKSEScaleform_BeginRotateMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	g_Camera.OnRotateBegin(args->args[0].GetNumber(), args->args[1].GetNumber());
};

void SKSEScaleform_DoRotateMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	g_Camera.OnRotate(args->args[0].GetNumber(), args->args[1].GetNumber());
};

void SKSEScaleform_EndRotateMesh::Invoke(Args * args)
{
	g_Camera.OnRotateEnd();
};

void SKSEScaleform_BeginPanMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	g_Camera.OnMoveBegin(args->args[0].GetNumber(), args->args[1].GetNumber());
};

void SKSEScaleform_DoPanMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	g_Camera.OnMove(args->args[0].GetNumber(), args->args[1].GetNumber());
};

void SKSEScaleform_EndPanMesh::Invoke(Args * args)
{
	g_Camera.OnMoveEnd();
};

void SKSEScaleform_BeginPaintMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	SInt32 x = args->args[0].GetNumber();
	SInt32 y = args->args[1].GetNumber();

	bool hitMesh = false;
	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushPickerBegin brushStroke(brush);
		brushStroke.SetMirror(brush->IsMirror());
		if (g_World.Pick(x, y, brushStroke))
			hitMesh = true;
	}

	args->result->SetBool(hitMesh);
};

void SKSEScaleform_DoPaintMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	SInt32 x = args->args[0].GetNumber();
	SInt32 y = args->args[1].GetNumber();

	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushPickerUpdate brushStroke(brush);
		brushStroke.SetMirror(brush->IsMirror());
		g_World.Pick(x, y, brushStroke);
	}
};

void SKSEScaleform_EndPaintMesh::Invoke(Args * args)
{
	CDXBrush * brush = g_World.GetCurrentBrush();
	if(brush)
		brush->EndStroke();
};


void SKSEScaleform_GetCurrentBrush::Invoke(Args * args)
{
	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush)
		args->result->SetNumber(brush->GetType());
	else
		args->result->SetNull();
}

void SKSEScaleform_SetCurrentBrush::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	CDXBrush::BrushType brushType = (CDXBrush::BrushType)(UInt32)args->args[0].GetNumber();
	CDXBrush * brush = g_World.GetBrush(brushType);
	if (brush)
		g_World.SetCurrentBrush(brushType);

	args->result->SetBool(brush != NULL);
}

void SKSEScaleform_GetBrushes::Invoke(Args * args)
{
	args->movie->CreateArray(args->result);
	for (auto brush : g_World.GetBrushes()) {
		GFxValue object;
		args->movie->CreateObject(&object);
		RegisterNumber(&object, "type", brush->GetType());
		RegisterNumber(&object, "radius", brush->GetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Value));
		RegisterNumber(&object, "radiusMin", brush->GetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Min));
		RegisterNumber(&object, "radiusMax", brush->GetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Max));
		RegisterNumber(&object, "radiusInterval", brush->GetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Interval));
		RegisterNumber(&object, "strength", brush->GetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Value));
		RegisterNumber(&object, "strengthMin", brush->GetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Min));
		RegisterNumber(&object, "strengthMax", brush->GetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Max));
		RegisterNumber(&object, "strengthInterval", brush->GetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Interval));
		RegisterNumber(&object, "falloff", brush->GetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Value));
		RegisterNumber(&object, "falloffMin", brush->GetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Min));
		RegisterNumber(&object, "falloffMax", brush->GetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Max));
		RegisterNumber(&object, "falloffInterval", brush->GetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Interval));
		RegisterNumber(&object, "mirror", brush->IsMirror() ? 1.0 : 0.0);
		args->result->PushBack(&object);
	}
}

void SKSEScaleform_SetBrushData::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);

	CDXBrush::BrushType brushType = (CDXBrush::BrushType)(UInt32)args->args[0].GetNumber();
	CDXBrush * brush = g_World.GetBrush(brushType);
	if (brush) {
		GFxValue radius;
		args->args[1].GetMember("radius", &radius);
		GFxValue strength;
		args->args[1].GetMember("strength", &strength);
		GFxValue falloff;
		args->args[1].GetMember("falloff", &falloff);
		GFxValue mirror;
		args->args[1].GetMember("mirror", &mirror);

		brush->SetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Value, radius.GetNumber());
		brush->SetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Value, strength.GetNumber());
		brush->SetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Value, falloff.GetNumber());
		brush->SetMirror(mirror.GetNumber() > 0.0 ? true : false);

		args->result->SetBool(true);
	}
	else {
		args->result->SetBool(false);
	}
}

void SKSEScaleform_GetMeshes::Invoke(Args * args)
{
	args->movie->CreateArray(args->result);
	for (UInt32 i = 0; i < g_World.GetNumMeshes(); i++) {
		CDXNifMesh * mesh = static_cast<CDXNifMesh*>(g_World.GetNthMesh(i));
		if (mesh) {
			NiGeometry * geometry = mesh->GetNifGeometry();
			if (geometry) {
				GFxValue object;
				args->movie->CreateObject(&object);
				RegisterString(&object, args->movie, "name", geometry->m_name);
				RegisterNumber(&object, "meshIndex", i);
				RegisterBool(&object, "wireframe", mesh->ShowWireframe());
				RegisterBool(&object, "locked", mesh->IsLocked());
				RegisterBool(&object, "visible", mesh->IsVisible());
				RegisterBool(&object, "morphable", mesh->IsMorphable());
				RegisterNumber(&object, "vertices", mesh->GetVertexCount());

				CDXMaterial * material = mesh->GetMaterial();
				if (material) {
					CDXVec3 fColor = material->GetWireframeColor();
					UInt32 color = 0xFF000000 | (UInt32)(fColor.x * 255) << 16 | (UInt32)(fColor.y * 255) << 8 | (UInt32)(fColor.z * 255);
					RegisterNumber(&object, "wfColor", color);
				}

				args->result->PushBack(&object);
			}
		}
	}
}

void SKSEScaleform_SetMeshData::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);

	UInt32 i = (UInt32)args->args[0].GetNumber();
	CDXNifMesh * mesh = static_cast<CDXNifMesh*>(g_World.GetNthMesh(i));
	if (mesh) {
		GFxValue wireframe;
		args->args[1].GetMember("wireframe", &wireframe);
		GFxValue locked;
		args->args[1].GetMember("locked", &locked);
		GFxValue visible;
		args->args[1].GetMember("visible", &visible);
		GFxValue wfColor;
		args->args[1].GetMember("wfColor", &wfColor);

		mesh->SetLocked(locked.GetBool());
		mesh->SetShowWireframe(wireframe.GetBool());
		mesh->SetVisible(visible.GetBool());
		CDXMaterial * material = mesh->GetMaterial();
		if (material) {
			UInt32 color = wfColor.GetNumber();
			material->SetWireframeColor(CDXVec3(((color >> 16) & 0xFF) / 255.0, ((color >> 8) & 0xFF) / 255.0, (color & 0xFF) / 255.0));
		}

		args->result->SetBool(true);
	}
	else {
		args->result->SetBool(false);
	}
}

void SKSEScaleform_GetActionLimit::Invoke(Args * args)
{
	args->result->SetNumber(g_undoStack.GetLimit());
}

void SKSEScaleform_UndoAction::Invoke(Args * args)
{
	args->result->SetNumber(g_undoStack.Undo(true));
}

void SKSEScaleform_RedoAction::Invoke(Args * args)
{
	args->result->SetNumber(g_undoStack.Redo(true));
}

void SKSEScaleform_GoToAction::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	SInt32 previous = g_undoStack.GetIndex();
	SInt32 result = g_undoStack.GoTo(args->args[0].GetNumber(), true);

	if (result != previous) {
		Actor * actor = g_World.GetWorkingActor();
		if (actor) {
			NiNode * rootFaceGen = actor->GetFaceGenNiNode();
			UpdateModelFace(rootFaceGen);
		}
	}

	args->result->SetNumber(result);
}

void SKSEScaleform_GetMeshCameraRadius::Invoke(Args * args)
{
	args->result->SetNumber(g_Camera.GetRadius());
}

void SKSEScaleform_SetMeshCameraRadius::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	g_Camera.SetRadius(args->args[0].GetNumber());
	g_Camera.Update();
}

#include <Shlwapi.h>

void ReadFileDirectory(const char * lpFolder, const char ** lpFilePattern, UInt32 numPatterns, std::function<void(char*, WIN32_FIND_DATA &, bool)> file)
{
	char szFullPattern[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFindFile;
	// first we are going to process any subdirectories
	PathCombine(szFullPattern, lpFolder, "*");
	hFindFile = FindFirstFile(szFullPattern, &FindFileData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// found a subdirectory; recurse into it
				PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
				if (FindFileData.cFileName[0] == '.')
					continue;

				file(szFullPattern, FindFileData, true);
			}
		} while (FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
	// now we are going to look for the matching files
	for (UInt32 i = 0; i < numPatterns; i++)
	{
		PathCombine(szFullPattern, lpFolder, lpFilePattern[i]);
		hFindFile = FindFirstFile(szFullPattern, &FindFileData);
		if (hFindFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					// found a file; do something with it
					PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
					file(szFullPattern, FindFileData, false);
				}
			} while (FindNextFile(hFindFile, &FindFileData));
			FindClose(hFindFile);
		}
	}
}

void SKSEScaleform_GetExternalFiles::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Array);

	const char * path = args->args[0].GetString();
	
	UInt32 numPatterns = args->args[1].GetArraySize();

	const char ** patterns = (const char **)ScaleformHeap_Allocate(numPatterns * sizeof(const char*));
	for (UInt32 i = 0; i < numPatterns; i++) {
		GFxValue str;
		args->args[1].GetElement(i, &str);
		patterns[i] = str.GetString();
	}

	args->movie->CreateArray(args->result);

	ReadFileDirectory(path, patterns, numPatterns, [args](char* dirPath, WIN32_FIND_DATA & fileData, bool dir)
	{
		GFxValue fileInfo;
		args->movie->CreateObject(&fileInfo);
		RegisterString(&fileInfo, args->movie, "path", dirPath);
		RegisterString(&fileInfo, args->movie, "name", fileData.cFileName);
		UInt64 fileSize = (UInt64)fileData.nFileSizeHigh << 32 | fileData.nFileSizeLow;
		RegisterNumber(&fileInfo, "size", fileSize);
		SYSTEMTIME sysTime;
		FileTimeToSystemTime(&fileData.ftLastWriteTime, &sysTime);
		GFxValue date;
		GFxValue params[7];
		params[0].SetNumber(sysTime.wYear);
		params[1].SetNumber(sysTime.wMonth - 1); // Flash Month is 0-11, System time is 1-12
		params[2].SetNumber(sysTime.wDay);
		params[3].SetNumber(sysTime.wHour);
		params[4].SetNumber(sysTime.wMinute);
		params[5].SetNumber(sysTime.wSecond);
		params[6].SetNumber(sysTime.wMilliseconds);
		args->movie->CreateObject(&date, "Date", params, 7);
		fileInfo.SetMember("lastModified", &date);
		RegisterBool(&fileInfo, "directory", dir);
		args->result->PushBack(&fileInfo);
	});

	ScaleformHeap_Free(patterns);
}

