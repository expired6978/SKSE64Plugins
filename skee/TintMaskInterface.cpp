#include "TintMaskInterface.h"
#include "ItemDataInterface.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"
#include "FileUtils.h"

#include "skse64/GameData.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"

#include "skse64/GameStreams.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiProperties.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"

#include "skse64/PapyrusEvents.h"

#include "tinyxml2.h"

#include "CDXD3DDevice.h"
#include "CDXNifTextureRenderer.h"
#include "CDXNifPixelShaderCache.h"
#include "CDXShaderFactory.h"

#include <vector>
#include <algorithm>

CDXShaderFactory			g_shaderFactory;
CDXNifPixelShaderCache		g_pixelShaders(&g_shaderFactory);
extern TintMaskInterface	g_tintMaskInterface;
extern UInt32	g_tintHairSlot;

UInt32 TintMaskInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void TintMaskInterface::CreateTintsFromData(TESObjectREFR * refr, std::map<SInt32, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, ItemAttributeDataPtr & overrides, UInt32 & flags)
{
	UInt32 skinColor = 0;
	UInt32 hairColor = 0;

	if (refr->baseForm && refr->baseForm->formType == TESNPC::kTypeID)
	{
		TESNPC* actorBase = static_cast<TESNPC*>(refr->baseForm);
		skinColor = ((UInt32)actorBase->color.red << 16) | ((UInt32)actorBase->color.green << 8) | (UInt32)actorBase->color.blue;

		auto headData = actorBase->headData;
		if (headData) {
			auto hairColorForm = headData->hairColor;
			if (hairColorForm) {
				// Dunno why the hell they multiplied hairColor by 2
				hairColor |= min((hairColorForm->abgr & 0xFF) * 2, 255) << 16;
				hairColor |= min(((hairColorForm->abgr >> 8) & 0xFF) * 2, 255) << 8;
				hairColor |= min(((hairColorForm->abgr >> 16) & 0xFF) * 2, 255);
			}
		}
	}

	for (auto base : layerTarget.textureData) {
		masks[base.first].texture = base.second;
	}
	for (auto base : layerTarget.colorData) {
		if (base.second == kPreset_Skin)
		{
			masks[base.first].color = skinColor;
			flags |= kUpdate_Skin;
		}
		else if (base.second == kPreset_Hair)
		{
			masks[base.first].color = hairColor;
			flags |= kUpdate_Hair;
		}
		else
		{
			masks[base.first].color = base.second;
		}
	}
	for (auto base : layerTarget.blendData) {
		masks[base.first].technique = base.second;
	}
	for (auto base : layerTarget.typeData) {
		masks[base.first].textureType = static_cast<CDXTextureRenderer::TextureType>(base.second);
	}
	for (auto base : layerTarget.alphaData) {
		masks[base.first].color |= (UInt32)(base.second * 255) << 24;
	}

	if (overrides) {
		auto layer = overrides->m_tintData.find(layerTarget.targetIndex);
		if (layer != overrides->m_tintData.end())
		{
			auto & layerData = layer->second;

			for (auto base : layerData.m_textureMap) {
				auto it = masks.find(base.first);
				if (it != masks.end()) {
					masks[base.first].texture = *base.second;
				}
			}

			if (layerTarget.slots.empty())
			{
				for (auto base : layerData.m_colorMap) {
					auto it = masks.find(base.first);
					if (it != masks.end()) {
						masks[base.first].color = base.second;
					}
				}
			}
			else
			{
				for (auto base : layerData.m_colorMap) {
					auto it = layerTarget.slots.equal_range(base.first);
					for (auto itr = it.first; itr != it.second; ++itr) {
						auto it = masks.find(itr->second);
						if (it != masks.end()) {
							masks[itr->second].color = base.second;
						}
					}
				}
			}

			
			for (auto base : layerData.m_blendMap) {
				auto it = masks.find(base.first);
				if (it != masks.end()) {
					masks[base.first].technique = *base.second;
				}
			}
			for (auto base : layerData.m_typeMap) {
				auto it = masks.find(base.first);
				if (it != masks.end()) {
					masks[base.first].textureType = static_cast<CDXTextureRenderer::TextureType>(base.second);
				}
			}
		}
	}
}

NIOVTaskDeferredMask::NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, ItemAttributeDataPtr overrides)
{
	m_firstPerson = isFirstPerson;
	m_formId = refr->formID;
	m_armorId = armor ? armor->formID : 0;
	m_addonId = addon ? addon->formID : 0;
	m_object = object;
	m_overrides = overrides;
}

void NIOVTaskDeferredMask::Dispose()
{

}

void NIOVTaskDeferredMask::Run()
{
	TESForm * refrForm = LookupFormByID(m_formId);
	TESForm * armorForm = LookupFormByID(m_armorId);
	TESForm * addonForm = m_addonId ? LookupFormByID(m_addonId) : nullptr;
	if (refrForm && armorForm) {
		TESObjectREFR * refr = DYNAMIC_CAST(refrForm, TESForm, TESObjectREFR);
		TESObjectARMO * armor = DYNAMIC_CAST(armorForm, TESForm, TESObjectARMO);
		TESObjectARMA * addon = addonForm ? DYNAMIC_CAST(addonForm, TESForm, TESObjectARMA) : nullptr;

		if (refr && armor) {
			g_tintMaskInterface.ApplyMasks(refr, m_firstPerson, armor, addon, m_object, TintMaskInterface::kUpdate_All, m_overrides);
		}
	}
}

void TintMaskInterface::ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, 
	NiAVObject * rootNode, UInt32 flags, ItemAttributeDataPtr overrides, LayerFunctor layerFunctor)
{
	LayerTargetList layerList;
	VisitObjects(rootNode, [&](NiAVObject* object)
	{
		LayerTarget mask;
		mask.targetIndex = 0; // Target the diffuse by default
		if (mask.object = object->GetAsBSGeometry()) {
			auto textureData = ni_cast(object->GetExtraData(BSFixedString("MASKT").data), NiStringsExtraData);
			if (textureData) {
				for (SInt32 i = 0; i < textureData->m_size; ++i)
				{
					mask.textureData[i] = textureData->m_data[i];
				}
				
			}
			auto colorData = ni_cast(object->GetExtraData(BSFixedString("MASKC").data), NiIntegersExtraData);
			if (colorData) {
				for (SInt32 i = 0; i < colorData->m_size && i < colorData->m_size; ++i)
				{
					mask.colorData[i] = colorData->m_data[i];
				}
			}

			auto alphaData = ni_cast(object->GetExtraData(BSFixedString("MASKA").data), NiFloatsExtraData);
			if (alphaData) {
				for (SInt32 i = 0; i < alphaData->m_size && i < alphaData->m_size; ++i)
				{
					mask.alphaData[i] = alphaData->m_data[i];
				}
			}

			if (mask.object && mask.textureData.size() > 0)
				layerList.push_back(mask);
		}

		return false;
	});

	m_modelMap.ApplyLayers(refr, isFirstPerson, armor, addon, rootNode, [&](NiPointer<BSGeometry> geom, SInt32 targetIndex, TextureLayer* layer)
	{
		LayerTarget obj;
		obj.object = geom;
		obj.targetIndex = targetIndex;
		obj.targetFlags = 0;
		obj.textureData = layer->textures;
		obj.colorData = layer->colors;
		obj.alphaData = layer->alphas;
		obj.blendData = layer->blendModes;
		obj.typeData = layer->types;
		obj.slots = layer->slots;
		layerList.push_back(obj);
	});

	std::shared_ptr<ItemAttributeData> itemOverrideData;
	if (overrides && !layerList.empty()) {
		itemOverrideData = overrides;
	}

	std::unique_ptr<CDXD3DDevice> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext = g_renderManager->context;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	pDeviceContext->GetDevice(&pDevice);
	if (!pDevice) {
		return;
	}
	device = std::make_unique<CDXD3DDevice>(pDevice, pDeviceContext);

	int i = 0;
	for (auto & layer : layerList)
	{
		NiPointer<BSShaderProperty> shaderProperty = niptr_cast<BSShaderProperty>(layer.object->m_spEffectState);
		if(shaderProperty) {
			BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
			if(lightingShader) {
				BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)lightingShader->material;

				UInt32 changedFlags = 0;
				std::map<SInt32, CDXNifTextureRenderer::MaskData> tintMasks;
				CreateTintsFromData(refr, tintMasks, layer, itemOverrideData, changedFlags);

				// Must have selective update
				if (flags != kUpdate_All && (changedFlags & flags) == 0) {
					continue;
				}

				const char * texturePath = nullptr;
				NiPointer<NiTexture> sourceTexture;
				if (material->textureSet)
				{
					texturePath = material->textureSet->GetTexturePath(layer.targetIndex);
					if (!texturePath) {
						continue;
					}

					LoadTexture(texturePath, 1, sourceTexture, false);
				}

				// No source texture at this path, bad texture
				if (!sourceTexture) {
					continue;
				}

				std::shared_ptr<CDXNifTextureRenderer> renderer;
				if (m_maskMap.IsCaching())
				{
					renderer = m_maskMap.GetRenderTarget(lightingShader, layer.targetIndex);
				}
				if(!renderer)
				{
					renderer = std::make_shared<CDXNifTextureRenderer>();
					if (!renderer->Init(device.get(), &g_pixelShaders))
					{
						continue;
					}

					if (renderer && m_maskMap.IsCaching()) {
						m_maskMap.AddRenderTargetGroup(lightingShader, layer.targetIndex, renderer);
					}
				}

				char path[512];
				_snprintf_s(path, 512, "RT [%08X][%08X][%08X](%s)", refr->formID, armor->formID, addon ? addon->formID : 0, layer.object->m_name);

				NiPointer<NiTexture> output;
				if (renderer->ApplyMasksToTexture(device.get(), sourceTexture, tintMasks, path, output))
				{
					BSLightingShaderMaterial * newMaterial = static_cast<BSLightingShaderMaterial*>(material->Create());
					newMaterial->Copy(material);

					// If the target material has HairTint we should neutralize the color and block it from receiving tint updates
					// But only if theres a layer in the XML that says to do this, this is still missing TODO
					if (newMaterial->GetShaderType() == BSLightingShaderMaterial::kShaderType_HairTint)
					{
						BSLightingShaderMaterialHairTint* hairMaterial = static_cast<BSLightingShaderMaterialHairTint*>(newMaterial);
						hairMaterial->tintColor.r = 0.5;
						hairMaterial->tintColor.g = 0.5;
						hairMaterial->tintColor.b = 0.5;

						NiExtraData* extraData = lightingShader->GetExtraData("NO_TINT");
						if (!extraData) {
							extraData = NiBooleanExtraData::Create("NO_TINT", true);
							lightingShader->AddExtraData(extraData);
						}
					}

					NiTexturePtr * targetTexture = GetTextureFromIndex(newMaterial, layer.targetIndex);
					if (targetTexture) {
						*targetTexture = output;
					}

					if (layerFunctor) {
						layerFunctor(armor, addon, texturePath, output, layer);
					}

#if DUMP_TEXTURE
					char texturePath[MAX_PATH];
					_snprintf_s(texturePath, MAX_PATH, "Layer_%d_%s.dds", i++, layer.object->m_name);
					int len = strlen(texturePath);
					for (int i = 0; i < len; ++i)
					{
						if (isalpha(texturePath[i]) || isdigit(texturePath[i]) || texturePath[i] == '.' || texturePath[i] == '_')
							continue;
						texturePath[i] = '_';
					}
					SaveRenderedDDS(output.get(), texturePath);
#endif

					CALL_MEMBER_FN(lightingShader, SetMaterial)(newMaterial, 1); // This creates a new material from the one we created above, so we destroy it after
					CALL_MEMBER_FN(lightingShader, InitializeShader)(layer.object);
					
					newMaterial->~BSLightingShaderMaterial();
					Heap_Free(newMaterial);
				}

#if 0
					
				NiTexture * newTarget = NULL;
				if (m_maskMap.IsCaching())
					newTarget = m_maskMap.GetRenderTarget(lightingShader);
				if (!newTarget) {
					UInt32 width = 0;
					UInt32 height = 0;
					if (mask.resolutionWData)
						width = mask.resolutionWData;
					if (mask.resolutionHData)
						height = mask.resolutionHData;
					else
						height = mask.resolutionWData;

					newTarget = CreateSourceTexture("TintMask");
					newTarget->rendererData = g_renderManager->CreateRenderTexture(width, height);

					if (newTarget && m_maskMap.IsCaching()) {
						m_maskMap.AddRenderTargetGroup(lightingShader, newTarget);
					}
				}
				if(newTarget) {
					tArray<TintMask*> tintMasks;
					CreateTintsFromData(tintMasks, mask.layerCount, mask.textureData, mask.colorData, mask.alphaData, overrideMap);

					newTarget->IncRef();
					if (ApplyMasksToRenderTarget(&tintMasks, newTarget)) {
						BSLightingShaderMaterialFacegen * tintedMaterial = static_cast<BSLightingShaderMaterialFacegen*>(CreateShaderMaterial(BSLightingShaderMaterialFacegen::kShaderType_FaceGen));
						CALL_MEMBER_FN(tintedMaterial, CopyFrom)(material);
						tintedMaterial->renderedTexture = newTarget;
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x0A, true); // Enable detailmap
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x15, false); // Disable FaceGen_RGB
						//material->ReleaseTextures();
						CALL_MEMBER_FN(lightingShader, SetMaterial)(tintedMaterial, 1); // New material takes texture ownership
						if (newTarget) // Let the material now take ownership since the old target is destroyed now
							newTarget->DecRef();
						CALL_MEMBER_FN(lightingShader, InitializeShader)(mask.object);
					}

					newTarget->DecRef();

					ReleaseTintsFromData(tintMasks);
				}

#endif
			}
		}
	}
}

void TintMaskMap::ManageRenderTargetGroups()
{
	SimpleLocker locker(&m_lock);
	m_caching = true;
}

std::shared_ptr<CDXNifTextureRenderer> TintMaskMap::GetRenderTarget(BSLightingShaderProperty* key, SInt32 index)
{
	auto & it = m_data.find(key);
	if (it != m_data.end()) {
		auto & idx = it->second.find(index);
		if (idx != it->second.end()) {
			return idx->second;
		}
	}

	return nullptr;
}

void TintMaskMap::AddRenderTargetGroup(BSLightingShaderProperty* key, SInt32 index, std::shared_ptr<CDXNifTextureRenderer> value)
{
	SimpleLocker locker(&m_lock);
	m_data[key][index] = value;
}

void TintMaskMap::ReleaseRenderTargetGroups()
{
	SimpleLocker locker(&m_lock);
	m_data.clear();
	m_caching = false;
}

TextureLayer * TextureLayerMap::GetTextureLayer(SKEEFixedString texture)
{
	auto & it = find(texture);
	if (it != end()) {
		return &it->second;
	}

	return nullptr;
}

TextureLayerMap * MaskTriShapeMap::GetTextureMap(SKEEFixedString triShape)
{
	auto & it = find(triShape);
	if (it != end()) {
		return &it->second;
	}

	return nullptr;
}

TextureLayer * MaskModelMap::GetMask(SKEEFixedString nif, SKEEFixedString trishape, SKEEFixedString texture)
{
	return &m_data[nif][trishape][GetSanitizedPath(texture)];
}

MaskTriShapeMap * MaskModelMap::GetTriShapeMap(SKEEFixedString nifPath)
{
	auto & it = m_data.find(nifPath);
	if (it != m_data.end()) {
		return &it->second;
	}

	return nullptr;
}

bool ApplyMaskData(MaskTriShapeMap * triShapeMap, NiAVObject * object, const char * nameOverride, std::function<void(NiPointer<BSGeometry>, SInt32, TextureLayer*)> functor)
{
	NiPointer<BSGeometry> geometry = object->GetAsBSGeometry();
	if (!geometry) {
		return false;
	}

	auto textureMap = triShapeMap->GetTextureMap(nameOverride ? nameOverride : object->m_name);
	if (!textureMap) {
		return false;
	}

	NiPointer<NiProperty> shaderProperty = niptr_cast<NiProperty>(geometry->m_spEffectState);
	if (!shaderProperty) {
		return false;
	}

	auto lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
	if (!lightingShader) {
		return false;
	}

	auto material = static_cast<BSLightingShaderMaterial*>(lightingShader->material);
	if (!material) {
		return false;
	}

	auto textureSet = material->textureSet;
	if (!textureSet) {
		return false;
	}

	// Pass over all the textures to match with those that exist in the mapping
	for (UInt32 i = 0; i < 8; ++i)
	{
		const char * texture = textureSet->GetTexturePath(i);
		if (!texture)
			continue;

		auto it = textureMap->find(GetSanitizedPath(texture));
		if (it == textureMap->end())
		{
			char buff[std::numeric_limits<SInt32>::digits10 + 1];
			sprintf_s(buff, "%d", i);
			it = textureMap->find(buff);
		}
		if (it == textureMap->end())
		{
			continue;
		}

		functor(geometry, i, &it->second);
	}

	return true;
}

SKEEFixedString MaskModelMap::GetModelPath(UInt8 gender, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * arma)
{
	SKEEFixedString modelPath;
	if (arma) {
		modelPath = arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName();
		if (isFirstPerson && modelPath.length() == 0) { // If first person was not found, try third person
			modelPath = arma->models[0][gender].GetModelName();
			if (modelPath.length() == 0) { // If gender not found, try male
				modelPath = arma->models[0][0].GetModelName();
			}
		}
	}
	else if (armor) {
		modelPath = armor->bipedModel.textureSwap[gender].GetModelName();
		if (modelPath.length() == 0) { // If gender not found, try male
			modelPath = armor->bipedModel.textureSwap[0].GetModelName();
		}
	}

	return modelPath;
}

void MaskModelMap::ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * arma, NiAVObject * node, std::function<void(NiPointer<BSGeometry>, SInt32, TextureLayer*)> functor)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&m_lock);

	// Special case if the addon has no 1p model, use the 3p model
	SKEEFixedString modelPath = GetModelPath(gender, isFirstPerson, armor, arma);
	auto triShapeMap = GetTriShapeMap(modelPath);
	if (!triShapeMap) {
		return;
	}

	UInt32 count = 0;
	VisitObjects(node, [&](NiAVObject* object)
	{
		if (ApplyMaskData(triShapeMap, object, nullptr, functor))
			count++;

		return false;
	});

	if (count == 0)
		ApplyMaskData(triShapeMap, node, "", functor);
}

bool TintMaskInterface::IsDyeable(TESObjectARMO * armor)
{
	SimpleLocker locker(&m_dyeableLock);

	auto it = m_dyeable.find(armor->formID);
	if (it != m_dyeable.end())
	{
		return it->second;
	}
	else
	{
		// This could be expensive so lets just cache items we've seen
		for (UInt8 g = 0; g <= 1; ++g)
		{
			for (UInt8 fp = 0; fp <= 1; ++fp)
			{
				for (UInt32 i = 0; i < armor->armorAddons.count; i++)
				{
					TESObjectARMA * arma = nullptr;
					if (armor->armorAddons.GetNthItem(i, arma) && arma)
					{
						SKEEFixedString modelPath = m_modelMap.GetModelPath(g, fp == 1, armor, arma);
						if (m_modelMap.GetTriShapeMap(modelPath))
						{
							m_dyeable.emplace(armor->formID, true);
							return true;
						}
					}
				}
			}
		}

		m_dyeable.emplace(armor->formID, false);
	}

	return false;
}

void TintMaskInterface::VisitTemplateData(TESObjectREFR* refr, TESObjectARMO* armor, std::function<void(MaskTriShapeMap*)> functor)
{
	UInt8 gender = 0;
	TESNPC* actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase) {
		Actor* actor = static_cast<Actor*>(refr);
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

		for (UInt32 i = 0; i < armor->armorAddons.count; ++i)
		{
			TESObjectARMA* addon = nullptr;
			armor->armorAddons.GetNthItem(i, addon);

			if (!addon->IsPlayable() || !addon->isValidRace(actor->race))
				continue;

			auto shapeMap = m_modelMap.GetTriShapeMap(m_modelMap.GetModelPath(gender, false, armor, addon));
			if (shapeMap)
			{
				functor(shapeMap);
			}
		}
	}
}

void TintMaskInterface::GetTemplateColorMap(TESObjectREFR* refr, TESObjectARMO * armor, std::map<SInt32, UInt32>& colorMap)
{
	VisitTemplateData(refr, armor, [&](auto shapeMap)
	{
		if (shapeMap->IsRemappable())
		{
			for (auto& shape : *shapeMap)
			{
				// Try all textures, though we probably only care about the diffuse
				for (auto& layer : shape.second)
				{
					for (auto& slot : layer.second.slots)
					{
						colorMap[slot.first] = 0;

						auto cit = layer.second.colors.find(slot.second);
						if (cit != layer.second.colors.end())
						{
							colorMap[slot.first] = cit->second & 0xFFFFFF;
						}

						auto ait = layer.second.alphas.find(slot.second);
						if (ait != layer.second.alphas.end())
						{
							colorMap[slot.first] |= UInt32(ait->second * 255) << 24;
						}
					}
				}
			}
		}
		else
		{
			// Try all shapes in this nif
			for (auto& shape : *shapeMap)
			{
				// Try all textures, though we probably only care about the diffuse
				for (auto& layer : shape.second)
				{
					// For each color mapping extract the template color and alpha
					for (auto& color : layer.second.colors)
					{
						colorMap[color.first] = color.second & 0xFFFFFF;

						auto it = layer.second.alphas.find(color.first);
						if (it != layer.second.alphas.end())
						{
							colorMap[color.first] |= UInt32(it->second * 255) << 24;
						}
					}
				}
			}
		}
	});
}


void TintMaskInterface::GetSlotTextureIndexMap(TESObjectREFR* refr, TESObjectARMO* armor, std::map<SInt32, UInt32>& slotTextureIndexMap)
{
	VisitTemplateData(refr, armor, [&](auto shapeMap)
	{
		if (shapeMap->IsRemappable())
		{
			for (auto& shape : *shapeMap)
			{
				// Try all textures, though we probably only care about the diffuse
				for (auto& layer : shape.second)
				{
					SInt32 textureIndex = 0;
					if (sscanf_s(layer.first, "%d", &textureIndex))
					{
						for (auto& slot : layer.second.slots)
						{
							slotTextureIndexMap[slot.first] = textureIndex;
						}
					}
				}
			}
		}
	});
}

void TintMaskInterface::LoadMods()
{
	m_dyeableLock.Lock();
	m_dyeable.clear();
	m_dyeableLock.Release();
	m_modelMap.Lock();
	m_modelMap.m_data.clear();
	
	std::vector<SKEEFixedString> tintFiles;
	FileUtils::GetAllFiles("Data\\SKSE\\Plugins\\NiOverride\\TintData\\", "*.xml", tintFiles);
	std::sort(tintFiles.begin(), tintFiles.end());

	for (auto tintFile : tintFiles)
	{
		ParseTintData(tintFile.c_str());
	}

	m_modelMap.Release();
}

void TintMaskInterface::ParseTintData(LPCTSTR filePath)
{
	std::string path(filePath);
	path.erase(0, 5);

	BSResourceNiBinaryStream bStream(path.c_str());
	std::string data;
	BSFileUtil::ReadAll(&bStream, data);

	tinyxml2::XMLDocument tintDoc;
	tintDoc.Parse(data.c_str(), data.size());

	if (tintDoc.Error()) {
		_ERROR("%s", tintDoc.GetErrorStr1());
		return;
	}

	auto element = tintDoc.FirstChildElement("tintmasks");
	if (element) {
		auto object = element->FirstChildElement("object");
		while (object)
		{
			auto objectPath = SKEEFixedString(object->Attribute("path"));
			if (object->BoolAttribute("override")) {
				auto trishapeMap = m_modelMap.GetTriShapeMap(objectPath);
				if (trishapeMap) {
					trishapeMap->clear();
				}
			}

			bool remappable = object->BoolAttribute("remappable");

			UInt32 index = 0;
			auto child = object->FirstChildElement("geometry");
			while (child)
			{
				auto trishapeName = child->Attribute("name");
				auto trishape = SKEEFixedString(trishapeName ? trishapeName : "");

				const char * texture = child->Attribute("texture");
				if (!texture)
				{
					texture = child->Attribute("diffuse");
					if (!texture)
					{
						texture = "0";
					}
				}
				
				auto layer = m_modelMap.GetMask(objectPath, trishape, texture);
				if (child->BoolAttribute("override")) {
					layer->types.clear();
					layer->colors.clear();
					layer->alphas.clear();
					layer->blendModes.clear();
					layer->textures.clear();
				}

				if (remappable) {
					index = 0;
				}

				
				auto mask = child->FirstChildElement("mask");
				while (mask) {
					auto path = mask->Attribute("path");
					auto color = mask->Attribute("color");

					UInt32 colorValue;
					if (_strnicmp(color, "skin", 4) == 0) {
						colorValue = kPreset_Skin;
					}
					else if (_strnicmp(color, "hair", 4) == 0) {
						colorValue = kPreset_Hair;
					}
					else if (color) {
						sscanf_s(color, "%x", &colorValue);
					}
					else {
						colorValue = 0xFFFFFF;
					}
					
					auto alpha = mask->DoubleAttribute("alpha");

					const char* blend = mask->Attribute("blend");
					const char* type = mask->Attribute("type");
					if (!type) {
						type = "mask";
					}

					UInt8 typeNumber = static_cast<UInt8>(CDXNifTextureRenderer::TextureType::Mask);
					if (_strnicmp(type, "mask", 4) == 0) {
						 typeNumber = static_cast<UInt8>(CDXNifTextureRenderer::TextureType::Mask);
					}
					else if (_strnicmp(type, "normal", 6) == 0) {
						typeNumber = static_cast<UInt8>(CDXNifTextureRenderer::TextureType::Normal);
					}
					else if (_strnicmp(type, "solid", 5) == 0 || _strnicmp(type, "color", 5) == 0) {
						typeNumber = static_cast<UInt8>(CDXNifTextureRenderer::TextureType::Color);
					}
					else {
						SInt32 typeValue = 0;
						sscanf_s(type, "%d", &typeValue);
						typeNumber = static_cast<UInt8>(typeValue);
					}
					
					int i = index;
					mask->QueryIntAttribute("index", &i);

					int slot = -1;
					mask->QueryIntAttribute("slot", &slot);

					layer->textures[i] = path ? path : "";
					layer->colors[i] = colorValue;
					layer->alphas[i] = alpha;
					layer->blendModes[i] = blend ? blend : "overlay";
					layer->types[i] = typeNumber;

					if (remappable && slot >= 0) {
						layer->slots.emplace(slot, i);
					}

					mask = mask->NextSiblingElement("mask");
					index++;
				}

				child = child->NextSiblingElement("geometry");
			}

			auto trishapeMap = m_modelMap.GetTriShapeMap(objectPath);
			if (trishapeMap) {
				trishapeMap->SetRemappable(remappable);
			}

			object = object->NextSiblingElement("object");
		}
	}
}

void TintMaskInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	UInt32 armorMask = armor->bipedObject.GetSlotMask();
	UInt32 addonMask = addon->biped.GetSlotMask();
	if (((armorMask & addonMask) & (g_tintHairSlot)) != 0)
	{
		Character * actor = refr->formType == Character::kTypeID ? static_cast<Character*>(refr) : nullptr;
		NiColorA value;
		TESNPC * actorBase = static_cast<TESNPC*>(refr->baseForm);
		if (GetActorHairColor(actor, value)) {
			NiColorA* color = &value;
			UpdateModelHair(object, &color);
		}
	}
}

bool TintMaskInterface::GetActorHairColor(Actor* actor, NiColorA& color)
{
	BGSColorForm* hairColorForm = nullptr;
	TESNPC* actorBase = static_cast<TESNPC*>(actor->baseForm);
	if (actorBase) {
		UInt8 gender = CALL_MEMBER_FN(actorBase, GetSex)();
		// Try our parent templates
		TESNPC* templateNPC = actorBase;
		do {
			auto headData = templateNPC->headData;
			if (headData) {
				hairColorForm = headData->hairColor;
			}
			templateNPC = templateNPC->nextTemplate;
		} while (templateNPC && !hairColorForm);

		// No templates had a hair color record, get the default from the race
		if (!hairColorForm)
		{
			TESRace* race = actor->race;
			if (!race) {
				race = actorBase->race.race;
			}

			// Race had no chargen data, they probably don't have a hair record
			auto chargenData = race->chargenData[gender];
			if (!chargenData)
			{
				return false;
			}

			hairColorForm = race->chargenData[gender]->defaultColor;
		}
	}

	if (hairColorForm) {
		color.r = static_cast<float>(min((hairColorForm->abgr & 0xFF) * 2, 255)) / 255.0f;
		color.g = static_cast<float>(min(((hairColorForm->abgr >> 8) & 0xFF) * 2, 255)) / 255.0f;
		color.b = static_cast<float>(min(((hairColorForm->abgr >> 16) & 0xFF) * 2, 255)) / 255.0f;
		color.a = 1.0f;
		return true;
	}

	return false;
}

EventResult TintMaskInterface::ReceiveEvent(SKSENiNodeUpdateEvent * evn, EventDispatcher<SKSENiNodeUpdateEvent>* dispatcher)
{
	TESObjectREFR* refr = evn->reference;
	if (refr && refr->formType == Character::kTypeID)
	{
		Character * actor = static_cast<Character*>(refr);
		NiColorA value;
		if (GetActorHairColor(actor, value)) {
			NiColorA* color = &value;
			VisitEquippedNodes(actor, g_tintHairSlot, [&](TESObjectARMO* armor, TESObjectARMA* arma, NiAVObject* node, bool isFP)
			{
				UpdateModelHair(node, &color);
			});
		}
	}
	return kEvent_Continue;
}
