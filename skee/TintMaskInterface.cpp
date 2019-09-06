#include "TintMaskInterface.h"
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

UInt32 TintMaskInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void TintMaskInterface::CreateTintsFromData(TESObjectREFR * refr, std::map<SInt32, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, std::shared_ptr<ItemAttributeData> & overrides, UInt32 & flags)
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
		masks[base.first].textureType = base.second;
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
			for (auto base : layerData.m_colorMap) {
				auto it = masks.find(base.first);
				if (it != masks.end()) {
					masks[base.first].color = base.second;
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
					masks[base.first].textureType = base.second;
				}
			}
		}
	}
}

NIOVTaskDeferredMask::NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<std::shared_ptr<ItemAttributeData>()> overrides)
{
	m_firstPerson = isFirstPerson;
	m_formId = refr->formID;
	m_armorId = armor->formID;
	m_addonId = addon->formID;
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
	TESForm * addonForm = LookupFormByID(m_addonId);
	if (refrForm && armorForm && addonForm) {
		TESObjectREFR * refr = DYNAMIC_CAST(refrForm, TESForm, TESObjectREFR);
		TESObjectARMO * armor = DYNAMIC_CAST(armorForm, TESForm, TESObjectARMO);
		TESObjectARMA * addon = DYNAMIC_CAST(addonForm, TESForm, TESObjectARMA);

		if (refr && armor && addon) {
			g_tintMaskInterface.ApplyMasks(refr, m_firstPerson, armor, addon, m_object, m_overrides, TintMaskInterface::kUpdate_All);
		}
	}
}

void TintMaskInterface::ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * rootNode, std::function<std::shared_ptr<ItemAttributeData>()> overrides, UInt32 flags)
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

	m_modelMap.ApplyLayers(refr, isFirstPerson, addon, rootNode, [&](NiPointer<BSGeometry> geom, SInt32 targetIndex, TextureLayer* layer)
	{
		LayerTarget obj;
		obj.object = geom;
		obj.targetIndex = targetIndex;
		obj.textureData = layer->textures;
		obj.colorData = layer->colors;
		obj.alphaData = layer->alphas;
		obj.blendData = layer->blendModes;
		obj.typeData = layer->types;
		layerList.push_back(obj);
	});

	std::shared_ptr<ItemAttributeData> itemOverrideData;
	if (overrides && !layerList.empty()) {
		itemOverrideData = overrides();
	}

	std::unique_ptr<CDXD3DDevice> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext = g_renderManager->context;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	pDeviceContext->GetDevice(&pDevice);
	if (!pDevice) {
		return;
	}
	device = std::make_unique<CDXD3DDevice>(pDevice, pDeviceContext);

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

				NiPointer<NiTexture> sourceTexture;
				if (material->textureSet)
				{
					const char * texturePath = material->textureSet->GetTexturePath(layer.targetIndex);
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
				_snprintf_s(path, 512, "RT [%08X][%08X][%08X](%s)", refr->formID, armor->formID, addon->formID, layer.object->m_name);

				NiPointer<NiTexture> output;
				if (renderer->ApplyMasksToTexture(device.get(), sourceTexture, tintMasks, path, output))
				{
					BSLightingShaderMaterial * newMaterial = static_cast<BSLightingShaderMaterial*>(material->Create());
					newMaterial->Copy(material);

					NiTexturePtr * targetTexture = GetTextureFromIndex(newMaterial, layer.targetIndex);
					if (targetTexture) {
						*targetTexture = output;
					}

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

void MaskModelMap::ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMA * arma, NiAVObject * node, std::function<void(NiPointer<BSGeometry>, SInt32, TextureLayer*)> functor)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker locker(&m_lock);

	// Special case if the addon has no 1p model, use the 3p model
	SKEEFixedString modelPath = arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName();
	if (isFirstPerson && modelPath.length() == 0) {
		modelPath = arma->models[0][gender].GetModelName();
	}

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

				UInt32 index = 0;
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

					layer->textures[i] = path ? path : "";
					layer->colors[i] = colorValue;
					layer->alphas[i] = alpha;
					layer->blendModes[i] = blend ? blend : "overlay";
					layer->types[i] = typeNumber;

					mask = mask->NextSiblingElement("mask");
					index++;
				}

				child = child->NextSiblingElement("geometry");
			}

			object = object->NextSiblingElement("object");
		}
	}
}