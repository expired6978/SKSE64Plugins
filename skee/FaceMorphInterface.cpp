#include "common/IFileStream.h"
#include "skse64_common/skse_version.h"

#include "skse64/PluginAPI.h"

#include "skse64/GameAPI.h"
#include "skse64/GameData.h"
#include "skse64/GameObjects.h"
#include "skse64/GameMenus.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameResources.h"
#include "skse64/GameStreams.h"

#include "skse64/NiNodes.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiProperties.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiSerialization.h"

#include "skse64/ScaleformMovie.h"

#include "skse64/HashUtil.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"
#include "ScaleformFunctions.h"
#include "NifUtils.h"
#include "SKEEHooks.h"

#include "StringTable.h"
#include "ShaderUtilities.h"
#include "Utilities.h"
#include "Morpher.h"

#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"

extern OverrideInterface	g_overrideInterface;
extern NiTransformInterface g_transformInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern StringTable			g_stringTable;
extern SKSETaskInterface*	g_task;

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

extern SKSEMessagingInterface	* g_messaging;
extern PluginHandle	g_pluginHandle;

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

UInt32 FaceMorphInterface::GetVersion()
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

SliderSetPtr RaceMap::GetSliderSet(TESRace * race)
{
	RaceMap::iterator it = find(race);
	if(it != end())
		return it->second;
	
	return NULL;
}

SInt32 ReadTRIVertexCount(const char * triPath)
{
	if(triPath[0] == 0) {
		return -1;
	}

	char filePath[MAX_PATH];
	memset(filePath, 0, MAX_PATH);
	sprintf_s(filePath, MAX_PATH, "Meshes\\%s", triPath);
	BSFixedString newPath(filePath);

	// Cached file already exists, load it
	BSResourceNiBinaryStream file(newPath.data);
	if (!file.IsValid()) {
		return -1;
	}

	char header[0x08];
	file.Read(header, 0x08);
	if(strncmp(header, "FRTRI003", 8) != 0)
		return -1;

	UInt32 vertexNum = 0;
	file.Read(&vertexNum, sizeof(vertexNum));
	return vertexNum;
}


bool TRIFile::Load(const char * triPath)
{
	if (triPath[0] == 0) {
		return -1;
	}

	char filePath[MAX_PATH];
	memset(filePath, 0, MAX_PATH);
	sprintf_s(filePath, MAX_PATH, "Meshes\\%s", triPath);
	BSFixedString newPath(filePath);

	BSResourceNiBinaryStream file(newPath.data);
	if (!file.IsValid()) {
		return false;
	}

	char header[0x08];
	file.Read(header, 0x08);
	if (strncmp(header, "FRTRI003", 8) != 0)
		return false;

	file.Read(&vertexCount, sizeof(vertexCount));

	UInt32 polytris = 0, polyquads = 0, unk2 = 0, unk3 = 0, 
		uvverts = 0, flags = 0, numMorphs = 0, numMods = 0, 
		modVerts = 0, unk7 = 0, unk8 = 0, unk9 = 0, unk10 = 0;

	file.Read(&polytris, sizeof(polytris));
	file.Read(&polyquads, sizeof(polyquads));
	file.Read(&unk2, sizeof(unk2));
	file.Read(&unk3, sizeof(unk3));
	file.Read(&uvverts, sizeof(uvverts));
	file.Read(&flags, sizeof(flags));
	file.Read(&numMorphs, sizeof(numMorphs));
	file.Read(&numMods, sizeof(numMods));
	file.Read(&modVerts, sizeof(modVerts));
	file.Read(&unk7, sizeof(unk7));
	file.Read(&unk8, sizeof(unk8));
	file.Read(&unk9, sizeof(unk9));
	file.Read(&unk10, sizeof(unk10));

	// Skip reference verts
	file.Seek(vertexCount * 3 * sizeof(float));

	// Skip polytris
	file.Seek(polytris * 3 * sizeof(UInt32));

	// Skip UV
	if (uvverts > 0)
		file.Seek(uvverts * 2 * sizeof(float));

	// Skip text coords
	file.Seek(polytris * 3 * sizeof(UInt32));

	for (UInt32 i = 0; i < numMorphs; i++)
	{
		UInt32 strLen = 0;
		file.Read(&strLen, sizeof(strLen));

		char * name = new char[strLen+1];
		for (UInt32 l = 0; l < strLen; l++)
		{
			file.Read(&name[l], sizeof(char));
		}
		name[strLen] = 0;

		float mult = 0.0f;
		file.Read(&mult, sizeof(mult));

		Morph morph;
		morph.name = name;
		morph.multiplier = mult;

		for (UInt32 v = 0; v < vertexCount; v++)
		{
			Morph::Vertex vert;
			file.Read(&vert, sizeof(vert));
			morph.vertices.push_back(vert);
		}

		morphs.insert(std::make_pair(morph.name, morph));
	}

	return true;
}

bool TRIFile::Apply(BSGeometry * geometry, SKEEFixedString morphName, float relative)
{
	BSFaceGenBaseMorphExtraData * extraData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
	if (!extraData)
		return false;

	// Found morph doesn't match the cached morph
	if (extraData->vertexCount != vertexCount)
		return false;

	// Morph name wasn't found
	auto & morph = morphs.find(morphName);
	if (morph == morphs.end())
		return false;

	// What?
	if (extraData->vertexCount != morph->second.vertices.size())
		return false;

	UInt32 size = morph->second.vertices.size();
	for (UInt32 i = 0; i < size; i++)
	{
		auto & vert = morph->second.vertices.at(i);
		extraData->vertexData[i].x += (float)((double)vert.x * (double)morph->second.multiplier * (double)relative);
		extraData->vertexData[i].y += (float)((double)vert.y * (double)morph->second.multiplier * (double)relative);
		extraData->vertexData[i].z += (float)((double)vert.z * (double)morph->second.multiplier * (double)relative);
	}

	UpdateModelFace(geometry);
	return true;
}

class RacePartVisitor
{
public:
	virtual bool Accept(BGSHeadPart * headPart) = 0;
};

void VisitRaceParts(TESRace * race, UInt32 gender, RacePartVisitor & visitor)
{
	TESRace::CharGenData * chargenData = race->chargenData[gender];
	if(chargenData) {
		tArray<BGSHeadPart*> * headParts = chargenData->headParts;
		if(headParts) {
			for(UInt32 i = 0; i < headParts->count; i++) {
				BGSHeadPart* headPart = NULL;
				if(headParts->GetNthItem(i, headPart)) {
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
	RacePartByType::RacePartByType(UInt32 partType) : m_type(partType), m_headPart(NULL) {}
	virtual bool Accept(BGSHeadPart * headPart)
	{
		if(headPart->type == m_type) {
			m_headPart = headPart;
			return true;
		}

		return false;
	}

	UInt32			m_type;
	BGSHeadPart		* m_headPart;
};

class RacePartDefaultGen : public RacePartVisitor
{
public:
	RacePartDefaultGen::RacePartDefaultGen(TESRace * sourceRace, TESRace * targetRace, std::vector<BSFixedString> * parts, UInt32 gender) : m_sourceRace(sourceRace), m_targetRace(targetRace), m_gender(gender), m_partList(parts), m_acceptDefault(false) {}

	virtual bool Accept(BGSHeadPart * headPart)
	{
		BSFixedString sourceMorphPath(headPart->chargenMorph.GetModelName());
		if(sourceMorphPath == BSFixedString(""))
			return false;

		auto it = std::find(m_partList->begin(), m_partList->end(), sourceMorphPath);
		if (it != m_partList->end()) { // Found part tri file
			if(headPart->type == BGSHeadPart::kTypeFace)
				m_acceptDefault = true;
		} else {
			RacePartByType racePart(headPart->type);
			VisitRaceParts(m_targetRace, m_gender, racePart);

			BGSHeadPart * targetPart = racePart.m_headPart;
			if(targetPart) {
				BSFixedString targetMorphPath(targetPart->chargenMorph.GetModelName());
				if(targetMorphPath == BSFixedString("")) {
					_ERROR("%s - Could not bind default morphs for %s on %s[%d] using %s. No valid morph path.", __FUNCTION__, headPart->partName.data, m_sourceRace->editorId.data, m_gender, m_targetRace->editorId.data, sourceMorphPath.data);
					return false;
				}

				TRIModelData sourceData, targetData;
				g_morphInterface.GetModelTri(sourceMorphPath, sourceData);
				g_morphInterface.GetModelTri(targetMorphPath, targetData);

				if (sourceData.vertexCount == targetData.vertexCount && sourceData.vertexCount > 0 && targetData.vertexCount > 0) {
					// Bind additional morphs here, the source and target morphs are identical
					_DMESSAGE("%s - Binding default morphs for %s (%s) on %s[%d] using %s. (%s - %d | %s - %d)", __FUNCTION__, headPart->partName.data, sourceMorphPath.data, m_sourceRace->editorId.data, m_gender, m_targetRace->editorId.data, sourceMorphPath.data, sourceData.vertexCount, targetMorphPath.data, targetData.vertexCount);
					auto titer = g_morphInterface.m_morphMap.find(targetMorphPath);
					if(titer != g_morphInterface.m_morphMap.end())
						g_morphInterface.m_morphMap.emplace(sourceMorphPath, titer->second);
					if(headPart->type == BGSHeadPart::kTypeFace)
						m_acceptDefault = true;
				} else if(sourceData.vertexCount == 0 || targetData.vertexCount == 0) {
					_ERROR("%s - Could not bind default morphs for %s on %s[%d] using %s. Invalid vertex count (%s - %d | %s - %d).", __FUNCTION__, headPart->partName.data, m_sourceRace->editorId.data, m_gender, m_targetRace->editorId.data, sourceMorphPath.data, sourceData.vertexCount, targetMorphPath.data, targetData.vertexCount);
				} else {
					_ERROR("%s - Could not bind default morphs for %s on %s[%d] using %s. Vertex mismatch (%s - %d | %s - %d).", __FUNCTION__, headPart->partName.data, m_sourceRace->editorId.data, m_gender, m_targetRace->editorId.data, sourceMorphPath.data, sourceData.vertexCount, targetMorphPath.data, targetData.vertexCount);
				}
			}
		}

		return false;
	}

	UInt32	m_gender;
	TESRace	* m_sourceRace;
	TESRace	* m_targetRace;
	std::vector<BSFixedString>	* m_partList;
	bool	m_acceptDefault;
};

class RacePartFiles : public RacePartVisitor
{
public:
	RacePartFiles::RacePartFiles(std::vector<BSFixedString> * parts) : m_parts(parts) {}
	virtual bool Accept(BGSHeadPart * headPart)
	{
		if (headPart->chargenMorph.name.data != BSFixedString("").data)
			m_parts->push_back(headPart->chargenMorph.name);
		if (headPart->raceMorph.name.data != BSFixedString("").data)
			m_parts->push_back(headPart->raceMorph.name);
		
		return false;
	}

	std::vector<BSFixedString>		* m_parts;
};

bool RaceMap::CreateDefaultMap(TESRace * race)
{
	TESRace * templateRace = GetRaceByName(g_raceTemplate);
	RaceMap::iterator rit = find(templateRace);
	if(rit != end()) { // Found NordRace slider maps

		std::vector<BSFixedString> templateFiles;
		for (UInt32 gender = 0; gender <= 1; gender++) {
			RacePartFiles parts(&templateFiles);
			VisitRaceParts(templateRace, gender, parts);
		}

		bool acceptDefault = false;
		for(UInt32 gender = 0; gender <= 1; gender++) { // Iterate genders
			RacePartDefaultGen defaultGen(race, templateRace, &templateFiles, gender);
			VisitRaceParts(race, gender, defaultGen);
			acceptDefault = defaultGen.m_acceptDefault;
		}

		if(acceptDefault) {
			SliderSetPtr sliderMaps = rit->second;
			if(sliderMaps->size() > 0) {
				UInt32 addedMaps = 0;
				for(auto smit = sliderMaps->begin(); smit != sliderMaps->end(); ++smit) {
					if(AddSliderMap(race, *smit))
						addedMaps++;
				}
				if(addedMaps > 0) {
					_DMESSAGE("%s - Added default slider maps for %s from %s", __FUNCTION__, race->editorId.data, rit->first->editorId.data);
					return true;
				}
			}
		}
	}

	return false;
}

bool RaceMap::AddSliderMap(TESRace * race, SliderMapPtr sliderMap)
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

SliderInternalPtr FaceMorphInterface::GetSliderByIndex(TESRace * race, UInt32 index)
{
	RaceSliders::iterator it = m_internalMap.find(race);
	if(it != m_internalMap.end()) {
		if(index < it->second.size())
			return it->second.at(index);
	}
	return NULL;
}

float ValueMap::GetMorphValueByName(TESNPC* npc, const SKEEFixedString & name)
{
	ValueMap::iterator it = find(npc);
	if(it != end()) {
		return it->second.GetValue(name);
	}

	return 0.0;
}

void ValueMap::SetMorphValue(TESNPC* npc, const SKEEFixedString & name, float value)
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

ValueSet * ValueMap::GetValueSet(TESNPC* npc)
{
	ValueMap::iterator it = find(npc);
	if(it != end()) {
		return &it->second;
	}

	return NULL;
}

void ValueMap::EraseNPC(TESNPC * npc)
{
	auto it = find(npc);
	if (it != end())
		erase(it);
}

float FaceMorphInterface::GetMorphValueByName(TESNPC* npc, const SKEEFixedString & name)
{
	return m_valueMap.GetMorphValueByName(npc, name);
}

void FaceMorphInterface::SetMorphValue(TESNPC* npc, const SKEEFixedString & name, float value)
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
	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		ForEachMod([&](ModInfo * modInfo)
		{
			std::string fixedPath = "Meshes\\";
			fixedPath.append(SLIDER_MOD_DIRECTORY);
			std::string modPath = modInfo->name;
			modPath.append("\\");

			ReadRaces(fixedPath, modPath, "races.ini");
			if (g_extendedMorphs)
				ReadMorphs(fixedPath, modPath, "morphs.ini");

			ReadPartReplacements(fixedPath, modPath, "replacements.ini");
		});

		if (g_extendedMorphs) {
			BGSHeadPart * part = NULL;
			for (UInt32 i = 0; i < dataHandler->headParts.count; i++)
			{
				if (dataHandler->headParts.GetNthItem(i, part)) {
					if (CacheHeadPartModel(part)) {

						BSFixedString key = part->chargenMorph.GetModelName();

						// Cache all of the extended morphs
						ExtendedMorphCache extendedCache;
						VisitMorphMap(key, extendedCache);
					}
				}
			}
		}

		// Create default slider maps
		TESRace * race = NULL;
		for (UInt32 i = 0; i < dataHandler->races.count; i++)
		{
			if (dataHandler->races.GetNthItem(i, race)) {

				if (g_allowAllMorphs) {
					for (UInt32 i = 0; i <= 1; i++) {
						if (race->chargenData[i]) {
							for (UInt32 t = 0; t < FacePresetList::kNumPresets; t++) {
								race->chargenData[i]->presetFlags[t][0] = 0xFFFFFFFF;
								race->chargenData[i]->presetFlags[t][1] = 0xFFFFFFFF;
								auto presetList = FacePresetList::GetSingleton();
								auto gameSetting = presetList->presets[t].gameSetting;
								race->chargenData[i]->totalPresets[t] = gameSetting->data.s32;
							}
						}
					}
				}

				if ((race->data.raceFlags & TESRace::kRace_FaceGenHead) == TESRace::kRace_FaceGenHead)
					m_raceMap.CreateDefaultMap(race);
			}
		}
	}
}

bool FaceMorphInterface::CacheHeadPartModel(BGSHeadPart * headPart, bool cacheTRI)
{
	BSFixedString modelPath = headPart->chargenMorph.GetModelName();
	if (modelPath == BSFixedString(""))
		return false;

	ModelMap::iterator it = m_modelMap.find(modelPath);
	if (it == m_modelMap.end()) {
		TRIModelData data;

		data.morphModel = &headPart->chargenMorph;
		if (!cacheTRI) {
			data.vertexCount = ReadTRIVertexCount(modelPath.data);
		}
		else {
			data.triFile = std::make_shared<TRIFile>();
			data.triFile->Load(modelPath.data);
			data.vertexCount = data.triFile->vertexCount;
		}

		m_modelMap.emplace(modelPath, data);
	}
	else if (cacheTRI && !it->second.triFile) {
		it->second.triFile = std::make_shared<TRIFile>();
		it->second.triFile->Load(modelPath.data);
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
	BSFixedString morphFile(filePath.c_str());
	ModelMap::iterator it = m_modelMap.find(morphFile);
	if(it == m_modelMap.end()) {
		void* memory = Heap_Allocate(sizeof(TESModelTri));
		memset(memory, 0, sizeof(TESModelTri));
		((uintptr_t*)memory)[0] = TESModelTri_vtbl.GetUIntPtr();
		TESModelTri* xData = (TESModelTri*)memory;
		xData->SetModelName(morphFile.c_str());

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

PresetDataPtr FaceMorphInterface::GetPreset(TESNPC* npc)
{
	auto it = m_mappedPreset.find(npc);
	if (it != m_mappedPreset.end())
		return it->second;

	return NULL;
}

void FaceMorphInterface::AssignPreset(TESNPC * npc, PresetDataPtr presetData)
{
	ErasePreset(npc);
	m_mappedPreset.emplace(npc, presetData);
}

bool FaceMorphInterface::ErasePreset(TESNPC * npc)
{
	auto it = m_mappedPreset.find(npc);
	if (it != m_mappedPreset.end()) {
		m_mappedPreset.erase(it);
		return true;
	}

	return false;
}

void FaceMorphInterface::ClearPresets()
{
	m_mappedPreset.clear();
}

void FaceMorphInterface::ApplyPreset(TESNPC * npc, BSFaceGenNiNode * faceNode, BGSHeadPart * headPart)
{
	PresetDataPtr presetData = GetPreset(npc);
	if (presetData && faceNode && headPart) {
		NiAVObject * object = faceNode->GetObjectByName(&headPart->partName.data);
		if (object) {
			BSGeometry * geometry = object->GetAsBSGeometry();
			if (geometry) {
				BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
				if (shaderProperty) {
					if (ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty)) {
						BSLightingShaderProperty * lightingShader = (BSLightingShaderProperty *)shaderProperty;
						BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
						if (headPart->type == BGSHeadPart::kTypeFace) {

							BSShaderTextureSet * newTextureSet = BSShaderTextureSet::Create();
							for (UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
							{
								newTextureSet->SetTexturePath(i, material->textureSet->GetTexturePath(i));
							}
							newTextureSet->SetTexturePath(6, presetData->tintTexture.data);
							material->SetTextureSet(newTextureSet);

							NiPointer<NiTexture> newTexture;
							LoadTexture(presetData->tintTexture.data, 1, newTexture, false);

							NiTexturePtr * targetTexture = GetTextureFromIndex(material, 6);
							if (targetTexture) {
								*targetTexture = newTexture;
							}

							CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
						}
						else if (material->GetShaderType() == BSLightingShaderMaterial::kShaderType_HairTint) {
							BSLightingShaderMaterialHairTint * tintedMaterial = (BSLightingShaderMaterialHairTint *)material; // I don't know what this * 2.0 bullshit is.
							tintedMaterial->tintColor.r = (((presetData->hairColor >> 16) & 0xFF) / 255.0) * 2.0;
							tintedMaterial->tintColor.g = (((presetData->hairColor >> 8) & 0xFF) / 255.0) * 2.0;
							tintedMaterial->tintColor.b = ((presetData->hairColor & 0xFF) / 255.0) * 2.0;
						}
					}
				}
			}
		}
	}
}

class ValidRaceFinder : public BGSListForm::Visitor
{
public:
	ValidRaceFinder::ValidRaceFinder(TESRace * race) : m_race(race) { }
	virtual bool Accept(TESForm * form)
	{
		if (m_race == form)
			return true;

		return false;
	};
	TESRace * m_race;
};

void FaceMorphInterface::ApplyPresetData(Actor * actor, PresetDataPtr presetData, bool setSkinColor, ApplyTypes applyType)
{
	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	TESRace * race = npc->race.race;

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

	if (presetData->headTexture) {
		if (!npc->headData) {
			npc->headData = (TESNPC::HeadData*)Heap_Allocate(sizeof(TESNPC::HeadData));

			// If we had no head data we probably have no hair color assigned, this isn't good
			// lets just assign the first color from our race settings incase
			BGSColorForm* color = nullptr;
			if(race->chargenData[gender]->colors->count > 0)
				race->chargenData[gender]->colors->GetNthItem(0, color);

			npc->headData->hairColor = color;
			npc->headData->headTexture = nullptr;
		}
		npc->headData->headTexture = presetData->headTexture;
	}
	

	// Replace the old parts with the new parts if they are the right sex
	for (auto & part : presetData->headParts) {
		if ((gender == 0 && (part->partFlags & BGSHeadPart::kFlagMale) == BGSHeadPart::kFlagMale) ||
			(gender == 1 && (part->partFlags & BGSHeadPart::kFlagFemale) == BGSHeadPart::kFlagFemale))
		{
			ValidRaceFinder partFinder(race);
			if (part->validRaces) {
				if (part->validRaces->Visit(partFinder))
					CALL_MEMBER_FN(npc, ChangeHeadPart)(part);
			}
		}
	}

	npc->weight = presetData->weight;

	if (!npc->faceMorph)
		npc->faceMorph = (TESNPC::FaceMorphs*)Heap_Allocate(sizeof(TESNPC::FaceMorphs));

	UInt32 i = 0;
	for (auto value : presetData->presets) {
		npc->faceMorph->presets[i] = value;
		i++;
	}

	i = 0;
	for (auto value : presetData->morphs) {
		npc->faceMorph->option[i] = value;
		i++;
	}

	for (auto & tint : presetData->tints) {
		float alpha = (tint.color >> 24) / 255.0;
		TintMask * tintMask = NULL;
		if (player == actor && player->tintMasks.GetNthItem(tint.index, tintMask)) {
			tintMask->color.red = (tint.color >> 16) & 0xFF;
			tintMask->color.green = (tint.color >> 8) & 0xFF;
			tintMask->color.blue = tint.color & 0xFF;
			tintMask->alpha = alpha;
			if (tintMask->alpha > 0)
				tintMask->texture->str = tint.name;
		}

		if (tint.index == 0 && setSkinColor)
		{
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
	}

	g_morphInterface.EraseSculptData(npc);
	if (presetData->sculptData) {
		if (presetData->sculptData->size() > 0) {
			g_morphInterface.SetSculptTarget(npc, presetData->sculptData);
		}
	}

	g_morphInterface.EraseMorphData(npc);
	for (auto & morph : presetData->customMorphs)
		g_morphInterface.SetMorphValue(npc, morph.name, morph.value);

	g_overrideInterface.RemoveAllReferenceNodeOverrides(actor);
	
	g_overlayInterface.RevertOverlays(actor, true);
	if (!g_overlayInterface.HasOverlays(actor))
	{
		g_overlayInterface.AddOverlays(actor);
	}

	if ((applyType & kPresetApplyOverrides) == kPresetApplyOverrides)
	{
		for (auto & nodes : presetData->overrideData) {
			for (auto & value : nodes.second) {
				g_overrideInterface.AddNodeOverride(actor, gender == 1 ? true : false, nodes.first, value);
			}
		}

		g_overrideInterface.SetNodeProperties(actor->formID, false);
	}

	if ((applyType & kPresetApplySkinOverrides) == kPresetApplySkinOverrides)
	{
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto & slot : presetData->skinData[i]) {
				for (auto & value : slot.second) {
					g_overrideInterface.AddSkinOverride(actor, gender == 1 ? true : false, i == 1 ? true : false, slot.first, value);
				}
			}
		}

		g_overrideInterface.SetSkinProperties(actor->formID, false);
	}

	g_transformInterface.Impl_RemoveAllReferenceTransforms(actor);

	if ((applyType & kPresetApplyTransforms) == kPresetApplyTransforms)
	{
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
	
	g_bodyMorphInterface.ClearMorphs(actor);

	if ((applyType & kPresetApplyBodyMorphs) == kPresetApplyBodyMorphs)
	{
		for (auto & morph : presetData->bodyMorphData) {
			for (auto & keys : morph.second)
				g_bodyMorphInterface.SetMorph(actor, morph.first, keys.first, keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}

bool MorphMap::Visit(const SKEEFixedString & key, Visitor & visitor)
{
	MorphMap::iterator it = find(key);
	if(it != end())
	{
#ifdef _DEBUG_MORPHAPPLICATOR
		_DMESSAGE("%s - Applying %d additional morphs to %s", __FUNCTION__, it->second.size(), key.data);
		gLog.Indent();
#endif
		for(auto iter = it->second.begin(); iter != it->second.end(); ++iter)
		{
#ifdef _DEBUG_MORPHAPPLICATOR
			_DMESSAGE("%s - Visting %s", __FUNCTION__, (*iter).data);
#endif
			if(visitor.Accept(*iter))
				break;
		}
#ifdef _DEBUG_MORPHAPPLICATOR
		gLog.Outdent();
#endif
		return true;
	}
#ifdef _DEBUG_MORPHAPPLICATOR
	else {
		_DMESSAGE("%s - No additional morphs for %s", __FUNCTION__, key.data);
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
	BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.IsValid()) {
		return;
	}

	UInt32 lineCount = 0;
	std::string str = "";
	while(BSFileUtil::ReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if(str.length() == 0)
			continue;
		if(str.at(0) == '#')
			continue;

		std::vector<std::string> side = explode(str, '=');
		if(side.size() < 2) {
			_ERROR("ReadMorphs Error - Line (%d) loading a morph from %s has no left-hand side.", lineCount, fullPath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		if(_strnicmp(lSide.c_str(), "extension", 9) != 0) {
			_ERROR("ReadMorphs Error - Line (%d) loading a morph from %s invalid left-hand side.", lineCount, fullPath.c_str());
			continue;
		}

		std::vector<std::string> params = explode(rSide, ',');
		if(params.size() < 2) {
			_ERROR("ReadMorphs Error - Line (%d) slider %s from %s has less than 2 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		// Trim all parameters
		for(UInt32 i = 0; i < params.size(); i++)
			params[i] = std::trim(params[i]);

		std::string key = params[0];
		for(UInt32 i = 1; i < params.size(); i++) {
#ifdef _DEBUG_DATAREADER
			_DMESSAGE("ReadMorphs Info - Line (%d) added %s morph to %s from %s.", lineCount, params[i].c_str(), key.c_str(), fullPath.c_str());
#endif
			m_morphMap.AddMorph(key, params[i]);
		}
	}
}

void FaceMorphInterface::ReadRaces(std::string fixedPath, std::string modPath, std::string fileName)
{
	std::string fullPath = fixedPath + modPath + fileName;
	BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.IsValid()) {
		return;
	}

	std::map<std::string, SliderMapPtr> fileMap;

	UInt32 lineCount = 0;
	std::string str = "";
	while(BSFileUtil::ReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if(str.length() == 0)
			continue;
		if(str.at(0) == '#')
			continue;

		std::vector<std::string> side = explode(str, '=');
		if(side.size() < 2) {
			_ERROR("ReadRaces Error - Line (%d) loading a race from %s has insufficient parameters.", lineCount, fullPath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		std::vector<std::string> files = explode(rSide, ',');
		for(UInt32 i = 0; i < files.size(); i++)
			files[i] = std::trim(files[i]);

		for(UInt32 i = 0; i < files.size(); i++)
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
					_ERROR("ReadRaces Error - Line (%d) failed to load slider map for %s from %s.", lineCount, lSide.c_str(), fullPath.c_str());
				}
			}

			if(sliderMap) {
#ifdef _DEBUG_DATAREADER
				_DMESSAGE("ReadRaces Info - Line (%d) Loaded %s for Race %s from %s.", lineCount, files[i].c_str(), lSide.c_str(), fullPath.c_str());
#endif

				TESRace * race = GetRaceByName(lSide);
				if(race)
					m_raceMap.AddSliderMap(race, sliderMap);
			}
		}
	}
}

void SliderMap::AddSlider(const SKEEFixedString & key, UInt8 gender, SliderInternal & sliderInternal)
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
	BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.IsValid()) {
		return NULL;
	}

	sliderMap = std::make_shared<SliderMap>();

	UInt8 gender = 0;
	UInt32 lineCount = 0;
	std::string str = "";
	while(BSFileUtil::ReadLine(&file, &str))
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
			_ERROR("ReadSliders Error - Line (%d) slider from %s has no left-hand side.", lineCount, fullPath.c_str());
			continue;
		}
		
		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		std::vector<std::string> params = explode(rSide, ',');
		if(params.size() < 3) {
			_ERROR("ReadSliders Error - Line (%d) slider %s from %s has less than 3 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		// Trim all parameters
		for(UInt32 i = 0; i < params.size(); i++)
			params[i] = std::trim(params[i]);

		SKEEFixedString sliderName = SKEEFixedString(lSide.c_str());
		SliderInternal sliderInternal;
		sliderInternal.name = sliderName;
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
				_ERROR("ReadSliders Error - Line (%d) loading slider %s from %s has invalid category (%d).", lineCount, lSide.c_str(), fullPath.c_str(), sliderInternal.category);
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
			_ERROR("ReadSliders Error - Line (%d) loading slider %s from %s has invalid slider type (%s).", lineCount, lSide.c_str(), fullPath.c_str(), params[1].c_str());
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
					_ERROR("ReadSliders Error - Line (%d) slider %s from %s has less than 4 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
					continue;
				}

				sliderInternal.lowerBound = SKEEFixedString(params[2].c_str());
				sliderInternal.upperBound = SKEEFixedString(params[3].c_str());

				if(sliderInternal.lowerBound == SKEEFixedString("None"))
					sliderInternal.lowerBound = SKEEFixedString("");
				if(sliderInternal.upperBound == SKEEFixedString("None"))
					sliderInternal.upperBound = SKEEFixedString("");
			}
			break;
			case SliderInternal::kTypePreset:
			{
				// Additional morphs are disabled
				if (!g_extendedMorphs)
					continue;

				if (params.size() < 4) {
					_ERROR("ReadSliders Error - Line (%d) slider %s from %s has less than 4 parameters.", lineCount, lSide.c_str(), fullPath.c_str());
					continue;
				}

				sliderInternal.lowerBound = SKEEFixedString(params[2].c_str());
				UInt32 presetCount = atoi(params[3].c_str());
				if (presetCount > 255) {
					presetCount = 255;
					_WARNING("ReadSliders Warning - Line (%d) loading slider %s from %s has exceeded a preset count of %d.", lineCount, lSide.c_str(), fullPath.c_str(), presetCount);
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
		_DMESSAGE("ReadSliders Info - Line (%d) Added Slider (%s, %d, %d, %d, %s, %s) to Gender %d %s from %s.", lineCount, sliderInternal.name.data, sliderInternal.category, sliderInternal.type, sliderInternal.presetCount, sliderInternal.lowerBound.data, sliderInternal.upperBound.data, gender, lSide.c_str(), fullPath.c_str());
#endif
		sliderMap->AddSlider(sliderName, gender, sliderInternal);
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
			_DMESSAGE("Slider - Name: %s Gender: Male Type: %d Cat: %d LowerBound: %s UpperBound: %s PresetCount: %d", it->first.data, gender->slider[0]->type, gender->slider[0]->category, gender->slider[0]->lowerBound.data, gender->slider[0]->upperBound.data, gender->slider[0]->presetCount);
		if(gender->slider[1])
			_DMESSAGE("Slider - Name: %s Gender: Female Type: %d Cat: %d LowerBound: %s UpperBound: %s PresetCount: %d", it->first.data, gender->slider[1]->type, gender->slider[1]->category, gender->slider[1]->lowerBound.data, gender->slider[1]->upperBound.data, gender->slider[1]->presetCount);
	}
}

void RaceMap::DumpMap()
{
	for(RaceMap::iterator it = begin(); it != end(); ++it)
	{
		_DMESSAGE("Race: %08X - %s", it->first->formID, it->first->editorId.data);
		gLog.Indent();
		for(SliderSet::iterator sit = it->second.begin(); sit != it->second.end(); ++sit)
		{
			_DMESSAGE("Map: %08X", (*sit));
			(*sit)->DumpMap();
		}
		gLog.Outdent();
	}
}

void MorphMap::DumpMap()
{
	for(MorphMap::iterator it = begin(); it != end(); ++it)
	{
		_DMESSAGE("Morph: %s", it->first.data);
		DumpVisitor visitor;
		gLog.Indent();
		Visit(it->first, visitor);
		gLog.Outdent();
	}
}

void MorphHandler::DumpAll()
{
	m_raceMap.DumpMap();
	m_morphMap.DumpMap();
}
#endif

SliderInternalPtr FaceMorphInterface::GetSlider(TESRace * race, UInt8 gender, SKEEFixedString name)
{
	SliderSetPtr sliderMaps = m_raceMap.GetSliderSet(race);
	if(sliderMaps)
	{
		SliderInternalPtr sliderInternal;
		sliderMaps->for_each_slider([&](SliderGenderPtr genders) {
			sliderInternal = genders->slider[gender];
			if (sliderInternal && sliderInternal->name == name)
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
	return s1->name < s2->name;
}

SliderList * FaceMorphInterface::CreateSliderList(TESRace * race, UInt8 gender)
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

void FaceMorphInterface::AddSlider(TESRace * race, SliderInternalPtr & slider)
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

void FaceMorphInterface::ApplyMorph(TESNPC * npc, BGSHeadPart * headPart, BSFaceGenNiNode * faceNode)
{
	char buffer[MAX_PATH];
	
	auto sculptTarget = GetSculptTarget(npc, false);
	if (sculptTarget) {
		if (headPart) {

			NiAVObject * object = faceNode->GetObjectByName(&headPart->partName.data);
			if (object) {
				BSGeometry * geometry = object->GetAsBSGeometry();
				if (geometry) {
					auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
					if (sculptHost) {
						BSFaceGenBaseMorphExtraData * extraData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
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

	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();
	
	for(auto it = valueSet->begin(); it != valueSet->end(); ++it)
	{
		SliderInternalPtr slider = GetSlider(npc->race.race, gender, *it->first);
		if(slider) {
			if(slider->type == SliderInternal::kTypePreset) {
				if(it->second != 0) { // There should be no zero morph for presets
					memset(buffer, 0, MAX_PATH);
					sprintf_s(buffer, MAX_PATH, "%s%d", slider->lowerBound.c_str(), (UInt32)it->second);
					BSFixedString morphName(buffer);
#ifdef _DEBUG_MORPH
					_DMESSAGE("Applying Single Preset %s value %f to %s", morphName.data, it->second, headPart->partName.data);
#endif
					CALL_MEMBER_FN(FaceGen::GetSingleton(), ApplyMorph)(faceNode, headPart, &morphName, 1.0);
				}
			} else {
				BSFixedString morphName = slider->lowerBound;
				if(it->second < 0)
					morphName = slider->lowerBound;
				if(it->second > 0)
					morphName = slider->upperBound;

				float relative = abs(it->second);
				if(relative > 1.0) {
					UInt32 count = (UInt32)relative;
					float difference = relative - count;
					for(UInt32 i = 0; i < count; i++)
						CALL_MEMBER_FN(FaceGen::GetSingleton(), ApplyMorph)(faceNode, headPart, &morphName, 1.0);
					relative = difference;
				}
#ifdef _DEBUG_MORPH
				_DMESSAGE("Applying Single Slider %s value %f to %s", morphName.data, it->second, headPart->partName.data);
#endif
				CALL_MEMBER_FN(FaceGen::GetSingleton(), ApplyMorph)(faceNode, headPart, &morphName, relative);
			}
		}
	}

	if (g_enableFaceNormalRecalculate)
	{
		g_task->AddTask(new SKSETaskApplyMorphNormals(faceNode));
	}
}

void FaceMorphInterface::ApplyMorphs(TESNPC * npc, BSFaceGenNiNode * faceNode)
{
	char buffer[MAX_PATH];
	auto sculptTarget = GetSculptTarget(npc, false);
	if (sculptTarget) {
		VisitObjects(faceNode, [&](NiAVObject* object)
		{
			if (BSGeometry * geometry = object->GetAsBSGeometry()) {
				std::string headPartName = object->m_name;
				BGSHeadPart * headPart = GetHeadPartByName(headPartName);
				if (headPart) {
					auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
					if (sculptHost) {
						BSFaceGenBaseMorphExtraData * extraData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
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

	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();

	
	for(auto it = valueSet->begin(); it != valueSet->end(); ++it)
	{
		SliderInternalPtr slider = GetSlider(npc->race.race, gender, *it->first);
		if(slider) {
			if(slider->type == SliderInternal::kTypePreset) {
				if(it->second != 0) { // There should be no zero morph for presets
					memset(buffer, 0, MAX_PATH);
					sprintf_s(buffer, MAX_PATH, "%s%d", slider->lowerBound.c_str(), (UInt32)it->second);
					SKEEFixedString morphName(buffer);
#ifdef _DEBUG_MORPH
					_DMESSAGE("Applying Full Preset %s value %f to all parts", morphName.c_str(), it->second);
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
					UInt32 count = (UInt32)relative;
					float difference = relative - count;
					for(UInt32 i = 0; i < count; i++)
						SetMorph(npc, faceNode, morphName, 1.0);
					relative = difference;
				}

#ifdef _DEBUG_MORPH
				_DMESSAGE("Applying Full Slider %s value %f to all parts", morphName.c_str(), it->second);
#endif
				SetMorph(npc, faceNode, morphName, relative);
			}
		}
	}

	if (g_enableFaceNormalRecalculate)
	{
		g_task->AddTask(new SKSETaskApplyMorphNormals(faceNode));
	}
}

void FaceMorphInterface::SetMorph(TESNPC * npc, BSFaceGenNiNode * faceNode, const SKEEFixedString & name, float relative)
{
#ifdef _DEBUG_MORPH
	_DMESSAGE("Applying Morph %s to all parts", name);
#endif
	BSFixedString morphName(name.c_str());
	FaceGenApplyMorph(FaceGen::GetSingleton(), faceNode, npc, &morphName, relative);

	if (g_enableFaceNormalRecalculate)
	{
		g_task->AddTask(new SKSETaskApplyMorphNormals(faceNode, false));
	}
}

SInt32 FaceMorphInterface::LoadSliders(tArray<RaceMenuSlider> * sliderArray, RaceMenuSlider * slider)
{
	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	UInt32 sliderId = sliderArray->count;
	UInt32 morphIndex = SLIDER_OFFSET;

	currentList = CreateSliderList(player->race, CALL_MEMBER_FN(npc, GetSex)());
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
				if(slider->name == *it->first) {
					foundMorph = true;
					break;
				}
			}

			if (!foundMorph) {
				_DMESSAGE("Erasing %s", it->first->c_str());
				valueSet->erase(it++);
			}
			else
				it++;
		}
	}

	UInt32 i = 0;
	for(auto it = currentList->begin(); it != currentList->end(); ++it)
	{
		SliderInternalPtr slider = (*it);
		std::string sliderName = "$";
		sliderName.append(slider->name.c_str());

		float value = valueSet ? valueSet->GetValue(slider->name) : 0.0;

		UInt32 sliderIndex = morphIndex + i;
		float lowerBound = slider->lowerBound == BSFixedString("") ? 0.0 : -1.0;
		float upperBound = slider->upperBound == BSFixedString("") ? 0.0 : 1.0;
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
			lowerBound = 0.0;
			interval = 1;
			lowerMultiplier = 1.0;
			upperMultiplier = 1.0;
			HeadPartList * headPartList = g_partSet.GetPartList(slider->presetCount);
			BGSHeadPart * headPart = npc->GetHeadPartByType(slider->presetCount);
			SInt32 partIndex = -1;
			if(headPart && headPartList)
				partIndex = g_partSet.GetPartIndex(headPartList, headPart);
			if(partIndex != -1)
				value = (float)(partIndex + 1);
			if(headPartList)
				upperBound = (float)headPartList->size();
			else
				upperBound = 0;
		}

		BSFixedString morphName(sliderName.c_str());

#ifdef _DEBUG_SLIDER
		_DMESSAGE("Adding slider: %s Morph: %s Value: %f SliderID: %d Index: %d", morphName.data, slider->name.data, value, sliderId, sliderIndex);
#endif

		RaceMenuSlider newSlider(slider->category, morphName.c_str(), "ChangeDoubleMorph", sliderId++, sliderIndex, 2, 1, lowerBound * lowerMultiplier, upperBound * upperMultiplier, value, interval, 0);
		AddRaceMenuSlider(sliderArray, &newSlider);
		i++;
	}

	return sliderId;
}

void FaceMorphInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	auto sculptData = m_sculptStorage.GetSculptTarget(npc, false);
	if (sculptData) {
		if (sculptData->size() > 0) {
			intfc->OpenRecord('SCDT', kVersion);

			UInt16 numValidParts = 0;
			for (auto part : *sculptData) {
				if (part.first->length() != 0 && part.second->size() > 0) {
					numValidParts++;
				}
			}

			intfc->WriteRecordData(&numValidParts, sizeof(numValidParts));
#ifdef _DEBUG
			_DMESSAGE("%s - Saving %d sculpts", __FUNCTION__, numValidParts);
#endif
			if (numValidParts > 0) {
				for (auto part : *sculptData) {
					UInt16 diffCount = part.second->size();
					if (diffCount > 0) {
						intfc->OpenRecord('SCPT', kVersion);
						g_stringTable.WriteString(intfc, part.first);
						intfc->WriteRecordData(&diffCount, sizeof(diffCount));

#ifdef _DEBUG
						_DMESSAGE("%s - Saving sculpt to %s with %d diffs", __FUNCTION__, part.first->c_str(), diffCount);
#endif

						for (auto diff : *part.second) {
							intfc->WriteRecordData(&diff.first, sizeof(diff.first));
							intfc->WriteRecordData(&diff.second, sizeof(NiPoint3));
						}
					}
				}
			}
		}
	}

	ValueSet * valueSet = m_valueMap.GetValueSet(npc);
	if (valueSet) {
		// Count only non-zero morphs
		UInt32 numMorphs = 0;
		for (auto it = valueSet->begin(); it != valueSet->end(); ++it) {
			if (it->second != 0.0)
				numMorphs++;
		}

		if (numMorphs > 0) {
			intfc->OpenRecord('MRST', kVersion);
			intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));
#ifdef _DEBUG
			_DMESSAGE("%s - Saving %d morphs", __FUNCTION__, numMorphs);
#endif
			for (auto it = valueSet->begin(); it != valueSet->end(); ++it) {
				if (it->second != 0.0) {
					intfc->OpenRecord('MRPH', kVersion);
					g_stringTable.WriteString(intfc, it->first);
					intfc->WriteRecordData(&it->second, sizeof(it->second));
#ifdef _DEBUG
					_DMESSAGE("%s - Saving %s with %f", __FUNCTION__, it->first->c_str(), it->second);
#endif
				}
			}
		}
	}
}

bool FaceMorphInterface::LoadMorphData(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32	type;
	UInt32	length;

	bool error = false;

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * playerBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	UInt32 numMorphs = 0;
	if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
	{
		_MESSAGE("Error loading morph count");
		error = true;
		return true;
	}

#ifdef _DEBUG
	_DMESSAGE("%s - Loading %d morphs", __FUNCTION__, numMorphs);
#endif

	for (UInt32 i = 0; i < numMorphs; i++)
	{
		StringTableItem sculptName;
		UInt32 index = 0;
		float value = 0.0;
		UInt32 subVersion = 0;

		if (intfc->GetNextRecordInfo(&type, &subVersion, &length))
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
						_MESSAGE("Error loading sculpt name");
						error = true;
						return true;
					}
				}
				else if (kVersion >= FaceMorphInterface::kSerializationVersion1)
				{
					UInt16 nameLength = 0;
					if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
						_MESSAGE("Error loading morph name length");
						error = true;
						return true;
					}

					std::unique_ptr<char[]> name(new char[nameLength + 1]);
					if (!intfc->ReadRecordData(name.get(), nameLength)) {
						_MESSAGE("Error loading morph name");
						error = true;
						return true;
					}
					name[nameLength] = 0;
					sculptName = g_stringTable.GetString(name.get());
				}

				if (!intfc->ReadRecordData(&value, sizeof(value))) {
					_MESSAGE("Error loading morph value");
					error = true;
					return true;
				}
			}
			break;
			default:
				_MESSAGE("%s - unhandled type %08X", __FUNCTION__, type);
				error = true;
				break;
			}
		}

		if (value != 0.0) {
			_MESSAGE("Loaded Morph: %s - Value: %f", sculptName->c_str(), value);
			m_valueMap.SetMorphValue(playerBase, *sculptName, value);
		}
	}

	return error;
}

bool FaceMorphInterface::LoadSculptData(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32	type;
	UInt32	length;
	bool	error = false;

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * playerBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	UInt16 numParts = 0;
	if (!intfc->ReadRecordData(&numParts, sizeof(numParts)))
	{
		_MESSAGE("Error loading sculpt part count");
		error = true;
		return true;
	}

	for (UInt32 i = 0; i < numParts; i++)
	{
		UInt32 subVersion = 0;
		if (intfc->GetNextRecordInfo(&type, &subVersion, &length))
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
						_MESSAGE("Error loading sculpt name");
						error = true;
						return true;
					}
				}
				else if (kVersion >= FaceMorphInterface::kSerializationVersion1)
				{
					UInt16 nameLength = 0;
					if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
						_MESSAGE("Error loading sculpt part name length");
						error = true;
						return true;
					}

					std::unique_ptr<char[]> name(new char[nameLength + 1]);
					if (!intfc->ReadRecordData(name.get(), nameLength)) {
						_MESSAGE("Error loading sculpt part name");
						error = true;
						return true;
					}
					name[nameLength] = 0;
					sculptName = g_stringTable.GetString(name.get());
				}

				UInt16 totalDifferences = 0;
				if (!intfc->ReadRecordData(&totalDifferences, sizeof(totalDifferences))) {
					_MESSAGE("Error loading sculpt difference count");
					error = true;
					return true;
				}

				SculptDataPtr sculptTarget;
				if (totalDifferences > 0)
					sculptTarget = m_sculptStorage.GetSculptTarget(playerBase, true);
				MappedSculptDataPtr sculptHost;
				if (sculptTarget && totalDifferences > 0)
					sculptHost = sculptTarget->GetSculptHost(*sculptName, true);

				UInt16 index = 0;
				NiPoint3 value;
				for (UInt16 i = 0; i < totalDifferences; i++)
				{
					if (!intfc->ReadRecordData(&index, sizeof(index))) {
						_MESSAGE("Error loading sculpt index");
						error = true;
						return true;
					}

					if (!intfc->ReadRecordData(&value, sizeof(value))) {
						_MESSAGE("Error loading sculpt index");
						error = true;
						return true;
					}

					if (sculptTarget && sculptHost)
						sculptHost->force_insert(std::make_pair(index, value));
				}
			}
			break;
			default:
				_MESSAGE("unhandled type %08X", type);
				error = true;
				break;
			}
		}
	}

	return error;
}

struct PresetHeader
{
	enum
	{
		kSignature =		MACRO_SWAP32('SKSE'),	// endian-swapping so the order matches
		kVersion =			3,

		kVersion_Invalid =	0
	};

	UInt32	signature;
	UInt32	formatVersion;
	UInt32	skseVersion;
	UInt32	runtimeVersion;
};

#include <json/json.h>

bool FaceMorphInterface::SaveJsonPreset(const char * filePath)
{
	Json::StyledWriter writer;
	Json::Value root;

	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath);
	if (!currentFile.Create(filePath))
	{
		_ERROR("%s: couldn't create preset file (%s) Error (%d)", __FUNCTION__, filePath, GetLastError());
		return true;
	}

	Json::Value versionInfo;
	versionInfo["signature"] = PresetHeader::kSignature;
	versionInfo["formatVersion"] = PresetHeader::kVersion;
	versionInfo["skseVersion"] = PACKED_SKSE_VERSION;
	versionInfo["runtimeVersion"] = RUNTIME_VERSION_1_4_2;


	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	DataHandler * dataHandler = DataHandler::GetSingleton();

	bool isFemale = false;
	if (npc)
		isFemale = CALL_MEMBER_FN(npc, GetSex)() == 1;

	std::map<UInt8, const char *> modListLegacy;
	std::set<std::string> modList;
	std::map<UInt8, BGSHeadPart*> partList;

	UInt32 numHeadParts = 0;
	BGSHeadPart ** headParts = NULL;
	if (CALL_MEMBER_FN(npc, HasOverlays)()) {
		numHeadParts = GetNumActorBaseOverlays(npc);
		headParts = GetActorBaseOverlays(npc);
	}
	else {
		numHeadParts = npc->numHeadParts;
		headParts = npc->headparts;
	}

	// Acquire only vanilla dependencies
	for (UInt32 i = 0; i < numHeadParts; i++)
	{
		BGSHeadPart * headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {			
			ModInfo * modInfo = GetModInfoByFormID(headPart->formID, false);
			if (modInfo) {
				modListLegacy.emplace(modInfo->GetPartialIndex(), modInfo->name);
			}

			partList.emplace(i, headPart);
		}
	}

	// Acquire all mod dependencies
	for (UInt32 i = 0; i < numHeadParts; i++)
	{
		BGSHeadPart * headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			ModInfo * modInfo = GetModInfoByFormID(headPart->formID, true);
			if (modInfo) {
				modList.emplace(modInfo->name);
			}
		}
	}

	std::map<UInt8, std::pair<UInt32, const char*>> tintList;
	for (UInt32 i = 0; i < player->tintMasks.count; i++)
	{
		TintMask * tintMask = NULL;
		if (player->tintMasks.GetNthItem(i, tintMask))
		{
			UInt32 tintColor = ((UInt32)(tintMask->alpha * 255.0) << 24) | tintMask->color.red << 16 | tintMask->color.green << 8 | tintMask->color.blue;
			if (tintMask->texture)
				tintList.emplace(i, std::make_pair(tintColor, tintMask->texture->str.data));
		}
	}

	Json::Value modInfo;
	for (auto mIt = modListLegacy.begin(); mIt != modListLegacy.end(); ++mIt)
	{
		Json::Value mod;
		mod["index"] = mIt->first;
		mod["name"] = mIt->second;
		modInfo.append(mod);
	}

	Json::Value modNames;
	for (auto mIt = modList.begin(); mIt != modList.end(); ++mIt)
	{
		modNames.append(*mIt);
	}

	Json::Value headPartInfo;
	for (auto pIt = partList.begin(); pIt != partList.end(); ++pIt)
	{
		Json::Value partInfo;
		partInfo["type"] = pIt->first;
		partInfo["formId"] = (Json::UInt)pIt->second->formID;
		partInfo["formIdentifier"] = GetFormIdentifier(pIt->second);
		headPartInfo.append(partInfo);
	}

	Json::Value tintInfo;
	for (auto tmIt = tintList.begin(); tmIt != tintList.end(); ++tmIt)
	{
		Json::Value tint;
		tint["index"] = tmIt->first;
		tint["color"] = (Json::UInt)tmIt->second.first;
		tint["texture"] = tmIt->second.second;
		tintInfo.append(tint);
	}

	Json::Value morphInfo;
	if (npc->faceMorph)
	{
		for (UInt8 p = 0; p < TESNPC::FaceMorphs::kNumPresets; p++) {
			Json::Value morphValue = (Json::UInt)npc->faceMorph->presets[p];
			morphInfo["presets"].append(morphValue);
		}

		for (UInt8 o = 0; o < TESNPC::FaceMorphs::kNumOptions; o++) {
			Json::Value morphValue = npc->faceMorph->option[o];
			morphInfo["morphs"].append(morphValue);
		}
	}

	Json::Value customMorphInfo;
	ValueSet * valueSet = m_valueMap.GetValueSet(npc);
	if (valueSet)
	{
		for (auto it = valueSet->begin(); it != valueSet->end(); ++it)
		{
			if (it->second != 0.0) {
				Json::Value morphValue;
				morphValue["name"] = it->first->c_str();
				morphValue["value"] = it->second;
				customMorphInfo.append(morphValue);
			}
		}
	}

	
	Json::Value sculptData;
	auto sculptTarget = GetSculptTarget(npc, false);
	if (sculptTarget) {
		for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			BGSHeadPart * headPart = headParts[i];
			if (headPart) {
				BSFixedString morphPath = SculptData::GetHostByPart(headPart);
				auto sculptHost = sculptTarget->GetSculptHost(morphPath, false);
				if (sculptHost) {
					Json::Value hostData;
					hostData["host"] = morphPath.data;

					TRIModelData data;
					GetModelTri(morphPath, data);
					
					hostData["vertices"] = (Json::UInt)data.vertexCount;

					for (auto morph : *sculptHost) {
						Json::Value value;
						value.append(morph.first);
						value.append((Json::Int)(morph.second.x * VERTEX_MULTIPLIER));
						value.append((Json::Int)(morph.second.y * VERTEX_MULTIPLIER));
						value.append((Json::Int)(morph.second.z * VERTEX_MULTIPLIER));
						hostData["data"].append(value);
					}

					sculptData.append(hostData);
				}
			}
		}
	}

	Json::Value textureInfo;
	BGSHeadPart * facePart = npc->GetCurrentHeadPartByType(BGSHeadPart::kTypeFace);
	if (facePart) {
		BGSTextureSet * textureSet = GetTextureSetForPart(npc, facePart);
		if (textureSet) {
			for (UInt8 i = 0; i < BSShaderTextureSet::kNumTextures; i++) {
				const char * texturePath = textureSet->textureSet.GetTexturePath(i);
				if (texturePath != NULL) {
					Json::Value textureValue;
					textureValue["index"] = i;
					textureValue["texture"] = texturePath;
					textureInfo.append(textureValue);
				}
			}
		}
	}
	
	// Collect override data
	PresetData::OverrideData overrideData;
	g_overrideInterface.VisitNodes(player, [&overrideData](SKEEFixedString node, OverrideVariant & value)
	{
		overrideData[node].push_back(value);
	});

	// Collect skin data
	PresetData::SkinData skinData[2];
	for (UInt32 i = 0; i <= 1; i++) {
		g_overrideInterface.VisitSkin(player, isFemale, i == 1, [&i, &skinData](UInt32 slotMask, OverrideVariant & value)
		{
			skinData[i][slotMask].push_back(value);
			return false;
		});
	}
	
	// Collect transform data
	PresetData::TransformData transformData[2];
	for (UInt32 i = 0; i <= 1; i++) {
		g_transformInterface.Impl_VisitNodes(player, i == 1, isFemale, [&i, &transformData](SKEEFixedString node, OverrideRegistration<StringTableItem> * keys)
		{
			keys->Visit([&i, &node, &transformData](const StringTableItem & key, OverrideSet * set)
			{
				if (*key == SKEEFixedString("internal"))
					return false;

				set->Visit([&i, &node, &transformData, &key](OverrideVariant * value)
				{
					transformData[i][node][*key].push_back(*value);
					return false;
				});
				return false;
			});

			return false;
		});
	}

	// Collect body morph data
	PresetData::BodyMorphData bodyMorphData;
	g_bodyMorphInterface.Impl_VisitMorphs(player, [&](SKEEFixedString name, std::unordered_map<StringTableItem, float> * map)
	{
		for (auto & it : *map)
		{
			bodyMorphData[name][*it.first] = it.second;
		}
	});

	for (UInt32 i = 0; i <= 1; i++) {
		for (auto & data : transformData[i]) {
			Json::Value transform;
			transform["firstPerson"] = (bool)(i == 1);
			transform["node"] = data.first.c_str();

			for (auto & key : data.second) {
				Json::Value transformKey;
				transformKey["name"] = key.first.c_str();

				for (auto & value : key.second) {
					Json::Value jvalue;
					jvalue["key"] = value.key;
					jvalue["type"] = value.type;
					jvalue["index"] = value.index;
					switch (value.type) {
						case OverrideVariant::kType_Bool:
						jvalue["data"] = value.data.b;
						break;
						case OverrideVariant::kType_Int:
						jvalue["data"] = value.data.i;
						break;
						case OverrideVariant::kType_Float:
						jvalue["data"] = value.data.f;
						break;
						case OverrideVariant::kType_String:
						jvalue["data"] = value.str ? value.str->c_str() : "";
						break;
					}
					transformKey["values"].append(jvalue);
				}
				transform["keys"].append(transformKey);
			}
			root["transforms"].append(transform);
		}
	}
	for (auto & data : overrideData) {
		Json::Value ovr;
		ovr["node"] = data.first.c_str();

		for (auto & value : data.second) {
			Json::Value jvalue;
			jvalue["key"] = value.key;
			jvalue["type"] = value.type;
			jvalue["index"] = value.index;
			switch (value.type) {
				case OverrideVariant::kType_Bool:
				jvalue["data"] = value.data.b;
				break;
				case OverrideVariant::kType_Int:
				jvalue["data"] = value.data.i;
				break;
				case OverrideVariant::kType_Float:
				jvalue["data"] = value.data.f;
				break;
				case OverrideVariant::kType_String:
				jvalue["data"] = value.str ? value.str->c_str() : "";
				break;
			}
			ovr["values"].append(jvalue);
		}
		root["overrides"].append(ovr);
	}

	for (UInt32 i = 0; i <= 1; i++) {
		for (auto & data : skinData[i]) {
			Json::Value slot;
			slot["firstPerson"] = (bool)(i == 1);
			slot["slotMask"] = (Json::UInt)data.first;

			for (auto & value : data.second) {
				Json::Value jvalue;
				jvalue["key"] = value.key;
				jvalue["type"] = value.type;
				jvalue["index"] = value.index;
				switch (value.type) {
					case OverrideVariant::kType_Bool:
						jvalue["data"] = value.data.b;
						break;
					case OverrideVariant::kType_Int:
						jvalue["data"] = value.data.i;
						break;
					case OverrideVariant::kType_Float:
						jvalue["data"] = value.data.f;
						break;
					case OverrideVariant::kType_String:
						jvalue["data"] = value.str ? value.str->c_str() : "";
						break;
				}
				slot["values"].append(jvalue);
			}

			root["skinOverrides"].append(slot);
		}
	}

	for (auto & data : bodyMorphData) {
		Json::Value bm;
		bm["name"] = data.first.c_str();
		for (auto & keys : data.second)
		{
			Json::Value jvalue;
			jvalue["key"] = keys.first.c_str();
			jvalue["value"] = keys.second;
			bm["keys"].append(jvalue);
		}
		root["bodyMorphs"].append(bm);
	}
	
	root["version"] = versionInfo;
	root["mods"] = modInfo;
	root["modNames"] = modNames;
	root["headParts"] = headPartInfo;
	root["actor"]["weight"] = npc->weight;

	if (npc->headData) {
		auto hairColor = npc->headData->hairColor;
		if (hairColor)
			root["actor"]["hairColor"] = (Json::UInt)(hairColor->color.red << 16 | hairColor->color.green << 8 | hairColor->color.blue);
		
		auto headTexture = npc->headData->headTexture;
		if (headTexture) {
			root["actor"]["headTexture"] = GetFormIdentifier(headTexture);

			ModInfo * modInfo = GetModInfoByFormID(headTexture->formID, true);
			if (modInfo) {
				modList.emplace(modInfo->name);
			}
		}
	}
	
	root["tintInfo"] = tintInfo;
	root["faceTextures"] = textureInfo;
	root["morphs"]["default"] = morphInfo;
	root["morphs"]["custom"] = customMorphInfo;
	root["morphs"]["sculptDivisor"] = VERTEX_MULTIPLIER;
	root["morphs"]["sculpt"] = sculptData;

	std::string data = writer.write(root);
	currentFile.WriteBuf(data.c_str(), data.length());
	currentFile.Close();
	return false;
}

bool FaceMorphInterface::SaveBinaryPreset(const char * filePath)
{
	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath);

	_DMESSAGE("creating preset");
	if(!currentFile.Create(filePath))
	{
		_ERROR("%s: couldn't create preset file (%s) Error (%d)", __FUNCTION__, filePath, GetLastError());
		return true;
	}

	try
	{
		PresetHeader fileHeader;
		fileHeader.signature =		PresetHeader::kSignature;
		fileHeader.formatVersion =	PresetHeader::kVersion;
		fileHeader.skseVersion =	PACKED_SKSE_VERSION;
		fileHeader.runtimeVersion = RUNTIME_VERSION_1_4_2;

		currentFile.Skip(sizeof(fileHeader));

		PlayerCharacter * player = (*g_thePlayer);
		TESNPC * npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
		DataHandler * dataHandler = DataHandler::GetSingleton();

		typedef std::map<UInt8, const char *> ModMap;
		typedef std::pair<UInt8, const char*> ModPair;

		typedef std::map<UInt8, BGSHeadPart*> PartMap;
		typedef std::pair<UInt8, BGSHeadPart*> PartPair;

		typedef std::pair<UInt32, const char*> TintCouple;
		typedef std::map<UInt8, TintCouple> TintMap;
		typedef std::pair<UInt8, TintCouple> TintPair;

		ModMap modList;
		PartMap partList;

		UInt32 numHeadParts = 0;
		BGSHeadPart ** headParts = NULL;
		if (CALL_MEMBER_FN(npc, HasOverlays)()) {
			numHeadParts = GetNumActorBaseOverlays(npc);
			headParts = GetActorBaseOverlays(npc);
		} else {
			numHeadParts = npc->numHeadParts;
			headParts = npc->headparts;
		}
		for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			BGSHeadPart * headPart = headParts[i];
			if(headPart && !headPart->IsExtraPart()) {
				ModInfo* modInfo = GetModInfoByFormID(headPart->formID);
				if(modInfo) {
					modList.emplace(modInfo->modIndex, modInfo->name);
					partList.emplace(i, headPart);
				}
			}
		}

		TintMap tintList;
		for(UInt32 i = 0; i < player->tintMasks.count; i++)
		{
			TintMask * tintMask = NULL;
			if(player->tintMasks.GetNthItem(i, tintMask))
			{
				UInt32 tintColor = ((UInt32)(tintMask->alpha * 255.0) << 24) | tintMask->color.red << 16 | tintMask->color.green << 8 | tintMask->color.blue;
				tintList.emplace(i, TintCouple(tintColor, tintMask->texture->str.data));
			}
		}

		UInt8 modCount = modList.size();
		UInt8 partCount = partList.size();
		UInt8 tintCount = tintList.size();

		currentFile.Write8(modCount);
		for(auto mIt = modList.begin(); mIt != modList.end(); ++mIt)
		{
			currentFile.Write8(mIt->first);

			UInt16 strLen = strlen(mIt->second);
			currentFile.Write16(strLen);
			currentFile.WriteBuf(mIt->second, strLen);
		}

		currentFile.Write8(partCount);
		for(auto pIt = partList.begin(); pIt != partList.end(); ++pIt)
		{
			currentFile.Write8(pIt->first);
			currentFile.Write32(pIt->second->formID);
		}

		currentFile.WriteFloat(npc->weight);

		if (npc->faceMorph)
		{
			currentFile.Write8(TESNPC::FaceMorphs::kNumPresets);
			for (UInt8 p = 0; p < TESNPC::FaceMorphs::kNumPresets; p++)
				currentFile.Write8(npc->faceMorph->presets[p]);

			currentFile.Write8(TESNPC::FaceMorphs::kNumOptions);
			for (UInt8 o = 0; o < TESNPC::FaceMorphs::kNumOptions; o++)
				currentFile.WriteFloat(npc->faceMorph->option[o]);
		}
		else {
			currentFile.Write8(0);
			currentFile.Write8(0);
		}		

		UInt32 hairColor = 0;
		if(npc->headData && npc->headData->hairColor) {
			hairColor = npc->headData->hairColor->color.red << 16 | npc->headData->hairColor->color.green << 8 | npc->headData->hairColor->color.blue;
		}
		currentFile.Write32(hairColor);

		currentFile.Write8(tintCount);
		for(auto tmIt = tintList.begin(); tmIt != tintList.end(); ++tmIt)
		{
			currentFile.Write8(tmIt->first);
			currentFile.Write32(tmIt->second.first);

			UInt16 strLen = strlen(tmIt->second.second);
			currentFile.Write16(strLen);
			currentFile.WriteBuf(tmIt->second.second, strLen);
		}

		SInt64 offset = currentFile.GetOffset();
		currentFile.Skip(sizeof(UInt8));
		UInt8 totalMorphs = 0;

		ValueSet * valueSet = m_valueMap.GetValueSet(npc);
		if(valueSet)
		{
			for(auto it = valueSet->begin(); it != valueSet->end(); ++it)
			{
				if(it->second != 0.0) {
					UInt16 strLen = strlen(it->first->c_str());
					currentFile.Write16(strLen);
					currentFile.WriteBuf(it->first->c_str(), strLen);

					currentFile.WriteFloat(it->second);
					totalMorphs++;
				}
			}
		}
		
		SInt64 jumpBack = currentFile.GetOffset();
		currentFile.SetOffset(offset);
		currentFile.Write8(totalMorphs);
		
		currentFile.SetOffset(jumpBack);
		offset = currentFile.GetOffset();
		currentFile.Skip(sizeof(UInt8));
		UInt8 totalTextures = 0;
		BGSHeadPart * facePart = npc->GetCurrentHeadPartByType(BGSHeadPart::kTypeFace);
		if (facePart) {
			BGSTextureSet * textureSet = GetTextureSetForPart(npc, facePart);
			if (textureSet) {
				for (UInt8 i = 0; i < BSShaderTextureSet::kNumTextures; i++) {
					const char * texturePath = textureSet->textureSet.GetTexturePath(i);
					if (texturePath != NULL) {
						UInt16 strLen = strlen(texturePath);
						currentFile.Write8(i);
						currentFile.Write16(strLen);
						currentFile.WriteBuf(texturePath, strLen);
						totalTextures++;
					}
				}
			}
		}
		currentFile.SetOffset(offset);
		currentFile.Write8(totalTextures);

		// write header
		currentFile.SetOffset(0);
		currentFile.WriteBuf(&fileHeader, sizeof(fileHeader));

	}
	catch(...)
	{
		_ERROR("SavePreset: exception during save");
	}

	currentFile.Close();
	return false;
}

PresetData::PresetData()
{
	weight = 0;
	hairColor = 0;
	headTexture = nullptr;
}

bool FaceMorphInterface::LoadJsonPreset(const char * filePath, PresetDataPtr presetData)
{
	bool loadError = false;
	BSResourceNiBinaryStream file(filePath);
	if (!file.IsValid()) {
		_ERROR("%s: File %s failed to open.", __FUNCTION__, filePath);
		loadError = true;
		return loadError;
	}

	std::string in;
	BSFileUtil::ReadAll(&file, in);

	Json::Features features;
	features.all();

	Json::Value root;
	Json::Reader reader(features);

	bool parseSuccess = reader.parse(in, root);
	if (!parseSuccess) {
		_ERROR("%s: Error occured parsing json for %s.", __FUNCTION__, filePath);
		loadError = true;
		return loadError;
	}

	Json::Value defaultValue;
	Json::Value version = root["version"];
	if (version.empty()) {
		_ERROR("%s: No version header.", __FUNCTION__);
		loadError = true;
		return loadError;
	}

	UInt32 signature = version["signature"].asUInt();
	if (signature != PresetHeader::kSignature)
	{
		_ERROR("%s: invalid file signature (found %08X expected %08X)", __FUNCTION__, signature, PresetHeader::kSignature);
		loadError = true;
		return loadError;
	}

	UInt32 formatVersion = version["formatVersion"].asUInt();
	if (formatVersion <= PresetHeader::kVersion_Invalid)
	{
		_ERROR("%s: version invalid (%08X)", __FUNCTION__, formatVersion);
		loadError = true;
		return loadError;
	}

	Json::Value mods = root["mods"];
	Json::Value modNames = root["modNames"];
	if (mods.empty() && modNames.empty()) {
		_ERROR("%s: No mods header.", __FUNCTION__);
		loadError = true;
		return loadError;
	}

	DataHandler * dataHandler = DataHandler::GetSingleton();

	std::map<UInt32, std::string> modList;
	if (mods.type() == Json::arrayValue) {
		for (auto & mod : mods) {
			UInt32 modIndex = mod["index"].asUInt();
			std::string modName = mod["name"].asString();

			modList.emplace(modIndex, modName);
			presetData->modList.push_back(modName);
		}
	}

	if (root.isMember("modNames") && modNames.type() == Json::arrayValue) {
		presetData->modList.clear();
		for (auto & mod : modNames) {
			presetData->modList.push_back(mod.asString());
		}
	}

	Json::Value headParts = root["headParts"];
	if (!headParts.empty() && headParts.type() == Json::arrayValue) {
		for (auto & part : headParts) {
			if (part.isMember("formIdentifier")) {
				TESForm * headPartForm = GetFormFromIdentifier(part["formIdentifier"].asString());
				if (headPartForm) {
					BGSHeadPart * headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
					if (headPart) {
						presetData->headParts.push_back(headPart);
					}
				}
			}
			else if (part.isMember("formId")) {
				UInt8 partType = part["type"].asUInt();
				UInt32 formId = part["formId"].asUInt();

				UInt32 modIndex = formId >> 24;
				auto it = modList.find(modIndex != 0xFE ? modIndex : (formId >> 12));
				if (it != modList.end()) {
					const ModInfo * modInfo = dataHandler->LookupModByName(it->second.c_str());
					if (modInfo && modInfo->IsActive()) {
						formId = modInfo->GetFormID(formId);
						TESForm * headPartForm = LookupFormByID(formId);
						if (headPartForm) {
							BGSHeadPart * headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
							if (headPart) {
								presetData->headParts.push_back(headPart);
							}
						}
						else {
							_WARNING("Could not resolve part %08X", formId);
						}
					}
					else {
						_WARNING("Could not load part type %d from %s; mod not found.", partType, it->second.c_str());
					}
				}
			}
		}
	}

	Json::Value actor = root["actor"];
	if (!actor.empty() && actor.type() == Json::objectValue) {
		presetData->weight = actor["weight"].asFloat();
		presetData->hairColor = actor["hairColor"].asUInt();
		if (actor.isMember("headTexture")) {
			presetData->headTexture = DYNAMIC_CAST(GetFormFromIdentifier(actor["headTexture"].asString()), TESForm, BGSTextureSet);
		}
	}


	Json::Value tintInfo = root["tintInfo"];
	if (!tintInfo.empty() && tintInfo.type() == Json::arrayValue) {
		for (auto & tint : tintInfo) {
			PresetData::Tint tintData;
			tintData.color = tint["color"].asUInt();
			tintData.index = tint["index"].asUInt();
			tintData.name = tint["texture"].asString().c_str();
			presetData->tints.push_back(tintData);
		}
	}

	Json::Value faceTextures = root["faceTextures"];
	if (!faceTextures.empty() && faceTextures.type() == Json::arrayValue) {
		for (auto & faceTexture : faceTextures) {
			PresetData::Texture texture;
			texture.index = faceTexture["index"].asUInt();
			texture.name = faceTexture["texture"].asString().c_str();
			presetData->faceTextures.push_back(texture);
		}
	}

	Json::Value morphs = root["morphs"];
	if (!morphs.empty()) {
		Json::Value defaultMorphs = morphs["default"];
		if (!defaultMorphs.empty()) {
			Json::Value presets = defaultMorphs["presets"];
			for (auto & preset : presets) {
				UInt32 presetValue = preset.asUInt();
				if (presetValue == 255)
					presetValue = -1;

				presetData->presets.push_back(presetValue);
			}

			Json::Value morphs = defaultMorphs["morphs"];
			for (auto & morph : morphs) {
				presetData->morphs.push_back(morph.asFloat());
			}
		}
		Json::Value customMorphs = morphs["custom"];
		if (!customMorphs.empty()) {
			for (auto & customMorph : customMorphs) {
				PresetData::Morph morph;
				morph.name = customMorph["name"].asString().c_str();
				morph.value = customMorph["value"].asFloat();
				presetData->customMorphs.push_back(morph);
			}
		}

		SInt32 multiplier = -1;

		Json::Value sculptMult = morphs["sculptDivisor"];
		if (!sculptMult.empty())
			multiplier = sculptMult.asInt();

		Json::Value sculptData = morphs["sculpt"];
		if (!sculptData.empty()) {
			presetData->sculptData = std::make_shared<SculptData>();
			for (auto & hostFile : sculptData) {
				SKEEFixedString host = hostFile["host"].asString().c_str();
				Json::Value data = hostFile["data"];

				auto sculptedData = std::make_shared<MappedSculptData>();
				for (auto & morphData : data) {
					UInt16 index = morphData[0].asUInt();
					NiPoint3 pt;

					if (multiplier > 0) {
						pt.x = (float)morphData[1].asInt() / (float)multiplier;
						pt.y = (float)morphData[2].asInt() / (float)multiplier;
						pt.z = (float)morphData[3].asInt() / (float)multiplier;
					} else {
						pt.x = morphData[1].asFloat();
						pt.y = morphData[2].asFloat();
						pt.z = morphData[3].asFloat();
					}

					sculptedData->force_insert(std::make_pair(index, pt));
				}

				presetData->sculptData->emplace(g_stringTable.GetString(host), sculptedData);
			}
		}
	}

	Json::Value transforms = root["transforms"];
	if (!transforms.empty()) {
		for (auto & xForm : transforms) {
			bool isFirstPerson = xForm["firstPerson"].asBool();
			BSFixedString nodeName = xForm["node"].asString().c_str();

			Json::Value keys = xForm["keys"];
			for (auto & key : keys) {
				BSFixedString keyName = key["name"].asString().c_str();

				Json::Value values = key["values"];
				for (auto & jvalue : values) {
					OverrideVariant value;
					value.key = jvalue["key"].asUInt();
					value.type = jvalue["type"].asInt();
					value.index = jvalue["index"].asInt();
					switch (value.type) {
						case OverrideVariant::kType_Bool:
						value.data.b = jvalue["data"].asBool();
						break;
						case OverrideVariant::kType_Int:
						value.data.i = jvalue["data"].asInt();
						break;
						case OverrideVariant::kType_Float:
						value.data.f = jvalue["data"].asFloat();
						break;
						case OverrideVariant::kType_String:
						value.str = g_stringTable.GetString(jvalue["data"].asString().c_str());
						break;
					}

					presetData->transformData[isFirstPerson ? 1 : 0][nodeName][keyName].push_back(value);
				}
			}
		}
	}

	Json::Value overrides = root["overrides"];
	if (!overrides.empty()) {
		for (auto & ovr : overrides) {
			BSFixedString node = ovr["node"].asString().c_str();
			Json::Value values = ovr["values"];
			for (auto & jvalue : values) {
				OverrideVariant value;
				value.key = jvalue["key"].asUInt();
				value.type = jvalue["type"].asInt();
				value.index = jvalue["index"].asInt();
				switch (value.type) {
					case OverrideVariant::kType_Bool:
					value.data.b = jvalue["data"].asBool();
					break;
					case OverrideVariant::kType_Int:
					value.data.i = jvalue["data"].asInt();
					break;
					case OverrideVariant::kType_Float:
					value.data.f = jvalue["data"].asFloat();
					break;
					case OverrideVariant::kType_String:
					value.str = g_stringTable.GetString(jvalue["data"].asString().c_str());
					break;
				}
				presetData->overrideData[node].push_back(value);
			}
		}
	}

	Json::Value skinOverrides = root["skinOverrides"];
	if (!skinOverrides.empty()) {
		for (auto & skinData : skinOverrides) {
			bool isFirstPerson = skinData["firstPerson"].asBool();
			UInt32 slotMask = skinData["slotMask"].asUInt();

			Json::Value values = skinData["values"];
			for (auto & jvalue : values) {
				OverrideVariant value;
				value.key = jvalue["key"].asUInt();
				value.type = jvalue["type"].asInt();
				value.index = jvalue["index"].asInt();
				switch (value.type) {
					case OverrideVariant::kType_Bool:
						value.data.b = jvalue["data"].asBool();
						break;
					case OverrideVariant::kType_Int:
						value.data.i = jvalue["data"].asInt();
						break;
					case OverrideVariant::kType_Float:
						value.data.f = jvalue["data"].asFloat();
						break;
					case OverrideVariant::kType_String:
						value.str = g_stringTable.GetString(jvalue["data"].asString().c_str());
						break;
				}

				presetData->skinData[isFirstPerson ? 1 : 0][slotMask].push_back(value);
			}
		}
	}

	Json::Value bodyMorphs = root["bodyMorphs"];
	if (!bodyMorphs.empty()) {
		for (auto & bm : bodyMorphs) {
			BSFixedString name = bm["name"].asString().c_str();

			// Legacy version
			Json::Value keyless = bm["value"];
			if (!keyless.empty())
			{
				float value = bm["value"].asFloat();
				presetData->bodyMorphData[name]["RSMLegacy"] = value;
			}

			// New version
			Json::Value values = bm["keys"];
			if (!values.empty())
			{
				for (auto & jvalue : values) {
					SKEEFixedString key = jvalue["key"].asString().c_str();
					float value = jvalue["value"].asFloat();

					// If the keys were mapped by mod name, skip them if they arent in load order
					std::string strKey(key);
					SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
					if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
					{
						const ModInfo * modInfo = dataHandler->LookupModByName(key.c_str());
						if (!modInfo || !modInfo->IsActive())
							continue;
					}

					presetData->bodyMorphData[name][key] = value;
				}
			}
		}
	}

	return loadError;
}

bool FaceMorphInterface::LoadBinaryPreset(const char * filePath, PresetDataPtr presetData)
{
	bool loadError = false;
	BSResourceNiBinaryStream file(filePath);
	if (!file.IsValid()) {
		_ERROR("%s: File %s failed to open.", __FUNCTION__, filePath);
		loadError = true;
		return loadError;
	}

	try
	{
		PresetHeader header;
		file.Read(&header, sizeof(header));

		if(header.signature != PresetHeader::kSignature)
		{
			_ERROR("%s: invalid file signature (found %08X expected %08X)", __FUNCTION__, header.signature, PresetHeader::kSignature);
			loadError = true;
			goto done;
		}

		if(header.formatVersion <= PresetHeader::kVersion_Invalid)
		{
			_ERROR("%s: version invalid (%08X)", __FUNCTION__, header.formatVersion);
			loadError = true;
			goto done;
		}

		if(header.formatVersion < 2)
		{
			_ERROR("%s: version too old (found %08X current %08X)", __FUNCTION__, header.formatVersion, PresetHeader::kVersion);
			goto done;
		}

		DataHandler * dataHandler = DataHandler::GetSingleton();

		typedef std::map<UInt8, std::string> ModMap;
		typedef std::pair<UInt8, std::string> ModPair;

		ModMap modList;
		UInt8 modCount = 0;
		file.Read(&modCount, sizeof(modCount));

		char textBuffer[MAX_PATH];
		for(UInt8 i = 0; i < modCount; i++)
		{
			UInt8 modIndex;
			file.Read(&modIndex, sizeof(modIndex));

			UInt16 strLen = 0;
			file.Read(&strLen, sizeof(strLen));

			memset(textBuffer, 0, MAX_PATH);
			file.Read(textBuffer, strLen);

			std::string modName(textBuffer);

			modList.emplace(modIndex, modName);
			presetData->modList.push_back(modName);
		}

		UInt8 partCount = 0;
		file.Read(&partCount, sizeof(partCount));

		for(UInt8 i = 0; i < partCount; i++)
		{
			UInt8 partType = 0;
			file.Read(&partType, sizeof(partType));

			UInt32 formId = 0;
			file.Read(&formId, sizeof(formId));

			UInt32 modIndex = formId >> 24;
			auto it = modList.find(modIndex != 0xFE ? modIndex : (formId >> 12));
			if (it != modList.end()) {
				const ModInfo * modInfo = dataHandler->LookupModByName(it->second.c_str());
				if (modInfo && modInfo->IsActive()) {
					formId = modInfo->GetFormID(formId);
					TESForm * headPartForm = LookupFormByID(formId);
					if(headPartForm) {
						BGSHeadPart * headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
						if(headPart) {
							presetData->headParts.push_back(headPart);
						}
					} else {
						_WARNING("Could not resolve part %08X", formId);
					}
				} else {
					_WARNING("Could not load part type %d from %s; mod not found.", partType, it->second.c_str());
				}
			}
		}

		float weight = 0.0;
		file.Read(&weight, sizeof(weight));

		presetData->weight = weight;

		UInt8 presetCount = 0;
		file.Read(&presetCount, sizeof(presetCount));

		for(UInt8 i = 0; i < presetCount; i++)
		{
			SInt32 preset = 0;
			file.Read(&preset, sizeof(UInt8));

			if(preset == 255)
				preset = -1;

			presetData->presets.push_back(preset);
		}

		UInt8 optionCount = 0;
		file.Read(&optionCount, sizeof(optionCount));

		for(UInt8 i = 0; i < optionCount; i++)
		{
			float option = 0.0;
			file.Read(&option, sizeof(option));

			presetData->morphs.push_back(option);
		}

		UInt32 hairColor = 0;
		file.Read(&hairColor, sizeof(hairColor));

		presetData->hairColor = hairColor;

		UInt8 tintCount = 0;
		file.Read(&tintCount, sizeof(tintCount));

		for(UInt8 i = 0; i < tintCount; i++)
		{
			UInt8 tintIndex = 0;
			file.Read(&tintIndex, sizeof(tintIndex));

			UInt32 tintColor = 0;
			file.Read(&tintColor, sizeof(tintColor));

			UInt16 strLen = 0;
			file.Read(&strLen, sizeof(strLen));

			memset(textBuffer, 0, MAX_PATH);
			file.Read(textBuffer, strLen);

			BSFixedString tintPath = textBuffer;

			PresetData::Tint tint;
			tint.color = tintColor;
			tint.index = tintIndex;
			tint.name = tintPath;
			presetData->tints.push_back(tint);
		}

		UInt8 morphCount = 0;
		file.Read(&morphCount, sizeof(morphCount));

		for(UInt8 i = 0; i < morphCount; i++)
		{
			UInt16 strLen = 0;
			file.Read(&strLen, sizeof(strLen));

			memset(textBuffer, 0, MAX_PATH);
			file.Read(textBuffer, strLen);

			float morphValue = 0.0;
			file.Read(&morphValue, sizeof(morphValue));

			PresetData::Morph morph;
			morph.name = textBuffer;
			morph.value = morphValue;

			presetData->customMorphs.push_back(morph);
		}

		if(header.formatVersion >= 3)
		{
			UInt8 textureCount = 0;
			file.Read(&textureCount, sizeof(textureCount));

			for(UInt8 i = 0; i < textureCount; i++)
			{
				UInt8 textureIndex = 0;
				file.Read(&textureIndex, sizeof(textureIndex));

				UInt16 strLen = 0;
				file.Read(&strLen, sizeof(strLen));

				memset(textBuffer, 0, MAX_PATH);
				file.Read(textBuffer, strLen);

				BSFixedString texturePath(textBuffer);

				PresetData::Texture texture;
				texture.index = textureIndex;
				texture.name = texturePath;

				presetData->faceTextures.push_back(texture);
			}
		}
	}
	catch(...)
	{
		_ERROR("%s: exception during load", __FUNCTION__);
		loadError = true;
	}

done:
	return loadError;
}

SKEEFixedString SculptData::GetHostByPart(BGSHeadPart * headPart)
{
	const char * morphPath = headPart->chargenMorph.GetModelName();
	if (morphPath != NULL && morphPath[0] != NULL)
		return morphPath;

	morphPath = headPart->morph.GetModelName();
	if (morphPath != NULL && morphPath[0] != NULL)
		return morphPath;

	morphPath = headPart->raceMorph.GetModelName();
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

SculptDataPtr SculptStorage::GetSculptTarget(TESNPC * target, bool create)
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

void SculptStorage::SetSculptTarget(TESNPC * target, SculptDataPtr sculptData)
{
	auto it = find(target);
	if (it != end())
		it->second = sculptData;
	else {
		emplace(target, sculptData);
	}
}

void SculptStorage::EraseNPC(TESNPC * npc)
{
	auto sculptTarget = find(npc);
	if (sculptTarget != end()) {
		erase(sculptTarget);
	}
}

SKSETaskApplyMorphs::SKSETaskApplyMorphs(Actor * actor)
{
	m_formId = actor->formID;
}

void SKSETaskApplyMorphs::Run()
{
	if (!m_formId)
		return;

	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if (!actor)
		return;

	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (!actorBase)
		return;

	BSFaceGenNiNode * faceNode = actor->GetFaceGenNiNode();
	if (faceNode) {
		g_morphInterface.ApplyMorphs(actorBase, faceNode);
	}
}

SKSETaskApplyMorphNormals::SKSETaskApplyMorphNormals(NiPointer<NiAVObject> faceNode, bool updateModel)
	: m_faceNode(faceNode)
	, m_updateModel(updateModel)
{

}


void SKSETaskApplyMorphNormals::Run()
{
	// Copies from the FOD into the Dynamic Geometry
	if (m_updateModel)
	{
		UpdateModelFace(m_faceNode);
	}
	
	VisitGeometry(m_faceNode, [](BSGeometry* geometry)
	{
		UInt64 vertexDesc = geometry->vertexDesc;

		bool hasNormals = (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_NORMAL) == VertexFlags::VF_NORMAL;
		bool hasTangents = (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_TANGENT) == VertexFlags::VF_TANGENT;

		if (!hasNormals && !hasTangents)
		{
			return false;
		}

		BSShaderProperty* shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if (!shaderProperty || !ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
		{
			return false;
		}

		BSLightingShaderMaterial* material = (BSLightingShaderMaterial*)shaderProperty->material;
		if (!material || material->GetShaderType() != BSLightingShaderMaterial::kShaderType_FaceGen)
		{
			return false;
		}

		NiSkinInstance* skinInstance = niptr_cast<NiSkinInstance>(geometry->m_spSkinInstance);
		if (!skinInstance) {
			return false;
		}

		NiSkinPartition* skinPartition = niptr_cast<NiSkinPartition>(skinInstance->m_spSkinPartition);
		if (!skinPartition) {
			return false;
		}

		NiSkinPartition* newSkinPartition = nullptr;
		CALL_MEMBER_FN(skinPartition, DeepCopy)((NiObject**)&newSkinPartition);

		if (!newSkinPartition) {
			return false;
		}

		auto& partition = newSkinPartition->m_pkPartitions[0];
		UInt32 vertexSize = newSkinPartition->GetVertexSize(partition.vertexDesc);

		{
			NormalApplicator applicator(geometry, newSkinPartition);
			applicator.Apply();
		}

		// Propagate the data to the other partitions
		for (UInt32 p = 1; p < newSkinPartition->m_uiPartitions; ++p)
		{
			auto& pPartition = newSkinPartition->m_pkPartitions[p];
			memcpy(pPartition.shapeData->m_RawVertexData, partition.shapeData->m_RawVertexData, newSkinPartition->vertexCount * vertexSize);
		}

		NIOVTaskUpdateSkinPartition update(skinInstance, newSkinPartition);
		newSkinPartition->DecRef(); // DeepCopy started refcount at 1, passed ownership to the task
		update.Run();
		return false;
	});
}

