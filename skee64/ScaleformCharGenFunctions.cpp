#include <REX/W32/KERNEL32.h>
#include <filesystem>
#include <ctime>
#include <cstdio>
#include "SKEETasks.h"


#include "ScaleformCharGenFunctions.h"
#include "ScaleformUtils.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"
#include "SKEEHooks.h"


#include "NifUtils.h"
#include "FaceLists.h"

#include "RE/N/NiRTTI.h"
#include "RE/N/NiGeometry.h"


#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "PresetInterface.h"
#include "FormTagInterface.h"

#include "ScaleformUtils.h"
#include <cstdint>
#include <cassert>

extern FaceMorphInterface	g_morphInterface;
extern PresetInterface		g_presetInterface;
extern FormTagInterface		g_formTagInterface;
extern PartSet	g_partSet;

extern const SKSE::TaskInterface* g_task;

#include "RE/N/NiExtraData.h"

#include <DirectXMath.h>
#include "REX/W32/D3D11_4.h"

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



std::uint32_t colors[] = {
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
extern std::int32_t						g_viewWidth;
extern std::int32_t						g_viewHeight;
extern bool							g_enableHeadExport;

extern float	g_sculptOffsetX;
extern float	g_sculptOffsetY;
extern float	g_sculptOffsetZ;

void SKSEScaleform_SavePreset::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kBoolean);

	bool saveJson = a_params.args[1].GetBool();
	const char	* strData = a_params.args[0].GetString();

	if (saveJson)
		a_params.retVal->SetBoolean(g_presetInterface.SaveJsonPreset(strData, (RE::PlayerCharacter::GetSingleton())));
	else
		a_params.retVal->SetBoolean(g_presetInterface.SaveBinaryPreset(strData));
}

void SKSEScaleform_LoadPreset::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kObject);
	assert(a_params.args[2].GetType() == RE::GFxValue::ValueType::kBoolean);

	bool loadJson = a_params.args[2].GetBool();

	const char	* strData = a_params.args[0].GetString();

	RE::GFxValue	* object = nullptr;
	if (a_params.argCount >= 2)
		object = &a_params.args[1];

	auto presetData = std::make_shared<PresetData>();
	bool loadError = loadJson ? g_presetInterface.LoadJsonPreset(strData, presetData) : g_presetInterface.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, a_params.movie, object);
	if (!loadError) {
		g_presetInterface.ApplyPresetData(RE::PlayerCharacter::GetSingleton(), presetData);

		RegisterNumber(object, "hairColor", presetData->hairColor);

		RE::GFxValue tintArray{};
		a_params.movie->CreateArray(&tintArray);

		for(auto & tint : presetData->tints) {
			RE::GFxValue tintObject{};
			a_params.movie->CreateObject(&tintObject);
			RegisterNumber(&tintObject, "color", tint.color);
			RegisterNumber(&tintObject, "index", tint.index);
			RegisterString(&tintObject, a_params.movie, "texture", tint.name.c_str());
			tintArray.PushBack(tintObject);
		}

		object->SetMember("tints", tintArray);
	}

	a_params.retVal->SetBoolean(loadError);
}

const char * GetGameSettingString(const char * key)
{
	RE::Setting * setting = RE::GameSettingCollection::GetSingleton()->GetSetting(key);
	if (setting && setting->GetType() == RE::Setting::Type::kString)
		return setting->GetString();

	return NULL;
}

void SKSEScaleform_ReadPreset::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kObject);
	assert(a_params.args[2].GetType() == RE::GFxValue::ValueType::kBoolean);

	bool loadJson = a_params.args[2].GetBool();
	const char	* strData = a_params.args[0].GetString();
	

	RE::GFxValue	* object = nullptr;
	if (a_params.argCount >= 2)
		object = &a_params.args[1];

	RE::TESDataHandler * dataHandler = RE::TESDataHandler::GetSingleton();
	auto presetData = std::make_shared<PresetData>();
	bool loadError = loadJson ? g_presetInterface.LoadJsonPreset(strData, presetData) : g_presetInterface.LoadBinaryPreset(strData, presetData);//g_morphHandler.LoadPreset(strData, a_params.movie, object);
	if(!loadError) {
		RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
		RE::TESNPC * npc = player->GetBaseObject() ? player->GetBaseObject()->As<RE::TESNPC>() : nullptr;

		RE::GFxValue modArray{};
		a_params.movie->CreateArray(&modArray);
		for(std::vector<std::string>::iterator it = presetData->modList.begin(); it != presetData->modList.end(); ++it) {

			const RE::TESFile * modInfo = dataHandler->LookupModByName((*it).c_str());
			if (!modInfo || !BSFileUtil::IsActive(modInfo))
				continue;

			RE::GFxValue modObject{};
			a_params.movie->CreateObject(&modObject);
			RegisterString(&modObject, a_params.movie, "name", (*it).c_str());
			RegisterNumber(&modObject, "loadedIndex", modInfo->GetPartialIndex());
			modArray.PushBack(modObject);
		}
		object->SetMember("mods", modArray);

		RE::GFxValue partArray{};
		a_params.movie->CreateArray(&partArray);
		for(std::vector<RE::BGSHeadPart*>::iterator it = presetData->headParts.begin(); it != presetData->headParts.end(); ++it) {
			RE::GFxValue partObject{};
			a_params.movie->CreateString(&partObject, (*it)->fullName.c_str());
			partArray.PushBack(partObject);
		}
		object->SetMember("headParts", partArray);

		RE::GFxValue weightObject{};
		a_params.movie->CreateObject(&weightObject);
		RegisterUnmanagedString(&weightObject, "name", GetGameSettingString("sRSMWeight"));
		RegisterNumber(&weightObject, "value", presetData->weight);
		object->SetMember("weight", weightObject);

		RE::GFxValue hairObject{};
		a_params.movie->CreateObject(&hairObject);
		RegisterUnmanagedString(&hairObject, "name", GetGameSettingString("sRSMHairColorPresets"));
		RegisterNumber(&hairObject, "value", presetData->hairColor);
		object->SetMember("hair", hairObject);

		RE::GFxValue tintArray{};
		a_params.movie->CreateArray(&tintArray);
		for(std::vector<PresetData::Tint>::iterator it = presetData->tints.begin(); it != presetData->tints.end(); ++it) {
			PresetData::Tint & tint = (*it);
			RE::GFxValue tintObject{};
			a_params.movie->CreateObject(&tintObject);
			RegisterNumber(&tintObject, "color", tint.color);
			RegisterNumber(&tintObject, "index", tint.index);
			RegisterString(&tintObject, a_params.movie, "texture", tint.name.c_str());
			tintArray.PushBack(tintObject);
		}
		object->SetMember("tints", tintArray);

		RE::GFxValue morphArray;
		a_params.movie->CreateArray(&morphArray);

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

		std::uint32_t i = 0;
		for(std::vector<std::int32_t>::iterator it = presetData->presets.begin(); it != presetData->presets.end(); ++it) {
			RE::GFxValue presetObject{};
			a_params.movie->CreateObject(&presetObject);
			if(presetNames[i])
				RegisterUnmanagedString(&presetObject, "name", presetNames[i]);
			RegisterNumber(&presetObject, "value", *it);
			RegisterNumber(&presetObject, "type", 0);
			RegisterNumber(&presetObject, "index", i);
			morphArray.PushBack(presetObject);
			i++;
		}

		i = 0;
		for(auto & it : presetData->morphs) {
			RE::GFxValue presetObject{};
			a_params.movie->CreateObject(&presetObject);
			if (i < FaceMorphList::kNumMorphs && morphNames[i])
				RegisterUnmanagedString(&presetObject, "name", morphNames[i]);
			RegisterNumber(&presetObject, "value", it);
			RegisterNumber(&presetObject, "type", 1);
			RegisterNumber(&presetObject, "index", i);
			morphArray.PushBack(presetObject);
			i++;
		}

		i = 0;
		for(auto & it : presetData->customMorphs) {
			std::string morphName = "$";
			morphName.append(it.name.c_str());
			RE::GFxValue customObject{};
			a_params.movie->CreateObject(&customObject);
			RegisterString(&customObject, a_params.movie, "name", morphName.c_str());
			RegisterNumber(&customObject, "value", it.value);
			RegisterNumber(&customObject, "type", 2);
			RegisterNumber(&customObject, "index", i);
			morphArray.PushBack(customObject);
			i++;
		}
		i = 0;
		for (auto & it : presetData->bodyMorphData) {
			RE::GFxValue customObject{};
			a_params.movie->CreateObject(&customObject);
			RegisterString(&customObject, a_params.movie, "name", it.first.c_str());

			float morphSum = 0;
			for (auto & keys : it.second)
				morphSum += keys.second;

			RegisterNumber(&customObject, "value", morphSum);
			RegisterNumber(&customObject, "type", 3);
			RegisterNumber(&customObject, "index", i);
			morphArray.PushBack(customObject);
			i++;
		}
		object->SetMember("morphs", morphArray);
	}

	a_params.retVal->SetBoolean(loadError);
}

void SKSEScaleform_ReloadSliders::Call(RE::GFxFunctionHandler::Params& a_params)
{
	RE::UI * mm = RE::UI::GetSingleton();
	if (mm) {
		auto raceMenu = mm->GetMenu<RE::RaceSexMenu>();
		if(raceMenu) {
			RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
			SKEE::LoadSliders(raceMenu.get(), (std::uint64_t)player->GetBaseObject(), 0);
			player->DoReset3D(true);
		}
	}
}

std::pair<RE::RaceSexMenu*, RE::RaceMenuSlider*> GetRaceMenuSlider(std::uint32_t sliderId)
{
	RE::UI* mm = RE::UI::GetSingleton();
	if (mm)
	{
		auto raceMenu = mm->GetMenu<RE::RaceSexMenu>();
		if (raceMenu)
		{
			RE::RaceMenuSlider* slider = NULL;
			RE::RaceComponent* raceData = NULL;

			std::uint8_t gender = 0;
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			RE::TESNPC* actorBase = player->GetBaseObject() ? player->GetBaseObject()->As<RE::TESNPC>() : nullptr;
			if (actorBase)
				gender = actorBase->GetSex();

			auto& menuData = raceMenu->GetRuntimeData();
			if (menuData.unk188 < menuData.sliderData[gender].size())
				raceData = &menuData.sliderData[gender][menuData.unk188];
			if (raceData && sliderId < raceData->sliders.size())
				slider = &raceData->sliders[sliderId];

			if (raceData && slider)
			{
				return {raceMenu.get(), slider};
			}
		}
	}
	return {nullptr, nullptr};
}

void SKSEScaleform_GetSliderPartData::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);

	std::uint32_t sliderId = (std::uint32_t)a_params.args[0].GetNumber();

	auto createTagsForPart = [](RE::GFxMovie* view, RE::GFxValue* object, RE::BGSHeadPart* headPart)
	{
		if (g_formTagInterface.HasTags(headPart))
		{
			class TagVisitor : public IFormTagInterface::TagVisitor
			{
			public:
				TagVisitor(RE::GFxMovie* view, RE::GFxValue* arr) : m_view(view), m_array(arr) { };
				virtual void Visit(const char* tag) override
				{
					RE::GFxValue str{};
					m_view->CreateString(&str, tag);
					m_array->PushBack(str);
				}
				RE::GFxMovie* m_view;
				RE::GFxValue* m_array;
			};
			TagVisitor visitor{ view, object };
			g_formTagInterface.GetTags(headPart, visitor);
		}
	};

	auto createFilterTags = [](RE::GFxMovie* view, RE::GFxValue* object, uint32_t partType)
	{
		if (g_formTagInterface.HasPartTags(partType))
		{
			class PartTagVisitor : public IFormTagInterface::PartTagVisitor
			{
			public:
				PartTagVisitor(RE::GFxMovie* view, RE::GFxValue* arr) : m_view(view), m_array(arr) { };
				virtual void Visit(const char* name, const char* label) override
				{
					RE::GFxValue obj{};
					m_view->CreateObject(&obj);
					RegisterString(&obj, m_view, "name", name);
					RegisterString(&obj, m_view, "label", label);
					m_array->PushBack(obj);
				}
				RE::GFxMovie* m_view;
				RE::GFxValue* m_array;
			};
			PartTagVisitor visitor{ view, object };
			g_formTagInterface.GetPartTags(partType, visitor);
		}
	};

	auto addHeadPart = [&createTagsForPart](RE::GFxMovie* view, RE::GFxValue* object, RE::BGSHeadPart* headPart, uint32_t index)
	{
		RE::GFxValue partObject{};
		view->CreateObject(&partObject);
		SKEEFixedString full(headPart->fullName.c_str());
		SKEEFixedString edid(headPart->formEditorID.c_str());
		std::string partName(headPart->fullName.c_str());
		if (partName.empty())
			partName = headPart->formEditorID.c_str();
		else if(full != edid)
			partName += " <font size='14' color='#666666'>[" + std::string(headPart->formEditorID.c_str()) + "]</font>";

		RegisterString(&partObject, view, "name", partName.c_str());
		RegisterNumber(&partObject, "index", static_cast<double>(index));

		RE::GFxValue tagArray{};
		view->CreateArray(&tagArray);
		partObject.SetMember("tags", tagArray);
		createTagsForPart(view, &tagArray, headPart);

		// Add the ModName as a tag, and a property
		if (headPart)
		{
			auto mod = GetModInfoByFormID(headPart->formID);
			if (mod) {
				RE::GFxValue str{};
				view->CreateString(&str, mod->GetFilename().data());
				tagArray.PushBack(str);

				RegisterString(&partObject, view, "source", mod->GetFilename().data());
			}
		}

		object->PushBack(partObject);
	};

	std::set<std::string> modSet;

	auto [raceMenu, slider] = GetRaceMenuSlider(sliderId);
	if (raceMenu && slider)
	{
		a_params.movie->CreateObject(a_params.retVal);

		RE::GFxValue tagArray{};
		a_params.movie->CreateArray(&tagArray);
		a_params.retVal->SetMember("tags", tagArray);

		RE::GFxValue partArray{};
		a_params.movie->CreateArray(&partArray);
		a_params.retVal->SetMember("parts", partArray);

		switch(slider->type)
		{
		case RE::RaceMenuSlider::kTypeHeadPart:
			{
				if(slider->index < skee::kNumHeadPartLists)
				{
					createFilterTags(a_params.movie, &tagArray, slider->index);

					auto& headPartList = raceMenu->GetRuntimeData().headParts[slider->index];
					RE::BGSHeadPart * headPart = NULL;
					for (int32_t i = 0; i < (int32_t)headPartList.size(); ++i)
					{
						headPart = headPartList[i];
						if (headPart)
						{
							addHeadPart(a_params.movie, &partArray, headPart, i);

							auto mod = GetModInfoByFormID(headPart->formID);
							if (mod) {
								modSet.insert(std::string(mod->GetFilename()));
							}
						}
					}
				}
			}
			break;
		case RE::RaceMenuSlider::kTypeDoubleMorph:
			{
				// Provide case for custom parts
				if(slider->index >= SLIDER_OFFSET) {
					std::uint32_t sliderIndex = slider->index - SLIDER_OFFSET;
					SliderInternalPtr sliderInternal = g_morphInterface.GetSliderByIndex((RE::PlayerCharacter::GetSingleton())->race, sliderIndex);
					if(sliderInternal) {
						switch (sliderInternal->type)
						{
							// Only acquire part information for actual part sliders
							case SliderInternal::kTypeHeadPart:
							{
								std::uint8_t partType = sliderInternal->presetCount;
								HeadPartList * partList = g_partSet.GetPartList(partType);
								if (partList)
								{
									createFilterTags(a_params.movie, &tagArray, partType);

									uint32_t i = 0;
									for (auto& headPart : *partList)
									{
										addHeadPart(a_params.movie, &partArray, headPart, i++);

										auto mod = GetModInfoByFormID(headPart->formID);
										if (mod) {
											modSet.insert(std::string(mod->GetFilename()));
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
			RE::GFxValue obj{};
			a_params.movie->CreateObject(&obj);
			RegisterString(&obj, a_params.movie, "name", mod.c_str());
			RegisterString(&obj, a_params.movie, "label", mod.c_str());
			tagArray.PushBack(obj);
		}
	}
}

void SKSEScaleform_GetSliderData::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	std::uint32_t sliderId = (std::uint32_t)a_params.args[0].GetNumber();
	double value = a_params.args[1].GetNumber();

	auto [raceMenu, slider] = GetRaceMenuSlider(sliderId);
	if (raceMenu && slider)
	{
		a_params.movie->CreateObject(a_params.retVal);
		RegisterNumber(a_params.retVal, "type", slider->type);
		RegisterNumber(a_params.retVal, "index", slider->index);

		switch(slider->type)
		{
		case RE::RaceMenuSlider::kTypeHeadPart:
			{
				if(slider->index < skee::kNumHeadPartLists)
				{
					auto& headPartList = raceMenu->GetRuntimeData().headParts[slider->index];
					RE::BGSHeadPart * headPart = (value < headPartList.size()) ? headPartList[(std::uint32_t)value] : NULL;
					if(headPart) {
						RegisterNumber(a_params.retVal, "formId", headPart->formID);
						RegisterString(a_params.retVal, a_params.movie, "partName", headPart->formEditorID.c_str());
					}
					RegisterNumber(a_params.retVal, "parts", static_cast<double>(headPartList.size()));
				}
			}
			break;
		case RE::RaceMenuSlider::kTypeDoubleMorph:
			{
				// Provide case for custom parts
				if(slider->index >= SLIDER_OFFSET) {
					std::uint32_t sliderIndex = slider->index - SLIDER_OFFSET;
					SliderInternalPtr sliderInternal = g_morphInterface.GetSliderByIndex((RE::PlayerCharacter::GetSingleton())->race, sliderIndex);
					if(sliderInternal) {
						RegisterNumber(a_params.retVal, "subType", sliderInternal->type);
						switch (sliderInternal->type)
						{
							// Only acquire part information for actual part sliders
							case SliderInternal::kTypeHeadPart:
							{
								std::uint8_t partType = sliderInternal->presetCount;
								HeadPartList * partList = g_partSet.GetPartList(partType);
								if (partList)
								{
									RE::BGSHeadPart * targetPart = g_partSet.GetPartByIndex(partList, (std::uint32_t)value);
									if (targetPart) {
										RegisterNumber(a_params.retVal, "formId", targetPart->formID);
										RegisterString(a_params.retVal, a_params.movie, "partName", targetPart->formEditorID.c_str());
									}
									RegisterNumber(a_params.retVal, "parts", static_cast<double>(partList->size()));
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

void SKSEScaleform_GetModName::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);

	std::uint32_t formId = (std::uint32_t)a_params.args[0].GetNumber();

	RE::TESFile* modInfo = GetModInfoByFormID(formId);
	if(modInfo) {
		a_params.movie->CreateString(a_params.retVal, modInfo->GetFilename().data());
	}
}

void SKSEScaleform_ExportHead::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);

	const char	* strData = a_params.args[0].GetString();

	// Get the Editor's working actor
	RE::Actor * actor = (RE::PlayerCharacter::GetSingleton());//g_World.GetWorkingActor();
	if (!actor)
		return;

	std::string nifPath = strData;
	nifPath.append(".nif");
	std::string ddsPath = strData;
	ddsPath.append(".dds");

	if(g_enableHeadExport)
		SKEE_AddTask(g_task, new SKSETaskExportHead(actor, nifPath.c_str(), ddsPath.c_str()));
}

void SKSEScaleform_ImportHead::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);

	const char	* strData = a_params.args[0].GetString();

	// Release the previous import just in case
	g_World.ReleaseImport();

	// Get the Editor's working actor
	RE::Actor * actor = g_World.GetWorkingActor();
	if (!actor)
		return;

	RE::BSFaceGenNiNode * faceNode = GetFaceGenNiNode(actor);
	RE::TESNPC * actorBase = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (!actorBase || !faceNode)
		return;

	NifStreamWrapper niStreamScope;
	RE::NiStream * niStream = niStreamScope.get();

	RE::NiNode * rootNode = NULL;
	RE::BSResourceNiBinaryStream binaryStream(strData);
	if (binaryStream.good())
	{		
		niStream->Load1(&binaryStream);
		if (niStream->topObjects.size() > 0)
		{
			if (niStream->topObjects[0].get()) // Get the root node
				rootNode = niStream->topObjects[0].get() ? niStream->topObjects[0].get()->AsNode() : nullptr;
			if (rootNode)
			{
				a_params.movie->CreateArray(a_params.retVal);
				
				std::int32_t gIndex = 0;
				VisitObjects(rootNode, [&](RE::NiAVObject* trishape) -> bool
				{
					RE::NiNode * parent = trishape->parent;
					if (parent && RE::BSFixedString(parent->name) == RE::BSFixedString("BSFaceGenNiNodeSkinned")) {

						std::uint32_t numVertices = 0;

						RE::NiGeometry * legacyGeometry = trishape ? trishape->AsNiGeometry() : nullptr;
						RE::BSTriShape * geometry = trishape ? trishape->AsTriShape() : nullptr;

						if (geometry) {
							numVertices = geometry->vertexCount;
						}
						else if (legacyGeometry) {
							RE::NiGeometryData * geometryData = legacyGeometry->spModelData.get();
							if (geometryData) {
								numVertices = geometryData->vertices;
							}
						}
						else {
							gIndex++;
							return false;
						}
						
						RE::GFxValue gfxGeom;
						a_params.movie->CreateObject(&gfxGeom);
						RegisterString(&gfxGeom, a_params.movie, "name", trishape->name.c_str());
						RegisterNumber(&gfxGeom, "vertices", numVertices);
						RegisterNumber(&gfxGeom, "gIndex", gIndex);
						a_params.retVal->PushBack(gfxGeom);
						gIndex++;
					}

					return false;
				});
			}
		}
	}

	// Add the Root node to the Editor
	if (rootNode) {
		rootNode->IncRefCount();
		g_World.SetImportRoot(rootNode);
	}

	// niStreamScope's destructor releases the stream automatically.
}

void SKSEScaleform_ReleaseImportedHead::Call(RE::GFxFunctionHandler::Params& a_params)
{
	g_World.ReleaseImport();
}

void SKSEScaleform_LoadImportedHead::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kArray);

	std::uint32_t meshLength = g_World.GetNumMeshes();
	std::uint32_t reqLength = a_params.args[0].GetArraySize();

	for (std::uint32_t i = 0, m = 0; i < meshLength && m < reqLength; i++)
	{
		CDXNifMesh * mesh = dynamic_cast<CDXNifMesh*>(g_World.GetNthMesh(i));
		if (mesh) {
			RE::NiNode * importRoot = g_World.GetImportRoot();
			if (importRoot) {

				std::int32_t searchIndex = -1;

				RE::GFxValue gfxIndex{};
				a_params.args[0].GetElement(m, &gfxIndex);
				searchIndex = gfxIndex.GetNumber();

				RE::NiAVObject * sourceGeometry = NULL;
				std::int32_t gIndex = 0;
				VisitObjects(importRoot, [&gIndex, &searchIndex, &sourceGeometry](RE::NiAVObject* trishape) -> bool
				{
					RE::NiNode * parent = trishape->parent;
					if (parent && RE::BSFixedString(parent->name) == RE::BSFixedString("BSFaceGenNiNodeSkinned")) {
						RE::NiTriBasedGeometry * legacyGeometry = netimmerse_cast<RE::NiTriBasedGeometry*>(trishape);
						RE::BSTriShape * geometry = trishape ? trishape->AsTriShape() : nullptr;

						if (legacyGeometry) {
							RE::NiGeometryData * geometryData = legacyGeometry->spModelData.get();
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

void SKSEScaleform_ClearSculptData::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kArray);

	std::uint32_t meshLength = a_params.args[0].GetArraySize();

	for (std::uint32_t i = 0; i < meshLength; i++)
	{
		RE::GFxValue gfxIndex{};
		a_params.args[0].GetElement(i, &gfxIndex);
		std::int32_t meshIndex = gfxIndex.GetNumber();

		CDXNifMesh * mesh = dynamic_cast<CDXNifMesh*>(g_World.GetNthMesh(meshIndex));
		if (mesh) {	
			std::shared_ptr<CDXNifResetSculpt> resetGeometry = std::make_shared<CDXNifResetSculpt>(mesh);
			if (resetGeometry->Length() > 0)
				resetGeometry->Apply(g_undoStack.Push(resetGeometry));
		}
	}
}

void CreateHeadPartObject(RE::GFxValue& object, RE::GFxMovie* movie, RE::BGSHeadPart* headPart)
{
	using namespace ScaleformUtils;

	RegisterString(&object, movie, "partName", headPart->formEditorID.c_str());
	RegisterNumber(&object, "partFlags", headPart->flags.underlying());
	RegisterNumber(&object, "partType", headPart->type.underlying());
	RegisterString(&object, movie, "modelPath", headPart->GetModel());
	RegisterString(&object, movie, "chargenMorphPath", headPart->morphs[RE::BGSHeadPart::MorphIndex::kChargenMorph].GetModel());
	RegisterString(&object, movie, "raceMorphPath", headPart->morphs[RE::BGSHeadPart::MorphIndex::kRaceMorph].GetModel());
}

void SKSEScaleform_GetHeadParts::Call(RE::GFxFunctionHandler::Params& a_params)
{
	a_params.movie->CreateObject(a_params.retVal);

	RE::GFxValue partList{};
	a_params.movie->CreateArray(&partList);
	a_params.retVal->SetMember("parts", partList);

	RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
	RE::TESNPC * actorBase = player->GetBaseObject()->As<RE::TESNPC>();
	if(!actorBase)
		return;

	std::uint32_t numHeadParts = actorBase->numHeadParts;
	RE::BGSHeadPart** headParts = actorBase->headParts;
	if (actorBase->HasOverlays())
	{
		numHeadParts = std::max((std::uint32_t)actorBase->numHeadParts, actorBase->GetNumBaseOverlays());
		headParts = actorBase->GetBaseOverlays();
	}

	for(std::uint32_t i = 0; i < numHeadParts; i++)
	{
		RE::GFxValue partData{};
		a_params.movie->CreateObject(&partData);

		RE::BGSHeadPart * headPart = i < actorBase->numHeadParts ? actorBase->headParts[i] : headParts[i];

		RE::GFxValue headPartData{};
		a_params.movie->CreateObject(&headPartData);
		CreateHeadPartObject(headPartData, a_params.movie, headPart);
		partData.SetMember("base", headPartData);

		// Get the overlay, if there is one
		if(actorBase->HasOverlays()) {
			RE::BGSHeadPart * overlayPart = actorBase->GetHeadPartOverlayByType(static_cast<RE::BGSHeadPart::HeadPartType>(headPart->type.underlying()));
			if(overlayPart) {
				RE::GFxValue overlayPartData{};
				a_params.movie->CreateObject(&overlayPartData);
				CreateHeadPartObject(overlayPartData, a_params.movie, overlayPart);
				partData.SetMember("overlay", overlayPartData);
			}
		}

		partList.PushBack(partData);
	}
}

void SKSEScaleform_GetPlayerPosition::Call(RE::GFxFunctionHandler::Params& a_params)
{
	RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
	RE::NiNode * root = player->Get3D(false) ? player->Get3D(false)->AsNode() : nullptr;
	if(root) {
		a_params.movie->CreateObject(a_params.retVal);
		RE::GFxValue x{};
		x.SetNumber(root->local.translate.x);
		a_params.retVal->SetMember("x", x);
		RE::GFxValue y{};
		y.SetNumber(root->local.translate.y);
		a_params.retVal->SetMember("y", y);
		RE::GFxValue z{};
		z.SetNumber(root->local.translate.z);
		a_params.retVal->SetMember("z", z);
	}
}

void SKSEScaleform_GetPlayerRotation::Call(RE::GFxFunctionHandler::Params& a_params)
{
	RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
	RE::NiNode * root = player->Get3D(false) ? player->Get3D(false)->AsNode() : nullptr;
	if (root)
	{
		a_params.movie->CreateArray(a_params.retVal);
		for (std::uint32_t i = 0; i < 3 * 3; i++)
		{
			RE::GFxValue index{};
			index.SetNumber(((float*)(root->local.rotate.entry))[i]);
			a_params.retVal->PushBack(index);
		}
	}
}

void SKSEScaleform_SetPlayerRotation::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kArray);
	assert(a_params.args[0].GetArraySize() == 9);

	RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
	RE::NiNode * root = player->Get3D(false) ? player->Get3D(false)->AsNode() : nullptr;

	if (root)
	{
		for (std::uint32_t i = 0; i < 3 * 3; i++)
		{
			RE::GFxValue val{};
			a_params.args[0].GetElement(i, &val);
			if (val.GetType() != RE::GFxValue::ValueType::kNumber)
				break;

			((float*)root->local.rotate.entry)[i] = val.GetNumber();
		}

		RE::NiUpdateData ctx;
		root->UpdateWorldData(&ctx);
	}
}

void SKSEScaleform_GetRaceSexCameraRot::Call(RE::GFxFunctionHandler::Params& a_params)
{
	RE::RaceSexMenu * raceMenu = RE::UI::GetSingleton()->GetMenu<RE::RaceSexMenu>().get();
	if(raceMenu) {
		RE::NiNode * raceCamera = raceMenu->camera.cameraRoot.get();
		a_params.movie->CreateArray(a_params.retVal);
		for(std::uint32_t i = 0; i < 3 * 3; i++)
		{
			RE::GFxValue index{};
			index.SetNumber(((float*)raceCamera->local.rotate.entry)[i]);
			a_params.retVal->PushBack(index);
		}
	}
}

void SKSEScaleform_GetRaceSexCameraPos::Call(RE::GFxFunctionHandler::Params& a_params)
{
	RE::RaceSexMenu * raceMenu = RE::UI::GetSingleton()->GetMenu<RE::RaceSexMenu>().get();
	if(raceMenu) {
		RE::NiNode * raceCamera = raceMenu->camera.cameraRoot.get();
		a_params.movie->CreateObject(a_params.retVal);
		RE::GFxValue x{};
		x.SetNumber(raceCamera->local.translate.x);
		a_params.retVal->SetMember("x", x);
		RE::GFxValue y{};
		y.SetNumber(raceCamera->local.translate.y);
		a_params.retVal->SetMember("y", y);
		RE::GFxValue z{};
		z.SetNumber(raceCamera->local.translate.z);
		a_params.retVal->SetMember("z", z);
	}
}

void SKSEScaleform_SetRaceSexCameraPos::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kObject);

	RE::RaceSexMenu * raceMenu = RE::UI::GetSingleton()->GetMenu<RE::RaceSexMenu>().get();
	if(raceMenu) {
		RE::NiNode * raceCamera = raceMenu->camera.cameraRoot.get();

		RE::GFxValue val{};
		a_params.args[0].GetMember("x", &val);
		if(val.GetType() == RE::GFxValue::ValueType::kNumber)
			raceCamera->local.translate.x = val.GetNumber();

		a_params.args[0].GetMember("y", &val);
		if(val.GetType() == RE::GFxValue::ValueType::kNumber)
			raceCamera->local.translate.y = val.GetNumber();

		a_params.args[0].GetMember("z", &val);
		if(val.GetType() == RE::GFxValue::ValueType::kNumber)
			raceCamera->local.translate.z = val.GetNumber();

		RE::NiUpdateData ctx;
		raceCamera->UpdateWorldData(&ctx);
	}
}

void SKSEScaleform_CreateMorphEditor::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	REX::W32::ID3D11DeviceContext* r3dCtx = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	if (!r3dCtx) { // This shouldnt happen
		SKSE::log::error("{} - No REX::W32::D3D device context available.", __FUNCTION__);
		return;
	}

	REX::W32::ComPtr<REX::W32::ID3D11Device> r3dDev;
	r3dCtx->GetDevice(r3dDev.GetAddressOf());
	if (!r3dDev.Get()) { // This shouldnt happen
		SKSE::log::error("{} - Failed to acquire device from context.", __FUNCTION__);
		return;
	}

	// CDXD3DeviceInfo stores global d3d11.h COM pointers. REX::W32 mirrors the same ABI, so bridge via raw-pointer transfer (no double-free). The renderer keeps its own ref to the context; the ComPtr ctor takes an owning ref.
	REX::W32::ComPtr<REX::W32::ID3D11Device> pDevice;
	pDevice.Attach(reinterpret_cast<REX::W32::ID3D11Device*>(r3dDev.Detach()));
	REX::W32::ComPtr<REX::W32::ID3D11DeviceContext> pDeviceContext(r3dCtx);

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
		SKSE::log::error("{} - Failed to setup world.", __FUNCTION__);
		return;
	}

	RE::PlayerCharacter * player = (RE::PlayerCharacter::GetSingleton());
	if (!player) {
		SKSE::log::error("{} - Invalid working actor.", __FUNCTION__);
		return;
	}

	RE::TESNPC * actorBase = player->GetBaseObject() ? player->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (!actorBase) {
		SKSE::log::error("{} - No actor base.", __FUNCTION__);
		return;
	}

	RE::NiNode * rootFaceGen = GetFaceGenNiNode(player);
	if (!rootFaceGen) {
		SKSE::log::error("{} - No FaceGen node.", __FUNCTION__);
		return;
	}

	g_World.SetWorkingActor(player);

	RE::BSFaceGenAnimationData * animationData = player->GetFaceGenAnimationData();
	if (animationData) {
		RE::BSFaceGenManager::GetSingleton()->isReset = 0;
		// Legacy keyFrames[] maps to the contiguous 0x20-byte keyframe blocks starting at
		// expressionKeyFrame. Legacy enum: kKeyframeType_Expression=0 .. kKeyframeType_Phoneme=3.
		constexpr std::uint32_t kKeyframeType_Expression = 0;
		constexpr std::uint32_t kKeyframeType_Phoneme = 3;
		auto* keyFrames = reinterpret_cast<RE::BSFaceGenKeyframeMultiple*>(&animationData->expressionKeyFrame);
		for (std::uint32_t t = kKeyframeType_Expression; t <= kKeyframeType_Phoneme; t++)
		{
			RE::BSFaceGenKeyframeMultiple * keyframe = &keyFrames[t];
			for (std::uint32_t i = 0; i < keyframe->count; i++)
				keyframe->values[i] = 0.0;
			keyframe->isUpdated = 0;
		}
		SKEE::UpdateModelFace(rootFaceGen);
	}

	std::uint32_t numHeadParts = actorBase->numHeadParts;
	RE::BGSHeadPart ** headParts = actorBase->headParts;
	if (actorBase->HasOverlays()) {
		numHeadParts = actorBase->GetNumBaseOverlays();
		headParts = actorBase->GetBaseOverlays();
	}

	// What??
	if (!headParts) {
		SKSE::log::error("{} - No head parts found.", __FUNCTION__);
		return;
	}

	std::set<RE::BGSHeadPart*> extraParts; // Collect extra hair parts
	RE::BGSHeadPart * hairPart = actorBase->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kHair);
	if (hairPart) {
		RE::BGSHeadPart * extraPart = NULL;
		for (std::uint32_t p = 0; p < hairPart->extraParts.size(); p++) {
			extraPart = hairPart->extraParts[p];
			if (extraPart)
				extraParts.insert(extraPart);
		}
	}

	for(std::uint32_t i = 0; i < rootFaceGen->children.size(); i++)
	{
		for(std::uint32_t h = 0; h < numHeadParts; h++) {
			RE::BGSHeadPart * headPart = headParts[h];
			if (!headPart)
				continue;

			RE::NiAVObject * object = rootFaceGen->children[i].get();
			if (!object)
				continue;

			if(headPart->formEditorID == RE::BSFixedString(object->name)) {
				RE::NiGeometry* legacyGeometry = object ? object->AsNiGeometry() : nullptr;
				RE::BSTriShape* geometry = object ? object->AsTriShape() : nullptr;

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

				if (!headPart->type.any(RE::BGSHeadPart::HeadPartType::kFace))
					mesh->SetLocked(true);

				mesh->SetTransform(DirectX::XMMatrixTranslation(g_sculptOffsetX, g_sculptOffsetY, g_sculptOffsetZ));

				g_World.AddMesh(mesh);
				break;
			}
		}
	}

	if (animationData) {
		animationData->exprOverride = 0;
		animationData->Reset(1.0f, true, true, false, false);
		RE::BSFaceGenManager::GetSingleton()->isReset = 1;
		SKEE::UpdateModelFace(rootFaceGen);
	}

	a_params.movie->CreateObject(a_params.retVal);
	RegisterNumber(a_params.retVal, "width", initParams.viewportWidth);
	RegisterNumber(a_params.retVal, "height", initParams.viewportHeight);
}

void SKSEScaleform_ReleaseMorphEditor::Call(RE::GFxFunctionHandler::Params& a_params)
{
	g_World.Release();
	delete g_Device;
	g_Device = nullptr;
}

void SKSEScaleform_BeginRotateMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	g_Camera.OnRotateBegin(a_params.args[0].GetNumber(), a_params.args[1].GetNumber());
};

void SKSEScaleform_DoRotateMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	g_Camera.OnRotate(a_params.args[0].GetNumber(), a_params.args[1].GetNumber());
};

void SKSEScaleform_EndRotateMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	g_Camera.OnRotateEnd();
};

void SKSEScaleform_BeginPanMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	g_Camera.OnMoveBegin(a_params.args[0].GetNumber(), a_params.args[1].GetNumber());
};

void SKSEScaleform_DoPanMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	g_Camera.OnMove(a_params.args[0].GetNumber(), a_params.args[1].GetNumber());
};

void SKSEScaleform_EndPanMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	g_Camera.OnMoveEnd();
};

void SKSEScaleform_BeginPaintMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	std::int32_t x = a_params.args[0].GetNumber();
	std::int32_t y = a_params.args[1].GetNumber();

	bool hitMesh = false;

	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushPickerBegin brushStroke(brush);
		brushStroke.SetMirror(brush->IsMirror());
		if (g_World.Pick(&g_Camera, x, y, brushStroke))
			hitMesh = true;
	}

	a_params.retVal->SetBoolean(hitMesh);
};

void SKSEScaleform_DoPaintMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	std::int32_t x = a_params.args[0].GetNumber();
	std::int32_t y = a_params.args[1].GetNumber();

	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushPickerUpdate brushStroke(brush);
		brushStroke.SetMirror(brush->IsMirror());
		g_World.Pick(&g_Camera, x, y, brushStroke);
	}
};

void SKSEScaleform_EndPaintMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	CDXBrush * brush = g_World.GetCurrentBrush();
	if(brush)
		brush->EndStroke();
};

void SKSEScaleform_DoHoverMesh::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kNumber);

	std::int32_t x = a_params.args[0].GetNumber();
	std::int32_t y = a_params.args[1].GetNumber();

	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush) {
		CDXBrushTranslator translator(brush, static_cast<CDXBrushMesh*>(g_World.GetNthMesh(0)), static_cast<CDXBrushMesh*>(g_World.GetNthMesh(1)));
		g_World.Pick(&g_Camera, x, y, translator);
	}
};

void SKSEScaleform_GetCurrentBrush::Call(RE::GFxFunctionHandler::Params& a_params)
{
	CDXBrush * brush = g_World.GetCurrentBrush();
	if (brush)
		a_params.retVal->SetNumber(brush->GetType());
	else
		a_params.retVal->SetNull();
}

void SKSEScaleform_SetCurrentBrush::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);

	CDXBrush::BrushType brushType = (CDXBrush::BrushType)(std::uint32_t)a_params.args[0].GetNumber();
	CDXBrush * brush = g_World.GetBrush(brushType);
	if (brush)
		g_World.SetCurrentBrush(brushType);

	a_params.retVal->SetBoolean(brush != NULL);
}

void SKSEScaleform_GetBrushes::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	a_params.movie->CreateArray(a_params.retVal);

	for (const auto& brush : g_World.GetBrushes()) {
		RE::GFxValue object{};
		a_params.movie->CreateObject(&object);
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
		a_params.retVal->PushBack(object);
	}
}

void SKSEScaleform_SetBrushData::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kObject);

	CDXBrush::BrushType brushType = (CDXBrush::BrushType)(std::uint32_t)a_params.args[0].GetNumber();
	CDXBrush * brush = g_World.GetBrush(brushType);
	if (brush) {
		RE::GFxValue radius{};
		a_params.args[1].GetMember("radius", &radius);
		RE::GFxValue strength{};
		a_params.args[1].GetMember("strength", &strength);
		RE::GFxValue falloff{};
		a_params.args[1].GetMember("falloff", &falloff);
		RE::GFxValue mirror{};
		a_params.args[1].GetMember("mirror", &mirror);

		brush->SetProperty(CDXBrush::kBrushProperty_Radius, CDXBrush::kBrushPropertyValue_Value, radius.GetNumber());
		brush->SetProperty(CDXBrush::kBrushProperty_Strength, CDXBrush::kBrushPropertyValue_Value, strength.GetNumber());
		brush->SetProperty(CDXBrush::kBrushProperty_Falloff, CDXBrush::kBrushPropertyValue_Value, falloff.GetNumber());
		brush->SetMirror(mirror.GetNumber() > 0.0 ? true : false);

		a_params.retVal->SetBoolean(true);
	}
	else {
		a_params.retVal->SetBoolean(false);
	}
}

void SKSEScaleform_GetMeshes::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	a_params.movie->CreateArray(a_params.retVal);

	for (std::uint32_t i = 0; i < g_World.GetNumMeshes(); i++) {
		CDXMesh * mesh = g_World.GetNthMesh(i);
		if (mesh && mesh->IsEditable()) {
			RE::GFxValue object{};
			a_params.movie->CreateObject(&object);
			RegisterString(&object, a_params.movie, "name", mesh->GetName());
			RegisterNumber(&object, "meshIndex", i);
			RegisterBool(&object, "wireframe", mesh->ShowWireframe());
			RegisterBool(&object, "locked", mesh->IsLocked());
			RegisterBool(&object, "visible", mesh->IsVisible());
			RegisterBool(&object, "morphable", mesh->IsMorphable());
			RegisterNumber(&object, "vertices", mesh->GetVertexCount());

			auto material = mesh->GetMaterial();
			if (material) {
				DirectX::XMFLOAT4 fColor = material->GetWireframeColor();
				std::uint32_t color = 0xFF000000 | (std::uint32_t)(fColor.x * 255) << 16 | (std::uint32_t)(fColor.y * 255) << 8 | (std::uint32_t)(fColor.z * 255);
				RegisterNumber(&object, "wfColor", color);
			}

			a_params.retVal->PushBack(object);
		}
	}
}

void SKSEScaleform_SetMeshData::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 2);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kObject);

	std::uint32_t i = (std::uint32_t)a_params.args[0].GetNumber();

	CDXEditableMesh * mesh = dynamic_cast<CDXEditableMesh*>(g_World.GetNthMesh(i));
	if (mesh) {
		RE::GFxValue wireframe{};
		a_params.args[1].GetMember("wireframe", &wireframe);
		RE::GFxValue locked{};
		a_params.args[1].GetMember("locked", &locked);
		RE::GFxValue visible{};
		a_params.args[1].GetMember("visible", &visible);
		RE::GFxValue wfColor{};
		a_params.args[1].GetMember("wfColor", &wfColor);

		mesh->SetLocked(locked.GetBool());
		mesh->SetShowWireframe(wireframe.GetBool());
		mesh->SetVisible(visible.GetBool());
		auto material = mesh->GetMaterial();
		if (material) {
			std::uint32_t color = wfColor.GetNumber();
			material->SetWireframeColor(DirectX::XMFLOAT4(((color >> 16) & 0xFF) / 255.0, ((color >> 8) & 0xFF) / 255.0, (color & 0xFF) / 255.0, 1.0f));
		}

		a_params.retVal->SetBoolean(true);
	}
	else {
		a_params.retVal->SetBoolean(false);
	}
}

void SKSEScaleform_GetActionLimit::Call(RE::GFxFunctionHandler::Params& a_params)
{
	a_params.retVal->SetNumber(g_undoStack.GetLimit());
}

void SKSEScaleform_UndoAction::Call(RE::GFxFunctionHandler::Params& a_params)
{
	a_params.retVal->SetNumber(g_undoStack.Undo(true));
}

void SKSEScaleform_RedoAction::Call(RE::GFxFunctionHandler::Params& a_params)
{
	a_params.retVal->SetNumber(g_undoStack.Redo(true));
}

void SKSEScaleform_GoToAction::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);

	std::int32_t previous = g_undoStack.GetIndex();
	std::int32_t result = g_undoStack.GoTo(a_params.args[0].GetNumber(), true);

	if (result != previous) {
		RE::Actor * actor = g_World.GetWorkingActor();
		if (actor) {
			RE::NiNode * rootFaceGen = GetFaceGenNiNode(actor);
			SKEE::UpdateModelFace(rootFaceGen);
		}
	}

	a_params.retVal->SetNumber(result);
}

void SKSEScaleform_GetMeshCameraRadius::Call(RE::GFxFunctionHandler::Params& a_params)
{
	a_params.retVal->SetNumber(g_Camera.GetRadius());
}

void SKSEScaleform_SetMeshCameraRadius::Call(RE::GFxFunctionHandler::Params& a_params)
{
	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kNumber);

	g_Camera.SetRadius(a_params.args[0].GetNumber());
	g_Camera.Update();
}

namespace {
// std-filesystem path join into a fixed buffer (replaces shlwapi PathCombine).
void SKEEPathCombine(char (&dst)[REX::W32::MAX_PATH], const char* dir, const char* file)
{
	auto p = std::filesystem::path(dir) / file;
	std::snprintf(dst, sizeof(dst), "%s", p.string().c_str());
}

// REX::W32::FILETIME (100ns ticks since 1601-01-01, UTC) -> Flash Date params [7].
void FileTimeToDateParams(const REX::W32::FILETIME& ft, double* out /* [7] */)
{
	std::uint64_t ticks = static_cast<std::uint64_t>(ft);
	// 100ns ticks since 1601-01-01 -> Unix seconds (since 1970-01-01).
	const std::int64_t kFiletimeToUnixOffset = 11644473600LL;  // seconds between 1601 and 1970
	std::time_t t = static_cast<std::time_t>(static_cast<std::int64_t>(ticks / 10000000ULL) - kFiletimeToUnixOffset);
	std::tm tm{};
	gmtime_s(&tm, &t);  // FILETIME is UTC
	out[0] = static_cast<double>(tm.tm_year + 1900);  // year
	out[1] = static_cast<double>(tm.tm_mon);          // Flash month is 0-11 (tm_mon already is)
	out[2] = static_cast<double>(tm.tm_mday);         // day
	out[3] = static_cast<double>(tm.tm_hour);         // hour
	out[4] = static_cast<double>(tm.tm_min);          // minute
	out[5] = static_cast<double>(tm.tm_sec);          // second
	out[6] = static_cast<double>((ticks % 10000000ULL) / 10000ULL);  // milliseconds
}
}

void ReadFileDirectory(const char * lpFolder, const char ** lpFilePattern, std::uint32_t numPatterns, std::function<void(char*, REX::W32::WIN32_FIND_DATAA &, bool)> file)
{
	char szFullPattern[REX::W32::MAX_PATH];
	REX::W32::WIN32_FIND_DATAA FindFileData;
	REX::W32::HANDLE hFindFile;
	// first we are going to process any subdirectories
	SKEEPathCombine(szFullPattern, lpFolder, "*");
	hFindFile = REX::W32::FindFirstFileA(szFullPattern, &FindFileData);
	if (hFindFile != REX::W32::INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FindFileData.fileAttributes & REX::W32::FILE_ATTRIBUTE_DIRECTORY)
			{
				// found a subdirectory; recurse into it
				SKEEPathCombine(szFullPattern, lpFolder, FindFileData.fileName);
				if (FindFileData.fileName[0] == '.')
					continue;

				file(szFullPattern, FindFileData, true);
			}
		} while (REX::W32::FindNextFileA(hFindFile, &FindFileData));
		REX::W32::FindClose(hFindFile);
	}
	// now we are going to look for the matching files
	for (std::uint32_t i = 0; i < numPatterns; i++)
	{
		SKEEPathCombine(szFullPattern, lpFolder, lpFilePattern[i]);
		hFindFile = REX::W32::FindFirstFileA(szFullPattern, &FindFileData);
		if (hFindFile != REX::W32::INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(FindFileData.fileAttributes & REX::W32::FILE_ATTRIBUTE_DIRECTORY))
				{
					// found a file; do something with it
					SKEEPathCombine(szFullPattern, lpFolder, FindFileData.fileName);
					file(szFullPattern, FindFileData, false);
				}
			} while (REX::W32::FindNextFileA(hFindFile, &FindFileData));
			REX::W32::FindClose(hFindFile);
		}
	}
}

void SKSEScaleform_GetExternalFiles::Call(RE::GFxFunctionHandler::Params& a_params)
{
	using namespace ScaleformUtils;

	assert(a_params.argCount >= 1);
	assert(a_params.args[0].GetType() == RE::GFxValue::ValueType::kString);
	assert(a_params.args[1].GetType() == RE::GFxValue::ValueType::kArray);

	const char * path = a_params.args[0].GetString();
	
	std::uint32_t numPatterns = a_params.args[1].GetArraySize();

	const char ** patterns = (const char **)RE::GMemory::Alloc(numPatterns * sizeof(const char*));
	for (std::uint32_t i = 0; i < numPatterns; i++) {
		RE::GFxValue str{};
		a_params.args[1].GetElement(i, &str);
		patterns[i] = str.GetString();
	}

	a_params.movie->CreateArray(a_params.retVal);

	ReadFileDirectory(path, patterns, numPatterns, [&a_params](char* dirPath, REX::W32::WIN32_FIND_DATAA & fileData, bool dir)
	{
		RE::GFxValue fileInfo{};
		a_params.movie->CreateObject(&fileInfo);
		RegisterString(&fileInfo, a_params.movie, "path", dirPath);
		RegisterString(&fileInfo, a_params.movie, "name", fileData.fileName);
		std::uint64_t fileSize = (std::uint64_t)fileData.fileSizeHi << 32 | fileData.fileSizeLo;
		RegisterNumber(&fileInfo, "size", fileSize);
		double dateParams[7];
		FileTimeToDateParams(fileData.lastWriteTime, dateParams);
		RE::GFxValue date{};
		RE::GFxValue params[7] = {};
		params[0].SetNumber(dateParams[0]);
		params[1].SetNumber(dateParams[1]); // Flash month is 0-11 (already converted)
		params[2].SetNumber(dateParams[2]);
		params[3].SetNumber(dateParams[3]);
		params[4].SetNumber(dateParams[4]);
		params[5].SetNumber(dateParams[5]);
		params[6].SetNumber(dateParams[6]);
		a_params.movie->CreateObject(&date, "Date", params, 7);
		fileInfo.SetMember("lastModified", date);
		RegisterBool(&fileInfo, "directory", dir);
		a_params.retVal->PushBack(fileInfo);
	});

	RE::GMemory::Free(patterns);
}

