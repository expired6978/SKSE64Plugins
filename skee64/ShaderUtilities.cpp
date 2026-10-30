#include "ShaderUtilities.h"
#include "OverrideVariant.h"
#include "NifUtils.h"

#include "skse64/GameObjects.h"

#include "skse64/NiNodes.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiProperties.h"
#include "skse64/NiControllers.h"
#include "skse64/NiExtraData.h"

#include <regex>
#include <algorithm>

extern SKSETaskInterface				* g_task;

void GetShaderProperty(NiAVObject * node, OverrideVariant * value)
{
	bool shaderError = false;
	if (value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
	{
		SInt8 currentIndex = 0;
		SInt8 controllerIndex = value->index;
		if (controllerIndex != -1)
		{
			NiTimeController* foundController = NULL;
			NiTimeController* controller = ni_cast(node->m_controller, NiTimeController);
			while (controller)
			{
				if (currentIndex == controllerIndex) {
					foundController = controller;
					break;
				}

				controller = ni_cast(controller->next, NiTimeController);
				currentIndex++;
			}

			if (foundController)
			{
				switch (value->key)
				{
				case OverrideVariant::kParam_ControllerFrequency:	PackValue<float>(value, value->key, value->index, &foundController->m_fFrequency);	break;
				case OverrideVariant::kParam_ControllerPhase:		PackValue<float>(value, value->key, value->index, &foundController->m_fPhase);		break;
				case OverrideVariant::kParam_ControllerStartTime:	PackValue<float>(value, value->key, value->index, &foundController->m_fLoKeyTime);	break;
				case OverrideVariant::kParam_ControllerStopTime:	PackValue<float>(value, value->key, value->index, &foundController->m_fHiKeyTime);	break;

					// Special cases
				case OverrideVariant::kParam_ControllerStartStop:
				{
					float val = 0.0;
					PackValue<float>(value, value->key, value->index, &val);	break;
				}
				break;
				default:
					_MESSAGE("Unknown controller key %d %s", value->key, node->m_name);
					shaderError = true;
					break;
				}
			}
		}
		return;
	}
	
	BSGeometry * geometry = node->GetAsBSGeometry();
	if(geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if(!shaderProperty) {		
			_MESSAGE("Shader does not exist for %s", node->m_name);
			shaderError = true;
			return;
		}
		if(ni_is_type(shaderProperty->GetRTTI(), BSEffectShaderProperty))
		{
			BSEffectShaderMaterial * material = (BSEffectShaderMaterial*)shaderProperty->material;
			switch(value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		PackValue<NiColorA>(value, value->key, value->index, &material->emissiveColor);		break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	PackValue<float>(value, value->key, value->index, &material->emissiveMultiple);		break;
			default:
				_MESSAGE("Unknown shader key %d %s", value->key, node->m_name);
				break;
			}
#ifdef _DEBUG
			_MESSAGE("Applied EffectShader property %d %X to %s", value->key, value->data.u, node->m_name);
#endif
		}
		else if(ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
		{
			BSLightingShaderProperty * lightingShader = (BSLightingShaderProperty *)shaderProperty;
			BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
			switch(value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		PackValue<NiColor>(value, value->key, value->index, lightingShader->emissiveColor);		break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	PackValue<float>(value, value->key, value->index, &lightingShader->emissiveMultiple);	break;
			case OverrideVariant::kParam_ShaderAlpha:				PackValue<float>(value, value->key, value->index, &material->alpha);					break;
			case OverrideVariant::kParam_ShaderGlossiness:			PackValue<float>(value, value->key, value->index, &material->glossiness);				break;
			case OverrideVariant::kParam_ShaderSpecularStrength:	PackValue<float>(value, value->key, value->index, &material->specularStrength);			break;
			case OverrideVariant::kParam_ShaderLightingEffect1:		PackValue<float>(value, value->key, value->index, &material->lightingEffect1);			break;
			case OverrideVariant::kParam_ShaderLightingEffect2:		PackValue<float>(value, value->key, value->index, &material->lightingEffect2);			break;

				// Special cases
			case OverrideVariant::kParam_ShaderTexture:
				{
					if(value->index < BSTextureSet::kNumTextures)
					{
						BSFixedString texture = material->textureSet->GetTexturePath(value->index);
						PackValue<BSFixedString>(value, value->key, value->index, &texture);
					}
				}
				break;
			case OverrideVariant::kParam_ShaderTextureSet:
				{
					PackValue<BGSTextureSet*>(value, value->key, value->index, NULL);
				}
				break;
			case OverrideVariant::kParam_ShaderTintColor:
				{
					if(material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGenRGBTint || material->GetShaderType() == BSShaderMaterial::kShaderType_HairTint) {
						BSLightingShaderMaterialFacegenTint * tintedMaterial = (BSLightingShaderMaterialFacegenTint *)material;
						PackValue<NiColor>(value, value->key, value->index, &tintedMaterial->tintColor);
					}
				}
				break;
			default:
				_MESSAGE("Unknown lighting shader key %d %s", value->key, node->m_name);
				shaderError = true;
				break;
			}
#ifdef _DEBUG
			_MESSAGE("Applied LightingShader property %d %X to %s", value->key, value->data.u, node->m_name);
#endif
		}
	} else {
		_MESSAGE("%s - Failed to cast %s to geometry", __FUNCTION__, node->m_name);
		shaderError = true;
	}

	if(shaderError) {
		UInt32 def = 0;
		PackValue<UInt32>(value, value->key, -1, &def);
	}
}

void NIOVTaskUpdateTexture::Run()
{
	if(m_geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(m_geometry->m_spEffectState);
		if(!shaderProperty) {
			_MESSAGE("Shader does not exist for %s", m_geometry->m_name);
			return;
		}

		BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
		if(lightingShader)
		{
			BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
			if(m_index < BSTextureSet::kNumTextures) {
				// Need to update the texture path of the BSTextureSet
				BSShaderTextureSet * newTextureSet = BSShaderTextureSet::Create();
				for (UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
				{
					newTextureSet->SetTexturePath(i, material->textureSet->GetTexturePath(i));
				}
				newTextureSet->SetTexturePath(m_index, m_texture->AsBSFixedString().c_str());
				material->SetTextureSet(newTextureSet);

				// Load the texture requested and then assign it to the material
				NiPointer<NiTexture> newTexture;
				LoadTexture(m_texture->c_str(), 1, newTexture, false);

				NiTexturePtr * targetTexture = GetTextureFromIndex(material, m_index);
				if (targetTexture) {
					*targetTexture = newTexture;
				}

				CALL_MEMBER_FN(lightingShader, InitializeShader)(m_geometry);
#if 0
				BSShaderTextureSet * newTextureSet = BSShaderTextureSet::Create();
				for(UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
				{
					const char * texturePath = material->textureSet->GetTexturePath(i);
					newTextureSet->SetTexturePath(i, texturePath);
				}
				newTextureSet->SetTexturePath(m_index, m_texture->AsBSFixedString().c_str());
				material->ReleaseTextures();
				material->SetTextureSet(newTextureSet);
				CALL_MEMBER_FN(lightingShader, InvalidateTextures)(0);
				CALL_MEMBER_FN(lightingShader, InitializeShader)(m_geometry);
#endif
			}
		}
	}
}

void SetShaderProperty(NiAVObject * node, OverrideVariant * value, bool immediate)
{

	if (value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
	{
		SInt8 currentIndex = 0;
		SInt8 controllerIndex = value->index;
		if (controllerIndex != -1)
		{
			NiTimeController* foundController = NULL;
			NiTimeController* controller = ni_cast(node->m_controller, NiTimeController);
			while (controller)
			{
				if (currentIndex == controllerIndex) {
					foundController = controller;
					break;
				}

				controller = ni_cast(controller->next, NiTimeController);
				currentIndex++;
			}

			if (foundController)
			{
				switch (value->key)
				{
				case OverrideVariant::kParam_ControllerFrequency:	UnpackValue(&foundController->m_fFrequency, value);	return;	break;
				case OverrideVariant::kParam_ControllerPhase:		UnpackValue(&foundController->m_fPhase, value);		return;	break;
				case OverrideVariant::kParam_ControllerStartTime:	UnpackValue(&foundController->m_fLoKeyTime, value);	return;	break;
				case OverrideVariant::kParam_ControllerStopTime:	UnpackValue(&foundController->m_fHiKeyTime, value);	return;	break;

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
					_MESSAGE("Unknown controller key %d %s", value->key, node->m_name);
					return;
					break;
				}
			}
		}

		return; // Only working on controller properties
	}

	BSGeometry * geometry = node->GetAsBSGeometry();
	if(geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if(!shaderProperty) {
			_MESSAGE("Shader does not exist for %s", geometry->m_name);
			return;
		}

		BSEffectShaderProperty * effectShader = ni_cast(shaderProperty, BSEffectShaderProperty);
		if(effectShader)
		{
			BSEffectShaderMaterial * material = (BSEffectShaderMaterial*)shaderProperty->material;
			switch(value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		UnpackValue(&material->emissiveColor, value);		return;	break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	UnpackValue(&material->emissiveMultiple, value);	return;	break;
			default:
				_MESSAGE("Unknown shader key %d %s", value->key, node->m_name);
				return;
				break;
			}
#ifdef _DEBUG
			_MESSAGE("Applied EffectShader property %d %X to %s", value->key, value->data.u, node->m_name);
#endif
		}

		BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
		if(lightingShader)
		{
			BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)lightingShader->material;
			switch(value->key)
			{
			case OverrideVariant::kParam_ShaderEmissiveColor:		UnpackValue(lightingShader->emissiveColor, value);		return;	break;
			case OverrideVariant::kParam_ShaderEmissiveMultiple:	UnpackValue(&lightingShader->emissiveMultiple, value);	return;	break;
			case OverrideVariant::kParam_ShaderAlpha:				UnpackValue(&material->alpha, value);					return;	break;
			case OverrideVariant::kParam_ShaderGlossiness:			UnpackValue(&material->glossiness, value);				return;	break;
			case OverrideVariant::kParam_ShaderSpecularStrength:	UnpackValue(&material->specularStrength, value);		return;	break;
			case OverrideVariant::kParam_ShaderLightingEffect1:	UnpackValue(&material->lightingEffect1, value);			return;	break;
			case OverrideVariant::kParam_ShaderLightingEffect2:	UnpackValue(&material->lightingEffect2, value);			return;	break;

				// Special cases
			case OverrideVariant::kParam_ShaderTexture:
				{
					SKEEFixedString texture;
					UnpackValue(&texture, value);

					if(value->index >= 0 && value->index < BSTextureSet::kNumTextures) {
						if (immediate) {
							NIOVTaskUpdateTexture(geometry, value->index, g_stringTable.GetString(texture)).Run();
						}
						else {
							g_task->AddTask(new NIOVTaskUpdateTexture(geometry, value->index, g_stringTable.GetString(texture)));
						}

					}
					return;
				}
				break;
			case OverrideVariant::kParam_ShaderTextureSet:
				{
					BGSTextureSet * textureSet = NULL;
					UnpackValue(&textureSet, value);
					if(textureSet)
					{
						if(immediate)
						{
							BSShaderTextureSet * newTextureSet = BSShaderTextureSet::Create();
							for(UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
							{
								const char * texturePath = textureSet->textureSet.GetTexturePath(i);
								newTextureSet->SetTexturePath(i, texturePath);
							}
							material->ReleaseTextures();
							material->SetTextureSet(newTextureSet);
							CALL_MEMBER_FN(lightingShader, InvalidateTextures)(0);
							CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
						}
						else
							CALL_MEMBER_FN(BSTaskPool::GetSingleton(), SetNiGeometryTexture)(geometry, textureSet);
					}
					return;
				}
				break;
			case OverrideVariant::kParam_ShaderTintColor:
				{
					// Convert the shaderType to support tints
					if(material->GetShaderType() != BSShaderMaterial::kShaderType_FaceGenRGBTint && material->GetShaderType() != BSShaderMaterial::kShaderType_HairTint)//if(CALL_MEMBER_FN(lightingShader, HasFlags)(0x0A))
					{
						BSLightingShaderMaterialFacegenTint * tintedMaterial = CreateFacegenTintMaterial();
						CALL_MEMBER_FN(tintedMaterial, CopyFrom)(material);
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x0A, false);
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x15, true);
						CALL_MEMBER_FN(lightingShader, SetMaterial)(tintedMaterial, 1);
						CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
						tintedMaterial->~BSLightingShaderMaterialFacegenTint();
						Heap_Free(tintedMaterial);
					}

					material = (BSLightingShaderMaterial *)shaderProperty->material;
					if(material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGenRGBTint || material->GetShaderType() == BSShaderMaterial::kShaderType_HairTint) {
						BSLightingShaderMaterialFacegenTint * tintedMaterial = (BSLightingShaderMaterialFacegenTint *)material;
						UnpackValue(&tintedMaterial->tintColor, value);
					}
					return;
				}
				break;
			default:
				_ERROR("Unknown lighting shader key %d %s", value->key, node->m_name);
				return;
				break;
			}
#ifdef _DEBUG
			_DMESSAGE("Applied LightingShader property %d %X to %s", value->key, value->data.u, node->m_name);
#endif
		}
	} else {
		_ERROR("Failed to cast %s to geometry", node->m_name);
	}
}

SKEEFixedString GetSanitizedPath(const SKEEFixedString & path)
{
	std::string fullPath = path.AsString();

	fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\"); // Replace multiple slashes or forward slashes with one backslash
	fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), ""); // Remove all backslashes from the front
	fullPath = std::regex_replace(fullPath, std::regex(R"(.*?[^\s]textures\\|^textures\\)", std::regex_constants::icase), ""); // Remove everything before and including the textures path root

	return fullPath;
}

NiTexturePtr * GetTextureFromIndex(BSLightingShaderMaterial * material, UInt32 index)
{
	switch (index)
	{
	case 0:
		return &material->texture1;
		break;
	case 1:
		return &material->texture2;
		break;
	case 2:
	{
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen)
		{
			return &static_cast<BSLightingShaderMaterialFacegen*>(material)->unkB0;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_GlowMap)
		{
			return &static_cast<BSLightingShaderMaterialGlowmap*>(material)->glowMap;
		}
		else
		{
			return &material->texture3;
		}
	}
	break;
	case 3:
	{
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen)
		{
			return &static_cast<BSLightingShaderMaterialFacegen*>(material)->unkA8;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_Parallax)
		{
			return &static_cast<BSLightingShaderMaterialParallax*>(material)->unkA0;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_ParallaxOcc)
		{
			return &static_cast<BSLightingShaderMaterialParallaxOcc*>(material)->unkA0;
		}
	}
	break;
	case 4:
	{
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_Eye)
		{
			return &static_cast<BSLightingShaderMaterialEye*>(material)->unkA0;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_EnvironmentMap)
		{
			return &static_cast<BSLightingShaderMaterialEnvmap*>(material)->unkA0;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_MultilayerParallax)
		{
			return &static_cast<BSLightingShaderMaterialMultiLayerParallax*>(material)->unkA8;
		}
	}
	break;
	case 5:
	{
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_Eye)
		{
			return &static_cast<BSLightingShaderMaterialEye*>(material)->unkA8;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_EnvironmentMap)
		{
			return &static_cast<BSLightingShaderMaterialEnvmap*>(material)->unkA0;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_MultilayerParallax)
		{
			return &static_cast<BSLightingShaderMaterialMultiLayerParallax*>(material)->unkB0;
		}
	}
	break;
	case 6:
	{
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen)
		{
			return &static_cast<BSLightingShaderMaterialFacegen*>(material)->renderedTexture;
		}
		else if (material->GetShaderType() == BSShaderMaterial::kShaderType_MultilayerParallax)
		{
			return &static_cast<BSLightingShaderMaterialMultiLayerParallax*>(material)->unkA0;
		}
	}
	break;
	case 7:
		return &material->texture4;
		break;
	}

	return nullptr;
}

void DumpNodeChildren(NiAVObject * node)
{
	_MESSAGE("{%s} {%s} {%p}", node->GetRTTI()->name, node->m_name, (void*)node);
	if (node->m_extraDataLen > 0) {
		gLog.Indent();
		for (UInt16 i = 0; i < node->m_extraDataLen; i++) {
			_MESSAGE("{%s} {%s} {%p}", node->m_extraData[i]->GetRTTI()->name, node->m_extraData[i]->m_pcName, (void*)node);
		}
		gLog.Outdent();
	}

	NiNode * niNode = node->GetAsNiNode();
	if (niNode && niNode->m_children.m_emptyRunStart > 0)
	{
		gLog.Indent();
		for (int i = 0; i < niNode->m_children.m_emptyRunStart; i++)
		{
			NiAVObject * object = niNode->m_children.m_data[i];
			if (object) {
				NiNode * childNode = object->GetAsNiNode();
				BSGeometry * geometry = object->GetAsBSGeometry();
				if (geometry) {
					_MESSAGE("{%s} {%s} {%p} - Geometry", object->GetRTTI()->name, object->m_name, (void*)object);
					NiPointer<BSShaderProperty> shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
					if (shaderProperty) {
						BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
						if (lightingShader) {
							BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)lightingShader->material;

							gLog.Indent();
							for (int i = 0; i < BSTextureSet::kNumTextures; ++i)
							{
								const char * texturePath = material->textureSet->GetTexturePath(i);
								if (!texturePath) {
									continue;
								}

								const char * textureName = "";
								NiTexturePtr * texture = GetTextureFromIndex(material, i);
								if (texture) {
									textureName = texture->get()->name;
								}

								_MESSAGE("Texture %d - %s (%s)", i, texturePath, textureName);
							}
							
							gLog.Outdent();
						}
					}
				}
				else if (childNode) {
					_MESSAGE("{%s} {%s} {%p} - Node", object->GetRTTI()->name, object->m_name, (void*)object);
					DumpNodeChildren(childNode);
				}
				else {
					_MESSAGE("{%s} {%s} {%p}", object->GetRTTI()->name, object->m_name, (void*)object);
				}
			}
		}
		gLog.Outdent();
	}
}

void NIOVTaskUpdateWorldData::Run()
{
	NiAVObject::ControllerUpdateContext ctx;
	ctx.flags = 0;
	ctx.delta = 0;
	m_object->UpdateWorldData(&ctx);
}

void NIOVTaskMoveNode::Run()
{
	NiPointer<NiNode> currentParent = m_object->m_parent;
	if (currentParent)
		currentParent->RemoveChild(m_object);
	if (m_destination)
		m_destination->AttachChild(m_object, true);
}
