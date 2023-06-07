#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"

#include "skse64/GameAPI.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameData.h"
#include "skse64/GameReferences.h"
#include "skse64/GameFormComponents.h"
#include "skse64/GameStreams.h"

#include "skse64/NiMaterial.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiNodes.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiSerialization.h"

#include "ActorUpdateManager.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverrideVariant.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"
#include "Utilities.h"

#include <unordered_set>

extern ActorUpdateManager				g_actorUpdateManager;
extern OverlayInterface					g_overlayInterface;
extern OverrideInterface				g_overrideInterface;
extern BodyMorphInterface				g_bodyMorphInterface;

extern SKSETaskInterface				* g_task;

extern bool		g_enableFaceOverlays;
extern bool		g_enableOverlays;
extern bool		g_playerOnly;

extern UInt32	g_numBodyOverlays;
extern UInt32	g_numHandOverlays;
extern UInt32	g_numFeetOverlays;
extern UInt32	g_numFaceOverlays;
extern UInt32	g_numSpellBodyOverlays;
extern UInt32	g_numSpellHandOverlays;
extern UInt32	g_numSpellFeetOverlays;
extern UInt32	g_numSpellFaceOverlays;

extern bool		g_overlayAlphaOverride;
extern UInt16	g_overlayAlphaFlags;
extern UInt16	g_overlayAlphaThreshold;
extern bool		g_overlayForceDecal;

extern bool		g_immediateArmor;


extern std::unordered_set<void*> g_adjustedBlocks;

skee_u32 OverlayInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverlayInterface::UninstallOverlay(const char * nodeName, TESObjectREFR * refr, NiNode * parent)
{
	if(!parent)
		return;

	// Remove all overlay instances
	BSFixedString overlayName(nodeName);
	NiAVObject* foundObject = parent->GetObjectByName(&overlayName.data);
	while(foundObject)
	{
		BSGeometry* foundGeometry = foundObject->GetAsBSGeometry();
		if(foundGeometry)
		{
			foundGeometry->m_spSkinInstance = nullptr;
		}

		if (foundObject->m_parent)
			foundObject->m_parent->RemoveChild(foundObject);
		
		foundObject = parent->GetObjectByName(&overlayName.data);
	}
}

void OverlayInterface::InstallOverlay(const char * nodeName, const char * path, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet)
{
	NiNode * rootNode = NULL;
	NiAVObjectPtr newShape;
	NiPropertyPtr alphaProperty;
	NiPropertyPtr shaderProperty;
	BSFixedString overlayName(nodeName);
	NiAVObject * foundGeometry = destination->GetObjectByName(&overlayName.data);
	if (foundGeometry)
		newShape = foundGeometry->GetAsBSGeometry();

	bool attachNew = false;
	if(!newShape)
	{
		BSResourceNiBinaryStream binaryStream(path);
		if(!binaryStream.IsValid()) {
			return;
		}

		NifStreamWrapper niStream;
		if (!niStream.LoadStream(&binaryStream)) {
			return;
		}

		niStream.VisitObjects([&](NiObject* root) {
			if (NiNode* node = root->GetAsNiNode()) {
				VisitObjects(node, [&](NiAVObject* object) {
					if (NiGeometry* geometry = object->GetAsNiGeometry()) {
						newShape = geometry;
						attachNew = true;
						return true;
					}
					return false;
				});
			} else if (NiGeometry* geometry = root->GetAsNiGeometry()) {
				newShape = geometry;
				attachNew = true;
				return true;
			}
			
			return false;
		});

		NiGeometry* legacyGeometry = newShape->GetAsNiGeometry();
		if (legacyGeometry) {
			shaderProperty = legacyGeometry->m_spEffectState;
			alphaProperty = legacyGeometry->m_spPropertyState;
		}

		if (source->shapeType == BSGeometry::kShapeType_Dynamic)
		{
			newShape = CreateBSDynamicTriShape();
		}
		else
		{
			newShape = CreateBSTriShape();
		}

		if (newShape) {
			newShape->m_name = overlayName.data;
		}
	}
	

	BSGeometry * targetShape = newShape->GetAsBSGeometry();
	if(targetShape)
	{
		targetShape->vertexDesc = source->vertexDesc;

		if (shaderProperty)
			targetShape->m_spEffectState = shaderProperty;
		if (alphaProperty)
			targetShape->m_spPropertyState = alphaProperty;

		if (targetShape->shapeType == BSGeometry::kShapeType_Dynamic)
		{
			BSDynamicTriShape * sourceShape = static_cast<BSDynamicTriShape*>(source);
			BSDynamicTriShape * newDynShape = static_cast<BSDynamicTriShape*>(targetShape);

			newDynShape->dataSize = sourceShape->dataSize;
			newDynShape->frameCount = sourceShape->frameCount;
			if (g_enableFaceOverlays && g_adjustedBlocks.find(sourceShape->pDynamicData) != g_adjustedBlocks.end())
			{
				void * ptr = reinterpret_cast<void*>((uintptr_t)sourceShape->pDynamicData - 0x10);
				InterlockedIncrement((uintptr_t*)ptr);
				newDynShape->pDynamicData = sourceShape->pDynamicData;
			}
			else
			{
				newDynShape->pDynamicData = NiAllocate(sourceShape->dataSize);
				memcpy(newDynShape->pDynamicData, sourceShape->pDynamicData, sourceShape->dataSize);
			}
			
			newDynShape->unk178 = sourceShape->unk178;
			newDynShape->unk17C = 0;
		}

		targetShape->m_localTransform = source->m_localTransform;
		targetShape->m_spSkinInstance = source->m_spSkinInstance;

		NiProperty * newProperty = niptr_cast<NiProperty>(targetShape->m_spEffectState);
		NiProperty * sourceProperty = niptr_cast<NiProperty>(source->m_spEffectState);

		BSLightingShaderProperty * shaderProperty = ni_cast(newProperty, BSLightingShaderProperty);
		BSLightingShaderProperty * sourceShader = ni_cast(sourceProperty, BSLightingShaderProperty);
		if(sourceShader && shaderProperty) {
			if((sourceShader->shaderFlags2 & BSLightingShaderProperty::kSLSF2_Vertex_Colors) == BSLightingShaderProperty::kSLSF2_Vertex_Colors)
				shaderProperty->shaderFlags2 |= BSLightingShaderProperty::kSLSF2_Vertex_Colors;
			else
				shaderProperty->shaderFlags2 &= ~BSLightingShaderProperty::kSLSF2_Vertex_Colors;

			if ((sourceShader->shaderFlags1 & BSLightingShaderProperty::kSLSF1_Model_Space_Normals) == BSLightingShaderProperty::kSLSF1_Model_Space_Normals)
				shaderProperty->shaderFlags1 |= BSLightingShaderProperty::kSLSF1_Model_Space_Normals;
			else
				shaderProperty->shaderFlags1 &= ~BSLightingShaderProperty::kSLSF1_Model_Space_Normals;

			if (g_overlayForceDecal && (shaderProperty->shaderFlags1 & BSLightingShaderProperty::kSLSF1_Decal) == 0) {
				shaderProperty->shaderFlags1 |= BSLightingShaderProperty::kSLSF1_Decal;
			}

			if(ni_is_type(sourceShader->GetRTTI(), BSLightingShaderProperty) && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
			{
				BSLightingShaderMaterial * sourceMaterial = (BSLightingShaderMaterial *)sourceShader->material;
				BSLightingShaderMaterial * targetMaterial = (BSLightingShaderMaterial *)shaderProperty->material;

				if(sourceMaterial && targetMaterial)
				{
					// Copy the remaining textures
					if(!textureSet)
					{
						for(UInt32 i = 1; i < BSTextureSet::kNumTextures; i++)
						{
							const char * texturePath = sourceMaterial->textureSet->GetTexturePath(i);
							targetMaterial->textureSet->SetTexturePath(i, texturePath);
						}
					}
					else
					{
						for (UInt32 i = 1; i < BSTextureSet::kNumTextures; i++)
						{
							const char* texturePath = textureSet->textureSet.GetTexturePath(i);
							targetMaterial->textureSet->SetTexturePath(i, texturePath);
						}
					}
					
					targetMaterial->ReleaseTextures();
					CALL_MEMBER_FN(shaderProperty, InvalidateTextures)(0);
					CALL_MEMBER_FN(shaderProperty, InitializeShader)(targetShape);
				}
			}
		}

		if(g_overlayAlphaOverride) {
			NiAlphaProperty * alphaProperty = niptr_cast<NiAlphaProperty>(targetShape->m_spPropertyState);
			if(alphaProperty) {
				alphaProperty->alphaFlags = g_overlayAlphaFlags;
				alphaProperty->alphaThreshold = g_overlayAlphaThreshold;
			}
		}

		g_overrideInterface.Impl_ApplyNodeOverrides(refr, newShape, true);

		m_callbacks.Lock();
		for (auto cb : m_callbacks.m_data) {
			cb.second(refr, newShape);
		}
		m_callbacks.Release();

		if(attachNew) {
			destination->AttachChild(newShape, false);
#ifdef _DEBUG
			_DMESSAGE("%s - Successfully installed overlay %s to actor: %08X", __FUNCTION__, newShape->m_name, refr->formID);
#endif
		}
	}
}

void OverlayInterface::ResetOverlay(const char * nodeName, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet, bool resetDiffuse)
{
	NiNode* rootNode = nullptr;
	BSGeometry* foundGeometry = nullptr;

	BSFixedString overlayName(nodeName);
	NiAVObject* foundNode = destination->GetObjectByName(&overlayName.data);
	if(foundNode)
		foundGeometry = foundNode->GetAsBSGeometry();

	if(foundGeometry)
	{
		BSLightingShaderProperty* shaderProperty = ni_cast(foundGeometry->m_spEffectState, BSLightingShaderProperty);
		BSLightingShaderProperty* sourceShader = ni_cast(source->m_spEffectState, BSLightingShaderProperty);
		if(sourceShader && shaderProperty)
		{
			if(ni_is_type(sourceShader->GetRTTI(), BSLightingShaderProperty) && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
			{
				BSLightingShaderMaterial* sourceMaterial = (BSLightingShaderMaterial *)sourceShader->material;
				BSLightingShaderMaterial* targetMaterial = (BSLightingShaderMaterial *)shaderProperty->material;

				if(sourceMaterial && targetMaterial)
				{
					/*NiColor color;
					color.r = 0;
					color.g = 0;
					color.b = 0;
					OverrideVariant defaultValue;
					defaultValue.SetColor(OverrideVariant::kParam_ShaderEmissiveColor, -1, color);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderEmissiveMultiple, -1, 1.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderAlpha, -1, 1.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderGlossiness, -1, 30.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderSpecularStrength, -1, 3.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderLightingEffect1, -1, 0.4);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderLightingEffect2, -1, 2.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);*/

					// Copy the remaining textures
					if(!textureSet)
					{
						if (resetDiffuse)
							targetMaterial->textureSet->SetTexturePath(0, BSFixedString(GetDefaultTexture().c_str()));

						for(UInt32 i = 1; i < BSTextureSet::kNumTextures; i++)
						{
							const char * texturePath = sourceMaterial->textureSet->GetTexturePath(i);
							targetMaterial->textureSet->SetTexturePath(i, texturePath);
						}
					}
					else
					{
						if (resetDiffuse)
							targetMaterial->textureSet->SetTexturePath(0, BSFixedString(GetDefaultTexture().c_str()));

						for(UInt32 i = 1; i < BSTextureSet::kNumTextures; i++)
							targetMaterial->textureSet->SetTexturePath(i, textureSet->textureSet.GetTexturePath(i));
					}
					targetMaterial->ReleaseTextures();
					CALL_MEMBER_FN(shaderProperty, InvalidateTextures)(0);
					CALL_MEMBER_FN(shaderProperty, InitializeShader)(foundGeometry);
				}
			}
		}
	}
}

void OverlayInterface::RelinkOverlay(const char * nodeName, TESObjectREFR * refr, BSGeometry * source, NiNode * skeleton)
{
	NiNode* rootNode = nullptr;
	BSGeometry* foundGeometry = nullptr;
	BSFixedString overlayName(nodeName);
	NiAVObject* foundNode = skeleton->GetObjectByName(&overlayName.data);
	if (foundNode)
		foundGeometry = foundNode->GetAsBSGeometry();

	if (source && foundGeometry)
	{
		foundGeometry->vertexDesc = source->vertexDesc;
		auto sourceSkin = source->m_spSkinInstance;
		if (sourceSkin) {
			foundGeometry->m_spSkinInstance = sourceSkin->Clone();
		}
	}
}

TESObjectARMA * GetArmorAddonByMask(TESRace * race, TESObjectARMO * armor, UInt32 mask)
{
	TESObjectARMA* currentAddon = nullptr;
	for(UInt32 i = 0; i < armor->armorAddons.count; i++)
	{
		armor->armorAddons.GetNthItem(i, currentAddon);
		if(currentAddon->isValidRace(race) && (currentAddon->biped.GetSlotMask() & mask) == mask) {
			return currentAddon;
		}
	}

	return nullptr;
}

SKSETaskRevertOverlay::SKSETaskRevertOverlay(TESObjectREFR * refr, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_armorMask = armorMask;
	m_addonMask = addonMask;
	m_resetDiffuse = resetDiffuse;
}

void SKSETaskRevertOverlay::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
			TESForm * form = GetSkinForm(actor, m_armorMask);
			if(TESObjectARMO * armor = DYNAMIC_CAST(form, TESForm, TESObjectARMO))
			{
				TESObjectARMA * foundAddon = GetArmorAddonByMask(actor->race, armor, m_addonMask);
				if(foundAddon)
				{
					VisitArmorAddon(actor, armor, foundAddon, [&](bool isFP, NiNode * rootNode, NiAVObject * armorNode)
					{
						BSGeometry * firstSkin = GetFirstShaderType(armorNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
						if (firstSkin)
						{
							g_overlayInterface.ResetOverlay(m_nodeName.data, actor, firstSkin, rootNode, NULL, m_resetDiffuse);
						}
					});
				}
			}
		}
	}
}

void SKSETaskRevertOverlay::Dispose()
{
	delete this;
}


SKSETaskRevertFaceOverlay::SKSETaskRevertFaceOverlay(TESObjectREFR * refr, BSFixedString nodeName, UInt32 partType, UInt32 shaderType, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_partType = partType;
	m_shaderType = shaderType;
	m_resetDiffuse = resetDiffuse;
}

void SKSETaskRevertFaceOverlay::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
			BSFaceGenNiNode* faceNode = reference->GetFaceGenNiNode();
			TESNPC* actorBase = DYNAMIC_CAST(reference->baseForm, TESForm, TESNPC);
			BGSHeadPart* headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			BSFixedString rootName("NPC Root [Root]");
			NiNode* skeletonRoot = actor->GetNiRootNode(0);
			BGSTextureSet* textureSet = nullptr;
			if(actorBase->headData)
				textureSet = actorBase->headData->headTexture;

			if(skeletonRoot && faceNode && headPart)
			{
				NiAVObject* root = skeletonRoot->GetObjectByName(&rootName.data);
				if(root)
				{
					NiNode* rootNode = root->GetAsNiNode();
					if(rootNode)
					{
						NiAVObject* headNode = faceNode->GetObjectByName(&headPart->partName.data);
						if(headNode)
						{
							BSGeometry* firstFace = GetFirstShaderType(headNode, m_shaderType);
							if(firstFace)
							{
								g_overlayInterface.ResetOverlay(m_nodeName.data, actor, firstFace, rootNode, textureSet, m_resetDiffuse);
							}
						}
					}
				}
			}
		}
	}
}

void SKSETaskRevertFaceOverlay::Dispose()
{
	delete this;
}

SKSETaskInstallOverlay::SKSETaskInstallOverlay(TESObjectREFR * refr, BSFixedString nodeName, BSFixedString overlayPath, UInt32 armorMask, UInt32 addonMask)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_overlayPath = overlayPath;
	m_armorMask = armorMask;
	m_addonMask = addonMask;
}

void SKSETaskInstallOverlay::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor* actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
#ifdef _DEBUG
			_DMESSAGE("%s - Installing Overlay %s from %s to actor: %08X", __FUNCTION__, m_nodeName.data, m_overlayPath.data, actor->formID);
#endif
			TESForm* form = GetSkinForm(actor, m_armorMask);
			if(TESObjectARMO* armor = DYNAMIC_CAST(form, TESForm, TESObjectARMO))
			{
#ifdef _DEBUG
				_DMESSAGE("%s - Installing Overlay to Armor: %08X on Actor: %08X", __FUNCTION__, armor->formID, actor->formID);
#endif
				TESObjectARMA* foundAddon = GetArmorAddonByMask(actor->race, armor, m_addonMask);
				if(foundAddon)
				{
					VisitArmorAddon(actor, armor, foundAddon, [&](bool isFirstPerson, NiNode* skeleton, NiAVObject* armorNode)
					{
						BSGeometry* firstSkin = GetFirstShaderType(armorNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
						if (firstSkin)
						{
							BSFixedString rootName("NPC Root [Root]");
							NiAVObject* root = skeleton->GetObjectByName(&rootName.data);
							if (NiNode* rootNode = root->GetAsNiNode()) {
#ifdef _DEBUG
								_DMESSAGE("%s - Installing Overlay %s to %08X on skeleton [%p]", __FUNCTION__, m_nodeName.data, actor->formID, (void*)skeleton);
#endif
								g_overlayInterface.InstallOverlay(m_nodeName.data, m_overlayPath.data, actor, firstSkin, rootNode);
							}
						}
					});
				}
#ifdef _DEBUG
				else {
					_DMESSAGE("%s - Failed to locate addon by mask %d for Armor: %08X on Actor: %08X", __FUNCTION__, m_addonMask, armor->formID, actor->formID);
				}
#endif
			}
		}
	}
}

void SKSETaskInstallOverlay::Dispose()
{
	delete this;
}



SKSETaskInstallFaceOverlay::SKSETaskInstallFaceOverlay(TESObjectREFR * refr, BSFixedString nodeName, BSFixedString overlayPath, UInt32 partType, UInt32 shaderType)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_overlayPath = overlayPath;
	m_partType = partType;
	m_shaderType = shaderType;
}

void SKSETaskInstallFaceOverlay::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor* actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
			BSFaceGenNiNode* faceNode = reference->GetFaceGenNiNode();
			TESNPC* actorBase = DYNAMIC_CAST(reference->baseForm, TESForm, TESNPC);
			BGSHeadPart* headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			BSFixedString rootName("NPC Root [Root]");
			NiNode* skeletonRoot = actor->GetNiRootNode(0);
			BGSTextureSet* textureSet = nullptr;
			if(actorBase->headData)
				textureSet = actorBase->headData->headTexture;

#ifdef _DEBUG
			_DMESSAGE("%s - Installing Face Overlay %s from %s to actor: %08X - Face [%p] Skeleton [%p] HeadPart [%p]", __FUNCTION__, m_nodeName.data, m_overlayPath.data, actor->formID, (void*)faceNode, (void*)skeletonRoot, (void*)headPart);
#endif
			if(skeletonRoot && faceNode && headPart)
			{
				NiAVObject* root = skeletonRoot->GetObjectByName(&rootName.data);
				if(root)
				{
					NiNode* rootNode = root->GetAsNiNode();
					if(rootNode)
					{
						// Already installed, update its visibility
						if(NiAVObject* foundOverlay = root->GetObjectByName(&m_nodeName.data))
						{
							bool hiddenFlag = (faceNode->m_flags & 0x01) == 0x01;
							if(hiddenFlag)
								foundOverlay->m_flags |= 0x01;
							else
								foundOverlay->m_flags &= ~0x01;
#ifdef _DEBUG
							_MESSAGE("%s - Toggling Face Overlay %s to %08X on skeleton", __FUNCTION__, m_nodeName.data, actor->formID);
#endif
							return;
						}

						NiAVObject* headNode = faceNode->GetObjectByName(&headPart->partName.data);
						if(headNode)
						{
							BSGeometry* firstFace = GetFirstShaderType(headNode, m_shaderType);
							if(firstFace)
							{
#ifdef _DEBUG
								_MESSAGE("%s - Installing Face Overlay %s to %08X on skeleton", __FUNCTION__, m_nodeName.data, actor->formID);
#endif
								g_overlayInterface.InstallOverlay(m_nodeName.data, m_overlayPath.data, actor, firstFace, rootNode, textureSet);
							}
						}
					}
				}
			}
		}
	}
}

void SKSETaskInstallFaceOverlay::Dispose()
{
	delete this;
}


SKSETaskModifyOverlay::SKSETaskModifyOverlay(TESObjectREFR * refr, BSFixedString nodeName)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
}


void SKSETaskModifyOverlay::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor* actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if (actor)
		{
			VisitSkeletalRoots(actor, [&](NiNode* rootNode, bool isFirstPerson)
			{
				NiAVObject* overlayNode = rootNode->GetObjectByName(&m_nodeName.data);
				if (overlayNode)
				{
#ifdef _DEBUG
					_MESSAGE("%s - Modifying Overlay %s from %08X on skeleton", __FUNCTION__, m_nodeName.data, actor->formID);
#endif
					Modify(actor, overlayNode, rootNode);
				}
			});
		}
	}
}

void SKSETaskModifyOverlay::Dispose()
{
	delete this;
}

void SKSETaskUninstallOverlay::Modify(TESObjectREFR * reference, NiAVObject * targetNode, NiNode * parent)
{
	g_overlayInterface.UninstallOverlay(targetNode->m_name, reference, parent);
}

void OverlayInterface::SetupOverlay(UInt32 primaryCount, const char * primaryPath, const char * primaryNode, UInt32 secondaryCount, const char * secondaryPath, const char * secondaryNode, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode)
{
	BSGeometry * skin = GetFirstShaderType(resultNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
	if(skin)
	{
#ifdef _DEBUG
		_MESSAGE("%s - Installing body overlay for %s on %08X", __FUNCTION__, skin->m_name, refr->formID);
#endif
		for(UInt32 i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			InstallOverlay(nodeName.c_str(), primaryPath, refr, skin, boneTree);
		}
		for(UInt32 i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			InstallOverlay(nodeName.c_str(), secondaryPath, refr, skin, boneTree);
		}
	}
	else
	{
#ifdef _DEBUG
		_MESSAGE("%s - Uninstalling body overlay on %08X", __FUNCTION__, refr->formID);
#endif
		for(UInt32 i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			UninstallOverlay(nodeName.c_str(), refr, boneTree);
		}

		for(UInt32 i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			UninstallOverlay(nodeName.c_str(), refr, boneTree);
		}
	}
}

void OverlayInterface::RelinkOverlays(UInt32 primaryCount, const char * primaryNode, UInt32 secondaryCount, const char * secondaryNode, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode)
{
	BSGeometry* skin = GetFirstShaderType(resultNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
	if (skin)
	{
		for (UInt32 i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			RelinkOverlay(nodeName.c_str(), refr, skin, boneTree);
		}
		for (UInt32 i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			RelinkOverlay(nodeName.c_str(), refr, skin, boneTree);
		}
	}
}

void OverlayInterface::BuildOverlays(UInt32 armorMask, UInt32 addonMask, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode)
{
	if ((armorMask & BGSBipedObjectForm::kPart_Body) == BGSBipedObjectForm::kPart_Body && (addonMask & BGSBipedObjectForm::kPart_Body) == BGSBipedObjectForm::kPart_Body)
	{
		SetupOverlay(g_numBodyOverlays, BODY_MESH, BODY_NODE, g_numSpellBodyOverlays, BODY_MAGIC_MESH, BODY_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & BGSBipedObjectForm::kPart_Hands) == BGSBipedObjectForm::kPart_Hands && (addonMask & BGSBipedObjectForm::kPart_Hands) == BGSBipedObjectForm::kPart_Hands)
	{
		SetupOverlay(g_numHandOverlays, HAND_MESH, HAND_NODE, g_numSpellHandOverlays, HAND_MAGIC_MESH, HAND_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & BGSBipedObjectForm::kPart_Feet) == BGSBipedObjectForm::kPart_Feet && (addonMask & BGSBipedObjectForm::kPart_Feet) == BGSBipedObjectForm::kPart_Feet)
	{
		SetupOverlay(g_numFeetOverlays, FEET_MESH, FEET_NODE, g_numSpellFeetOverlays, FEET_MAGIC_MESH, FEET_NODE_SPELL, refr, boneTree, resultNode);
	}
}

void OverlayInterface::RebuildOverlays(UInt32 armorMask, UInt32 addonMask, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode)
{
	if ((armorMask & BGSBipedObjectForm::kPart_Body) == BGSBipedObjectForm::kPart_Body && (addonMask & BGSBipedObjectForm::kPart_Body) == BGSBipedObjectForm::kPart_Body)
	{
		RelinkOverlays(g_numBodyOverlays, BODY_NODE, g_numSpellBodyOverlays, BODY_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & BGSBipedObjectForm::kPart_Hands) == BGSBipedObjectForm::kPart_Hands && (addonMask & BGSBipedObjectForm::kPart_Hands) == BGSBipedObjectForm::kPart_Hands)
	{
		RelinkOverlays(g_numHandOverlays, HAND_NODE, g_numSpellHandOverlays, HAND_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & BGSBipedObjectForm::kPart_Feet) == BGSBipedObjectForm::kPart_Feet && (addonMask & BGSBipedObjectForm::kPart_Feet) == BGSBipedObjectForm::kPart_Feet)
	{
		RelinkOverlays(g_numFeetOverlays, FEET_NODE, g_numSpellFeetOverlays, FEET_NODE_SPELL, refr, boneTree, resultNode);
	}
}

void OverlayInterface::RevertOverlay(TESObjectREFR * reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		g_task->AddTask(new SKSETaskRevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse));
	}
	else {
		SKSETaskRevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse).Run();
	}
	
}

void OverlayInterface::EraseOverlays(TESObjectREFR * reference, bool immediate)
{
	RevertOverlays(reference, true, immediate);

	SimpleLocker locker(&overlays.m_lock);
	overlays.m_data.erase(reference->formID);
}

void OverlayInterface::RevertOverlays(TESObjectREFR * reference, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		g_task->AddTask(new SKSETaskRevertOverlays(reference, resetDiffuse));
	}
	else {
		SKSETaskRevertOverlays(reference, resetDiffuse).Run();
	}
}


void OverlayInterface::RevertHeadOverlay(TESObjectREFR * reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, nodeName, partType, shaderType, resetDiffuse));
	}
	else {
		SKSETaskRevertFaceOverlay(reference, nodeName, partType, shaderType, resetDiffuse).Run();
	}
}

void OverlayInterface::RevertHeadOverlays(TESObjectREFR * reference, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	// Face
	if (!immediate)
	{
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE, i).c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE_SPELL, i).c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
		}
	}
	else
	{
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE, i).c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse).Run();
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE_SPELL, i).c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse).Run();
		}
	}
	
}

bool OverlayInterface::HasOverlays(TESObjectREFR * reference)
{
	if(reference == (*g_thePlayer)) // Always true for the player
		return true;

	SimpleLocker locker(&overlays.m_lock);
	auto it = overlays.m_data.find(reference->formID);
	if(it != overlays.m_data.end())
		return true;

	return false;
}

void OverlayInterface::RemoveOverlays(TESObjectREFR * reference, bool immediate)
{
	if(!reference || reference == (*g_thePlayer)) // Cannot remove from player
		return;

	if (!immediate) {
		g_task->AddTask(new SKSETaskRemoveOverlays(reference));
	}
	else {
		SKSETaskRemoveOverlays(reference).Run();
	}

	SimpleLocker locker(&overlays.m_lock);
	overlays.m_data.erase(reference->formID);
}

void OverlayInterface::AddOverlays(TESObjectREFR * reference, bool immediate)
{
	if (!reference || reference == (*g_thePlayer)) // Cannot add to player, already exists
		return;

	SimpleLocker locker(&overlays.m_lock);
	overlays.m_data.insert(reference->formID);

	QueueOverlayBuild(reference, immediate);
}

void OverlayInterface::QueueOverlayBuild(TESObjectREFR* reference, bool immediate)
{
	if (!reference) // Cannot add to player, already exists
		return;

	if (!immediate) {
		g_task->AddTask(new SKSETaskUpdateOverlays(reference));
	}
	else {
		SKSETaskUpdateOverlays(reference).Run();
	}
}

void OverlayInterface::Revert()
{
	SimpleLocker locker(&overlays.m_lock);
	for (auto & formId : overlays.m_data) {
		TESForm* form = LookupFormByID(formId);
		if (form && form->formType == Character::kTypeID)
		{
			RevertOverlays(static_cast<TESObjectREFR*>(form), true);
		}
	}
	overlays.m_data.clear();
}

std::string & OverlayInterface::GetDefaultTexture()
{
	return defaultTexture;
}

void OverlayInterface::SetDefaultTexture(const std::string & newTexture)
{
	defaultTexture = newTexture;
}

skee_u32 OverlayInterface::GetOverlayCount(OverlayType type, OverlayLocation location)
{
	if (type == OverlayType::Normal) {
		switch (location) {
		case OverlayLocation::Body:	return g_numBodyOverlays;
		case OverlayLocation::Hand: return g_numHandOverlays;
		case OverlayLocation::Feet: return g_numFeetOverlays;
		case OverlayLocation::Face: return g_numFaceOverlays;
		}
	} else if (type == OverlayType::Spell) {
		switch (location) {
		case OverlayLocation::Body:	return g_numSpellBodyOverlays;
		case OverlayLocation::Hand: return g_numSpellHandOverlays;
		case OverlayLocation::Feet: return g_numSpellFeetOverlays;
		case OverlayLocation::Face: return g_numSpellFaceOverlays;
		}
	}
	return 0;
}

const char* OverlayInterface::GetOverlayFormat(OverlayType type, OverlayLocation location)
{
	if (type == OverlayType::Normal) {
		switch (location) {
		case OverlayLocation::Body:	return BODY_NODE;
		case OverlayLocation::Hand: return HAND_NODE;
		case OverlayLocation::Feet: return FEET_NODE;
		case OverlayLocation::Face: return FACE_NODE;
		}
	}
	else if (type == OverlayType::Spell) {
		switch (location) {
		case OverlayLocation::Body:	return BODY_NODE_SPELL;
		case OverlayLocation::Hand: return HAND_NODE_SPELL;
		case OverlayLocation::Feet: return FEET_NODE_SPELL;
		case OverlayLocation::Face: return FACE_NODE_SPELL;
		}
	}
	return nullptr;
}

bool OverlayInterface::RegisterInstallCallback(const char* key, OverlayInstallCallback cb)
{
	SimpleLocker locker(&m_callbacks.m_lock);
	auto it = m_callbacks.m_data.emplace(key, cb);
	return it.second;
}

bool OverlayInterface::UnregisterInstallCallback(const char* key)
{
	SimpleLocker locker(&m_callbacks.m_lock);
	size_t before = m_callbacks.m_data.size();
	m_callbacks.m_data.erase(key);
	return before != m_callbacks.m_data.size();
}

void OverlayHolder::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	SimpleLocker locker(&m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		intfc->OpenRecord('AOVL', kVersion);

		UInt64 handle = (*it);
		intfc->WriteRecordData(&handle, sizeof(handle));
	}
}

bool OverlayHolder::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	bool error = false;

	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_ERROR("%s - Error loading overlay actor", __FUNCTION__);
		error = true;
		return error;
	}

	UInt64 newHandle = 0;
	// Skip if handle is no longer valid.
	if (!ResolveAnyHandle(intfc, handle, &newHandle)) {
		return error;
	}

	UInt32 formId = newHandle & 0xFFFFFFFF;

#ifdef _DEBUG
	_DMESSAGE("%s - Loading overlay for %016llX", __FUNCTION__, newHandle);
#endif

	SimpleLocker locker(&m_lock);
	m_data.insert(formId);

	g_actorUpdateManager.AddOverlayUpdate(formId);

	return error;
}

void OverlayInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	overlays.Save(intfc, kVersion);
}

bool OverlayInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
#ifdef _DEBUG
	_DMESSAGE("%s Loading Overlays...", __FUNCTION__);
#endif
	return overlays.Load(intfc, kVersion);
}

void OverlayInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	if ((refr == (*g_thePlayer) && g_playerOnly) || !g_playerOnly || HasOverlays(refr))
	{
		if (g_actorUpdateManager.isReverting() || g_actorUpdateManager.isNewGame())
		{
			g_actorUpdateManager.AddOverlayUpdate(refr->formID);
		}
		else
		{
			UInt32 armorMask = armor->bipedObject.GetSlotMask();
			UInt32 addonMask = addon->biped.GetSlotMask();
			BuildOverlays(armorMask, addonMask, refr, root, object);
		}
	}
}

void OverlayInterface::Visit(std::function<void(UInt32)> visitor)
{
	SimpleLocker locker(&overlays.m_lock);
	for(auto overlay : overlays.m_data)
	{
		visitor(overlay);
	}
}

void OverlayInterface::PrintDiagnostics()
{
	Console_Print("OverlayInterface Diagnostics:");
	SimpleLocker locker(&overlays.m_lock);
	Console_Print("\t%llu actors with overlays", overlays.m_data.size());
}

void SKSETaskUpdateOverlays::Run()
{
	if (g_enableOverlays)
	{
		TESForm* form = LookupFormByID(m_formId);
		TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
		if (reference && g_overlayInterface.HasOverlays(reference))
		{
			// Body
			for (UInt32 i = 0; i < g_numBodyOverlays; i++)
			{
				auto nodeName = std::format(BODY_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), BODY_MESH, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body).Run();
			}
			for (UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
			{
				auto nodeName = std::format(BODY_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), BODY_MAGIC_MESH, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body).Run();
			}

			// Hand
			for (UInt32 i = 0; i < g_numHandOverlays; i++)
			{
				auto nodeName = std::format(HAND_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), HAND_MESH, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands).Run();
			}
			for (UInt32 i = 0; i < g_numSpellHandOverlays; i++)
			{
				auto nodeName = std::format(HAND_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), HAND_MAGIC_MESH, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands).Run();
			}

			// Feet
			for (UInt32 i = 0; i < g_numFeetOverlays; i++)
			{
				auto nodeName = std::format(FEET_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), FEET_MESH, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet).Run();
			}
			for (UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
			{
				auto nodeName = std::format(FEET_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), FEET_MAGIC_MESH, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet).Run();
			}

			// Face
			if (g_enableFaceOverlays)
			{
				for (UInt32 i = 0; i < g_numFaceOverlays; i++)
				{
					auto nodeName = std::format(FACE_NODE, i);
					SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
					SKSETaskInstallFaceOverlay(reference, nodeName.c_str(), FACE_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen).Run();
				}
				for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
				{
					auto nodeName = std::format(FACE_NODE_SPELL, i);
					SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
					SKSETaskInstallFaceOverlay(reference, nodeName.c_str(), FACE_MAGIC_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen).Run();
				}
			}
		}
	}
}

void SKSETaskUpdateOverlays::Dispose()
{
	delete this;
}

SKSETaskUpdateOverlays::SKSETaskUpdateOverlays(TESObjectREFR* refr)
{
	m_formId = refr->formID;
}

void SKSETaskRemoveOverlays::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference)
	{
		// Body
		for (UInt32 i = 0; i < g_numBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Hand
		for (UInt32 i = 0; i < g_numHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (UInt32 i = 0; i < g_numSpellHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Feet
		for (UInt32 i = 0; i < g_numFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Face
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
	}
}

void SKSETaskRemoveOverlays::Dispose()
{
	delete this;
}

SKSETaskRemoveOverlays::SKSETaskRemoveOverlays(TESObjectREFR* refr)
{
	m_formId = refr->formID;
}

void SKSETaskRevertOverlays::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	TESObjectREFR* reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference)
	{
		for (UInt32 i = 0; i < g_numBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body, m_resetDiffuse).Run();
		}
		for (UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body, m_resetDiffuse).Run();
		}

		// Hand
		for (UInt32 i = 0; i < g_numHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands, m_resetDiffuse).Run();
		}
		for (UInt32 i = 0; i < g_numSpellHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands, m_resetDiffuse).Run();
		}

		// Feet
		for (UInt32 i = 0; i < g_numFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet, m_resetDiffuse).Run();
		}
		for (UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet, m_resetDiffuse).Run();
		}

		// Face
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE, i);
			SKSETaskRevertFaceOverlay(reference, nodeName.c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, m_resetDiffuse).Run();
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE_SPELL, i);
			SKSETaskRevertFaceOverlay(reference, nodeName.c_str(), BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, m_resetDiffuse).Run();
		}
	}
}

void SKSETaskRevertOverlays::Dispose()
{
	delete this;
}

SKSETaskRevertOverlays::SKSETaskRevertOverlays(TESObjectREFR* refr, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_resetDiffuse = resetDiffuse;
}
