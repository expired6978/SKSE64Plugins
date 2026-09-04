#include "PresetInterface.h"
#include "SKEEHooks.h"
#include "SKEETasks.h"
#include <RE/M/MemoryManager.h>



#include <RE/B/BSFixedString.h>
#include <RE/B/BSShaderTextureSet.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderMaterialHairTint.h>
#include <RE/B/BSTextureSet.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiTexture.h>
#include <RE/N/NiSmartPointer.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/A/Actor.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BGSHeadPart.h>
#include <RE/N/NiColor.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/RTTI.h>
#include <RE/B/BSStream.h>
#include <RE/N/NiBinaryStream.h>
#include <SKSE/API.h>
#include <SKSE/Interfaces.h>



#include "OverrideInterface.h"
#include "NiTransformInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "FaceMorphInterface.h"

#include "ShaderUtilities.h"
#include "FileUtils.h"
#include "NifUtils.h"
#include <cstdint>

extern OverrideInterface	g_overrideInterface;
extern NiTransformInterface g_transformInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern StringTable			g_stringTable;
extern FaceMorphInterface	g_morphInterface;

// Running SKSE/game versions, captured from the LoadInterface in SKSE_PLUGIN_LOAD (main.cpp)
extern std::uint32_t g_skseVersion;
extern std::uint32_t g_runtimeVersion;

PresetDataPtr PresetInterface::GetMappedPreset(RE::TESNPC* npc)
{
	std::lock_guard<std::recursive_mutex> locker(m_mappedPreset.m_lock);
	auto it = m_mappedPreset.m_data.find(npc);
	if (it != m_mappedPreset.m_data.end())
		return it->second;

	return nullptr;
}

void PresetInterface::AssignMappedPreset(RE::TESNPC* npc, PresetDataPtr presetData)
{
	EraseMappedPreset(npc);
	std::lock_guard<std::recursive_mutex> locker(m_mappedPreset.m_lock);
	m_mappedPreset.m_data.emplace(npc, presetData);
}

bool PresetInterface::EraseMappedPreset(RE::TESNPC* npc)
{
	std::lock_guard<std::recursive_mutex> locker(m_mappedPreset.m_lock);
	auto it = m_mappedPreset.m_data.find(npc);
	if (it != m_mappedPreset.m_data.end()) {
		m_mappedPreset.m_data.erase(it);
		return true;
	}

	return false;
}

void PresetInterface::ClearMappedPresets()
{
	std::lock_guard<std::recursive_mutex> locker(m_mappedPreset.m_lock);
	m_mappedPreset.m_data.clear();
}

void PresetInterface::ApplyMappedPreset(RE::TESNPC* npc, RE::BSFaceGenNiNode* faceNode, RE::BGSHeadPart* headPart)
{
	PresetDataPtr presetData = GetMappedPreset(npc);
	if (presetData && faceNode && headPart) {
		RE::NiAVObject* object = faceNode->GetObjectByName(headPart->formEditorID.c_str());
		if (object) {
			RE::BSGeometry* geometry = object ? object->AsGeometry() : nullptr;
			if (geometry) {
				RE::BSShaderProperty* shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
				if (shaderProperty) {
					RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty);
					if (lightingShader) {
						RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
						if (headPart->type.any(RE::BGSHeadPart::HeadPartType::kFace)) {

							RE::BSShaderTextureSet* newTextureSet = RE::BSShaderTextureSet::Create();
							for (std::uint32_t i = 0; i < RE::BSTextureSet::Texture::kTotal; i++) {
								newTextureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), material->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)));
							}
							newTextureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(6), presetData->tintTexture.c_str());
							material->SetTextureSet(RE::NiPointer<RE::BSTextureSet>(newTextureSet));

							RE::NiPointer<RE::NiTexture> newTexture;
							RE::BSShaderManager::GetTexture(presetData->tintTexture.c_str(), 1, newTexture, false);

							auto targetTexture = GetTextureFromIndex(material, 6);
							if (targetTexture) {
								targetTexture->reset(static_cast<RE::NiSourceTexture*>(newTexture.get()));
							}

							SKEE::InitializeShader(lightingShader, geometry);
						}
						else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint) {
							RE::BSLightingShaderMaterialHairTint* tintedMaterial = static_cast<RE::BSLightingShaderMaterialHairTint*>(static_cast<RE::BSLightingShaderMaterialBase*>(material));
							tintedMaterial->tintColor.red = (((presetData->hairColor >> 16) & 0xFF) / 255.0) * 2.0;
							tintedMaterial->tintColor.green = (((presetData->hairColor >> 8) & 0xFF) / 255.0) * 2.0;
							tintedMaterial->tintColor.blue = ((presetData->hairColor & 0xFF) / 255.0) * 2.0;
						}
					}
				}
			}
		}
	}
}

void PresetInterface::ApplyPresetData(RE::Actor* actor, PresetDataPtr presetData, bool setSkinColor, ApplyTypes applyType)
{
	RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
	RE::TESNPC* npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	RE::TESRace* race = npc->GetRace();

	// Wipe the HeadPart list and replace it with the default race list
	std::uint8_t gender = (npc->GetSex() == RE::SEX::kFemale);
	RE::TESRace::FaceRelatedData* chargenData = race->faceRelatedData[gender];
	if (chargenData) {
		RE::BGSHeadPart** headParts = npc->headParts;
		RE::BSTArray<RE::BGSHeadPart*>* headPartList = race->faceRelatedData[gender]->headParts;
		if (headParts && headPartList) {
			RE::free(headParts);
			std::int8_t partCount = static_cast<std::int8_t>(headPartList->size());
			auto* newHeadParts = static_cast<RE::BGSHeadPart**>(RE::malloc(partCount * sizeof(RE::BGSHeadPart*)));
			for (std::uint32_t i = 0; i < headPartList->size(); i++)
				newHeadParts[i] = (*headPartList)[i];
			npc->numHeadParts = partCount;
			npc->headParts = newHeadParts;
		}
	}

	if (presetData->headTexture) {
		if (!npc->headRelatedData) {
			npc->headRelatedData = new RE::TESNPC::HeadRelatedData();

			// If we had no head data we probably have no hair color assigned, this isn't good
			// lets just assign the first color from our race settings incase
			RE::BGSColorForm* color = nullptr;
			if (race->faceRelatedData[gender] && race->faceRelatedData[gender]->availableHairColors && race->faceRelatedData[gender]->availableHairColors->size() > 0)
				color = (*race->faceRelatedData[gender]->availableHairColors)[0];

			npc->headRelatedData->hairColor = color;
			npc->headRelatedData->faceDetails = nullptr;
		}
		npc->headRelatedData->faceDetails = presetData->headTexture;
	}

	// Replace the old parts with the new parts if they are the right sex
	for (auto& part : presetData->headParts) {
		if ((gender == 0 && part->flags.any(RE::BGSHeadPart::Flag::kMale)) ||
			(gender == 1 && part->flags.any(RE::BGSHeadPart::Flag::kFemale)))
		{
			if (part->validRaces) {
				bool validRace = false;
				for (auto* form : part->validRaces->forms) {
					if (form == race) { validRace = true; break; }
				}
				if (validRace)
					npc->ChangeHeadPart(part);
			}
		}
	}

	npc->weight = presetData->weight;

	if (!npc->faceData)
		{ auto* fd = RE::malloc<RE::TESNPC::FaceData>();
		std::memset(fd, 0, sizeof(RE::TESNPC::FaceData));
		npc->faceData = fd; }

	std::uint32_t i = 0;
	for (auto value : presetData->presets) {
		npc->faceData->parts[i] = value;
		i++;
	}

	i = 0;
	for (auto value : presetData->morphs) {
		npc->faceData->morphs[i] = value;
		i++;
	}

	for (auto& tint : presetData->tints) {
		float alpha = (tint.color >> 24) / 255.0;
		auto& tintArr = player->GetPlayerRuntimeData().tintMasks;
		RE::TintMask* tintMask = nullptr;
		if (player == actor && tint.index < tintArr.size()) {
			tintMask = tintArr[tint.index];
			tintMask->color.red = (tint.color >> 16) & 0xFF;
			tintMask->color.green = (tint.color >> 8) & 0xFF;
			tintMask->color.blue = tint.color & 0xFF;
			tintMask->alpha = alpha;
			if (tintMask->alpha > 0)
				tintMask->texture->textureName = tint.name;
		}

		if (tint.index == 0 && setSkinColor)
		{
			float alpha = (tint.color >> 24) / 255.0;
			RE::TintMask tintMask;
			tintMask.color.red = (tint.color >> 16) & 0xFF;
			tintMask.color.green = (tint.color >> 8) & 0xFF;
			tintMask.color.blue = tint.color & 0xFF;
			tintMask.alpha = alpha;
			tintMask.type.set(RE::TintMask::Type::kSkinTone);

			RE::NiColorA colorResult;
			npc->SetSkinFromTint(&colorResult, &tintMask, true);
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
		for (std::uint32_t i = 0; i <= 1; i++) {
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
		for (std::uint32_t i = 0; i <= 1; i++) {
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
				g_bodyMorphInterface.SetMorph(actor, morph.first.c_str(), keys.first.c_str(), keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}

#include <json/json.h>


struct PresetHeader
{
	enum
	{
		kSignature = 0x45534B53,	// endian-swapping so the order matches
		kVersion = 3,

		kVersion_Invalid = 0
	};

	std::uint32_t	signature;
	std::uint32_t	formatVersion;
	std::uint32_t	skseVersion;
	std::uint32_t	runtimeVersion;
};

bool PresetInterface::SaveJsonPreset(const char* filePath, RE::Actor* actor)
{
	Json::StyledWriter writer;
	Json::Value root;

	BinaryStream		currentFile;
	FileUtils::MakeAllDirs(filePath);
	if (!currentFile.Create(filePath))
	{
		SKSE::log::error("{}: couldn't create preset file ({}) Error ({})", __FUNCTION__, filePath, GetLastError());
		return true;
	}

	Json::Value versionInfo;
	versionInfo["signature"] = PresetHeader::kSignature;
	versionInfo["formatVersion"] = PresetHeader::kVersion;
	versionInfo["skseVersion"] = g_skseVersion;
	versionInfo["runtimeVersion"] = g_runtimeVersion;

	RE::TESNPC* npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();

	bool isFemale = false;
	if (npc)
		isFemale = (npc->GetSex() == RE::SEX::kFemale);

	std::map<std::uint8_t, const char*> modListLegacy;
	std::set<std::string> modList;
	std::map<std::uint8_t, RE::BGSHeadPart*> partList;

	std::uint32_t numHeadParts = 0;
	RE::BGSHeadPart** headParts = nullptr;
	if (npc->HasOverlays()) {
		numHeadParts = npc->GetNumBaseOverlays();
		headParts = npc->GetBaseOverlays();
	} else {
		numHeadParts = npc->numHeadParts;
		headParts = npc->headParts;
	}

	// Acquire only vanilla dependencies
	for (std::uint32_t i = 0; i < numHeadParts; i++)
	{
		RE::BGSHeadPart* headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
			const RE::TESFile* modInfo = dataHandler->LookupLoadedModByIndex(headPart->formID >> 24);
			if (modInfo) {
				modListLegacy.emplace(static_cast<std::uint8_t>(modInfo->GetPartialIndex()), modInfo->GetFilename().data());
			}

			partList.emplace(i, headPart);
		}
	}

	// Acquire all mod dependencies (loaded and unloaded, matching the legacy
	// GetModInfoByFormID(formID, true) semantics)
	for (std::uint32_t i = 0; i < numHeadParts; i++)
	{
		RE::BGSHeadPart* headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			for (RE::TESFile* modInfo : dataHandler->files) {
				if (modInfo && modInfo->GetCompileIndex() == (headPart->formID >> 24)) {
					modList.emplace(modInfo->GetFilename());
					break;
				}
			}
		}
	}

	std::map<std::uint8_t, std::pair<std::uint32_t, const char*>> tintList;
	RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
	if (actor == player)
	{
        auto& tintArr = player->GetPlayerRuntimeData().tintMasks;
		for (std::uint32_t i = 0; i < tintArr.size(); i++)
		{
			RE::TintMask* tintMask = tintArr[i];
			if (tintMask)
			{
				std::uint32_t tintColor = ((std::uint32_t)(tintMask->alpha * 255.0) << 24) | tintMask->color.red << 16 | tintMask->color.green << 8 | tintMask->color.blue;
				if (tintMask->texture)
					tintList.emplace(i, std::make_pair(tintColor, tintMask->texture->textureName.c_str()));
			}
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
	if (npc->faceData)
	{
		for (std::uint8_t p = 0; p < RE::TESNPC::FaceData::kTotalPresets; p++) {
			Json::Value morphValue = (Json::UInt)npc->faceData->parts[p];
			morphInfo["presets"].append(morphValue);
		}

		for (std::uint8_t o = 0; o < RE::TESNPC::FaceData::Morphs::kTotal; o++) {
			Json::Value morphValue = npc->faceData->morphs[o];
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
		for (std::uint32_t i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			RE::BGSHeadPart* headPart = headParts[i];
			if (headPart) {
				RE::BSFixedString morphPath = SculptData::GetHostByPart(headPart);
				auto sculptHost = sculptTarget->GetSculptHost(morphPath, false);
				if (sculptHost) {
					Json::Value hostData;
					hostData["host"] = morphPath.c_str();

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
	RE::BGSHeadPart* facePart = npc->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace);
	if (facePart) {
		RE::BGSTextureSet* textureSet = GetTextureSetForPart(npc, facePart);
		if (textureSet) {
			for (std::uint8_t i = 0; i < RE::BSTextureSet::Texture::kTotal; i++) {
				const char* texturePath = textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i));
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
	g_overrideInterface.VisitNodes(actor, [&overrideData](SKEEFixedString node, OverrideVariant& value)
	{
		overrideData[node].push_back(value);
	});

	// Collect skin data
	PresetData::SkinData skinData[2];
	for (std::uint32_t i = 0; i <= 1; i++) {
		g_overrideInterface.VisitSkin(actor, isFemale, i == 1, [&i, &skinData](std::uint32_t slotMask, OverrideVariant& value)
		{
			skinData[i][slotMask].push_back(value);
			return false;
		});
	}

	// Collect transform data
	PresetData::TransformData transformData[2];
	for (std::uint32_t i = 0; i <= 1; i++) {
		g_transformInterface.Impl_VisitNodes(actor, i == 1, isFemale, [&i, &transformData](SKEEFixedString node, OverrideRegistration<StringTableItem>* keys)
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
	g_bodyMorphInterface.Impl_VisitMorphs(actor, [&](SKEEFixedString name, std::unordered_map<StringTableItem, float>* map)
	{
		for (auto& it : *map)
		{
			bodyMorphData[name][*it.first] = it.second;
		}
	});

	for (std::uint32_t i = 0; i <= 1; i++) {
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

	for (std::uint32_t i = 0; i <= 1; i++) {
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

	if (npc->headRelatedData) {
		auto hairColor = npc->headRelatedData->hairColor;
		if (hairColor)
			root["actor"]["hairColor"] = (Json::UInt)(hairColor->color.red << 16 | hairColor->color.green << 8 | hairColor->color.blue);

		auto headTexture = npc->headRelatedData->faceDetails;
		if (headTexture) {
			root["actor"]["headTexture"] = GetFormIdentifier(headTexture);

			RE::TESFile* modInfo = GetModInfoByFormID(headTexture->formID, true);
			if (modInfo) {
				modList.emplace(std::string(modInfo->GetFilename()));
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
	BinaryStream		currentFile;
	FileUtils::MakeAllDirs(filePath);

	SKSE::log::debug("creating preset");
	if (!currentFile.Create(filePath))
	{
		SKSE::log::error("{}: couldn't create preset file ({}) Error ({})", __FUNCTION__, filePath, GetLastError());
		return true;
	}

	try
	{
		PresetHeader fileHeader;
		fileHeader.signature = PresetHeader::kSignature;
		fileHeader.formatVersion = PresetHeader::kVersion;
		fileHeader.skseVersion = g_skseVersion;
		fileHeader.runtimeVersion = g_runtimeVersion;

		currentFile.Skip(sizeof(fileHeader));

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		RE::TESNPC* npc = player->GetBaseObject() ? player->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();

		typedef std::map<std::uint8_t, const char*> ModMap;
		typedef std::pair<std::uint8_t, const char*> ModPair;

		typedef std::map<std::uint8_t, RE::BGSHeadPart*> PartMap;
		typedef std::pair<std::uint8_t, RE::BGSHeadPart*> PartPair;

		typedef std::pair<std::uint32_t, const char*> TintCouple;
		typedef std::map<std::uint8_t, TintCouple> TintMap;
		typedef std::pair<std::uint8_t, TintCouple> TintPair;

		ModMap modList;
		PartMap partList;

		std::uint32_t numHeadParts = 0;
		RE::BGSHeadPart** headParts = NULL;
		if (npc->HasOverlays()) {
			numHeadParts = npc->GetNumBaseOverlays();
			headParts = npc->GetBaseOverlays();
		}
		else {
			numHeadParts = npc->numHeadParts;
			headParts = npc->headParts;
		}
		for (std::uint32_t i = 0; i < numHeadParts; i++) // Acquire all unique parts
		{
			RE::BGSHeadPart* headPart = headParts[i];
			if (headPart && !headPart->IsExtraPart()) {
				RE::TESFile* modInfo = GetModInfoByFormID(headPart->formID);
				if (modInfo) {
					modList.emplace(std::uint8_t(headPart->formID >> 24), modInfo->GetFilename().data());
					partList.emplace(i, headPart);
				}
			}
		}

		TintMap tintList;
		auto& tintArr = player->GetPlayerRuntimeData().tintMasks;
		for (std::uint32_t i = 0; i < tintArr.size(); i++)
		{
			RE::TintMask* tintMask = tintArr[i];
			if (tintMask)
			{
				std::uint32_t tintColor = ((std::uint32_t)(tintMask->alpha * 255.0) << 24) | tintMask->color.red << 16 | tintMask->color.green << 8 | tintMask->color.blue;
				tintList.emplace(i, TintCouple(tintColor, tintMask->texture->textureName.c_str()));
			}
		}

		std::uint8_t modCount = modList.size();
		std::uint8_t partCount = partList.size();
		std::uint8_t tintCount = tintList.size();

		currentFile.Write8(modCount);
		for (auto mIt = modList.begin(); mIt != modList.end(); ++mIt)
		{
			currentFile.Write8(mIt->first);

			std::uint16_t strLen = strlen(mIt->second);
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

		if (npc->faceData)
		{
			currentFile.Write8(RE::TESNPC::FaceData::kTotalPresets);
			for (std::uint8_t p = 0; p < RE::TESNPC::FaceData::kTotalPresets; p++)
				currentFile.Write8(npc->faceData->parts[p]);

			currentFile.Write8(RE::TESNPC::FaceData::Morphs::kTotal);
			for (std::uint8_t o = 0; o < RE::TESNPC::FaceData::Morphs::kTotal; o++)
				currentFile.WriteFloat(npc->faceData->morphs[o]);
		}
		else {
			currentFile.Write8(0);
			currentFile.Write8(0);
		}

		std::uint32_t hairColor = 0;
		if (npc->headRelatedData && npc->headRelatedData->hairColor) {
			hairColor = npc->headRelatedData->hairColor->color.red << 16 | npc->headRelatedData->hairColor->color.green << 8 | npc->headRelatedData->hairColor->color.blue;
		}
		currentFile.Write32(hairColor);

		currentFile.Write8(tintCount);
		for (auto tmIt = tintList.begin(); tmIt != tintList.end(); ++tmIt)
		{
			currentFile.Write8(tmIt->first);
			currentFile.Write32(tmIt->second.first);

			std::uint16_t strLen = strlen(tmIt->second.second);
			currentFile.Write16(strLen);
			currentFile.WriteBuf(tmIt->second.second, strLen);
		}

		std::int64_t offset = currentFile.GetOffset();
		currentFile.Skip(sizeof(std::uint8_t));
		std::uint8_t totalMorphs = 0;

		ValueSet* valueSet = g_morphInterface.GetValueMap().GetValueSet(npc);
		if (valueSet)
		{
			for (auto it = valueSet->begin(); it != valueSet->end(); ++it)
			{
				if (it->second != 0.0) {
					std::uint16_t strLen = strlen(it->first->c_str());
					currentFile.Write16(strLen);
					currentFile.WriteBuf(it->first->c_str(), strLen);

					currentFile.WriteFloat(it->second);
					totalMorphs++;
				}
			}
		}

		std::int64_t jumpBack = currentFile.GetOffset();
		currentFile.SetOffset(offset);
		currentFile.Write8(totalMorphs);

		currentFile.SetOffset(jumpBack);
		offset = currentFile.GetOffset();
		currentFile.Skip(sizeof(std::uint8_t));
		std::uint8_t totalTextures = 0;
		RE::BGSHeadPart* facePart = npc->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace);
		if (facePart) {
			RE::BGSTextureSet* textureSet = GetTextureSetForPart(npc, facePart);
			if (textureSet) {
				for (std::uint8_t i = 0; i < RE::BSTextureSet::Texture::kTotal; i++) {
					const char* texturePath = textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i));
					if (texturePath != NULL) {
						std::uint16_t strLen = strlen(texturePath);
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
		SKSE::log::error("SavePreset: exception during save");
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
	RE::BSResourceNiBinaryStream file(filePath);
	if (!file.good()) {
		SKSE::log::error("{}: File {} failed to open.", __FUNCTION__, filePath);
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
		SKSE::log::error("{}: Error occured parsing json for {}.", __FUNCTION__, filePath);
		loadError = true;
		return loadError;
	}

	Json::Value defaultValue;
	Json::Value version = root["version"];
	if (version.empty()) {
		SKSE::log::error("{}: No version header.", __FUNCTION__);
		loadError = true;
		return loadError;
	}

	std::uint32_t signature = version["signature"].asUInt();
	if (signature != PresetHeader::kSignature)
	{
		SKSE::log::error("{}: invalid file signature (found {:08X} expected {:08X})", __FUNCTION__, signature, (std::uint32_t)PresetHeader::kSignature);
		loadError = true;
		return loadError;
	}

	std::uint32_t formatVersion = version["formatVersion"].asUInt();
	if (formatVersion <= PresetHeader::kVersion_Invalid)
	{
		SKSE::log::error("{}: version invalid ({:08X})", __FUNCTION__, formatVersion);
		loadError = true;
		return loadError;
	}

	Json::Value mods = root["mods"];
	Json::Value modNames = root["modNames"];
	if (mods.empty() && modNames.empty()) {
		SKSE::log::error("{}: No mods header.", __FUNCTION__);
		loadError = true;
		return loadError;
	}

	RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();

	std::map<std::uint32_t, std::string> modList;
	if (mods.type() == Json::arrayValue) {
		for (auto& mod : mods) {
			std::uint32_t modIndex = mod["index"].asUInt();
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
				RE::TESForm* headPartForm = GetFormFromIdentifier(part["formIdentifier"].asString());
				if (headPartForm) {
					RE::BGSHeadPart* headPart = headPartForm ? headPartForm->As<RE::BGSHeadPart>() : nullptr;
					if (headPart) {
						presetData->headParts.push_back(headPart);
					}
				}
			}
			else if (part.isMember("formId")) {
				std::uint8_t partType = part["type"].asUInt();
				std::uint32_t formId = part["formId"].asUInt();

				std::uint32_t modIndex = formId >> 24;
				auto it = modList.find(modIndex != 0xFE ? modIndex : (formId >> 12));
				if (it != modList.end()) {
					const RE::TESFile* modInfo = dataHandler->LookupModByName(it->second.c_str());
					if (modInfo && BSFileUtil::IsActive(modInfo)) {
						formId = modInfo->GetFormID(formId);
						RE::TESForm* headPartForm = RE::TESForm::LookupByID(formId);
						if (headPartForm) {
							RE::BGSHeadPart* headPart = headPartForm ? headPartForm->As<RE::BGSHeadPart>() : nullptr;
							if (headPart) {
								presetData->headParts.push_back(headPart);
							}
						}
						else {
							SKSE::log::warn("Could not resolve part {:08X}", formId);
						}
					}
					else {
						SKSE::log::warn("Could not load part type {} from {}; mod not found.", partType, it->second.c_str());
					}
				}
			}
		}
	}

	Json::Value headData = root["actor"];
	if (!headData.empty() && headData.type() == Json::objectValue) {
		presetData->weight = headData["weight"].asFloat();
		presetData->hairColor = headData["hairColor"].asUInt();
		if (headData.isMember("headTexture")) {
			presetData->headTexture = GetFormFromIdentifier(headData["headTexture"].asString()) ? GetFormFromIdentifier(headData["headTexture"].asString())->As<RE::BGSTextureSet>() : nullptr;
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
				std::uint32_t presetValue = preset.asUInt();
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

		std::int32_t multiplier = -1;

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
					std::uint16_t index = morphData[0].asUInt();
					RE::NiPoint3 pt;

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
			RE::BSFixedString nodeName = xForm["node"].asString().c_str();

			Json::Value keys = xForm["keys"];
			for (auto& key : keys) {
				RE::BSFixedString keyName = key["name"].asString().c_str();

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
			RE::BSFixedString node = ovr["node"].asString().c_str();
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
			std::uint32_t slotMask = skinData["slotMask"].asUInt();

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
			RE::BSFixedString name = bm["name"].asString().c_str();

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
					std::string strKey(key.c_str());
					SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
					if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
					{
						const RE::TESFile* modInfo = dataHandler->LookupModByName(key.c_str());
						if (!modInfo || !BSFileUtil::IsActive(modInfo))
							continue;
					}

					presetData->bodyMorphData[name][key] = value;
				}
			}
		}
	}

	return loadError;
}

bool PresetInterface::SavePreset(const char* filePath, const char* tintPath, RE::Actor* actor)
{
	if (actor->IsNot(RE::FormType::ActorCharacter)) {
		return false;
	}

	RE::TESNPC* npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (!npc) {
		return false;
	}

	SaveJsonPreset(filePath, actor);

	if (tintPath)
	{
		std::string path = GetSanitizedPath(filePath).AsString();
		size_t idx = path.rfind('\\');
		if (idx != std::string::npos)
		{
			auto dir = path.substr(0, idx + 1);
			auto file = path.substr(idx + 1);
			
			SKEE_AddTask(SKSE::GetTaskInterface(), new SKSETaskExportTintMask(dir.c_str(), file.c_str()));
		}
	}

	return false;
}

bool PresetInterface::LoadPreset(const char* filePath, const char* tintPath, RE::Actor* actor, ApplyTypes applyTypes)
{
	if (actor->IsNot(RE::FormType::ActorCharacter)) {
		return false;
	}

	RE::TESNPC* npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (!npc) {
		return false;
	}

	if (!filePath)
	{
		return false;
	}

	auto path = GetSanitizedPath(filePath).AsString();
	auto presetData = std::make_shared<PresetData>();
	bool loadError = LoadJsonPreset(path.c_str(), presetData);
	if (loadError) {
		return false;
	}

	if (tintPath)
	{
		presetData->tintTexture = tintPath;
	}
	
	AssignMappedPreset(npc, presetData);
	ApplyPresetData(actor, presetData, true, applyTypes);

	actor->DoReset3D(true);
	return true;
}

bool PresetInterface::LoadBinaryPreset(const char* filePath, PresetDataPtr presetData)
{
	bool loadError = false;
	RE::BSResourceNiBinaryStream file(filePath);
	if (!file.good()) {
		SKSE::log::error("{}: File {} failed to open.", __FUNCTION__, filePath);
		loadError = true;
		return loadError;
	}

	try
	{
		PresetHeader header;
		file.get(header);

		if (header.signature != PresetHeader::kSignature)
		{
			SKSE::log::error("{}: invalid file signature (found {:08X} expected {:08X})", __FUNCTION__, header.signature, (std::uint32_t)PresetHeader::kSignature);
			loadError = true;
			goto done;
		}

		if (header.formatVersion <= PresetHeader::kVersion_Invalid)
		{
			SKSE::log::error("{}: version invalid ({:08X})", __FUNCTION__, header.formatVersion);
			loadError = true;
			goto done;
		}

		if (header.formatVersion < 2)
		{
			SKSE::log::error("{}: version too old (found {:08X} current {:08X})", __FUNCTION__, header.formatVersion, (std::uint32_t)PresetHeader::kVersion);
			goto done;
		}

		RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();

		typedef std::map<std::uint8_t, std::string> ModMap;
		typedef std::pair<std::uint8_t, std::string> ModPair;

		ModMap modList;
		std::uint8_t modCount = 0;
		file.get(modCount);

		char textBuffer[REX::W32::MAX_PATH];
		for (std::uint8_t i = 0; i < modCount; i++)
		{
			std::uint8_t modIndex;
			file.get(modIndex);

			std::uint16_t strLen = 0;
			file.get(strLen);

			memset(textBuffer, 0, REX::W32::MAX_PATH);
			file.read(textBuffer, strLen);

			std::string modName(textBuffer);

			modList.emplace(modIndex, modName);
			presetData->modList.push_back(modName);
		}

		std::uint8_t partCount = 0;
		file.get(partCount);

		for (std::uint8_t i = 0; i < partCount; i++)
		{
			std::uint8_t partType = 0;
			file.get(partType);

			std::uint32_t formId = 0;
			file.get(formId);

			std::uint32_t modIndex = formId >> 24;
			auto it = modList.find(modIndex != 0xFE ? modIndex : (formId >> 12));
			if (it != modList.end()) {
				const RE::TESFile* modInfo = dataHandler->LookupModByName(it->second.c_str());
				if (modInfo && BSFileUtil::IsActive(modInfo)) {
					formId = modInfo->GetFormID(formId);
					RE::TESForm* headPartForm = RE::TESForm::LookupByID(formId);
					if (headPartForm) {
						RE::BGSHeadPart* headPart = headPartForm ? headPartForm->As<RE::BGSHeadPart>() : nullptr;
						if (headPart) {
							presetData->headParts.push_back(headPart);
						}
					}
					else {
						SKSE::log::warn("Could not resolve part {:08X}", formId);
					}
				}
				else {
					SKSE::log::warn("Could not load part type {} from {}; mod not found.", partType, it->second.c_str());
				}
			}
		}

		float weight = 0.0;
		file.get(weight);

		presetData->weight = weight;

		std::uint8_t presetCount = 0;
		file.get(presetCount);

		for (std::uint8_t i = 0; i < presetCount; i++)
		{
			std::uint8_t presetByte = 0;
			file.get(presetByte);
			std::int32_t preset = presetByte;

			if (preset == 255)
				preset = -1;

			presetData->presets.push_back(preset);
		}

		std::uint8_t optionCount = 0;
		file.get(optionCount);

		for (std::uint8_t i = 0; i < optionCount; i++)
		{
			float option = 0.0;
			file.get(option);

			presetData->morphs.push_back(option);
		}

		std::uint32_t hairColor = 0;
		file.get(hairColor);

		presetData->hairColor = hairColor;

		std::uint8_t tintCount = 0;
		file.get(tintCount);

		for (std::uint8_t i = 0; i < tintCount; i++)
		{
			std::uint8_t tintIndex = 0;
			file.get(tintIndex);

			std::uint32_t tintColor = 0;
			file.get(tintColor);

			std::uint16_t strLen = 0;
			file.get(strLen);

			memset(textBuffer, 0, REX::W32::MAX_PATH);
			file.read(textBuffer, strLen);

			RE::BSFixedString tintPath = textBuffer;

			PresetData::Tint tint;
			tint.color = tintColor;
			tint.index = tintIndex;
			tint.name = tintPath;
			presetData->tints.push_back(tint);
		}

		std::uint8_t morphCount = 0;
		file.get(morphCount);

		for (std::uint8_t i = 0; i < morphCount; i++)
		{
			std::uint16_t strLen = 0;
			file.get(strLen);

			memset(textBuffer, 0, REX::W32::MAX_PATH);
			file.read(textBuffer, strLen);

			float morphValue = 0.0;
			file.get(morphValue);

			PresetData::Morph morph;
			morph.name = textBuffer;
			morph.value = morphValue;

			presetData->customMorphs.push_back(morph);
		}

		if (header.formatVersion >= 3)
		{
			std::uint8_t textureCount = 0;
			file.get(textureCount);

			for (std::uint8_t i = 0; i < textureCount; i++)
			{
				std::uint8_t textureIndex = 0;
				file.get(textureIndex);

				std::uint16_t strLen = 0;
				file.get(strLen);

				memset(textBuffer, 0, REX::W32::MAX_PATH);
				file.read(textBuffer, strLen);

				RE::BSFixedString texturePath(textBuffer);

				PresetData::Texture texture;
				texture.index = textureIndex;
				texture.name = texturePath;

				presetData->faceTextures.push_back(texture);
			}
		}
	}
	catch (...)
	{
		SKSE::log::error("{}: exception during load", __FUNCTION__);
		loadError = true;
	}

done:
	return loadError;
}

void PresetInterface::ApplyPreset(RE::Actor* actor, RE::TESRace* race, RE::TESNPC* npc, PresetDataPtr presetData, ApplyTypes applyType)
{
	// race change handled via headRelatedData in CommonLib

	// overlayRace removed (no CommonLib equivalent)
	npc->weight = presetData->weight;

	if (!npc->faceData)
		npc->faceData = new RE::TESNPC::FaceData();

	std::uint32_t i = 0;
	for (auto& preset : presetData->presets) {
		npc->faceData->parts[i] = preset;
		i++;
	}

	i = 0;
	for (auto& morph : presetData->morphs) {
		npc->faceData->morphs[i] = morph;
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
	std::uint8_t gender = (npc->GetSex() == RE::SEX::kFemale);
	RE::BSTArray<RE::BGSHeadPart*>* headPartList = race->faceRelatedData[gender]->headParts;
	if (headPartList && npc->headParts) {
		RE::BGSHeadPart** oldHeadParts = npc->headParts;
		std::int8_t partCount = static_cast<std::int8_t>(headPartList->size());
		auto* newHeadParts = static_cast<RE::BGSHeadPart**>(RE::malloc(partCount * sizeof(RE::BGSHeadPart*)));
		for (std::uint32_t i = 0; i < headPartList->size(); i++)
			newHeadParts[i] = (*headPartList)[i];
		RE::free(oldHeadParts);
		npc->numHeadParts = partCount;
		npc->headParts = newHeadParts;
	}

	// Force the remaining parts to change to that of the preset
	for (auto part : presetData->headParts)
		npc->ChangeHeadPart(part);

	//npc->MarkChanged(0x2000800); // Save FaceData and Race
	npc->AddChange(0x800); // Save FaceData (legacy TESForm::MarkChanged, vfunc 0A)

	// Grab the skin tint and convert it from RGBA to RGB on NPC
	if (presetData->tints.size() > 0) {
		PresetData::Tint& tint = presetData->tints.at(0);
		float alpha = (tint.color >> 24) / 255.0;
		RE::TintMask tintMask;
		tintMask.color.red = (tint.color >> 16) & 0xFF;
		tintMask.color.green = (tint.color >> 8) & 0xFF;
		tintMask.color.blue = tint.color & 0xFF;
		tintMask.alpha = alpha;
		tintMask.type.set(RE::TintMask::Type::kSkinTone);

		RE::NiColorA colorResult;
		npc->SetSkinFromTint(&colorResult, &tintMask, true);
	}

	// Queue a node update
	actor->DoReset3D(true);

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
		for (std::uint32_t i = 0; i <= 1; i++) {
			for (auto& slot : presetData->skinData[i]) {
				for (auto& value : slot.second) {
					g_overrideInterface.Impl_AddSkinOverride(actor, gender == 1 ? true : false, i == 1 ? true : false, slot.first, value);
				}
			}
		}
	}

	if ((applyType & ApplyTypes::kPresetApplyTransforms) == ApplyTypes::kPresetApplyTransforms) {
		g_transformInterface.Impl_RemoveAllReferenceTransforms(actor);
		for (std::uint32_t i = 0; i <= 1; i++) {
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
				g_bodyMorphInterface.SetMorph(actor, morph.first.c_str(), keys.first.c_str(), keys.second);
		}

		g_bodyMorphInterface.UpdateModelWeight(actor);
	}
}