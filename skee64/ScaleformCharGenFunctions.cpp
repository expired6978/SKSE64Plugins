#include "common/IFileStream.h"

#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"

#include "ScaleformCharGenFunctions.h"
#include "ScaleformUtils.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"

#include "skse64/GameAPI.h"
#include "skse64/GameData.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameStreams.h"
#include "skse64/GameMenus.h"

#include "NifUtils.h"

#include "skse64/NiRTTI.h"
#include "skse64/NiObjects.h"
#include "skse64/NiNodes.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiSerialization.h"

#include "skse64/ScaleformMovie.h"
#include "skse64/ScaleformLoader.h"

#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "PresetInterface.h"
#include "FormTagInterface.h"

#include "ScaleformUtils.h"

extern FaceMorphInterface	g_morphInterface;
extern PresetInterface		g_presetInterface;
extern FormTagInterface		g_formTagInterface;
extern PartSet	g_partSet;

extern SKSETaskInterface * g_task;

#include "skse64/NiRenderer.h"
#include "skse64/NiExtraData.h"

#include <DirectXMath.h>
#include <d3d11_4.h>

#include "CDXShader.h"
#include "CDXD3DDevice.h"
#include "CDXCamera.h"
#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXBrush.h"
#include "CDXBrushMesh.h"
#include "CDXUndo.h"
#include "CDXNifCommands.h"
#include "CDXShaderFactory.h"
#include "CDXBSShaderResource.h"

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

extern CDXD3DDevice*				g_Device;
extern CDXModelViewerCamera			g_Camera;
extern CDXNifScene					g_World;
extern float						g_panSpeed;
extern float						g_cameraFOV;
extern SInt32						g_viewWidth;
extern SInt32						g_viewHeight;
extern bool							g_enableHeadExport;

extern float	g_sculptOffsetX;
extern float	g_sculptOffsetY;
extern float	g_sculptOffsetZ;

void SKSEScaleform_SavePreset::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_String);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Bool);

	bool saveJson = args->args[1].GetBool();
	const char	* strData = args->args[0].GetString();

	if (saveJson)
		args->result->SetBool(g_presetInterface.SaveJsonPreset(strData, (*g_thePlayer)));
	else
		args->result->SetBool(g_presetInterface.SaveBinaryPreset(strData));
}

void SKSEScaleform_LoadPreset::Invoke(Args * args)
{
	using namespace ScaleformUtils;

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
	bool loadError = loadJson ? g_presetInterface.LoadJsonPreset(strData, presetData) : g_presetInterface.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, args->movie, object);
	if (!loadError) {
		g_presetInterface.ApplyPresetData(*g_thePlayer, presetData);

		RegisterNumber(object, "hairColor", presetData->hairColor);

		GFxValue tintArray;
		args->movie->CreateArray(&tintArray);

		for(auto & tint : presetData->tints) {
			GFxValue tintObject;
			args->movie->CreateObject(&tintObject);
			RegisterNumber(&tintObject, "color", tint.color);
			RegisterNumber(&tintObject, "index", tint.index);
			RegisterString(&tintObject, args->movie, "texture", tint.name.c_str());
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
	using namespace ScaleformUtils;

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
	bool loadError = loadJson ? g_presetInterface.LoadJsonPreset(strData, presetData) : g_presetInterface.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, args->movie, object);
	if(!loadError) {
		PlayerCharacter * player = (*g_thePlayer);
		TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

		GFxValue modArray;
		args->movie->CreateArray(&modArray);
		for(std::vector<std::string>::iterator it = presetData->modList.begin(); it != presetData->modList.end(); ++it) {

			const ModInfo * modInfo = dataHandler->LookupModByName((*it).c_str());
			if (!modInfo || !modInfo->IsActive())
				continue;

			GFxValue modObject;
			args->movie->CreateObject(&modObject);
			RegisterString(&modObject, args->movie, "name", (*it).c_str());
			RegisterNumber(&modObject, "loadedIndex", modInfo->GetPartialIndex());
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
			RegisterString(&tintObject, args->movie, "texture", tint.name.c_str());
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
			morphName.append(it.name.c_str());
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
			RegisterString(&customObject, args->movie, "name", it.first.c_str());

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
			CALL_MEMBER_FN(raceMenu, LoadSliders)((UInt64)player->baseForm, 0);
			//CALL_MEMBER_FN(raceMenu, UpdatePlayer)();
			CALL_MEMBER_FN((*g_thePlayer), QueueNiNodeUpdate)(true);
		}
	}
}

std::pair<RaceSexMenu*, RaceMenuSlider*> GetRaceMenuSlider(UInt32 sliderId)
{
	MenuManager* mm = MenuManager::GetSingleton();
	if (mm)
	{
		BSFixedString t("RaceSex Menu");
		RaceSexMenu* raceMenu = (RaceSexMenu*)mm->GetMenu(&t);
		if (raceMenu)
		{
			RaceMenuSlider* slider = NULL;
			RaceSexMenu::RaceComponent* raceData = NULL;

			UInt8 gender = 0;
			PlayerCharacter* player = (*g_thePlayer);
			TESNPC* actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
			if (actorBase)
				gender = CALL_MEMBER_FN(actorBase, GetSex)();

			if (raceMenu->raceIndex < raceMenu->sliderData[gender].count)
				raceData = &raceMenu->sliderData[gender][raceMenu->raceIndex];
			if (raceData && sliderId < raceData->sliders.count)
				slider = &raceData->sliders[sliderId];

			if (raceData && slider)
			{
				return {raceMenu, slider};
			}
		}
	}
	return {nullptr, nullptr};
}

void SKSEScaleform_GetSliderPartData::Invoke(Args* args)
{
	using namespace ScaleformUtils;

	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	UInt32 sliderId = (UInt32)args->args[0].GetNumber();

	auto createTagsForPart = [](GFxMovieView* view, GFxValue* object, BGSHeadPart* headPart)
	{
		if (g_formTagInterface.HasTags(headPart))
		{
			class TagVisitor : public IFormTagInterface::TagVisitor
			{
			public:
				TagVisitor(GFxMovieView* view, GFxValue* arr) : m_view(view), m_array(arr) { };
				virtual void Visit(const char* tag) override
				{
					GFxValue str;
					m_view->CreateString(&str, tag);
					m_array->PushBack(&str);
				}
				GFxMovieView* m_view;
				GFxValue* m_array;
			};
			TagVisitor visitor{ view, object };
			g_formTagInterface.GetTags(headPart, visitor);
		}
	};

	auto createFilterTags = [](GFxMovieView* view, GFxValue* object, uint32_t partType)
	{
		if (g_formTagInterface.HasPartTags(partType))
		{
			class PartTagVisitor : public IFormTagInterface::PartTagVisitor
			{
			public:
				PartTagVisitor(GFxMovieView* view, GFxValue* arr) : m_view(view), m_array(arr) { };
				virtual void Visit(const char* name, const char* label) override
				{
					GFxValue obj;
					m_view->CreateObject(&obj);
					RegisterString(&obj, m_view, "name", name);
					RegisterString(&obj, m_view, "label", label);
					m_array->PushBack(&obj);
				}
				GFxMovieView* m_view;
				GFxValue* m_array;
			};
			PartTagVisitor visitor{ view, object };
			g_formTagInterface.GetPartTags(partType, visitor);
		}
	};

	auto addHeadPart = [&createTagsForPart](GFxMovieView* view, GFxValue* object, BGSHeadPart* headPart, uint32_t index)
	{
		GFxValue partObject;
		view->CreateObject(&partObject);
		SKEEFixedString full(headPart->fullName.name.c_str());
		SKEEFixedString edid(headPart->partName.c_str());
		std::string partName(headPart->fullName.name.c_str());
		if (partName.empty())
			partName = headPart->partName.c_str();
		else if(full != edid)
			partName += " <font size='14' color='#666666'>[" + std::string(headPart->partName.c_str()) + "]</font>";

		RegisterString(&partObject, view, "name", partName.c_str());
		RegisterNumber(&partObject, "index", static_cast<double>(index));

		GFxValue tagArray;
		view->CreateArray(&tagArray);
		partObject.SetMember("tags", &tagArray);
		createTagsForPart(view, &tagArray, headPart);

		// Add the ModName as a tag, and a property
		if (headPart)
		{
			auto mod = GetModInfoByFormID(headPart->formID);
			if (mod) {
				GFxValue str;
				view->CreateString(&str, mod->name);
				tagArray.PushBack(&str);

				RegisterString(&partObject, view, "source", mod->name);
			}
		}

		object->PushBack(&partObject);
	};

	std::set<std::string> modSet;

	auto [raceMenu, slider] = GetRaceMenuSlider(sliderId);
	if (raceMenu && slider)
	{
		args->movie->CreateObject(args->result);

		GFxValue tagArray;
		args->movie->CreateArray(&tagArray);
		args->result->SetMember("tags", &tagArray);

		GFxValue partArray;
		args->movie->CreateArray(&partArray);
		args->result->SetMember("parts", &partArray);

		switch(slider->type)
		{
		case RaceMenuSlider::kTypeHeadPart:
			{
				if(slider->index < RaceSexMenu::kNumHeadPartLists)
				{
					createFilterTags(args->movie, &tagArray, slider->index);

					BGSHeadPart * headPart = NULL;
					for (int32_t i = 0; i < raceMenu->headParts[slider->index].count; ++i)
					{
						raceMenu->headParts[slider->index].GetNthItem(i, headPart);
						if (headPart)
						{
							addHeadPart(args->movie, &partArray, headPart, i);

							auto mod = GetModInfoByFormID(headPart->formID);
							if (mod) {
								modSet.insert(mod->name);
							}
						}
					}
				}
			}
			break;
		case RaceMenuSlider::kTypeDoubleMorph:
			{
				// Provide case for custom parts
				if(slider->index >= SLIDER_OFFSET) {
					UInt32 sliderIndex = slider->index - SLIDER_OFFSET;
					SliderInternalPtr sliderInternal = g_morphInterface.GetSliderByIndex((*g_thePlayer)->race, sliderIndex);
					if(sliderInternal) {
						switch (sliderInternal->type)
						{
							// Only acquire part information for actual part sliders
							case SliderInternal::kTypeHeadPart:
							{
								UInt8 partType = sliderInternal->presetCount;
								HeadPartList * partList = g_partSet.GetPartList(partType);
								if (partList)
								{
									createFilterTags(args->movie, &tagArray, partType);

									uint32_t i = 0;
									for (auto& headPart : *partList)
									{
										addHeadPart(args->movie, &partArray, headPart, i++);

										auto mod = GetModInfoByFormID(headPart->formID);
										if (mod) {
											modSet.insert(mod->name);
										}
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

		// Add the mods automatically as tags
		for (auto& mod : modSet)
		{
			GFxValue obj;
			args->movie->CreateObject(&obj);
			RegisterString(&obj, args->movie, "name", mod.c_str());
			RegisterString(&obj, args->movie, "label", mod.c_str());
			tagArray.PushBack(&obj);
		}
	}
}

void SKSEScaleform_GetSliderData::Invoke(Args * args)
{
	using namespace ScaleformUtils;

	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	UInt32 sliderId = (UInt32)args->args[0].GetNumber();
	double value = args->args[1].GetNumber();


	auto [raceMenu, slider] = GetRaceMenuSlider(sliderId);
	if (raceMenu && slider)
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
					RegisterNumber(args->result, "parts", static_cast<double>(raceMenu->headParts[slider->index].count));
				}
			}
			break;
		case RaceMenuSlider::kTypeDoubleMorph:
			{
				// Provide case for custom parts
				if(slider->index >= SLIDER_OFFSET) {
					UInt32 sliderIndex = slider->index - SLIDER_OFFSET;
					SliderInternalPtr sliderInternal = g_morphInterface.GetSliderByIndex((*g_thePlayer)->race, sliderIndex);
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
									RegisterNumber(args->result, "parts", static_cast<double>(partList->size()));
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


void SKSEScaleform_GetModName::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	UInt32 formId = (UInt32)args->args[0].GetNumber();

	ModInfo* modInfo = GetModInfoByFormID(formId);
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
	Actor * actor = (*g_thePlayer);//g_World.GetWorkingActor();
	if (!actor)
		return;

	std::string nifPath = strData;
	nifPath.append(".nif");
	std::string ddsPath = strData;
	ddsPath.append(".dds");

	if(g_enableHeadExport)
		g_task->AddTask(new SKSETaskExportHead(actor, nifPath.c_str(), ddsPath.c_str()));
}

void SKSEScaleform_ImportHead::Invoke(Args * args)
{
	using namespace ScaleformUtils;

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

	UInt8 niStreamMemory[sizeof(NiStream)];
	memset(niStreamMemory, 0, sizeof(NiStream));
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
				
				SInt32 gIndex = 0;
				VisitObjects(rootNode, [&gIndex, &args](NiAVObject* trishape)
				{
					NiNode * parent = trishape->m_parent;
					if (parent && BSFixedString(parent->m_name) == BSFixedString("BSFaceGenNiNodeSkinned")) {

						UInt32 numVertices = 0;

						NiGeometry * legacyGeometry = trishape->GetAsNiGeometry();
						BSTriShape * geometry = trishape->GetAsBSTriShape();

						if (geometry) {
							numVertices = geometry->numVertices;
						}
						else if (legacyGeometry) {
							NiGeometryData * geometryData = niptr_cast<NiGeometryData>(legacyGeometry->m_spModelData);
							if (geometryData) {
								numVertices = geometryData->m_usVertices;
							}
						}
						else {
							gIndex++;
							return false;
						}
						
						GFxValue gfxGeom;
						args->movie->CreateObject(&gfxGeom);
						RegisterString(&gfxGeom, args->movie, "name", trishape->m_name);
						RegisterNumber(&gfxGeom, "vertices", numVertices);
						RegisterNumber(&gfxGeom, "gIndex", gIndex);
						args->result->PushBack(&gfxGeom);
						gIndex++;
					}

					return false;
				});
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


	UInt32 meshLength = g_World.GetNumMeshes();
	UInt32 reqLength = args->args[0].GetArraySize();

	for (UInt32 i = 0, m = 0; i < meshLength && m < reqLength; i++)
	{
		CDXNifMesh * mesh = dynamic_cast<CDXNifMesh*>(g_World.GetNthMesh(i));
		if (mesh) {
			NiNode * importRoot = g_World.GetImportRoot();
			if (importRoot) {

				SInt32 searchIndex = -1;

				GFxValue gfxIndex;
				args->args[0].GetElement(m, &gfxIndex);
				searchIndex = gfxIndex.GetNumber();

				NiAVObject * sourceGeometry = NULL;
				SInt32 gIndex = 0;
				VisitObjects(importRoot, [&gIndex, &args, &searchIndex, &sourceGeometry](NiAVObject* trishape)
				{
					NiNode * parent = trishape->m_parent;
					if (parent && BSFixedString(parent->m_name) == BSFixedString("BSFaceGenNiNodeSkinned")) {
						NiTriBasedGeom * legacyGeometry = trishape->GetAsNiTriBasedGeom();
						BSTriShape * geometry = trishape->GetAsBSTriShape();

						if (legacyGeometry) {
							NiGeometryData * geometryData = niptr_cast<NiGeometryData>(legacyGeometry->m_spModelData);
							if (!geometryData) {
								gIndex++;
								return false;
							}
						}

						if (searchIndex == gIndex) {
							if (geometry) sourceGeometry = geometry;
							else if (legacyGeometry) sourceGeometry = legacyGeometry;
							else return false;
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

			m++;
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

		CDXNifMesh * mesh = dynamic_cast<CDXNifMesh*>(g_World.GetNthMesh(meshIndex));
		if (mesh) {	
			std::shared_ptr<CDXNifResetSculpt> resetGeometry = std::make_shared<CDXNifResetSculpt>(mesh);
			if (resetGeometry->Length() > 0)
				resetGeometry->Apply(g_undoStack.Push(resetGeometry));
		}
	}
}

void CreateHeadPartObject(GFxValue& object, GFxMovieView* movie, BGSHeadPart* headPart)
{
	using namespace ScaleformUtils;

	RegisterString(&object, movie, "partName", headPart->partName.data);
	RegisterNumber(&object, "partFlags", headPart->partFlags);
	RegisterNumber(&object, "partType", headPart->type);
	RegisterString(&object, movie, "modelPath", headPart->model.GetModelName());
	RegisterString(&object, movie, "chargenMorphPath", headPart->chargenMorph.GetModelName());
	RegisterString(&object, movie, "raceMorphPath", headPart->raceMorph.GetModelName());
}


void SKSEScaleform_GetHeadParts::Invoke(Args * args)
{
	args->movie->CreateObject(args->result);

	GFxValue partList;
	args->movie->CreateArray(&partList);

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	UInt32 numHeadParts = actorBase->numHeadParts;
	BGSHeadPart** headParts = actorBase->headparts;
	if (CALL_MEMBER_FN(actorBase, HasOverlays)())
	{
		numHeadParts = std::max((UInt32)actorBase->numHeadParts, GetNumActorBaseOverlays(actorBase));
		headParts = GetActorBaseOverlays(actorBase);
	}

	for(UInt32 i = 0; i < numHeadParts; i++)
	{
		GFxValue partData;
		args->movie->CreateObject(&partData);

		BGSHeadPart * headPart = i < actorBase->numHeadParts ? actorBase->headparts[i] : headParts[i];

		GFxValue headPartData;
		args->movie->CreateObject(&headPartData);
		CreateHeadPartObject(headPartData, args->movie, headPart);
		partData.SetMember("base", &headPartData);

		// Get the overlay, if there is one
		if(CALL_MEMBER_FN(actorBase, HasOverlays)()) {
			BGSHeadPart * overlayPart = actorBase->GetHeadPartOverlayByType(headPart->type);
			if(overlayPart) {
				GFxValue overlayPartData;
				args->movie->CreateObject(&overlayPartData);
				CreateHeadPartObject(overlayPartData, args->movie, overlayPart);
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
	if (root)
	{
		args->movie->CreateArray(args->result);
		for (UInt32 i = 0; i < 3 * 3; i++)
		{
			GFxValue index;
			index.SetNumber(((float*)(root->m_localTransform.rot.data))[i]);
			args->result->PushBack(&index);
		}
	}
}

void SKSEScaleform_SetPlayerRotation::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Array);
	ASSERT(args->args[0].GetArraySize() == 9);

	PlayerCharacter * player = (*g_thePlayer);
	NiNode * root = player->GetNiRootNode(0);

	if (root)
	{
		for (UInt32 i = 0; i < 3 * 3; i++)
		{
			GFxValue val;
			args->args[0].GetElement(i, &val);
			if (val.GetType() != GFxValue::kType_Number)
				break;

			((float*)root->m_localTransform.rot.data)[i] = val.GetNumber();
		}

		NiAVObject::ControllerUpdateContext ctx;
		root->UpdateWorldData(&ctx);
	}
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
	using namespace ScaleformUtils;

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext = g_renderManager->context;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	pDeviceContext->GetDevice(&pDevice);
	if (!pDevice) { // This shouldnt happen
		_ERROR("%s - Failed to acquire device from context.", __FUNCTION__);
		return;
	}

	g_Device = new CDXD3DDevice(pDevice, pDeviceContext);

	CDXShaderFactory shaderFactory;

	CDXInitParams initParams;
	initParams.camera = &g_Camera;
	initParams.device = g_Device;
	initParams.viewportWidth = g_viewWidth;
	initParams.viewportHeight = g_viewHeight;
	initParams.factory = &shaderFactory;

	CDXVec vecEye = DirectX::XMVectorSet(32.0f, 0.0f, 0.0f, 1.0f);
	CDXVec vecAt = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	g_Camera.SetWindow(initParams.viewportWidth, initParams.viewportHeight);
	g_Camera.SetViewParams(&vecEye, &vecAt);
	g_Camera.SetProjParams(g_cameraFOV * (DirectX::XM_PI / 180.0f), 1.0f, 1.0f, 1000.0f);
	g_Camera.SetPanSpeed(g_panSpeed);
	g_Camera.Update();

	CDXBSShaderResource vsShader("SKSE/Plugins/CharGen/Shaders/shader_vs.hlsl", "LightVertexShader");
	CDXBSShaderResource pvsShader("SKSE/Plugins/CharGen/Shaders/Compiled/shader_vs.cso");
	initParams.vShader[0] = &vsShader;
	initParams.vShader[1] = &pvsShader;

	CDXBSShaderResource psShader("SKSE/Plugins/CharGen/Shaders/shader_ps.hlsl", "LightPixelShader");
	CDXBSShaderResource ppsShader("SKSE/Plugins/CharGen/Shaders/Compiled/shader_ps.cso");
	initParams.pShader[0] = &psShader;
	initParams.pShader[1] = &ppsShader;

	CDXBSShaderResource wvShader("SKSE/Plugins/CharGen/Shaders/wireframe_vs.hlsl", "WireframeVertexShader");
	CDXBSShaderResource pwvShader("SKSE/Plugins/CharGen/Shaders/Compiled/wireframe_vs.cso");
	initParams.wvShader[0] = &wvShader;
	initParams.wvShader[1] = &pwvShader;

	CDXBSShaderResource wsShader("SKSE/Plugins/CharGen/Shaders/wireframe_ps.hlsl", "WireframePixelShader");
	CDXBSShaderResource pwsShader("SKSE/Plugins/CharGen/Shaders/Compiled/wireframe_ps.cso");
	initParams.wpShader[0] = &wsShader;
	initParams.wpShader[1] = &pwsShader;

	CDXBSShaderResource gsShader("SKSE/Plugins/CharGen/Shaders/wireframe_gs.hlsl", "WireframeGeometryShader");
	CDXBSShaderResource pgsShader("SKSE/Plugins/CharGen/Shaders/Compiled/wireframe_gs.cso");
	initParams.wgShader[0] = &gsShader;
	initParams.wgShader[1] = &pgsShader;

	if (!g_World.Setup(initParams)) {
		_ERROR("%s - Failed to setup world.", __FUNCTION__);
		return;
	}

	PlayerCharacter * player = (*g_thePlayer);
	if (!player) {
		_ERROR("%s - Invalid working actor.", __FUNCTION__);
		return;
	}

	TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	if (!actorBase) {
		_ERROR("%s - No actor base.", __FUNCTION__);
		return;
	}

	NiNode * rootFaceGen = player->GetFaceGenNiNode();
	if (!rootFaceGen) {
		_ERROR("%s - No FaceGen node.", __FUNCTION__);
		return;
	}

	g_World.SetWorkingActor(player);

	BSFaceGenAnimationData * animationData = player->GetFaceGenAnimationData();
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

	std::set<BGSHeadPart*> extraParts; // Collect extra hair parts
	BGSHeadPart * hairPart = actorBase->GetCurrentHeadPartByType(BGSHeadPart::kTypeHair);
	if (hairPart) {
		BGSHeadPart * extraPart = NULL;
		for (UInt32 p = 0; p < hairPart->extraParts.count; p++) {
			if (hairPart->extraParts.GetNthItem(p, extraPart))
				extraParts.insert(extraPart);
		}
	}

	for(UInt32 i = 0; i < rootFaceGen->m_children.m_emptyRunStart; i++)
	{
		for(UInt32 h = 0; h < numHeadParts; h++) {
			BGSHeadPart * headPart = headParts[h];
			if (!headPart)
				continue;

			NiAVObject * object = rootFaceGen->m_children.m_data[i];
			if (!object)
				continue;

			if(headPart->partName == BSFixedString(object->m_name)) {
				NiGeometry* legacyGeometry = object->GetAsNiGeometry();
				BSTriShape* geometry = object->GetAsBSTriShape();

				CDXNifMesh * mesh = nullptr;
				if (legacyGeometry) {
					mesh = CDXLegacyNifMesh::Create(g_Device, legacyGeometry);
				}
				if (geometry) {
					mesh = CDXBSTriShapeMesh::Create(g_Device, geometry);
				}

				if (!mesh)
					continue;

				if (extraParts.find(headPart) != extraParts.end()) // Is one of the hair parts toggle visibility
					mesh->SetVisible(false);
					
				auto material = mesh->GetMaterial();
				if (material)
					material->SetWireframeColor(DirectX::XMFLOAT4(((colors[i] >> 16) & 0xFF) / 255.0, ((colors[i] >> 8) & 0xFF) / 255.0, (colors[i] & 0xFF) / 255.0, 1.0f));

				if (headPart->type != BGSHeadPart::kTypeFace)
					mesh->SetLocked(true);

				mesh->SetTransform(DirectX::XMMatrixTranslation(g_sculptOffsetX, g_sculptOffsetY, g_sculptOffsetZ));

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
	RegisterNumber(args->result, "width", initParams.viewportWidth);
	RegisterNumber(args->result, "height", initParams.viewportHeight);
}

void SKSEScaleform_ReleaseMorphEditor::Invoke(Args * args)
{
	g_World.Release();
	delete g_Device;
	g_Device = nullptr;
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
		if (g_World.Pick(&g_Camera, x, y, brushStroke))
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
		g_World.Pick(&g_Camera, x, y, brushStroke);
	}
};

void SKSEScaleform_EndPaintMesh::Invoke(Args * args)
{
	CDXBrush * brush = g_World.GetCurrentBrush();
	if(brush)
		brush->EndStroke();
};

void SKSEScaleform_DoHoverMesh::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Number);

	SInt32 x = args->args[0].GetNumber();
	SInt32 y = args->args[1].GetNumber();

	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushTranslator translator(brush, static_cast<CDXBrushMesh*>(g_World.GetNthMesh(0)), static_cast<CDXBrushMesh*>(g_World.GetNthMesh(1)));
		g_World.Pick(&g_Camera, x, y, translator);
	}
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
	using namespace ScaleformUtils;

	args->movie->CreateArray(args->result);

	for (const auto& brush : g_World.GetBrushes()) {
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
	using namespace ScaleformUtils;

	args->movie->CreateArray(args->result);

	for (UInt32 i = 0; i < g_World.GetNumMeshes(); i++) {
		CDXMesh * mesh = g_World.GetNthMesh(i);
		if (mesh && mesh->IsEditable()) {
			GFxValue object;
			args->movie->CreateObject(&object);
			RegisterString(&object, args->movie, "name", mesh->GetName());
			RegisterNumber(&object, "meshIndex", i);
			RegisterBool(&object, "wireframe", mesh->ShowWireframe());
			RegisterBool(&object, "locked", mesh->IsLocked());
			RegisterBool(&object, "visible", mesh->IsVisible());
			RegisterBool(&object, "morphable", mesh->IsMorphable());
			RegisterNumber(&object, "vertices", mesh->GetVertexCount());

			auto material = mesh->GetMaterial();
			if (material) {
				DirectX::XMFLOAT4 fColor = material->GetWireframeColor();
				UInt32 color = 0xFF000000 | (UInt32)(fColor.x * 255) << 16 | (UInt32)(fColor.y * 255) << 8 | (UInt32)(fColor.z * 255);
				RegisterNumber(&object, "wfColor", color);
			}

			args->result->PushBack(&object);
		}
	}
}

void SKSEScaleform_SetMeshData::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);

	UInt32 i = (UInt32)args->args[0].GetNumber();

	CDXEditableMesh * mesh = dynamic_cast<CDXEditableMesh*>(g_World.GetNthMesh(i));
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
		auto material = mesh->GetMaterial();
		if (material) {
			UInt32 color = wfColor.GetNumber();
			material->SetWireframeColor(DirectX::XMFLOAT4(((color >> 16) & 0xFF) / 255.0, ((color >> 8) & 0xFF) / 255.0, (color & 0xFF) / 255.0, 1.0f));
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
	using namespace ScaleformUtils;

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

