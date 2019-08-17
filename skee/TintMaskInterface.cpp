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

#include <Shlwapi.h>

#include "tinyxml2.h"

#include "CDXD3DDevice.h"
#include "CDXNifTextureRenderer.h"
#include "CDXNifPixelShaderCache.h"

#include <vector>
#include <algorithm>

CDXNifPixelShaderCache		g_pixelShaders;
extern TintMaskInterface	g_tintMaskInterface;

UInt32 TintMaskInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void TintMaskInterface::CreateTintsFromData(std::map<SInt32, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, std::shared_ptr<ItemAttributeData> & overrides)
{
	for (auto base : layerTarget.textureData) {
		masks[base.first].texture = base.second;
	}
	for (auto base : layerTarget.colorData) {
		masks[base.first].color = base.second;
	}
	for (auto base : layerTarget.blendData) {
		masks[base.first].technique = base.second;
	}
	for (auto base : layerTarget.typeData) {
		masks[base.first].textureType = base.second;
	}

	if (overrides) {
		auto layer = overrides->m_tintData.find(layerTarget.targetIndex);
		if (layer != overrides->m_tintData.end())
		{
			auto & layerData = layer->second;

			for (auto base : layerData.m_textureMap) {
				masks[base.first].texture = *base.second;
			}
			for (auto base : layerData.m_colorMap) {
				masks[base.first].color = base.second;
			}
			for (auto base : layerData.m_blendMap) {
				masks[base.first].technique = *base.second;
			}
			for (auto base : layerData.m_typeMap) {
				masks[base.first].textureType = base.second;
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
			g_tintMaskInterface.ApplyMasks(refr, m_firstPerson, armor, addon, m_object, m_overrides);
		}
	}
}

void TintMaskInterface::ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * rootNode, std::function<std::shared_ptr<ItemAttributeData>()> overrides)
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

				std::map<SInt32, CDXNifTextureRenderer::MaskData> tintMasks;
				CreateTintsFromData(tintMasks, layer, itemOverrideData);

				NiPointer<NiTexture> output;
				if (renderer->ApplyMasksToTexture(device.get(), sourceTexture, tintMasks, output))
				{
					BSLightingShaderMaterial * newMaterial = static_cast<BSLightingShaderMaterial*>(CreateShaderMaterial(material->GetShaderType()));
					newMaterial->Copy(material);

					NiTexturePtr * targetTexture = GetTextureFromIndex(newMaterial, layer.targetIndex);
					if (targetTexture) {
						*targetTexture = output;
					}

					CALL_MEMBER_FN(lightingShader, SetMaterial)(newMaterial, 1); // New material takes texture ownership
					CALL_MEMBER_FN(lightingShader, InitializeShader)(layer.object);
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

	return NULL;
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

		SKEEFixedString texturePath = GetSanitizedPath(texture);
		auto it = textureMap->find(texturePath);
		if (it != textureMap->end())
		{
			functor(geometry, i, &it->second);
		}
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

	auto triShapeMap = GetTriShapeMap(arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName());
	if (!triShapeMap) {
		triShapeMap = GetTriShapeMap(arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName());
		if (!triShapeMap) {
			return;
		}
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

void TintMaskInterface::ReadTintData(LPCTSTR lpFolder, LPCTSTR lpFilePattern)
{
	TCHAR szFullPattern[MAX_PATH];
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
				ReadTintData(szFullPattern, lpFilePattern);
			}
		} while (FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
	// now we are going to look for the matching files
	PathCombine(szFullPattern, lpFolder, lpFilePattern);
	hFindFile = FindFirstFile(szFullPattern, &FindFileData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				// found a file; do something with it
				PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
				ParseTintData(szFullPattern);
			}
		} while (FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
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
			auto child = object->FirstChildElement("geometry");
			while (child)
			{
				auto trishapeName = child->Attribute("name");
				auto trishape = SKEEFixedString(trishapeName ? trishapeName : "");

				const char * diffuse = child->Attribute("diffuse");
				const char * texture = child->Attribute("texture");
				
				auto layer = m_modelMap.GetMask(objectPath, trishape, texture ? texture : diffuse);

				layer->textures.clear();
				layer->colors.clear();
				layer->alphas.clear();

				UInt32 index = 0;
				auto mask = child->FirstChildElement("mask");
				while (mask) {
					auto maskPath = SKEEFixedString(mask->Attribute("path"));
					auto color = mask->Attribute("color");

					UInt32 colorValue;
					if (color) {
						sscanf_s(color, "%x", &colorValue);
					}
					else {
						colorValue = 0xFFFFFF;
					}
					
					auto alpha = mask->DoubleAttribute("alpha");

					const char* blend = mask->Attribute("blend");
					if (!blend) {
						blend = "tint";
					}

					int type = static_cast<int>(CDXNifTextureRenderer::TextureType::Mask);
					mask->QueryIntAttribute("type", &type);

					int i = index;
					mask->QueryIntAttribute("index", &i);

					layer->textures[i] = maskPath;
					layer->colors[i] = colorValue;
					layer->alphas[i] = alpha;
					layer->blendModes[i] = blend;
					layer->types[i] = type;

					mask = mask->NextSiblingElement("mask");
					index++;
				}

				child = child->NextSiblingElement("geometry");
			}

			object = object->NextSiblingElement("object");
		}
	}
}