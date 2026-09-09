#include <RE/A/Actor.h>
#include "SKEETasks.h"
#include <RE/M/MemoryManager.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSShaderProperty.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiExtraData.h>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiSkinPartition.h>
#include <RE/N/NiSmartPointer.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESModelTri.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>
#include <RE/G/GameSettingCollection.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFile.h>
#include <RE/V/VertexDesc.h>
#include <REL/Relocation.h>
#include "NiRTTIUtils.h"
#include <SKSE/API.h>
#include <SKSE/Interfaces.h>

#include "FaceMorphInterface.h"
#include "PartHandler.h"
#include "NifUtils.h"
#include "SKEEHooks.h"

#include "StringTable.h"
#include "ShaderUtilities.h"
#include "Utilities.h"
#include "Morpher.h"

// CommonLibSSE-NG has no BSFileUtil; replicate the legacy line reader over a
// RE::BSResourceNiBinaryStream (read bytes until newline or EOF).
static bool SKEE_ReadLine(RE::BSResourceNiBinaryStream* file, std::string* out)
{
	out->clear();
	char c;
	while (file->get(c)) {
		if (c == '\n') {
			return true;
		}
		if (c != '\r') {
			out->push_back(c);
		}
	}
	return !out->empty();
}



#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include <cstdint>

extern StringTable			g_stringTable;

#include <map>
#include <vector>


extern bool	g_enableFaceNormalRecalculate;

extern float g_sliderMultiplier;
extern float g_sliderInterval;
extern PartSet	g_partSet;
extern FaceMorphInterface g_morphInterface;
extern std::string g_raceTemplate;
extern bool	g_extendedMorphs;
extern bool g_allowAllMorphs;

extern SKSE::PluginHandle g_pluginHandle;

void RaceMap::Revert()
{
	clear();
}

void FaceMorphInterface::Revert()
{
	m_valueMap.clear();
	m_sculptStorage.clear();
}

void FaceMorphInterface::RevertInternals()
{
	m_internalMap.clear();
}

skee_u32 FaceMorphInterface::GetVersion()
{
	return kSerializationVersion;
}

bool SliderSet::for_each_slider(std::function<bool(SliderGenderPtr)> func)
{
	// Iterate the list of SliderSet
	for (auto rit = begin(); rit != end(); ++rit)
	{
		// Iterate the SliderMap
		for (auto it = (*rit)->begin(); it != (*rit)->end(); ++it)
		{
			if (func(it->second))
				return true;
		}
	}

	return false;
}

SliderSetPtr RaceMap::GetSliderSet(RE::TESRace * race)
{
	RaceMap::iterator it = find(race);
	if(it != end())
		return it->second;
	
	return NULL;
}

std::int32_t ReadTRIVertexCount(const char * triPath)
{
	if(triPath[0] == 0) {
		return -1;
	}

	char filePath[REX::W32::MAX_PATH] = { 0 };
	sprintf_s(filePath, REX::W32::MAX_PATH, "Meshes\\%s", triPath);

	// Cached file already exists, load it
	RE::BSResourceNiBinaryStream file(filePath);
	if (!file.good()) {
		return -1;
	}

	char header[0x08];
	file.read(header, 0x08);
	if(strncmp(header, "FRTRI003", 8) != 0)
		return -1;

	std::uint32_t vertexNum = 0;
	file.get(vertexNum);
	return vertexNum;
}

bool TRIFile::Load(const char * triPath)
{
	if (triPath[0] == 0) {
		return false;
	}

	char filePath[REX::W32::MAX_PATH] = { 0 };
	sprintf_s(filePath, REX::W32::MAX_PATH, "Meshes\\%s", triPath);

	RE::BSResourceNiBinaryStream file(filePath);
	if (!file.good()) {
		return false;
	}

	char header[0x08];
	file.read(header, 0x08);
	if (strncmp(header, "FRTRI003", 8) != 0)
		return false;

	file.get(vertexCount);

	std::uint32_t polytris = 0, polyquads = 0, unk2 = 0, unk3 = 0, 
		uvverts = 0, flags = 0, numMorphs = 0, numMods = 0, 
		modVerts = 0, unk7 = 0, unk8 = 0, unk9 = 0, unk10 = 0;

	file.get(polytris);
	file.get(polyquads);
	file.get(unk2);
	file.get(unk3);
	file.get(uvverts);
	file.get(flags);
	file.get(numMorphs);
	file.get(numMods);
	file.get(modVerts);
	file.get(unk7);
	file.get(unk8);
	file.get(unk9);
	file.get(unk10);

	// Skip reference verts
	file.seek(vertexCount * 3 * sizeof(float));

	// Skip polytris
	file.seek(polytris * 3 * sizeof(std::uint32_t));

	// Skip UV
	if (uvverts > 0)
		file.seek(uvverts * 2 * sizeof(float));

	// Skip text coords
	file.seek(polytris * 3 * sizeof(std::uint32_t));

	for (std::uint32_t i = 0; i < numMorphs; i++)
	{
		std::uint32_t strLen = 0;
		file.get(strLen);

		char * name = new char[strLen+1];
		for (std::uint32_t l = 0; l < strLen; l++)
		{
			file.get(name[l]);
		}
		name[strLen] = 0;

		float mult = 0.0f;
		file.get(mult);

		Morph morph;
		morph.name = name;
		morph.multiplier = mult;

		for (std::uint32_t v = 0; v < vertexCount; v++)
		{
			Morph::Vertex vert;
			file.get(vert);
			morph.vertices.push_back(vert);
		}

		morphs.insert(std::make_pair(morph.name, morph));
	}

	return true;
}

bool TRIFile::Apply(RE::BSGeometry * geometry, SKEEFixedString morphName, float relative)
{
	RE::BSFaceGenBaseMorphExtraData * extraData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
	if (!extraData)
		return false;

	// Found morph doesn't match the cached morph
	if (extraData->vertexCount != vertexCount)
		return false;

	// Morph name wasn't found
	auto morph = morphs.find(morphName);
	if (morph == morphs.end())
		return false;

	// What?
	if (extraData->vertexCount != morph->second.vertices.size())
		return false;

	std::uint32_t size = morph->second.vertices.size();
	for (std::uint32_t i = 0; i < size; i++)
	{
		auto & vert = morph->second.vertices.at(i);
		extraData->vertexData[i].x += (float)((double)vert.x * (double)morph->second.multiplier * (double)relative);
		extraData->vertexData[i].y += (float)((double)vert.y * (double)morph->second.multiplier * (double)relative);
		extraData->vertexData[i].z += (float)((double)vert.z * (double)morph->second.multiplier * (double)relative);
	}

	SKEE::UpdateModelFace(geometry);
	return true;
}

class RacePartVisitor
{
public:
	virtual bool Accept(RE::BGSHeadPart * headPart) = 0;
};

void VisitRaceParts(RE::TESRace * race, std::uint32_t gender, RacePartVisitor & visitor)
{
	RE::TESRace::FaceRelatedData* chargenData = race->faceRelatedData[gender];
	if(chargenData) {
		RE::BSTArray<RE::BGSHeadPart*> * headParts = chargenData->headParts;
		if(headParts) {
			for(std::uint32_t i = 0; i < headParts->size(); i++) {
				RE::BGSHeadPart* headPart = (*headParts)[i];
				if(headPart) {
					if(visitor.Accept(headPart))
						break;
				}
			}
		}
	}
}

class RacePartByType : public RacePartVisitor
{
public:
	RacePartByType(std::uint32_t partType) : m_type(partType), m_headPart(NULL) {}
	virtual bool Accept(RE::BGSHeadPart * headPart)
	{
		if(headPart->type.underlying() == m_type) {
			m_headPart = headPart;
			return true;
		}

		return false;
	}

	std::uint32_t			m_type;
	RE::BGSHeadPart		* m_headPart;
};

class RacePartDefaultGen : public RacePartVisitor
{
public:
	RacePartDefaultGen(RE::TESRace * sourceRace, RE::TESRace * targetRace, std::vector<RE::BSFixedString> * parts, std::uint32_t gender) : m_sourceRace(sourceRace), m_targetRace(targetRace), m_gender(gender), m_partList(parts), m_acceptDefault(false) {}

	virtual bool Accept(RE::BGSHeadPart * headPart)
	{
		RE::BSFixedString sourceMorphPath(headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].GetModel());
		if(sourceMorphPath == RE::BSFixedString(""))
			return false;

		auto it = std::find(m_partList->begin(), m_partList->end(), sourceMorphPath);
		if (it != m_partList->end()) { // Found part tri file
			if(headPart->type == RE::BGSHeadPart::HeadPartType::kFace)
				m_acceptDefault = true;
		} else {
			RacePartByType racePart(headPart->type.underlying());
			VisitRaceParts(m_targetRace, m_gender, racePart);

			RE::BGSHeadPart * targetPart = racePart.m_headPart;
			if(targetPart) {
				RE::BSFixedString targetMorphPath(targetPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].GetModel());
				if(targetMorphPath == RE::BSFixedString("")) {
					SKSE::log::error("{} - Could not bind default morphs for {} on {}[{}] using {}. No valid morph path.", __FUNCTION__, headPart->GetName(), m_sourceRace->GetName(), m_gender, m_targetRace->GetName(), sourceMorphPath.c_str());
					return false;
				}

				TRIModelData sourceData, targetData;
				g_morphInterface.GetModelTri(sourceMorphPath, sourceData);
				g_morphInterface.GetModelTri(targetMorphPath, targetData);

				if (sourceData.vertexCount == targetData.vertexCount && sourceData.vertexCount > 0 && targetData.vertexCount > 0) {
					// Bind additional morphs here, the source and target morphs are identical
					SKSE::log::debug("{} - Binding default morphs for {} ({}) on {}[{}] using {}. ({} - {} | {} - {})", __FUNCTION__, headPart->GetName(), sourceMorphPath.c_str(), m_sourceRace->GetName(), m_gender, m_targetRace->GetName(), sourceMorphPath.c_str(), sourceData.vertexCount, targetMorphPath.c_str(), targetData.vertexCount);
					auto titer = g_morphInterface.m_morphMap.find(targetMorphPath);
					if(titer != g_morphInterface.m_morphMap.end())
						g_morphInterface.m_morphMap.emplace(sourceMorphPath, titer->second);
					if(headPart->type == RE::BGSHeadPart::HeadPartType::kFace)
						m_acceptDefault = true;
				} else if(sourceData.vertexCount == 0 || targetData.vertexCount == 0) {
					SKSE::log::error("{} - Could not bind default morphs for {} on {}[{}] using {}. Invalid vertex count ({} - {} | {} - {}).", __FUNCTION__, headPart->GetName(), m_sourceRace->GetName(), m_gender, m_targetRace->GetName(), sourceMorphPath.c_str(), sourceData.vertexCount, targetMorphPath.c_str(), targetData.vertexCount);
				} else {
					SKSE::log::error("{} - Could not bind default morphs for {} on {}[{}] using {}. Vertex mismatch ({} - {} | {} - {}).", __FUNCTION__, headPart->GetName(), m_sourceRace->GetName(), m_gender, m_targetRace->GetName(), sourceMorphPath.c_str(), sourceData.vertexCount, targetMorphPath.c_str(), targetData.vertexCount);
				}
			}
		}

		return false;
	}

	std::uint32_t	m_gender;
	RE::TESRace	* m_sourceRace;
	RE::TESRace	* m_targetRace;
	std::vector<RE::BSFixedString>	* m_partList;
	bool	m_acceptDefault;
};

class RacePartFiles : public RacePartVisitor
{
public:
	RacePartFiles(std::vector<RE::BSFixedString> * parts) : m_parts(parts) {}
	virtual bool Accept(RE::BGSHeadPart * headPart)
	{
		if (headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].model.c_str() != RE::BSFixedString("").c_str())
			m_parts->push_back(headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].model);
		if (headPart->morphs[RE::BGSHeadPart::MorphIndices::kRaceMorph].model.c_str() != RE::BSFixedString("").c_str())
			m_parts->push_back(headPart->morphs[RE::BGSHeadPart::MorphIndices::kRaceMorph].model);
		
		return false;
	}

	std::vector<RE::BSFixedString>		* m_parts;
};

bool RaceMap::CreateDefaultMap(RE::TESRace * race)
{
	RE::TESRace * templateRace = GetRaceByName(g_raceTemplate);
	RaceMap::iterator rit = find(templateRace);
	if(rit != end()) { // Found NordRace slider maps

		std::vector<RE::BSFixedString> templateFiles;
		for (std::uint32_t gender = 0; gender <= 1; gender++) {
			RacePartFiles parts(&templateFiles);
			VisitRaceParts(templateRace, gender, parts);
		}

		bool acceptDefault = false;
		for(std::uint32_t gender = 0; gender <= 1; gender++) { // Iterate genders
			RacePartDefaultGen defaultGen(race, templateRace, &templateFiles, gender);
			VisitRaceParts(race, gender, defaultGen);
			acceptDefault = defaultGen.m_acceptDefault;
		}

		if(acceptDefault) {
			SliderSetPtr sliderMaps = rit->second;
			if(sliderMaps->size() > 0) {
				std::uint32_t addedMaps = 0;
				for(auto smit = sliderMaps->begin(); smit != sliderMaps->end(); ++smit) {
					if(AddSliderMap(race, *smit))
						addedMaps++;
				}
				if(addedMaps > 0) {
					SKSE::log::debug("{} - Added default slider maps for {} from {}", __FUNCTION__, race->GetName(), rit->first->GetName());
					return true;
				}
			}
		}
	}

	return false;
}

bool RaceMap::AddSliderMap(RE::TESRace * race, SliderMapPtr sliderMap)
{
	RaceMap::iterator it = find(race);
	if(it != end()) {
		//std::pair<SliderSet::iterator,bool> ret;
		auto ret = it->second->insert(sliderMap);
		return ret.second;
	} else {
		SliderSetPtr sliderMaps = std::make_shared<SliderSet>();
		sliderMaps->insert(sliderMap);
		emplace(race, sliderMaps);
		return true;
	}

	return false;
}

SliderInternalPtr FaceMorphInterface::GetSliderByIndex(RE::TESRace * race, std::uint32_t index)
{
	RaceSliders::iterator it = m_internalMap.find(race);
	if(it != m_internalMap.end()) {
		if(index < it->second.size())
			return it->second.at(index);
	}
	return NULL;
}

float ValueMap::GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString & name)
{
	ValueMap::iterator it = find(npc);
	if(it != end()) {
		return it->second.GetValue(name);
	}

	return 0.0;
}

void ValueMap::SetMorphValue(RE::TESNPC* npc, const SKEEFixedString & name, float value)
{
	ValueMap::iterator it = find(npc);
	if(it != end()) {
		it->second.SetValue(name, value);
	} else {
		ValueSet newSet;
		newSet.emplace(g_stringTable.GetString(name), value);
		emplace(npc, newSet);
	}
}

void ValueSet::SetValue(const SKEEFixedString & name, float value)
{
	ValueSet::iterator val = find(g_stringTable.GetString(name));
	if (val != end())
		val->second = value;
	else
		emplace(g_stringTable.GetString(name), value);
}

void ValueSet::ClearValue(const SKEEFixedString & name)
{
	ValueSet::iterator val = find(g_stringTable.GetString(name));
	if(val != end())
		erase(val);
}

float ValueSet::GetValue(const SKEEFixedString & name)
{
	ValueSet::iterator val = find(g_stringTable.GetString(name));
	if(val != end())
		return val->second;

	return 0.0;
}

ValueSet * ValueMap::GetValueSet(RE::TESNPC* npc)
{
	ValueMap::iterator it = find(npc);
	if(it != end()) {
		return &it->second;
	}

	return NULL;
}

void ValueMap::EraseNPC(RE::TESNPC * npc)
{
	auto it = find(npc);
	if (it != end())
		erase(it);
}

float FaceMorphInterface::GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString & name)
{
	return m_valueMap.GetMorphValueByName(npc, name);
}

void FaceMorphInterface::SetMorphValue(RE::TESNPC* npc, const SKEEFixedString & name, float value)
{
	return m_valueMap.SetMorphValue(npc, name, value);
}

void MorphMap::Revert()
{
	clear();
}

class ExtendedMorphCache : public MorphMap::Visitor
{
public:
	virtual bool Accept(SKEEFixedString morphName)
	{
		g_morphInterface.GetExtendedModelTri(morphName);
		return false;
	}
};

void FaceMorphInterface::LoadMods()
{
	RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler)
	{
		ForEachMod([&](RE::TESFile* modInfo)
		{
			std::string fixedPath = "Meshes\\";
			fixedPath.append(SLIDER_MOD_DIRECTORY);
			std::string modPath = modInfo->fileName;
			modPath.append("\\");

			ReadRaces(fixedPath, modPath, "races.ini");
			if (g_extendedMorphs)
				ReadMorphs(fixedPath, modPath, "morphs.ini");

			ReadPartReplacements(fixedPath, modPath, "replacements.ini");
		});

		if (g_extendedMorphs) {
			auto& headParts = dataHandler->GetFormArray<RE::BGSHeadPart>();
			for (std::uint32_t i = 0; i < headParts.size(); i++)
			{
				RE::BGSHeadPart* part = headParts[i];
				if (part && CacheHeadPartModel(part)) {

					RE::BSFixedString key = part->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].GetModel();

					// Cache all of the extended morphs
					ExtendedMorphCache extendedCache;
					VisitMorphMap(key, extendedCache);
				}
			}
		}

		// Create default slider maps
		auto& races = dataHandler->GetFormArray<RE::TESRace>();
		for (std::uint32_t i = 0; i < races.size(); i++)
		{
			RE::TESRace* race = races[i];
			if (race) {

				if (g_allowAllMorphs) {
					// GMST names mirror the legacy FacePresetList table order
					static const char* kFacePresetSettings[RE::TESRace::FaceRelatedData::kNumVariants] = { "iNoseMorphCount", "iBrowMorphCount", "iEyeMorphCount", "iLipMorphCount" };
					auto* settingCollection = RE::GameSettingCollection::GetSingleton();
					for (std::uint32_t i = 0; i <= 1; i++) {
						auto* faceData = race->faceRelatedData[i];
						if (faceData) {
							for (std::uint32_t t = 0; t < RE::TESRace::FaceRelatedData::kNumVariants; t++) {
								faceData->availableMorphs[t].morphFlags = 0xFFFFFFFF;
								faceData->availableMorphs[t].unk04 = 0xFFFFFFFF;
								faceData->numFlagsSet[t] = 0;
								if (settingCollection) {
									auto* gameSetting = settingCollection->GetSetting(kFacePresetSettings[t]);
									if (gameSetting) {
										faceData->numFlagsSet[t] = gameSetting->data.i;
									}
								}
							}
						}
					}
				}

				if (race->data.flags.any(RE::RACE_DATA::Flag::kFaceGenHead))
					m_raceMap.CreateDefaultMap(race);
			}
		}
	}
}

bool FaceMorphInterface::CacheHeadPartModel(RE::BGSHeadPart * headPart, bool cacheTRI)
{
	RE::BSFixedString modelPath = headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].GetModel();
	if (modelPath == RE::BSFixedString(""))
		return false;

	ModelMap::iterator it = m_modelMap.find(modelPath);
	if (it == m_modelMap.end()) {
		TRIModelData data;

		data.morphModel = &headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph];
		if (!cacheTRI) {
			data.vertexCount = ReadTRIVertexCount(modelPath.c_str());
		}
		else {
			data.triFile = std::make_shared<TRIFile>();
			data.triFile->Load(modelPath.c_str());
			data.vertexCount = data.triFile->vertexCount;
		}

		m_modelMap.emplace(modelPath, data);
	}
	else if (cacheTRI && !it->second.triFile) {
		it->second.triFile = std::make_shared<TRIFile>();
		it->second.triFile->Load(modelPath.c_str());
	}

	return true;
}

bool FaceMorphInterface::GetModelTri(const SKEEFixedString & filePath, TRIModelData & modelData)
{
	ModelMap::iterator it = m_modelMap.find(filePath);
	if (it != m_modelMap.end()) {
		modelData = it->second;
		return true;
	}

	return false;
}

TRIModelData & FaceMorphInterface::GetExtendedModelTri(const SKEEFixedString & morphName, bool cacheTRI)
{
	std::string filePath(SLIDER_DIRECTORY);
	filePath.append(morphName.c_str());
	RE::BSFixedString morphFile(filePath.c_str());
	ModelMap::iterator it = m_modelMap.find(morphFile);
	if(it == m_modelMap.end()) {
		// Raw-allocate on the game heap with the game vtable (CommonLib does not
		// define TESModelTri's constructor); matches the legacy Heap_Allocate +
		// vtable pattern. Set the model through the virtual SetModel (vtable slot 05,
		// TESModelTri's override) exactly like the legacy SetModelName call.
		RE::TESModelTri* xData = RE::malloc<RE::TESModelTri>();
		std::memset(xData, 0, sizeof(RE::TESModelTri));
		*reinterpret_cast<std::uintptr_t*>(xData) = RE::TESModelTri::VTABLE[0].address();
		xData->SetModel(morphFile.c_str());

		TRIModelData data;
		data.morphModel = xData;
		if (!cacheTRI) {
			data.vertexCount = ReadTRIVertexCount(morphFile.c_str());
		}
		else {
			data.triFile = std::make_shared<TRIFile>();
			data.triFile->Load(morphFile.c_str());
			data.vertexCount = data.triFile->vertexCount;
		}

		auto ret = m_modelMap.emplace(morphFile, data);
		return ret.first->second;
	}
	else if(cacheTRI && !it->second.triFile) {
		it->second.triFile = std::make_shared<TRIFile>();
		it->second.triFile->Load(morphFile.c_str());
	}
	
	return it->second;
}

bool MorphMap::Visit(const SKEEFixedString & key, Visitor & visitor)
{
	MorphMap::iterator it = find(key);
	if(it != end())
	{
#ifdef _DEBUG_MORPHAPPLICATOR
		SKSE::log::debug("{} - Applying {} additional morphs to {}", __FUNCTION__, it->second.size(), key.c_str());
#endif
		for(auto iter = it->second.begin(); iter != it->second.end(); ++iter)
		{
#ifdef _DEBUG_MORPHAPPLICATOR
			SKSE::log::debug("{} - Visting {}", __FUNCTION__, (*iter).c_str());
#endif
			if(visitor.Accept(*iter))
				break;
		}
#ifdef _DEBUG_MORPHAPPLICATOR
#endif
		return true;
	}
#ifdef _DEBUG_MORPHAPPLICATOR
	else {
		SKSE::log::debug("{} - No additional morphs for {}", __FUNCTION__, key.c_str());
	}
#endif

	return false;
}

bool FaceMorphInterface::VisitMorphMap(const SKEEFixedString & key, MorphMap::Visitor & visitor)
{
	//key = toLower(key);
	return m_morphMap.Visit(key, visitor);
}

void MorphMap::AddMorph(const SKEEFixedString & key, const SKEEFixedString & value)
{
	//key = toLower(key);
	MorphMap::iterator it = find(key);
	if(it != end()) {
		if (std::find(it->second.begin(), it->second.end(), value) == it->second.end())
			it->second.push_back(value);
	} else {
		MorphSet firstSet;
		firstSet.push_back(value);
		emplace(key, firstSet);
	}
}

void FaceMorphInterface::ReadMorphs(std::string fixedPath, std::string modName, std::string fileName)
{
	std::string fullPath = fixedPath + modName + fileName;
	RE::BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.good()) {
		return;
	}

	std::uint32_t lineCount = 0;
	std::string str = "";
	while(SKEE_ReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if(str.length() == 0)
			continue;
		if(str.at(0) == '#')
			continue;

		std::vector<std::string> side = explode(str, '=');
		if(side.size() < 2) {
			SKSE::log::error("ReadMorphs Error - Line ({}) loading a morph from {} has no left-hand side.", lineCount, fullPath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		if(_strnicmp(lSide.c_str(), "extension", 9) != 0) {
			SKSE::log::error("ReadMorphs Error - Line ({}) loading a morph from {} invalid left-hand side.", lineCount, fullPath.c_str());
			continue;
		}

		std::vector<std::string> params = explode(rSide, ',');
		if(params.size() < 2) {
			SKSE::log::error("ReadMorphs Error - Line ({}) slider {} from {} has less than 2 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		// Trim all parameters
		for(std::uint32_t i = 0; i < params.size(); i++)
			params[i] = std::trim(params[i]);

		std::string key = params[0];
		for(std::uint32_t i = 1; i < params.size(); i++) {
#ifdef _DEBUG_DATAREADER
			SKSE::log::debug("ReadMorphs Info - Line ({}) added {} morph to {} from {}.", lineCount, params[i].c_str(), key.c_str(), fullPath.c_str());
#endif
			m_morphMap.AddMorph(key, params[i]);
		}
	}
}

void FaceMorphInterface::ReadRaces(std::string fixedPath, std::string modPath, std::string fileName)
{
	std::string fullPath = fixedPath + modPath + fileName;
	RE::BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.good()) {
		return;
	}

	std::map<std::string, SliderMapPtr> fileMap;

	std::uint32_t lineCount = 0;
	std::string str = "";
	while(SKEE_ReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if(str.length() == 0)
			continue;
		if(str.at(0) == '#')
			continue;

		std::vector<std::string> side = explode(str, '=');
		if(side.size() < 2) {
			SKSE::log::error("ReadRaces Error - Line ({}) loading a race from {} has insufficient parameters.", lineCount, fullPath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		std::vector<std::string> files = explode(rSide, ',');
		for(std::uint32_t i = 0; i < files.size(); i++)
			files[i] = std::trim(files[i]);

		for(std::uint32_t i = 0; i < files.size(); i++)
		{
			std::string pathOverride = modPath;
			if(files[i].at(0) == ':') {
				pathOverride = "";
				files[i].erase(0, 1);
			}

			SliderMapPtr sliderMap = NULL;
			std::map<std::string, SliderMapPtr>::iterator it = fileMap.find(files[i]);
			if(it != fileMap.end()) {
				sliderMap = it->second;
			} else {
				sliderMap = ReadSliders(fixedPath, pathOverride, files[i]);
				if(sliderMap) {
					fileMap.emplace(files[i], sliderMap);
				} else {
					SKSE::log::error("ReadRaces Error - Line ({}) failed to load slider map for {} from {}.", lineCount, lSide.c_str(), fullPath.c_str());
				}
			}

			if(sliderMap) {
#ifdef _DEBUG_DATAREADER
				SKSE::log::debug("ReadRaces Info - Line ({}) Loaded {} for Race {} from {}.", lineCount, files[i].c_str(), lSide.c_str(), fullPath.c_str());
#endif

				RE::TESRace * race = GetRaceByName(lSide);
				if(race)
					m_raceMap.AddSliderMap(race, sliderMap);
			}
		}
	}
}

void SliderMap::AddSlider(const SKEEFixedString & key, std::uint8_t gender, SliderInternal & sliderInternal)
{
	SliderMap::iterator it = find(key);
	if(it != end()) {
		it->second->slider[gender] = std::make_shared<SliderInternal>();
		it->second->slider[gender]->copy(&sliderInternal);
	} else {
		SliderGenderPtr sliderGender = std::make_shared<SliderGender>();
		sliderGender->slider[gender] = std::make_shared<SliderInternal>();
		sliderGender->slider[gender]->copy(&sliderInternal);
		emplace(key, sliderGender);
	}
}

SliderMapPtr FaceMorphInterface::ReadSliders(std::string fixedPath, std::string modPath, std::string fileName)
{
	SliderMapPtr sliderMap = NULL;
	std::string fullPath = fixedPath + modPath + fileName;
	RE::BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.good()) {
		return NULL;
	}

	sliderMap = std::make_shared<SliderMap>();

	std::uint8_t gender = 0;
	std::uint32_t lineCount = 0;
	std::string str = "";
	while(SKEE_ReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if(str.length() == 0)
			continue;
		if(str.at(0) == '#')
			continue;

		if(str.at(0) == '[')
		{
			str.erase(0, 1);
			if(_strnicmp(str.c_str(), "Male", 4) == 0)
				gender = 0;
			if(_strnicmp(str.c_str(), "Female", 6) == 0)
				gender = 1;
			continue;
		}

		std::vector<std::string> side = explode(str, '=');
		if(side.size() < 2) {
			SKSE::log::error("ReadSliders Error - Line ({}) slider from {} has no left-hand side.", lineCount, fullPath.c_str());
			continue;
		}
		
		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		std::vector<std::string> params = explode(rSide, ',');
		if(params.size() < 3) {
			SKSE::log::error("ReadSliders Error - Line ({}) slider {} from {} has less than 3 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		// Trim all parameters
		for(std::uint32_t i = 0; i < params.size(); i++)
			params[i] = std::trim(params[i]);

		SliderInternal sliderInternal;
		sliderInternal.name = lSide.c_str();
		sliderInternal.displayName = (std::string("$") + lSide).c_str();
		sliderInternal.category = atoi(params.at(0).c_str());
		if(sliderInternal.category == -1)
			sliderInternal.category = SliderInternal::kCategoryExtra;
		switch(sliderInternal.category)
		{
			case SliderInternal::kCategoryExpressions:
			case SliderInternal::kCategoryExtra:
			case SliderInternal::kCategoryBody:
			case SliderInternal::kCategoryHead:
			case SliderInternal::kCategoryFace:
			case SliderInternal::kCategoryEyes:
			case SliderInternal::kCategoryBrow:
			case SliderInternal::kCategoryMouth:
			case SliderInternal::kCategoryHair:
				break;
			default:
				SKSE::log::error("ReadSliders Error - Line ({}) loading slider {} from {} has invalid category ({}).", lineCount, lSide.c_str(), fullPath.c_str(), sliderInternal.category);
				continue;
				break;
		}
		if(_strnicmp(params[1].c_str(), "Slider", 6) == 0) {
			sliderInternal.type = SliderInternal::kTypeSlider;
		} else if(_strnicmp(params[1].c_str(), "Preset", 6) == 0) {
			sliderInternal.type = SliderInternal::kTypePreset;
		}  else if(_strnicmp(params[1].c_str(), "HeadPart", 8) == 0) {
			sliderInternal.type = SliderInternal::kTypeHeadPart;
		} else {
			SKSE::log::error("ReadSliders Error - Line ({}) loading slider {} from {} has invalid slider type ({}).", lineCount, lSide.c_str(), fullPath.c_str(), params[1].c_str());
			continue;
		}
		switch(sliderInternal.type)
		{
			case SliderInternal::kTypeSlider:
			{
				// Additional morphs are disabled
				if (!g_extendedMorphs)
					continue;

				if(params.size() < 4) {
					SKSE::log::error("ReadSliders Error - Line ({}) slider {} from {} has less than 4 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
					continue;
				}

				sliderInternal.lowerBound = params[2].c_str();
				sliderInternal.upperBound = params[3].c_str();

				if(SKEEFixedString(sliderInternal.lowerBound) == SKEEFixedString("None"))
					sliderInternal.lowerBound = "";
				if(SKEEFixedString(sliderInternal.upperBound) == SKEEFixedString("None"))
					sliderInternal.upperBound = "";
			}
			break;
			case SliderInternal::kTypePreset:
			{
				// Additional morphs are disabled
				if (!g_extendedMorphs)
					continue;

				if (params.size() < 4) {
					SKSE::log::error("ReadSliders Error - Line ({}) slider {} from {} has less than 4 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
					continue;
				}

				sliderInternal.lowerBound = params[2].c_str();
				std::uint32_t presetCount = atoi(params[3].c_str());
				if (presetCount > 255) {
					presetCount = 255;
					SKSE::log::warn("ReadSliders Warning - Line ({}) loading slider {} from {} has exceeded a preset count of {}.", lineCount, lSide.c_str(), fullPath.c_str(), presetCount);
				}
				sliderInternal.presetCount = presetCount;

			}
			break;
			case SliderInternal::kTypeHeadPart:
			{
				sliderInternal.presetCount = atoi(params[2].c_str());
			}
			break;
		}
#ifdef _DEBUG_DATAREADER
		SKSE::log::debug("ReadSliders Info - Line ({}) Added Slider ({}, {}, {}, {}, {}, {}) to Gender {} {} from {}.", lineCount, sliderInternal.name.c_str(), sliderInternal.category, sliderInternal.type, sliderInternal.presetCount, sliderInternal.lowerBound.c_str(), sliderInternal.upperBound.c_str(), gender, lSide.c_str(), fullPath.c_str());
#endif
		sliderMap->AddSlider(sliderInternal.name, gender, sliderInternal);
	}

	return sliderMap;
}

#ifdef _DEBUG_DATADUMP
void SliderMap::DumpMap()
{
	for(SliderMap::iterator it = begin(); it != end(); ++it)
	{
		SliderGender * gender = it->second;
		if(gender->slider[0])
			SKSE::log::debug("Slider - Name: {} Gender: Male Type: {} Cat: {} LowerBound: {} UpperBound: {} PresetCount: {}", it->first.c_str(), gender->slider[0]->type, gender->slider[0]->category, gender->slider[0]->lowerBound.c_str(), gender->slider[0]->upperBound.c_str(), gender->slider[0]->presetCount);
		if(gender->slider[1])
			SKSE::log::debug("Slider - Name: {} Gender: Female Type: {} Cat: {} LowerBound: {} UpperBound: {} PresetCount: {}", it->first.c_str(), gender->slider[1]->type, gender->slider[1]->category, gender->slider[1]->lowerBound.c_str(), gender->slider[1]->upperBound.c_str(), gender->slider[1]->presetCount);
	}
}

void RaceMap::DumpMap()
{
	for(RaceMap::iterator it = begin(); it != end(); ++it)
	{
		SKSE::log::debug("Race: {:08X} - {}", it->first->formID, it->first->GetName());
		for(SliderSet::iterator sit = it->second.begin(); sit != it->second.end(); ++sit)
		{
			SKSE::log::debug("Map: {:08X}", (*sit));
			(*sit)->DumpMap();
		}
	}
}

void MorphMap::DumpMap()
{
	for(MorphMap::iterator it = begin(); it != end(); ++it)
	{
		SKSE::log::debug("Morph: {}", it->first.c_str());
		DumpVisitor visitor;
		Visit(it->first, visitor);
	}
}

void MorphHandler::DumpAll()
{
	m_raceMap.DumpMap();
	m_morphMap.DumpMap();
}
#endif

SliderInternalPtr FaceMorphInterface::GetSlider(RE::TESRace * race, std::uint8_t gender, SKEEFixedString name)
{
	SliderSetPtr sliderMaps = m_raceMap.GetSliderSet(race);
	if(sliderMaps)
	{
		SliderInternalPtr sliderInternal;
		sliderMaps->for_each_slider([&](SliderGenderPtr genders) {
			sliderInternal = genders->slider[gender];
			if (sliderInternal && SKEEFixedString(sliderInternal->name) == name)
				return true;
			return false;
		});
		if (sliderInternal)
			return sliderInternal;
	}

	return NULL;
}

bool sortFixedStrings(SliderInternalPtr s1, SliderInternalPtr s2)
{
	return SKEEFixedString(s1->name) < SKEEFixedString(s2->name);
}

SliderList * FaceMorphInterface::CreateSliderList(RE::TESRace * race, std::uint8_t gender)
{
	// Clear the old map before building the new
	RaceSliders::iterator it = m_internalMap.find(race);
	if(it != m_internalMap.end()) {
		it->second.clear();
	}

	SliderSetPtr sliderMaps = m_raceMap.GetSliderSet(race);
	if(sliderMaps)
	{
		sliderMaps->for_each_slider([&](SliderGenderPtr genders) {
			SliderInternalPtr sliderInternal = genders->slider[gender];
			if (sliderInternal)
				AddSlider(race, sliderInternal);
			return false;
		});
	}

	// Return the list
	it = m_internalMap.find(race);
	if(it != m_internalMap.end()) {
		std::sort(it->second.begin(), it->second.end(), sortFixedStrings);
		it->second.resize(it->second.size());
		return &it->second;
	}

	return NULL;
}

void FaceMorphInterface::AddSlider(RE::TESRace * race, SliderInternalPtr & slider)
{
	RaceSliders::iterator it = m_internalMap.find(race);
	if(it != m_internalMap.end()) {
		it->second.push_back(slider);
	} else {
		SliderList newList;
		newList.push_back(slider);
		m_internalMap.emplace(race, newList);
	}
}

void FaceMorphInterface::ApplyMorph(RE::TESNPC * npc, RE::BGSHeadPart * headPart, RE::BSFaceGenNiNode * faceNode)
{
	char buffer[REX::W32::MAX_PATH];
	
	auto sculptTarget = GetSculptTarget(npc, false);
	if (sculptTarget) {
		if (headPart) {

			RE::NiAVObject * object = faceNode->GetObjectByName(RE::BSFixedString(headPart->GetName()));
			if (object) {
				RE::BSGeometry * geometry = object ? object->AsGeometry() : nullptr;
				if (geometry) {
					auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
					if (sculptHost) {
						RE::BSFaceGenBaseMorphExtraData * extraData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
						if (extraData) {
							for (auto data : *sculptHost)
								extraData->vertexData[data.first] += data.second;
						}
					}
				}
			}
		}
	}

	if (!g_extendedMorphs)
		return;
	
	ValueSet * valueSet = m_valueMap.GetValueSet(npc);
	if(!valueSet)
		return;

	std::uint8_t gender = npc->GetSex();
	
	for(auto it = valueSet->begin(); it != valueSet->end(); ++it)
	{
		SliderInternalPtr slider = GetSlider(npc->GetRace(), gender, *it->first);
		if(slider) {
			if(slider->type == SliderInternal::kTypePreset) {
				if(it->second != 0) { // There should be no zero morph for presets
					memset(buffer, 0, REX::W32::MAX_PATH);
					sprintf_s(buffer, REX::W32::MAX_PATH, "%s%d", slider->lowerBound.c_str(), (std::uint32_t)it->second);
					RE::BSFixedString morphName(buffer);
#ifdef _DEBUG_MORPH
					SKSE::log::debug("Applying Single Preset {} value {} to {}", morphName.c_str(), it->second, headPart->GetName());
#endif
					SKEE::FaceGen_ApplyMorphByPart(RE::BSFaceGenManager::GetSingleton(), faceNode, headPart, morphName, 1.0);
				}
			} else {
				RE::BSFixedString morphName = slider->lowerBound;
				if(it->second < 0)
					morphName = slider->lowerBound;
				if(it->second > 0)
					morphName = slider->upperBound;

				float relative = abs(it->second);
				if(relative > 1.0) {
					std::uint32_t count = (std::uint32_t)relative;
					float difference = relative - count;
					for(std::uint32_t i = 0; i < count; i++)
						SKEE::FaceGen_ApplyMorphByPart(RE::BSFaceGenManager::GetSingleton(), faceNode, headPart, morphName, 1.0);
					relative = difference;
				}
#ifdef _DEBUG_MORPH
				SKSE::log::debug("Applying Single Slider {} value {} to {}", morphName.c_str(), it->second, headPart->GetName());
#endif
				SKEE::FaceGen_ApplyMorphByPart(RE::BSFaceGenManager::GetSingleton(), faceNode, headPart, morphName, relative);
			}
		}
	}

	if (g_enableFaceNormalRecalculate)
	{
		SKEE_AddTask(SKSE::GetTaskInterface(), new SKSETaskApplyMorphNormals(RE::NiPointer<RE::NiAVObject>(faceNode)));
	}
}

void FaceMorphInterface::ApplyMorphs(RE::TESNPC * npc, RE::BSFaceGenNiNode * faceNode)
{
	char buffer[REX::W32::MAX_PATH];
	auto sculptTarget = GetSculptTarget(npc, false);
	if (sculptTarget) {
		VisitObjects(faceNode, [&](RE::NiAVObject* object)
		{
			if (RE::BSGeometry * geometry = object ? object->AsGeometry() : nullptr) {
				std::string headPartName = object->name.c_str();
				RE::BGSHeadPart * headPart = GetHeadPartByName(headPartName);
				if (headPart) {
					auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
					if (sculptHost) {
						RE::BSFaceGenBaseMorphExtraData * extraData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
						if (extraData) {
							for (auto data : *sculptHost)
								extraData->vertexData[data.first] += data.second;
						}
					}
				}
			}

			return false;
		});
	}

	if (!g_extendedMorphs)
		return;
	
	ValueSet * valueSet = m_valueMap.GetValueSet(npc);
	if(!valueSet)
		return;

	std::uint8_t gender = npc->GetSex();

	
	for(auto it = valueSet->begin(); it != valueSet->end(); ++it)
	{
		SliderInternalPtr slider = GetSlider(npc->GetRace(), gender, *it->first);
		if(slider) {
			if(slider->type == SliderInternal::kTypePreset) {
				if(it->second != 0) { // There should be no zero morph for presets
					memset(buffer, 0, REX::W32::MAX_PATH);
					sprintf_s(buffer, REX::W32::MAX_PATH, "%s%d", slider->lowerBound.c_str(), (std::uint32_t)it->second);
					SKEEFixedString morphName(buffer);
#ifdef _DEBUG_MORPH
					SKSE::log::debug("Applying Full Preset {} value {} to all parts", morphName.c_str(), it->second);
#endif
					SetMorph(npc, faceNode, morphName, 1.0);
				}
			} else {
				auto morphName = slider->lowerBound;
				if(it->second < 0.0)
					morphName = slider->lowerBound;
				if(it->second > 0.0)
					morphName = slider->upperBound;

				float relative = abs(it->second);
				if(relative > 1.0) {
					std::uint32_t count = (std::uint32_t)relative;
					float difference = relative - count;
					for(std::uint32_t i = 0; i < count; i++)
						SetMorph(npc, faceNode, morphName, 1.0);
					relative = difference;
				}

#ifdef _DEBUG_MORPH
				SKSE::log::debug("Applying Full Slider {} value {} to all parts", morphName.c_str(), it->second);
#endif
				SetMorph(npc, faceNode, morphName, relative);
			}
		}
	}

	if (g_enableFaceNormalRecalculate)
	{
		SKEE_AddTask(SKSE::GetTaskInterface(), new SKSETaskApplyMorphNormals(RE::NiPointer<RE::NiAVObject>(faceNode)));
	}
}

void FaceMorphInterface::SetMorph(RE::TESNPC * npc, RE::BSFaceGenNiNode * faceNode, const SKEEFixedString& name, float relative)
{
#ifdef _DEBUG_MORPH
	SKSE::log::debug("Applying Morph {} to all parts", name);
#endif
	RE::BSFixedString morphName(name.c_str());
	SKEE::FaceGen_ApplyMorph(RE::BSFaceGenManager::GetSingleton(), faceNode, npc, morphName, relative);

	if (g_enableFaceNormalRecalculate)
	{
		SKEE_AddTask(SKSE::GetTaskInterface(), new SKSETaskApplyMorphNormals(RE::NiPointer<RE::NiAVObject>(faceNode), false));
	}
}

std::int32_t FaceMorphInterface::LoadSliders(RE::RaceMenuSliderArray * sliderArray, RE::RaceMenuSlider * slider)
{
	RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC * npc = player->GetActorBase();

	std::uint32_t sliderId = sliderArray->size();
	std::uint32_t morphIndex = SLIDER_OFFSET;

	currentList = CreateSliderList(player->GetRace(), npc->GetSex());
	if(!currentList)
		return sliderId;

	ValueSet * valueSet = m_valueMap.GetValueSet(npc);

	// Clean up invalid morphs
	if(valueSet) {
		ValueSet::iterator it = valueSet->begin();
		while (it != valueSet->end()) {
			bool foundMorph = false;
			for(auto mit = currentList->begin(); mit != currentList->end(); ++mit) {
				SliderInternalPtr slider = (*mit);
				if(SKEEFixedString(slider->name) == *it->first) {
					foundMorph = true;
					break;
				}
			}

			if (!foundMorph) {
				SKSE::log::debug("Erasing {}", it->first->c_str());
				valueSet->erase(it++);
			}
			else
				it++;
		}
	}

	std::uint32_t i = 0;
	for(auto it = currentList->begin(); it != currentList->end(); ++it)
	{
		SliderInternalPtr slider = (*it);

		float value = valueSet ? valueSet->GetValue(slider->name) : 0.0;

		std::uint32_t sliderIndex = morphIndex + i;
		float lowerBound = SKEEFixedString(slider->lowerBound) == SKEEFixedString("") ? 0.0 : -1.0;
		float upperBound = SKEEFixedString(slider->upperBound) == SKEEFixedString("") ? 0.0 : 1.0;
		float interval = g_sliderInterval;
		float lowerMultiplier = g_sliderMultiplier;
		float upperMultiplier = g_sliderMultiplier;

		if(slider->type == SliderInternal::kTypePreset) {
			lowerBound = 0.0;
			interval = 1;
			lowerMultiplier = 1.0;
			upperMultiplier = 1.0;
			upperBound = (float)slider->presetCount;
		} else if(slider->type == SliderInternal::kTypeHeadPart) {
			lowerBound = -1.0;
			interval = 1;
			lowerMultiplier = 1.0;
			upperMultiplier = 1.0;
			HeadPartList * headPartList = g_partSet.GetPartList(slider->presetCount);
			RE::BGSHeadPart * headPart = npc->GetHeadPartByType(static_cast<RE::TESNPC::HeadPartType>(slider->presetCount));
			std::int32_t partIndex = -1;
			if(headPart && headPartList)
				value = g_partSet.GetPartIndex(headPartList, headPart);
			if(headPartList)
				upperBound = ((float)headPartList->size()) - 1.0f;
			else
				upperBound = -1.0f;
		}

#ifdef _DEBUG_SLIDER
		SKSE::log::debug("Adding slider: {} Morph: {} Value: {} SliderID: {} Index: {}", slider->displayName.c_str(), slider->name.c_str(), value, sliderId, sliderIndex);
#endif

		RE::RaceMenuSlider newSlider = skee::MakeRaceMenuSlider(slider->category, slider->displayName.c_str(), "ChangeDoubleMorph", sliderId++, sliderIndex, 2, 1, lowerBound * lowerMultiplier, upperBound * upperMultiplier, value, interval);
		SKEE::AddRaceMenuSlider(sliderArray, &newSlider);
		i++;
	}

	return sliderId;
}

void FaceMorphInterface::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC * npc = player->GetActorBase();

	auto sculptData = m_sculptStorage.GetSculptTarget(npc, false);
	if (sculptData) {
		if (sculptData->size() > 0) {
			if (!intfc->OpenRecord('SCDT', kVersion)) {
				SKSE::log::error("{} - Failed to open record", __FUNCTION__);
			}

			std::uint16_t numValidParts = 0;
			for (auto part : *sculptData) {
				if (part.first->length() != 0 && part.second->size() > 0) {
					numValidParts++;
				}
			}

			intfc->WriteRecordData(&numValidParts, sizeof(numValidParts));
#ifdef _DEBUG
			SKSE::log::debug("{} - Saving {} sculpts", __FUNCTION__, numValidParts);
#endif
			if (numValidParts > 0) {
				for (auto part : *sculptData) {
					std::uint16_t diffCount = part.second->size();
					if (diffCount > 0) {
						if (!intfc->OpenRecord('SCPT', kVersion)) {
							SKSE::log::error("{} - Failed to open record", __FUNCTION__);
						}
						g_stringTable.WriteString(intfc, part.first);
						intfc->WriteRecordData(&diffCount, sizeof(diffCount));

#ifdef _DEBUG
						SKSE::log::debug("{} - Saving sculpt to {} with {} diffs", __FUNCTION__, part.first->c_str(), diffCount);
#endif

						for (auto diff : *part.second) {
							intfc->WriteRecordData(&diff.first, sizeof(diff.first));
							intfc->WriteRecordData(&diff.second, sizeof(RE::NiPoint3));
						}
					}
				}
			}
		}
	}

	ValueSet * valueSet = m_valueMap.GetValueSet(npc);
	if (valueSet) {
		// Count only non-zero morphs
		std::uint32_t numMorphs = 0;
		for (auto it = valueSet->begin(); it != valueSet->end(); ++it) {
			if (it->second != 0.0)
				numMorphs++;
		}

		if (numMorphs > 0) {
			if (!intfc->OpenRecord('MRST', kVersion)) {
				SKSE::log::error("{} - Failed to open record", __FUNCTION__);
			}
			intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));
#ifdef _DEBUG
			SKSE::log::debug("{} - Saving {} morphs", __FUNCTION__, numMorphs);
#endif
			for (auto it = valueSet->begin(); it != valueSet->end(); ++it) {
				if (it->second != 0.0) {
					if (!intfc->OpenRecord('MRPH', kVersion)) {
						SKSE::log::error("{} - Failed to open record", __FUNCTION__);
					}
					g_stringTable.WriteString(intfc, it->first);
					intfc->WriteRecordData(&it->second, sizeof(it->second));
#ifdef _DEBUG
					SKSE::log::debug("{} - Saving {} with {}", __FUNCTION__, it->first->c_str(), it->second);
#endif
				}
			}
		}
	}
}

bool FaceMorphInterface::LoadMorphData(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t	type;
	std::uint32_t	length;

	bool error = false;

	RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC * playerBase = player->GetActorBase();

	std::uint32_t numMorphs = 0;
	if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
	{
		SKSE::log::info("Error loading morph count");
		error = true;
		return true;
	}

#ifdef _DEBUG
	SKSE::log::debug("{} - Loading {} morphs", __FUNCTION__, numMorphs);
#endif

	for (std::uint32_t i = 0; i < numMorphs; i++)
	{
		StringTableItem sculptName;
		std::uint32_t index = 0;
		float value = 0.0;
		std::uint32_t subVersion = 0;

		if (intfc->GetNextRecordInfo(type, subVersion, length))
		{
			switch (type)
			{
			case 'MRPH':
			{
				if (kVersion >= FaceMorphInterface::kSerializationVersion2)
				{
					sculptName = StringTable::ReadString(intfc, stringTable);
					if (!sculptName)
					{
						SKSE::log::info("Error loading sculpt name");
						error = true;
						return true;
					}
				}
				else if (kVersion >= FaceMorphInterface::kSerializationVersion1)
				{
					std::uint16_t nameLength = 0;
					if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
						SKSE::log::info("Error loading morph name length");
						error = true;
						return true;
					}

					std::unique_ptr<char[]> name(new char[nameLength + 1]);
					if (!intfc->ReadRecordData(name.get(), nameLength)) {
						SKSE::log::info("Error loading morph name");
						error = true;
						return true;
					}
					name[nameLength] = 0;
					sculptName = g_stringTable.GetString(name.get());
				}

				if (!intfc->ReadRecordData(&value, sizeof(value))) {
					SKSE::log::info("Error loading morph value");
					error = true;
					return true;
				}
			}
			break;
			default:
				SKSE::log::info("{} - unhandled type {:08X}", __FUNCTION__, type);
				error = true;
				break;
			}
		}

		if (value != 0.0) {
			SKSE::log::info("Loaded Morph: {} - Value: {}", sculptName->c_str(), value);
			m_valueMap.SetMorphValue(playerBase, *sculptName, value);
		}
	}

	return error;
}

bool FaceMorphInterface::LoadSculptData(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t	type;
	std::uint32_t	length;
	bool	error = false;

	RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC * playerBase = player->GetActorBase();

	std::uint16_t numParts = 0;
	if (!intfc->ReadRecordData(&numParts, sizeof(numParts)))
	{
		SKSE::log::info("Error loading sculpt part count");
		error = true;
		return true;
	}

	for (std::uint32_t i = 0; i < numParts; i++)
	{
		std::uint32_t subVersion = 0;
		if (intfc->GetNextRecordInfo(type, subVersion, length))
		{
			switch (type)
			{
			case 'SCPT':
			{
				StringTableItem sculptName;
				if (kVersion >= FaceMorphInterface::kSerializationVersion2)
				{
					sculptName = StringTable::ReadString(intfc, stringTable);
					if (!sculptName)
					{
						SKSE::log::info("Error loading sculpt name");
						error = true;
						return true;
					}
				}
				else if (kVersion >= FaceMorphInterface::kSerializationVersion1)
				{
					std::uint16_t nameLength = 0;
					if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
						SKSE::log::info("Error loading sculpt part name length");
						error = true;
						return true;
					}

					std::unique_ptr<char[]> name(new char[nameLength + 1]);
					if (!intfc->ReadRecordData(name.get(), nameLength)) {
						SKSE::log::info("Error loading sculpt part name");
						error = true;
						return true;
					}
					name[nameLength] = 0;
					sculptName = g_stringTable.GetString(name.get());
				}

				std::uint16_t totalDifferences = 0;
				if (!intfc->ReadRecordData(&totalDifferences, sizeof(totalDifferences))) {
					SKSE::log::info("Error loading sculpt difference count");
					error = true;
					return true;
				}

				SculptDataPtr sculptTarget;
				if (totalDifferences > 0)
					sculptTarget = m_sculptStorage.GetSculptTarget(playerBase, true);
				MappedSculptDataPtr sculptHost;
				if (sculptTarget && totalDifferences > 0)
					sculptHost = sculptTarget->GetSculptHost(*sculptName, true);

				std::uint16_t index = 0;
				RE::NiPoint3 value;
				for (std::uint16_t i = 0; i < totalDifferences; i++)
				{
					if (!intfc->ReadRecordData(&index, sizeof(index))) {
						SKSE::log::info("Error loading sculpt index");
						error = true;
						return true;
					}

					if (!intfc->ReadRecordData(&value, sizeof(value))) {
						SKSE::log::info("Error loading sculpt index");
						error = true;
						return true;
					}

					if (sculptTarget && sculptHost)
						sculptHost->force_insert(std::make_pair(index, value));
				}
			}
			break;
			default:
				SKSE::log::info("unhandled type {:08X}", type);
				error = true;
				break;
			}
		}
	}

	return error;
}

SKEEFixedString SculptData::GetHostByPart(RE::BGSHeadPart * headPart)
{
	const char * morphPath = headPart->morphs[RE::BGSHeadPart::MorphIndices::kChargenMorph].GetModel();
	if (morphPath != NULL && morphPath[0] != NULL)
		return morphPath;

	morphPath = headPart->morphs[RE::BGSHeadPart::MorphIndices::kDefaultMorph].GetModel();
	if (morphPath != NULL && morphPath[0] != NULL)
		return morphPath;

	morphPath = headPart->morphs[RE::BGSHeadPart::MorphIndices::kRaceMorph].GetModel();
	if (morphPath != NULL && morphPath[0] != NULL)
		return morphPath;

	return SKEEFixedString("");
}

MappedSculptDataPtr SculptData::GetSculptHost(SKEEFixedString host, bool create)
{
	auto stringItem = g_stringTable.GetString(host);
	auto it = find(stringItem);
	if (it != end())
		return it->second;
	else if (create) {
		auto data = std::make_shared<MappedSculptData>();
		emplace(stringItem, data);
		return data;
	}

	return nullptr;
}

SculptDataPtr SculptStorage::GetSculptTarget(RE::TESNPC * target, bool create)
{
	auto it = find(target);
	if (it != end())
		return it->second;
	else if (create) {
		auto data = std::make_shared<SculptData>();
		emplace(target, data);
		return data;
	}

	return nullptr;
}

void SculptStorage::SetSculptTarget(RE::TESNPC * target, SculptDataPtr sculptData)
{
	auto it = find(target);
	if (it != end())
		it->second = sculptData;
	else {
		emplace(target, sculptData);
	}
}

void SculptStorage::EraseNPC(RE::TESNPC * npc)
{
	auto sculptTarget = find(npc);
	if (sculptTarget != end()) {
		erase(sculptTarget);
	}
}

SKSETaskApplyMorphs::SKSETaskApplyMorphs(RE::Actor * actor)
{
	m_formId = actor->formID;
}

void SKSETaskApplyMorphs::Run()
{
	if (!m_formId)
		return;

	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::Actor * actor = form ? form->As<RE::Actor>() : nullptr;
	if (!actor)
		return;

	RE::TESNPC * actorBase = actor->GetActorBase();
	if (!actorBase)
		return;

	RE::BSFaceGenNiNode * faceNode = actor->GetFaceNode();
	if (faceNode) {
		g_morphInterface.ApplyMorphs(actorBase, faceNode);
	}
}

SKSETaskApplyMorphNormals::SKSETaskApplyMorphNormals(RE::NiPointer<RE::NiAVObject> faceNode, bool updateModel)
	: m_faceNode(faceNode)
	, m_updateModel(updateModel)
{

}

void SKSETaskApplyMorphNormals::Run()
{
	// Copies from the FOD into the Dynamic Geometry
	if (m_updateModel)
	{
		SKEE::UpdateModelFace(m_faceNode.get());
	}
	
	VisitGeometry(m_faceNode.get(), [](RE::BSGeometry* geometry)
	{
		auto vertexDesc = geometry->vertexDesc;

		bool hasNormals = ((vertexDesc).GetFlags() & RE::BSGraphics::Vertex::VF_NORMAL) == RE::BSGraphics::Vertex::VF_NORMAL;
		bool hasTangents = ((vertexDesc).GetFlags() & RE::BSGraphics::Vertex::VF_TANGENT) == RE::BSGraphics::Vertex::VF_TANGENT;

		if (!hasNormals && !hasTangents)
		{
			return false;
		}

		RE::BSShaderProperty* shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->shaderProperty.get());
		if (!shaderProperty || !netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
		{
			return false;
		}

		RE::BSLightingShaderMaterial* material = (RE::BSLightingShaderMaterial*)shaderProperty->material;
		if (!material || material->GetFeature() != RE::BSShaderMaterial::Feature::kFaceGen)
		{
			return false;
		}

		RE::NiSkinInstance* skinInstance = static_cast<RE::NiSkinInstance*>(geometry->skinInstance.get());
		if (!skinInstance) {
			return false;
		}

		RE::NiSkinPartition* skinPartition = static_cast<RE::NiSkinPartition*>(skinInstance->skinPartition.get());
		if (!skinPartition) {
			return false;
		}

		// Deep copy starts at refcount 1, held by skinPartitionCopy. The task takes its own
		// reference and transfers the partition into the skin instance when it runs; both
		// owners then release, leaving the skin instance as sole owner (no manual DecRef).
		RE::NiPointer<RE::NiObject> skinPartitionCopy;
		skinPartition->CreateDeepCopy(skinPartitionCopy);
		auto* newSkinPartition = netimmerse_cast<RE::NiSkinPartition*>(skinPartitionCopy.get());

		if (!newSkinPartition) {
			return false;
		}

		auto& partition = newSkinPartition->partitions[0];
		std::uint32_t vertexSize = partition.vertexDesc.GetSize();

		{
			NormalApplicator applicator{ RE::NiPointer<RE::BSGeometry>(geometry), RE::NiPointer<RE::NiSkinPartition>(newSkinPartition) };
			applicator.Apply();
		}

		// Propagate the data to the other partitions
		for (std::uint32_t p = 1; p < newSkinPartition->numPartitions; ++p)
		{
			auto& pPartition = newSkinPartition->partitions[p];
			memcpy(pPartition.buffData->rawVertexData, partition.buffData->rawVertexData, newSkinPartition->vertexCount * vertexSize);
		}

		NIOVTaskUpdateSkinPartition update(skinInstance, newSkinPartition);
		update.Run(); // transfers m_partition into skinInstance->skinPartition; refs release on scope exit
		return false;
	});
}

