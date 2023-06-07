#include "PresetInterface.h"

#include "common/IFileStream.h"
#include "skse64_common/skse_version.h"

#include "skse64/GameRTTI.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"
#include "skse64/GameData.h"
#include "skse64/GameStreams.h"

#include "OverrideInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "FaceMorphInterface.h"

#include "ShaderUtilities.h"
#include "FileUtils.h"
#include "NifUtils.h"

extern OverrideInterface	g_overrideInterface;
extern NiTransformInterface g_transformInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern StringTable			g_stringTable;
extern FaceMorphInterface	g_morphInterface;

class ValidRaceFinder : public BGSListForm::Visitor
{
public:
	ValidRaceFinder(TESRace* race) : m_race(race) { }
	virtual bool Accept(TESForm* form)
	{
		if (m_race == form)
			return true;

		return false;
	};
	TESRace* m_race;
};

PresetDataPtr PresetInterface::GetMappedPreset(TESNPC* npc)
{
	SimpleLocker locker(&m_mappedPreset.m_lock);
	auto it = m_mappedPreset.m_data.find(npc);
	if (it != m_mappedPreset.m_data.end())
		return it->second;

	return nullptr;
}

void PresetInterface::AssignMappedPreset(TESNPC* npc, PresetDataPtr presetData)
{
	EraseMappedPreset(npc);
	SimpleLocker locker(&m_mappedPreset.m_lock);
	m_mappedPreset.m_data.emplace(npc, presetData);
}

bool PresetInterface::EraseMappedPreset(TESNPC* npc)
{
	SimpleLocker locker(&m_mappedPreset.m_lock);
	auto it = m_mappedPreset.m_data.find(npc);
	if (it != m_mappedPreset.m_data.end()) {
		m_mappedPreset.m_data.erase(it);
		return true;
	}

	return false;
}

void PresetInterface::ClearMappedPresets()
{
	SimpleLocker locker(&m_mappedPreset.m_lock);
	m_mappedPreset.m_data.clear();
}

void PresetInterface::ApplyMappedPreset(TESNPC* npc, BSFaceGenNiNode* faceNode, BGSHeadPart* headPart)
{
	PresetDataPtr presetData = GetMappedPreset(npc);
	if (presetData && faceNode && headPart) {
		NiAVObject* object = faceNode->GetObjectByName(&headPart->partName.data);
		if (object) {
			BSGeometry* geometry = object->GetAsBSGeometry();
			if (geometry) {
				BSShaderProperty* shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
				if (shaderProperty) {
					if (ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty)) {
						BSLightingShaderProperty* lightingShader = (BSLightingShaderProperty*)shaderProperty;
						BSLightingShaderMaterial* material = (BSLightingShaderMaterial*)shaderProperty->material;
						if (headPart->type == BGSHeadPart::kTypeFace) {

							BSShaderTextureSet* newTextureSet = BSShaderTextureSet::Create();
							for (UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
							{
								newTextureSet->SetTexturePath(i, material->textureSet->GetTexturePath(i));
							}
							newTextureSet->SetTexturePath(6, presetData->tintTexture.data);
							material->SetTextureSet(newTextureSet);

							NiPointer<NiTexture> newTexture;
							LoadTexture(presetData->tintTexture.data, 1, newTexture, false);

							NiTexturePtr* targetTexture = GetTextureFromIndex(material, 6);
							if (targetTexture) {
								*targetTexture = newTexture;
							}

							CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
						}
						else if (material->GetShaderType() == BSLightingShaderMaterial::kShaderType_HairTint) {
							BSLightingShaderMaterialHairTint* tintedMaterial = (BSLightingShaderMaterialHairTint*)material; // I don't know what this * 2.0 bullshit is.
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

void PresetInterface::ApplyPresetData(Actor* actor, PresetDataPtr presetData, bool setSkinColor, ApplyTypes applyType)
{
	PlayerCharacter* player = (*g_thePlayer);
	TESNPC* npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	TESRace* race = npc->race.race;

	// Wipe the HeadPart list and replace it with the default race list
	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();
	TESRace::CharGenData* chargenData = race->chargenData[gender];
	if (chargenData) {
		BGSHeadPart** headParts = npc->headparts;
		tArray<BGSHeadPart*>* headPartList = race->chargenData[gender]->headParts;
		if (headParts && headPartList) {
			Heap_Free(headParts);
			npc->numHeadParts = headPartList->count;
			headParts = (BGSHeadPart**)Heap_Allocate(npc->numHeadParts * sizeof(BGSHeadPart*));
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
			if (race->chargenData[gender]->colors->count > 0)
				race->chargenData[gender]->colors->GetNthItem(0, color);

			npc->headData->hairColor = color;
			npc->headData->headTexture = nullptr;
		}
		npc->headData->headTexture = presetData->headTexture;
	}


	// Replace the old parts with the new parts if they are the right sex
	for (auto& part : presetData->headParts) {
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

	for (auto& tint : presetData->tints) {
		float alpha = (tint.color >> 24) / 255.0;
		TintMask* tintMask = NULL;
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
	for (auto& morph : presetData->customMorphs)
		g_morphInterface.SetMorphValue(npc, morph.name, morph.value);

	g_overrideInterface.Impl_RemoveAllReferenceNodeOverrides(actor);

	g_overlayInterface.RevertOverlays(actor, true);
	if (!g_overlayInterface.HasOverlays(actor))
	{
		g_overlayInterface.AddOverlays(actor);
	}

	if ((applyType & kPresetApplyOverrides) == kPresetApplyOverrides)
	{
		for (auto& nodes : presetData->overrideData) {
			for (auto& value : nodes.second) {
				g_overrideInterface.Impl_AddNodeOverride(actor, gender == 1 ? true : false, nodes.first, value);
			}
		}

		g_overrideInterface.Impl_SetNodeProperties(actor->formID, false);
	}

	if ((applyType & kPresetApplySkinOverrides) == kPresetApplySkinOverrides)
	{
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto& slot : presetData->skinData[i]) {
				for (auto& value : slot.second) {
					g_overrideInterface.Impl_AddSkinOverride(actor, gender == 1 ? true : false, i == 1 ? true : false, slot.first, value);
				}
			}
		}

		g_overrideInterface.Impl_SetSkinProperties(actor->formID, false);
	}

	g_transformInterface.Impl_RemoveAllReferenceTransforms(actor);

	if ((applyType & kPresetApplyTransforms) == kPresetApplyTransforms)
	{
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto& xForms : presetData->transformData[i]) {
				for (auto& key : xForms.second) {
					for (auto& value : key.second) {
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
		for (auto& morph : presetData->bodyMorphData) {
			for (auto& keys : morph.second)
				g_bodyMorphInterface.SetMorph(actor, morph.first, keys.first, keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}

#include <json/json.h>

struct PresetHeader
{
	enum
	{
		kSignature = MACRO_SWAP32('SKSE'),	// endian-swapping so the order matches
		kVersion = 3,

		kVersion_Invalid = 0
	};

	UInt32	signature;
	UInt32	formatVersion;
	UInt32	skseVersion;
	UInt32	runtimeVersion;
};

bool PresetInterface::SaveJsonPreset(const char* filePath)
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


	PlayerCharacter* player = (*g_thePlayer);
	TESNPC* npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	DataHandler* dataHandler = DataHandler::GetSingleton();

	bool isFemale = false;
	if (npc)
		isFemale = CALL_MEMBER_FN(npc, GetSex)() == 1;

	std::map<UInt8, const char*> modListLegacy;
	std::set<std::string> modList;
	std::map<UInt8, BGSHeadPart*> partList;

	UInt32 numHeadParts = 0;
	BGSHeadPart** headParts = nullptr;
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
		BGSHeadPart* headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			ModInfo* modInfo = GetModInfoByFormID(headPart->formID, false);
			if (modInfo) {
				modListLegacy.emplace(modInfo->GetPartialIndex(), modInfo->name);
			}

			partList.emplace(i, headPart);
		}
	}

	// Acquire all mod dependencies
	for (UInt32 i = 0; i < numHeadParts; i++)
	{
		BGSHeadPart* headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			ModInfo* modInfo = GetModInfoByFormID(headPart->formID, true);
			if (modInfo) {
				modList.emplace(modInfo->name);
			}
		}
	}

	std::map<UInt8, std::pair<UInt32, const char*>> tintList;
	for (UInt32 i = 0; i < player->tintMasks.count; i++)
	{
		TintMask* tintMask = nullptr;
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
	ValueSet* valueSet = g_morphInterface.GetValueMap().GetValueSet(npc);
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
	auto sculptTarget = g_morphInterface.GetSculptTarget(npc, false);
	if (sculptTarget) {
		for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			BGSHeadPart* headPart = headParts[i];
			if (headPart) {
				BSFixedString morphPath = SculptData::GetHostByPart(headPart);
				auto sculptHost = sculptTarget->GetSculptHost(morphPath, false);
				if (sculptHost) {
					Json::Value hostData;
					hostData["host"] = morphPath.data;

					TRIModelData data;
					g_morphInterface.GetModelTri(morphPath, data);

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
	BGSHeadPart* facePart = npc->GetCurrentHeadPartByType(BGSHeadPart::kTypeFace);
	if (facePart) {
		BGSTextureSet* textureSet = GetTextureSetForPart(npc, facePart);
		if (textureSet) {
			for (UInt8 i = 0; i < BSShaderTextureSet::kNumTextures; i++) {
				const char* texturePath = textureSet->textureSet.GetTexturePath(i);
				if (texturePath != nullptr) {
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
	g_overrideInterface.VisitNodes(player, [&overrideData](SKEEFixedString node, OverrideVariant& value)
	{
		overrideData[node].push_back(value);
	});

	// Collect skin data
	PresetData::SkinData skinData[2];
	for (UInt32 i = 0; i <= 1; i++) {
		g_overrideInterface.VisitSkin(player, isFemale, i == 1, [&i, &skinData](UInt32 slotMask, OverrideVariant& value)
		{
			skinData[i][slotMask].push_back(value);
			return false;
		});
	}

	// Collect transform data
	PresetData::TransformData transformData[2];
	for (UInt32 i = 0; i <= 1; i++) {
		g_transformInterface.Impl_VisitNodes(player, i == 1, isFemale, [&i, &transformData](SKEEFixedString node, OverrideRegistration<StringTableItem>* keys)
		{
			keys->Visit([&i, &node, &transformData](const StringTableItem& key, OverrideSet* set)
			{
				if (*key == SKEEFixedString("internal"))
					return false;

				set->Visit([&i, &node, &transformData, &key](OverrideVariant* value)
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
	g_bodyMorphInterface.Impl_VisitMorphs(player, [&](SKEEFixedString name, std::unordered_map<StringTableItem, float>* map)
	{
		for (auto& it : *map)
		{
			bodyMorphData[name][*it.first] = it.second;
		}
	});

	for (UInt32 i = 0; i <= 1; i++) {
		for (auto& data : transformData[i]) {
			Json::Value transform;
			transform["firstPerson"] = (bool)(i == 1);
			transform["node"] = data.first.c_str();

			for (auto& key : data.second) {
				Json::Value transformKey;
				transformKey["name"] = key.first.c_str();

				for (auto& value : key.second) {
					Json::Value jvalue;
					jvalue["key"] = value.key;
					jvalue["type"] = value.type;
					jvalue["index"] = value.index;
					switch (value.type) {
					case OverrideVariant::kType_Bool:
						jvalue["data"] = value.data.b;
						break;
					case OverrideVariant::kType_Int:
						jvalue["data"] = static_cast<Json::Int>(value.data.i);
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
	for (auto& data : overrideData) {
		Json::Value ovr;
		ovr["node"] = data.first.c_str();

		for (auto& value : data.second) {
			Json::Value jvalue;
			jvalue["key"] = value.key;
			jvalue["type"] = value.type;
			jvalue["index"] = value.index;
			switch (value.type) {
			case OverrideVariant::kType_Bool:
				jvalue["data"] = value.data.b;
				break;
			case OverrideVariant::kType_Int:
				jvalue["data"] = static_cast<Json::Int>(value.data.i);
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
		for (auto& data : skinData[i]) {
			Json::Value slot;
			slot["firstPerson"] = (bool)(i == 1);
			slot["slotMask"] = (Json::UInt)data.first;

			for (auto& value : data.second) {
				Json::Value jvalue;
				jvalue["key"] = value.key;
				jvalue["type"] = value.type;
				jvalue["index"] = value.index;
				switch (value.type) {
				case OverrideVariant::kType_Bool:
					jvalue["data"] = value.data.b;
					break;
				case OverrideVariant::kType_Int:
					jvalue["data"] = static_cast<Json::Int>(value.data.i);
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

	for (auto& data : bodyMorphData) {
		Json::Value bm;
		bm["name"] = data.first.c_str();
		for (auto& keys : data.second)
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

			ModInfo* modInfo = GetModInfoByFormID(headTexture->formID, true);
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

bool PresetInterface::SaveBinaryPreset(const char* filePath)
{
	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath);

	_DMESSAGE("creating preset");
	if (!currentFile.Create(filePath))
	{
		_ERROR("%s: couldn't create preset file (%s) Error (%d)", __FUNCTION__, filePath, GetLastError());
		return true;
	}

	try
	{
		PresetHeader fileHeader;
		fileHeader.signature = PresetHeader::kSignature;
		fileHeader.formatVersion = PresetHeader::kVersion;
		fileHeader.skseVersion = PACKED_SKSE_VERSION;
		fileHeader.runtimeVersion = RUNTIME_VERSION_1_4_2;

		currentFile.Skip(sizeof(fileHeader));

		PlayerCharacter* player = (*g_thePlayer);
		TESNPC* npc = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
		DataHandler* dataHandler = DataHandler::GetSingleton();

		typedef std::map<UInt8, const char*> ModMap;
		typedef std::pair<UInt8, const char*> ModPair;

		typedef std::map<UInt8, BGSHeadPart*> PartMap;
		typedef std::pair<UInt8, BGSHeadPart*> PartPair;

		typedef std::pair<UInt32, const char*> TintCouple;
		typedef std::map<UInt8, TintCouple> TintMap;
		typedef std::pair<UInt8, TintCouple> TintPair;

		ModMap modList;
		PartMap partList;

		UInt32 numHeadParts = 0;
		BGSHeadPart** headParts = NULL;
		if (CALL_MEMBER_FN(npc, HasOverlays)()) {
			numHeadParts = GetNumActorBaseOverlays(npc);
			headParts = GetActorBaseOverlays(npc);
		}
		else {
			numHeadParts = npc->numHeadParts;
			headParts = npc->headparts;
		}
		for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			BGSHeadPart* headPart = headParts[i];
			if (headPart && !headPart->IsExtraPart()) {
				ModInfo* modInfo = GetModInfoByFormID(headPart->formID);
				if (modInfo) {
					modList.emplace(modInfo->modIndex, modInfo->name);
					partList.emplace(i, headPart);
				}
			}
		}

		TintMap tintList;
		for (UInt32 i = 0; i < player->tintMasks.count; i++)
		{
			TintMask* tintMask = NULL;
			if (player->tintMasks.GetNthItem(i, tintMask))
			{
				UInt32 tintColor = ((UInt32)(tintMask->alpha * 255.0) << 24) | tintMask->color.red << 16 | tintMask->color.green << 8 | tintMask->color.blue;
				tintList.emplace(i, TintCouple(tintColor, tintMask->texture->str.data));
			}
		}

		UInt8 modCount = modList.size();
		UInt8 partCount = partList.size();
		UInt8 tintCount = tintList.size();

		currentFile.Write8(modCount);
		for (auto mIt = modList.begin(); mIt != modList.end(); ++mIt)
		{
			currentFile.Write8(mIt->first);

			UInt16 strLen = strlen(mIt->second);
			currentFile.Write16(strLen);
			currentFile.WriteBuf(mIt->second, strLen);
		}

		currentFile.Write8(partCount);
		for (auto pIt = partList.begin(); pIt != partList.end(); ++pIt)
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
		if (npc->headData && npc->headData->hairColor) {
			hairColor = npc->headData->hairColor->color.red << 16 | npc->headData->hairColor->color.green << 8 | npc->headData->hairColor->color.blue;
		}
		currentFile.Write32(hairColor);

		currentFile.Write8(tintCount);
		for (auto tmIt = tintList.begin(); tmIt != tintList.end(); ++tmIt)
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

		ValueSet* valueSet = g_morphInterface.GetValueMap().GetValueSet(npc);
		if (valueSet)
		{
			for (auto it = valueSet->begin(); it != valueSet->end(); ++it)
			{
				if (it->second != 0.0) {
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
		BGSHeadPart* facePart = npc->GetCurrentHeadPartByType(BGSHeadPart::kTypeFace);
		if (facePart) {
			BGSTextureSet* textureSet = GetTextureSetForPart(npc, facePart);
			if (textureSet) {
				for (UInt8 i = 0; i < BSShaderTextureSet::kNumTextures; i++) {
					const char* texturePath = textureSet->textureSet.GetTexturePath(i);
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
	catch (...)
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

bool PresetInterface::LoadJsonPreset(const char* filePath, PresetDataPtr presetData)
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
		_ERROR("%s: invalid file signature (found %08X expected %08X)", __FUNCTION__, signature, (UInt32)PresetHeader::kSignature);
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

	DataHandler* dataHandler = DataHandler::GetSingleton();

	std::map<UInt32, std::string> modList;
	if (mods.type() == Json::arrayValue) {
		for (auto& mod : mods) {
			UInt32 modIndex = mod["index"].asUInt();
			std::string modName = mod["name"].asString();

			modList.emplace(modIndex, modName);
			presetData->modList.push_back(modName);
		}
	}

	if (root.isMember("modNames") && modNames.type() == Json::arrayValue) {
		presetData->modList.clear();
		for (auto& mod : modNames) {
			presetData->modList.push_back(mod.asString());
		}
	}

	Json::Value headParts = root["headParts"];
	if (!headParts.empty() && headParts.type() == Json::arrayValue) {
		for (auto& part : headParts) {
			if (part.isMember("formIdentifier")) {
				TESForm* headPartForm = GetFormFromIdentifier(part["formIdentifier"].asString());
				if (headPartForm) {
					BGSHeadPart* headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
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
					const ModInfo* modInfo = dataHandler->LookupModByName(it->second.c_str());
					if (modInfo && modInfo->IsActive()) {
						formId = modInfo->GetFormID(formId);
						TESForm* headPartForm = LookupFormByID(formId);
						if (headPartForm) {
							BGSHeadPart* headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
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
		for (auto& tint : tintInfo) {
			PresetData::Tint tintData;
			tintData.color = tint["color"].asUInt();
			tintData.index = tint["index"].asUInt();
			tintData.name = tint["texture"].asString().c_str();
			presetData->tints.push_back(tintData);
		}
	}

	Json::Value faceTextures = root["faceTextures"];
	if (!faceTextures.empty() && faceTextures.type() == Json::arrayValue) {
		for (auto& faceTexture : faceTextures) {
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
			for (auto& preset : presets) {
				UInt32 presetValue = preset.asUInt();
				if (presetValue == 255)
					presetValue = -1;

				presetData->presets.push_back(presetValue);
			}

			Json::Value morphs = defaultMorphs["morphs"];
			for (auto& morph : morphs) {
				presetData->morphs.push_back(morph.asFloat());
			}
		}
		Json::Value customMorphs = morphs["custom"];
		if (!customMorphs.empty()) {
			for (auto& customMorph : customMorphs) {
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
			for (auto& hostFile : sculptData) {
				SKEEFixedString host = hostFile["host"].asString().c_str();
				Json::Value data = hostFile["data"];

				auto sculptedData = std::make_shared<MappedSculptData>();
				for (auto& morphData : data) {
					UInt16 index = morphData[0].asUInt();
					NiPoint3 pt;

					if (multiplier > 0) {
						pt.x = (float)morphData[1].asInt() / (float)multiplier;
						pt.y = (float)morphData[2].asInt() / (float)multiplier;
						pt.z = (float)morphData[3].asInt() / (float)multiplier;
					}
					else {
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
		for (auto& xForm : transforms) {
			bool isFirstPerson = xForm["firstPerson"].asBool();
			BSFixedString nodeName = xForm["node"].asString().c_str();

			Json::Value keys = xForm["keys"];
			for (auto& key : keys) {
				BSFixedString keyName = key["name"].asString().c_str();

				Json::Value values = key["values"];
				for (auto& jvalue : values) {
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
		for (auto& ovr : overrides) {
			BSFixedString node = ovr["node"].asString().c_str();
			Json::Value values = ovr["values"];
			for (auto& jvalue : values) {
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
		for (auto& skinData : skinOverrides) {
			bool isFirstPerson = skinData["firstPerson"].asBool();
			UInt32 slotMask = skinData["slotMask"].asUInt();

			Json::Value values = skinData["values"];
			for (auto& jvalue : values) {
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
		for (auto& bm : bodyMorphs) {
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
				for (auto& jvalue : values) {
					SKEEFixedString key = jvalue["key"].asString().c_str();
					float value = jvalue["value"].asFloat();

					// If the keys were mapped by mod name, skip them if they arent in load order
					std::string strKey(key);
					SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
					if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
					{
						const ModInfo* modInfo = dataHandler->LookupModByName(key.c_str());
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

bool PresetInterface::LoadBinaryPreset(const char* filePath, PresetDataPtr presetData)
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

		if (header.signature != PresetHeader::kSignature)
		{
			_ERROR("%s: invalid file signature (found %08X expected %08X)", __FUNCTION__, header.signature, (UInt32)PresetHeader::kSignature);
			loadError = true;
			goto done;
		}

		if (header.formatVersion <= PresetHeader::kVersion_Invalid)
		{
			_ERROR("%s: version invalid (%08X)", __FUNCTION__, header.formatVersion);
			loadError = true;
			goto done;
		}

		if (header.formatVersion < 2)
		{
			_ERROR("%s: version too old (found %08X current %08X)", __FUNCTION__, header.formatVersion, (UInt32)PresetHeader::kVersion);
			goto done;
		}

		DataHandler* dataHandler = DataHandler::GetSingleton();

		typedef std::map<UInt8, std::string> ModMap;
		typedef std::pair<UInt8, std::string> ModPair;

		ModMap modList;
		UInt8 modCount = 0;
		file.Read(&modCount, sizeof(modCount));

		char textBuffer[MAX_PATH];
		for (UInt8 i = 0; i < modCount; i++)
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

		for (UInt8 i = 0; i < partCount; i++)
		{
			UInt8 partType = 0;
			file.Read(&partType, sizeof(partType));

			UInt32 formId = 0;
			file.Read(&formId, sizeof(formId));

			UInt32 modIndex = formId >> 24;
			auto it = modList.find(modIndex != 0xFE ? modIndex : (formId >> 12));
			if (it != modList.end()) {
				const ModInfo* modInfo = dataHandler->LookupModByName(it->second.c_str());
				if (modInfo && modInfo->IsActive()) {
					formId = modInfo->GetFormID(formId);
					TESForm* headPartForm = LookupFormByID(formId);
					if (headPartForm) {
						BGSHeadPart* headPart = DYNAMIC_CAST(headPartForm, TESForm, BGSHeadPart);
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

		float weight = 0.0;
		file.Read(&weight, sizeof(weight));

		presetData->weight = weight;

		UInt8 presetCount = 0;
		file.Read(&presetCount, sizeof(presetCount));

		for (UInt8 i = 0; i < presetCount; i++)
		{
			SInt32 preset = 0;
			file.Read(&preset, sizeof(UInt8));

			if (preset == 255)
				preset = -1;

			presetData->presets.push_back(preset);
		}

		UInt8 optionCount = 0;
		file.Read(&optionCount, sizeof(optionCount));

		for (UInt8 i = 0; i < optionCount; i++)
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

		for (UInt8 i = 0; i < tintCount; i++)
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

		for (UInt8 i = 0; i < morphCount; i++)
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

		if (header.formatVersion >= 3)
		{
			UInt8 textureCount = 0;
			file.Read(&textureCount, sizeof(textureCount));

			for (UInt8 i = 0; i < textureCount; i++)
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
	catch (...)
	{
		_ERROR("%s: exception during load", __FUNCTION__);
		loadError = true;
	}

done:
	return loadError;
}

void PresetInterface::ApplyPreset(Actor* actor, TESRace* race, TESNPC* npc, PresetDataPtr presetData, ApplyTypes applyType)
{
	CALL_MEMBER_FN(actor, SetRace)(race, actor == (*g_thePlayer));

	npc->overlayRace = NULL;
	npc->weight = presetData->weight;

	if (!npc->faceMorph)
		npc->faceMorph = (TESNPC::FaceMorphs*)Heap_Allocate(sizeof(TESNPC::FaceMorphs));

	UInt32 i = 0;
	for (auto& preset : presetData->presets) {
		npc->faceMorph->presets[i] = preset;
		i++;
	}

	i = 0;
	for (auto& morph : presetData->morphs) {
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
	for (auto& it : presetData->customMorphs)
		g_morphInterface.SetMorphValue(npc, it.name, it.value);

	// Wipe the HeadPart list and replace it with the default race list
	UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();
	TESRace::CharGenData* chargenData = race->chargenData[gender];
	if (chargenData) {
		BGSHeadPart** headParts = npc->headparts;
		tArray<BGSHeadPart*>* headPartList = race->chargenData[gender]->headParts;
		if (headParts && headPartList) {
			Heap_Free(headParts);
			npc->numHeadParts = headPartList->count;
			headParts = (BGSHeadPart**)Heap_Allocate(npc->numHeadParts * sizeof(BGSHeadPart*));
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
		PresetData::Tint& tint = presetData->tints.at(0);
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

	if ((applyType & ApplyTypes::kPresetApplyOverrides) == ApplyTypes::kPresetApplyOverrides) {
		g_overrideInterface.Impl_RemoveAllReferenceNodeOverrides(actor);

		g_overlayInterface.RevertOverlays(actor, true);

		if (!g_overlayInterface.HasOverlays(actor) && presetData->overrideData.size() > 0)
			g_overlayInterface.AddOverlays(actor);

		for (auto& nodes : presetData->overrideData) {
			for (auto& value : nodes.second) {
				g_overrideInterface.Impl_AddNodeOverride(actor, gender == 1 ? true : false, nodes.first, value);
			}
		}
	}

	if ((applyType & ApplyTypes::kPresetApplySkinOverrides) == ApplyTypes::kPresetApplySkinOverrides)
	{
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto& slot : presetData->skinData[i]) {
				for (auto& value : slot.second) {
					g_overrideInterface.Impl_AddSkinOverride(actor, gender == 1 ? true : false, i == 1 ? true : false, slot.first, value);
				}
			}
		}
	}

	if ((applyType & ApplyTypes::kPresetApplyTransforms) == ApplyTypes::kPresetApplyTransforms) {
		g_transformInterface.Impl_RemoveAllReferenceTransforms(actor);
		for (UInt32 i = 0; i <= 1; i++) {
			for (auto& xForms : presetData->transformData[i]) {
				for (auto& key : xForms.second) {
					for (auto& value : key.second) {
						g_transformInterface.Impl_AddNodeTransform(actor, i == 1 ? true : false, gender == 1 ? true : false, xForms.first, key.first, value);
					}
				}
			}
		}
		g_transformInterface.Impl_UpdateNodeAllTransforms(actor);
	}

	if ((applyType & ApplyTypes::kPresetApplyBodyMorphs) == ApplyTypes::kPresetApplyBodyMorphs) {
		g_bodyMorphInterface.ClearMorphs(actor);

		for (auto& morph : presetData->bodyMorphData) {
			for (auto& keys : morph.second)
				g_bodyMorphInterface.SetMorph(actor, morph.first, keys.first, keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}