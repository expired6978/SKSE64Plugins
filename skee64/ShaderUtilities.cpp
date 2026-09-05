#include "ShaderUtilities.h"
#include "SKEETasks.h"
#include "OverrideVariant.h"
#include "NifUtils.h"

#include "RE/N/NiRTTI.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSLightingShaderMaterial.h"
#include "RE/B/BSLightingShaderMaterialFacegen.h"
#include "RE/B/BSLightingShaderMaterialFacegenTint.h"
#include "RE/B/BSLightingShaderMaterialGlowmap.h"
#include "RE/B/BSLightingShaderMaterialParallax.h"
#include "RE/B/BSLightingShaderMaterialParallaxOcc.h"
#include "RE/B/BSLightingShaderMaterialEye.h"
#include "RE/B/BSLightingShaderMaterialEnvmap.h"
#include "RE/B/BSLightingShaderMaterialMultiLayerParallax.h"
#include "RE/B/BSEffectShaderProperty.h"
#include "RE/B/BSEffectShaderMaterial.h"
#include "RE/B/BSShaderTextureSet.h"
#include "RE/B/BGSTextureSet.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiTimeController.h"
#include "RE/N/NiExtraData.h"
#include "RE/N/NiTexture.h"
#include "RE/T/TESForm.h"

#include <regex>
#include <algorithm>
#include <cstdint>
#include "NiRTTIUtils.h"
#include "SKEEHooks.h"

extern const SKSE::TaskInterface* g_task;

void GetShaderProperty(RE::NiAVObject* node, OverrideVariant* value)
{
	bool shaderError = false;
	auto * geometry = node ? node->AsGeometry() : nullptr;
	if (geometry)
	{
		RE::BSShaderProperty* shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get());
		if (!shaderProperty) {
			SKSE::log::info("Shader does not exist for {}", node->name);
			shaderError = true;
			return;
		}
		if (value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
		{
			std::int8_t currentIndex = 0;
			std::int8_t controllerIndex = value->index;
			if (controllerIndex != -1)
			{
				RE::NiTimeController* foundController = nullptr;
				RE::NiTimeController* controller = shaderProperty->controllers.get(); // legacy m_controller (NiObjectNET, offset 0x18)
				while (controller)
				{
					if (currentIndex == controllerIndex) {
						foundController = controller;
						break;
					}

					controller = controller->next.get();
					currentIndex++;
				}

				if (foundController)
				{
					switch (value->key)
				{
					case OverrideVariant::kParam_ControllerFrequency:	PackValue<float>(value, value->key, value->index, &foundController->frequency);	break;
					case OverrideVariant::kParam_ControllerPhase:		PackValue<float>(value, value->key, value->index, &foundController->phase);		break;
					case OverrideVariant::kParam_ControllerStartTime:	PackValue<float>(value, value->key, value->index, &foundController->loKeyTime);	break;
					case OverrideVariant::kParam_ControllerStopTime:	PackValue<float>(value, value->key, value->index, &foundController->hiKeyTime);	break;

						// Special cases
					case OverrideVariant::kParam_ControllerStartStop:
						{
							float val = 0.0;
							PackValue<float>(value, value->key, value->index, &val);	break;
						}
						break;
					default:
						SKSE::log::info("Unknown controller key {} {}", value->key, node->name);
						shaderError = true;
						break;
					}
				}
			}

			return; // Only working on controller properties
		}
		if (netimmerse_isKind<RE::BSEffectShaderProperty>(shaderProperty))
		{
			RE::BSEffectShaderMaterial* material = static_cast<RE::BSEffectShaderMaterial*>(shaderProperty->material);
			switch (value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		PackValue<RE::NiColorA>(value, value->key, value->index, &material->baseColor);		break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	PackValue<float>(value, value->key, value->index, &material->baseColorScale);		break;
			default:
				SKSE::log::info("Unknown shader key {} {}", value->key, node->name);
				break;
			}
#ifdef _DEBUG
			SKSE::log::info("Applied EffectShader property {} {:X} to {}", value->key, value->data.u, node->name);
#endif
		}
		else if (netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty))
		{
			RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty);
			RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
			switch (value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		{ auto* em = static_cast<RE::BSEffectShaderMaterial*>(lightingShader->material); PackValue<RE::NiColorA>(value, value->key, value->index, &em->baseColor); } break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	{ auto* em = static_cast<RE::BSEffectShaderMaterial*>(lightingShader->material); PackValue<float>(value, value->key, value->index, &em->baseColorScale); } break;
			case OverrideVariant::kParam_ShaderAlpha:				PackValue<float>(value, value->key, value->index, &material->materialAlpha);					break;
			case OverrideVariant::kParam_ShaderGlossiness:			PackValue<float>(value, value->key, value->index, &material->specularPower);				break;
			case OverrideVariant::kParam_ShaderSpecularStrength:	PackValue<float>(value, value->key, value->index, &material->specularColorScale);			break;
			case OverrideVariant::kParam_ShaderLightingEffect1:		PackValue<float>(value, value->key, value->index, &material->subSurfaceLightRolloff);			break;
			case OverrideVariant::kParam_ShaderLightingEffect2:		PackValue<float>(value, value->key, value->index, &material->rimLightPower);			break;

				// Special cases
			case OverrideVariant::kParam_ShaderTexture:
				{
					if (value->index < RE::BSTextureSet::Texture::kTotal)
					{
						RE::BSFixedString texture = material->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(value->index));
						PackValue<RE::BSFixedString>(value, value->key, value->index, &texture);
					}
				}
				break;
			case OverrideVariant::kParam_ShaderTextureSet:
				{
					PackValue<RE::BGSTextureSet*>(value, value->key, value->index, nullptr);
				}
				break;
			case OverrideVariant::kParam_ShaderTintColor:
				{
					if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint || material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint) {
						RE::BSLightingShaderMaterialFacegenTint* tintedMaterial = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(static_cast<RE::BSLightingShaderMaterialBase*>(material));
						PackValue<RE::NiColor>(value, value->key, value->index, &tintedMaterial->tintColor);
					}
				}
				break;
			default:
				SKSE::log::info("Unknown lighting shader key {} {}", value->key, node->name);
				shaderError = true;
				break;
			}
#ifdef _DEBUG
			SKSE::log::info("Applied LightingShader property {} {:X} to {}", value->key, value->data.u, node->name);
#endif
		}
	}
	else {
		SKSE::log::info("{} - Failed to cast {} to geometry", __FUNCTION__, node->name);
		shaderError = true;
	}

	if (shaderError) {
		std::uint32_t def = 0;
		PackValue<std::uint32_t>(value, value->key, -1, &def);
	}
}

void NIOVTaskUpdateTexture::Run()
{
	if (m_geometry)
	{
		RE::BSShaderProperty* shaderProperty = static_cast<RE::BSShaderProperty*>(m_geometry->GetGeometryRuntimeData().shaderProperty.get());
		if (!shaderProperty) {
			SKSE::log::info("Shader does not exist for {}", m_geometry->name);
			return;
		}

        RE::BSFixedString texturePath = m_texture->AsBSFixedString();
		RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty);
		if (lightingShader)
		{
			RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
			if (m_index < RE::BSTextureSet::Texture::kTotal) {
				// Need to update the texture path of the BSTextureSet
				auto* newTextureSetRaw = RE::BSShaderTextureSet::Create();
				for (std::uint32_t i = 0; i < RE::BSTextureSet::Texture::kTotal; i++)
				{
					newTextureSetRaw->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), material->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)));
				}
				newTextureSetRaw->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(m_index), texturePath.c_str());
				material->SetTextureSet(RE::NiPointer<RE::BSTextureSet>(newTextureSetRaw));

				// Load the texture requested and then assign it to the material
				RE::NiPointer<RE::NiTexture> newTexture;
				RE::BSShaderManager::GetTexture(texturePath.c_str(), 1, newTexture, false);

				auto targetTexture = GetTextureFromIndex(material, m_index);
				if (targetTexture) {
					targetTexture->reset(static_cast<RE::NiSourceTexture*>(newTexture.get()));
				}

				SKEE::InitializeShader(lightingShader, m_geometry.get());
			}
		}
	}
}

void SetShaderProperty(RE::NiAVObject* node, OverrideVariant* value, bool immediate)
{
	auto * geometry = node ? node->AsGeometry() : nullptr;
	if (geometry)
	{
		RE::BSShaderProperty* shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get());
		if (!shaderProperty) {
			SKSE::log::info("Shader does not exist for {}", geometry->name);
			return;
		}

		if (value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
		{
			std::int8_t currentIndex = 0;
			std::int8_t controllerIndex = value->index;
			if (controllerIndex != -1)
			{
				RE::NiTimeController* foundController = nullptr;
				RE::NiTimeController* controller = shaderProperty->controllers.get(); // legacy m_controller (NiObjectNET, offset 0x18)
				while (controller)
				{
					if (currentIndex == controllerIndex) {
						foundController = controller;
						break;
					}

					controller = controller->next.get();
					currentIndex++;
				}

				if (foundController)
				{
					switch (value->key)
				{
					case OverrideVariant::kParam_ControllerFrequency:	UnpackValue(&foundController->frequency, value);	return;	break;
					case OverrideVariant::kParam_ControllerPhase:		UnpackValue(&foundController->phase, value);		return;	break;
					case OverrideVariant::kParam_ControllerStartTime:	UnpackValue(&foundController->loKeyTime, value);	return;	break;
					case OverrideVariant::kParam_ControllerStopTime:	UnpackValue(&foundController->hiKeyTime, value);	return;	break;

						// Special cases
					case OverrideVariant::kParam_ControllerStartStop:
						{
							float fValue;
							UnpackValue(&fValue, value);
							if (fValue < 0.0)
							{
								foundController->Start(0);
								foundController->Stop();
							}
							else {
								foundController->Start(fValue);
							}
							return;
						}
						break;
					default:
						SKSE::log::info("Unknown controller key {} {}", value->key, node->name);
						return;
						break;
					}
				}
			}

			return; // Only working on controller properties
		}

		RE::BSEffectShaderProperty* effectShader = netimmerse_cast<RE::BSEffectShaderProperty*>(shaderProperty);
		if (effectShader)
		{
			RE::BSEffectShaderMaterial* material = static_cast<RE::BSEffectShaderMaterial*>(shaderProperty->material);
			switch (value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		UnpackValue(&material->baseColor, value);		return;	break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	UnpackValue(&material->baseColorScale, value);	return;	break;
			default:
				SKSE::log::info("Unknown shader key {} {}", value->key, node->name);
				return;
				break;
			}
#ifdef _DEBUG
			SKSE::log::info("Applied EffectShader property {} {:X} to {}", value->key, value->data.u, node->name);
#endif
		}

		RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty);
		if (lightingShader)
		{
			RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
			switch (value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		{ auto* em = static_cast<RE::BSEffectShaderMaterial*>(lightingShader->material); UnpackValue(&em->baseColor, value); } return;	break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	{ auto* em = static_cast<RE::BSEffectShaderMaterial*>(lightingShader->material); UnpackValue(&em->baseColorScale, value); } return;	break;
			case OverrideVariant::kParam_ShaderAlpha:				UnpackValue(&material->materialAlpha, value);					return;	break;
			case OverrideVariant::kParam_ShaderGlossiness:			UnpackValue(&material->specularPower, value);				return;	break;
			case OverrideVariant::kParam_ShaderSpecularStrength:	UnpackValue(&material->specularColorScale, value);		return;	break;
			case OverrideVariant::kParam_ShaderLightingEffect1:		UnpackValue(&material->subSurfaceLightRolloff, value);			return;	break;
			case OverrideVariant::kParam_ShaderLightingEffect2:		UnpackValue(&material->rimLightPower, value);			return;	break;

				// Special cases
			case OverrideVariant::kParam_ShaderTexture:
				{
					SKEEFixedString texture;
					UnpackValue(&texture, value);

					if (value->index >= 0 && value->index < RE::BSTextureSet::Texture::kTotal) {
						if (immediate) {
							NIOVTaskUpdateTexture(RE::NiPointer<RE::BSGeometry>(geometry), static_cast<std::uint32_t>(value->index), g_stringTable.GetString(texture)).Run();
						}
						else {
							SKEE_AddTask(g_task, new NIOVTaskUpdateTexture(RE::NiPointer<RE::BSGeometry>(geometry), static_cast<std::uint32_t>(value->index), g_stringTable.GetString(texture)));
						}

					}
					return;
				}
				break;
			case OverrideVariant::kParam_ShaderTextureSet:
				{
					RE::BGSTextureSet* textureSet = nullptr;
					UnpackValue(&textureSet, value);
					if (textureSet)
					{
						if (immediate)
						{
							auto* newTextureSetRaw = RE::BSShaderTextureSet::Create();
							for (std::uint32_t i = 0; i < RE::BSTextureSet::Texture::kTotal; i++)
							{
								const char* texturePath = textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i));
								newTextureSetRaw->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), texturePath);
							}
							material->ClearTextures();
							material->SetTextureSet(RE::NiPointer<RE::BSTextureSet>(newTextureSetRaw));
							SKEE::InvalidateTextures(lightingShader, 0);
							SKEE::InitializeShader(lightingShader, geometry);
						}
						else
							SKEE::SetNiGeometryTexture(RE::TaskQueueInterface::GetSingleton(), geometry, textureSet);
					}
					return;
				}
				break;
			case OverrideVariant::kParam_ShaderTintColor:
				{
					// Convert the shaderType to support tints
					if (material->GetFeature() != RE::BSShaderMaterial::Feature::kFaceGenRGBTint && material->GetFeature() != RE::BSShaderMaterial::Feature::kHairTint)
					{
						auto* tintedMaterial = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(RE::malloc(sizeof(RE::BSLightingShaderMaterialFacegenTint)));
						new (tintedMaterial) RE::BSLightingShaderMaterialFacegenTint();
						SKEE::CopyFrom(static_cast<RE::BSLightingShaderMaterial*>(static_cast<RE::BSLightingShaderMaterialBase*>(tintedMaterial)), material);
						lightingShader->SetFlags(static_cast<RE::BSShaderProperty::EShaderPropertyFlag8>(0x0A), false);
						lightingShader->SetFlags(static_cast<RE::BSShaderProperty::EShaderPropertyFlag8>(0x15), true);
						lightingShader->SetMaterial(tintedMaterial, 1);
						SKEE::InitializeShader(lightingShader, geometry);
						tintedMaterial->~BSLightingShaderMaterialFacegenTint();
						RE::free(tintedMaterial);
					}

					material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
					if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint || material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint) {
						RE::BSLightingShaderMaterialFacegenTint* tintedMaterial = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(static_cast<RE::BSLightingShaderMaterialBase*>(material));
						UnpackValue(&tintedMaterial->tintColor, value);
					}
					return;
				}
				break;
			default:
				SKSE::log::error("Unknown lighting shader key {} {}", value->key, node->name);
				return;
				break;
			}
#ifdef _DEBUG
			SKSE::log::debug("Applied LightingShader property {} {:X} to {}", value->key, value->data.u, node->name);
#endif
		}
	}
	else {
		SKSE::log::error("Failed to cast {} to geometry", node->name);
	}
}

SKEEFixedString GetSanitizedPath(const SKEEFixedString& path)
{
	std::string fullPath = path.AsString();

	fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\"); // Replace multiple slashes or forward slashes with one backslash
	fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), ""); // Remove all backslashes from the front
	fullPath = std::regex_replace(fullPath, std::regex(R"(.*?[^\s]textures\\|^textures\\)", std::regex_constants::icase), ""); // Remove everything before and including the textures path root

	return fullPath;
}

RE::NiPointer<RE::NiSourceTexture>* GetTextureFromIndex(RE::BSLightingShaderMaterial* material, std::uint32_t index)
{
	switch (index)
	{
	case 0:
		return &static_cast<RE::BSLightingShaderMaterialBase*>(material)->diffuseTexture;
	case 1:
		return &static_cast<RE::BSLightingShaderMaterialBase*>(material)->normalTexture;
	case 2:
	{
		if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen)
		{
			return &static_cast<RE::BSLightingShaderMaterialFacegen*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->subsurfaceTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kGlowMap)
		{
			return &static_cast<RE::BSLightingShaderMaterialGlowmap*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->glowTexture;
		}
		else
		{
			return &static_cast<RE::BSLightingShaderMaterialBase*>(material)->rimSoftLightingTexture;
		}
	}
	case 3:
	{
		if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen)
		{
			return &static_cast<RE::BSLightingShaderMaterialFacegen*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->detailTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kParallax)
		{
			return &static_cast<RE::BSLightingShaderMaterialParallax*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->heightTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kParallaxOcc)
		{
			return &static_cast<RE::BSLightingShaderMaterialParallaxOcc*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->heightTexture;
		}
	}
	case 4:
	{
		if (material->GetFeature() == RE::BSShaderMaterial::Feature::kEye)
		{
			return &static_cast<RE::BSLightingShaderMaterialEye*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kEnvironmentMap)
		{
			return &static_cast<RE::BSLightingShaderMaterialEnvmap*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kMultilayerParallax)
		{
			return &static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envTexture;
		}
	}
	case 5:
	{
		if (material->GetFeature() == RE::BSShaderMaterial::Feature::kEye)
		{
			return &static_cast<RE::BSLightingShaderMaterialEye*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envMaskTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kEnvironmentMap)
		{
			return &static_cast<RE::BSLightingShaderMaterialEnvmap*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kMultilayerParallax)
		{
			return &static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->envMaskTexture;
		}
	}
	case 6:
	{
		if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen)
		{
			return &static_cast<RE::BSLightingShaderMaterialFacegen*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->tintTexture;
		}
		else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kMultilayerParallax)
		{
			return &static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(static_cast<RE::BSLightingShaderMaterialBase*>(material))->layerTexture;
		}
	}
	case 7:
		return &static_cast<RE::BSLightingShaderMaterialBase*>(material)->specularBackLightingTexture;
	}

	return nullptr;
}

void DumpNodeChildren(RE::NiAVObject* node)
{
	SKSE::log::info("{} {} {:x}", std::string(node->GetRTTI()->name), std::string(node->name.c_str()), (std::uintptr_t)node);
	if (node->extraDataSize > 0) {
		for (std::uint16_t i = 0; i < node->extraDataSize; i++) {
			RE::NiExtraData* extraData = node->extra[i];
			if (extraData) {
				SKSE::log::info("{} {} {:x}", std::string(extraData->GetRTTI()->name), std::string(extraData->name.c_str()), (std::uintptr_t)extraData);
			}
		}
	}

	RE::NiNode* niNode = node ? node->AsNode() : nullptr;
	if (niNode && niNode->children.size() > 0)
	{
		for (int i = 0; i < niNode->children.size(); i++)
		{
			RE::NiAVObject* object = niNode->children[i].get();
			if (object) {
				RE::NiNode* childNode = object ? object->AsNode() : nullptr;
				RE::BSGeometry* geometry = object ? object->AsGeometry() : nullptr;
				if (geometry) {
					SKSE::log::info("{} {} {:x} - Geometry", std::string(object->GetRTTI()->name), std::string(object->name.c_str()), (std::uintptr_t)object);
					auto* shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get());
					if (shaderProperty) {
						RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProperty);
						if (lightingShader) {
							RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(lightingShader->material);

							for (int i = 0; i < RE::BSTextureSet::Texture::kTotal; ++i)
							{
								const char* texturePath = material->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i));
								if (!texturePath) {
									continue;
								}

								const char* textureName = "";
								auto texture = GetTextureFromIndex(material, i);
								if (texture) {
									textureName = (*texture)->name.c_str();
								}

								SKSE::log::info("Texture {} - {} ({})", i, texturePath, textureName);
							}
						}
					}
				}
				else if (childNode) {
					DumpNodeChildren(childNode);
				}
				else {
					SKSE::log::info("{} {} {:x}", std::string(object->GetRTTI()->name), std::string(object->name.c_str()), (std::uintptr_t)object);
				}
			}
		}
	}
}

void NIOVTaskUpdateWorldData::Run()
{
	RE::NiUpdateData ctx{};
	m_object->UpdateWorldData(&ctx);
}

void NIOVTaskMoveNode::Run()
{
	auto* currentParent = m_object->parent;
	if (currentParent)
		currentParent->DetachChild(m_object.get());
	if (m_destination)
		m_destination->AttachChild(m_object.get(), true);
}