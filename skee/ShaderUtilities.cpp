#include "ShaderUtilities.h"
#include "OverrideVariant.h"
#include "NifUtils.h"

#include "skse64/PluginAPI.h"

#include "skse64/GameThreads.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameExtraData.h"

#include "skse64/NiProperties.h"
#include "skse64/NiMaterial.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiControllers.h"
#include "skse64/NiExtraData.h"

extern SKSETaskInterface				* g_task;

void GetShaderProperty(NiAVObject * node, OverrideVariant * value)
{
	bool shaderError = false;
	BSGeometry * geometry = node->GetAsBSGeometry();
	if(geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if(!shaderProperty) {		
			_MESSAGE("Shader does not exist for %s", node->m_name);
			shaderError = true;
			return;
		}
		if(value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
		{
			SInt8 currentIndex = 0;
			SInt8 controllerIndex = value->index;
			if(controllerIndex != -1)
			{
				NiTimeController * foundController = NULL;
				NiTimeController * controller = ni_cast(shaderProperty->m_controller, NiTimeController);
				while(controller)
				{
					if(currentIndex == controllerIndex) {
						foundController = controller;
						break;
					}

					controller = ni_cast(controller->next, NiTimeController);
					currentIndex++;
				}

				if(foundController)
				{
					switch(value->key)
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

			return; // Only working on controller properties
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
			case OverrideVariant::kParam_ShaderLightingEffect1:	PackValue<float>(value, value->key, value->index, &material->lightingEffect1);			break;
			case OverrideVariant::kParam_ShaderLightingEffect2:	PackValue<float>(value, value->key, value->index, &material->lightingEffect2);			break;

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

NIOVTaskUpdateTexture::NIOVTaskUpdateTexture(BSGeometry * geometry, UInt32 index, StringTableItem texture)
{
	m_geometry = geometry;
	if (m_geometry)
		m_geometry->IncRef();

	m_index = index;
	m_texture = texture;
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
			}
		}
	}
}

void NIOVTaskUpdateTexture::Dispose()
{
	if (m_geometry)
		m_geometry->DecRef();
	delete this;
}

void SetShaderProperty(NiAVObject * node, OverrideVariant * value, bool immediate)
{
	BSGeometry * geometry = node->GetAsBSGeometry();
	if(geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if(!shaderProperty) {
			_MESSAGE("Shader does not exist for %s", geometry->m_name);
			return;
		}

		if(value->key >= OverrideVariant::kParam_ControllersStart && value->key <= OverrideVariant::kParam_ControllersEnd)
		{
			SInt8 currentIndex = 0;
			SInt8 controllerIndex = value->index;
			if(controllerIndex != -1)
			{
				NiTimeController * foundController = NULL;
				NiTimeController * controller = ni_cast(shaderProperty->m_controller, NiTimeController);
				while(controller)
				{
					if(currentIndex == controllerIndex) {
						foundController = controller;
						break;
					}

					controller = ni_cast(controller->next, NiTimeController);
					currentIndex++;
				}

				if(foundController)
				{
					switch(value->key)
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
							if(fValue < 0.0)
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

					if(immediate)
					{
						if(value->index >= 0 && value->index < BSTextureSet::kNumTextures) {
							BSShaderTextureSet * newTextureSet = BSShaderTextureSet::Create();
							for(UInt32 i = 0; i < BSTextureSet::kNumTextures; i++)
							{
								const char * texturePath = material->textureSet->GetTexturePath(i);
								newTextureSet->SetTexturePath(i, texturePath);
							}
							newTextureSet->SetTexturePath(value->index, texture.AsBSFixedString().c_str());
							material->ReleaseTextures();
							material->SetTextureSet(newTextureSet);
							CALL_MEMBER_FN(lightingShader, InvalidateTextures)(0);
							CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
						}
					} else {
						g_task->AddTask(new NIOVTaskUpdateTexture(geometry, value->index, g_stringTable.GetString(texture)));
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
						BSLightingShaderMaterialFacegenTint * tintedMaterial = (BSLightingShaderMaterialFacegenTint *)CreateShaderMaterial(BSShaderMaterial::kShaderType_HairTint);
						CALL_MEMBER_FN(tintedMaterial, CopyFrom)(material);
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x0A, false);
						CALL_MEMBER_FN(lightingShader, SetFlags)(0x15, true);
						CALL_MEMBER_FN(lightingShader, SetMaterial)(tintedMaterial, 1);
						CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
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

class MatchBySlot : public FormMatcher
{
	UInt32 m_mask;
public:
	MatchBySlot(UInt32 slot) : 
	  m_mask(slot) 
	  {

	  }

	  bool Matches(TESForm* pForm) const {
		  if (pForm) {
			  BGSBipedObjectForm* pBip = DYNAMIC_CAST(pForm, TESForm, BGSBipedObjectForm);
			  if (pBip) {
				  return (pBip->data.parts & m_mask) != 0;
			  }
		  }
		  return false;
	  }
};

TESForm* GetSkinForm(Actor* thisActor, UInt32 mask)
{
	TESForm * equipped = GetWornForm(thisActor, mask); // Check equipped item
	if(!equipped) {
		TESNPC * actorBase = DYNAMIC_CAST(thisActor->baseForm, TESForm, TESNPC);
		if(actorBase) {
			equipped = actorBase->skinForm.skin; // Check ActorBase
		}
		TESRace * race = actorBase->race.race;
		if(!equipped && race) {
			equipped = race->skin.skin; // Check Race
		}
	}

	return equipped;
}

TESForm* GetWornForm(Actor* thisActor, UInt32 mask)
{
	MatchBySlot matcher(mask);	
	ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(thisActor->extraData.GetByType(kExtraData_ContainerChanges));
	if (pContainerChanges) {
		EquipData eqD = pContainerChanges->FindEquipped(matcher);
		return eqD.pForm;
	}
	return NULL;
}

BSGeometry * GetFirstShaderType(NiAVObject * object, UInt32 shaderType)
{
	NiNode * node = object->GetAsNiNode();
	if(node)
	{
		for(UInt32 i = 0; i < node->m_children.m_emptyRunStart; i++)
		{
			NiAVObject * object = node->m_children.m_data[i];
			if(object) {
				BSGeometry * skin = GetFirstShaderType(object, shaderType);
				if(skin)
					return skin;
			}
		}
	}
	else
	{
		BSGeometry * geometry = object->GetAsBSGeometry();
		if(geometry)
		{
			BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
			if(shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
			{
				// Find first geometry if the type is any
				if(shaderType == 0xFFFF)
					return geometry;

				BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)shaderProperty->material;
				if(material && material->GetShaderType() == shaderType)
				{
					return geometry;
				}
			}
		}
	}

	return NULL;
}

void VisitGeometry(NiAVObject * parent, GeometryVisitor * visitor)
{
	NiNode * node = parent->GetAsNiNode();
	if(node)
	{
		for(UInt32 i = 0; i < node->m_children.m_emptyRunStart; i++)
		{
			NiAVObject * object = node->m_children.m_data[i];
			if(object) {
				VisitGeometry(object, visitor);
			}
		}
	}
	else
	{
		BSGeometry * geometry = parent->GetAsBSGeometry();
		if(geometry)
			visitor->Accept(geometry);
	}
}

bool NiExtraDataFinder::Accept(NiAVObject * object)
{
	m_data = object->GetExtraData(m_name.data);
	if(m_data)
		return true;

	return false;
};

NiExtraData * FindExtraData(NiAVObject * object, BSFixedString name)
{
	if (!object)
		return NULL;

	NiExtraData * extraData = NULL;
	VisitObjects(object, [&](NiAVObject*object)
	{
		extraData = object->GetExtraData(name.data);
		if (extraData)
			return true;

		return false;
	});

	return extraData;
}

void VisitArmorAddon(Actor * actor, TESObjectARMO * armor, TESObjectARMA * arma, std::function<void(bool, NiNode*,NiAVObject*)> functor)
{
	char addonString[MAX_PATH];
	memset(addonString, 0, MAX_PATH);
	arma->GetNodeName(addonString, actor, armor, -1);

	BSFixedString rootName("NPC Root [Root]");

	NiNode * skeletonRoot[2];
	skeletonRoot[0] = actor->GetNiRootNode(0);
	skeletonRoot[1] = actor->GetNiRootNode(1);

		// Skip second skeleton, it's the same as the first
	if (skeletonRoot[1] == skeletonRoot[0])
		skeletonRoot[1] = NULL;

	for (UInt32 i = 0; i <= 1; i++)
	{
		if (skeletonRoot[i])
		{
			NiAVObject * root = skeletonRoot[i]->GetObjectByName(&rootName.data);
			if (root)
			{
				NiNode * rootNode = root->GetAsNiNode();
				if (rootNode)
				{
					BSFixedString addonName(addonString); // Find the Armor name from the root
					NiAVObject * armorNode = skeletonRoot[i]->GetObjectByName(&addonName.data);
					if (functor && armorNode)
						functor(i == 1, rootNode, armorNode);
				}
#ifdef _DEBUG
				else {
					_DMESSAGE("%s - Failed to locate addon node for Armor: %08X on Actor: %08X", __FUNCTION__, armor->formID, actor->formID);
				}
#endif
			}
		}
	}
}

bool ResolveAnyHandle(SKSESerializationInterface * intfc, UInt64 handle, UInt64 * newHandle)
{
	if (((handle & 0xFF000000) >> 24) != 0xFF) {
		// Skip if handle is no longer valid.
		if (!intfc->ResolveHandle(handle, newHandle)) {
			return false;
		}
	}
	else { // This will resolve game-created forms
		TESForm * formCheck = LookupFormByID(handle & 0xFFFFFFFF);
		if (!formCheck) {
			return false;
		}
		TESObjectREFR * refr = DYNAMIC_CAST(formCheck, TESForm, TESObjectREFR);
		if (!refr || (refr && (refr->flags & TESForm::kFlagIsDeleted) == TESForm::kFlagIsDeleted)) {
			return false;
		}
		*newHandle = handle;
	}

	return true;
}

#ifdef _DEBUG
void DumpNodeChildren(NiAVObject * node)
{
	_MESSAGE("{%s} {%s} {%X}", node->GetRTTI()->name, node->m_name, node);
	if (node->m_extraDataLen > 0) {
		gLog.Indent();
		for (UInt16 i = 0; i < node->m_extraDataLen; i++) {
			_MESSAGE("{%s} {%s} {%X}", node->m_extraData[i]->GetRTTI()->name, node->m_extraData[i]->m_pcName, node);
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
					/*NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
					if (geometryData)
						_MESSAGE("{%s} {%s} {%X} {%s} {%X}", object->GetRTTI()->name, object->m_name, object, geometryData->GetRTTI()->name, geometryData);
					else*/
						_MESSAGE("{%s} {%s} {%X}", object->GetRTTI()->name, object->m_name, object);
				}
				else if (childNode) {
					DumpNodeChildren(childNode);
				}
				else {
					_MESSAGE("{%s} {%s} {%X}", object->GetRTTI()->name, object->m_name, object);
				}
			}
		}
		gLog.Outdent();
	}
}
#endif