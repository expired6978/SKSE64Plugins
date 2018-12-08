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

#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverrideVariant.h"
#include "ShaderUtilities.h"

extern OverlayInterface					g_overlayInterface;
extern OverrideInterface				g_overrideInterface;
extern BodyMorphInterface				g_bodyMorphInterface;

extern SKSETaskInterface				* g_task;

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

extern bool		g_alphaOverride;
extern UInt16	g_alphaFlags;
extern UInt16	g_alphaThreshold;
extern bool		g_immediateArmor;

UInt32 OverlayInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverlayInterface::UninstallOverlay(const char * nodeName, TESObjectREFR * refr, NiNode * parent)
{
	if(!parent)
		return;

	// Remove all overlay instances
	BSFixedString overlayName(nodeName);
	NiAVObject * foundObject = parent->GetObjectByName(&overlayName.data);
	while(foundObject)
	{
		BSGeometry * foundGeometry = foundObject->GetAsBSGeometry();
		if(foundGeometry)
		{
			foundGeometry->m_spSkinInstance = nullptr;
		}

		parent->RemoveChild(foundObject);
		foundObject = parent->GetObjectByName(&overlayName.data);
	}
}

void OverlayInterface::InstallOverlay(const char * nodeName, const char * path, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet)
{
	NiNode * rootNode = NULL;
	NiAVObject * newShape = NULL;
	NiPropertyPtr alphaProperty;
	NiPropertyPtr shaderProperty;

	UInt8 niStreamMemory[sizeof(NiStream)];
	memset(niStreamMemory, 0, sizeof(NiStream));
	NiStream * niStream = (NiStream *)niStreamMemory;
	CALL_MEMBER_FN(niStream, ctor)();

	BSFixedString overlayName(nodeName);
	NiAVObject * foundGeometry = destination->GetObjectByName(&overlayName.data);
	if (foundGeometry)
		newShape = foundGeometry->GetAsBSGeometry();

	bool attachNew = false;
	if(!newShape)
	{
		BSResourceNiBinaryStream binaryStream(path);
		if(!binaryStream.IsValid()) {
			goto destroy_stream;
		}

		niStream->LoadStream(&binaryStream);

		if(niStream->m_rootObjects.m_data)
		{
			if(niStream->m_rootObjects.m_data[0]) // Get the root node
				rootNode = niStream->m_rootObjects.m_data[0]->GetAsNiNode();
			if(rootNode) {
				if(rootNode->m_children.m_data) {
					if(rootNode->m_children.m_data[0]) { // Get first child of root
						newShape = rootNode->m_children.m_data[0]->GetAsNiGeometry();
						attachNew = true;
					}
				}
			}

			NiGeometry * legacyGeometry = newShape->GetAsNiGeometry();
			if (legacyGeometry) {
				shaderProperty = legacyGeometry->m_spEffectState;
				alphaProperty = legacyGeometry->m_spPropertyState;
			}

			newShape = CreateBSTriShape();
			if (newShape) {
				newShape->m_name = overlayName.data;
			}
		}
	}

	BSGeometry * targetShape = newShape->GetAsBSGeometry();
	if(targetShape)
	{
		targetShape->unk148 = source->unk148;

		if (shaderProperty)
			targetShape->m_spEffectState = shaderProperty;
		if (alphaProperty)
			targetShape->m_spPropertyState = alphaProperty;

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
						for(UInt32 i = 1; i < BSTextureSet::kNumTextures; i++)
							targetMaterial->textureSet->SetTexturePath(i, textureSet->textureSet.GetTexturePath(i));
					}
					
					targetMaterial->ReleaseTextures();
					CALL_MEMBER_FN(shaderProperty, InvalidateTextures)(0);
					CALL_MEMBER_FN(shaderProperty, InitializeShader)(targetShape);
				}				
			}
		}

		if(g_alphaOverride) {
			NiAlphaProperty * alphaProperty = niptr_cast<NiAlphaProperty>(targetShape->m_spPropertyState);
			if(alphaProperty) {
				alphaProperty->alphaFlags = g_alphaFlags;
				alphaProperty->alphaThreshold = g_alphaThreshold;
			}
		}

		g_overrideInterface.ApplyNodeOverrides(refr, newShape, true);

		if(attachNew) {
			NiNode * parent = newShape->m_parent;
			destination->AttachChild(newShape, false);
			newShape->IncRef();
			if(parent) {
				parent->RemoveChild(newShape);
				newShape->DecRef();
			}

#ifdef _DEBUG
			_DMESSAGE("%s - Successfully installed overlay %s to actor: %08X", __FUNCTION__, newShape->m_name, refr->formID);
#endif
		}
	}

destroy_stream:
	CALL_MEMBER_FN(niStream, dtor)();
}

void OverlayInterface::ResetOverlay(const char * nodeName, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet, bool resetDiffuse)
{
	NiNode * rootNode = NULL;
	BSGeometry * foundGeometry = NULL;

	BSFixedString overlayName(nodeName);
	NiAVObject * foundNode = destination->GetObjectByName(&overlayName.data);
	if(foundNode)
		foundGeometry = foundNode->GetAsBSGeometry();

	if(foundGeometry)
	{
		NiProperty * foundProperty = niptr_cast<NiProperty>(foundGeometry->m_spEffectState);
		NiProperty * sourceProperty = niptr_cast<NiProperty>(source->m_spEffectState);

		BSLightingShaderProperty * shaderProperty = ni_cast(foundProperty, BSLightingShaderProperty);
		BSLightingShaderProperty * sourceShader = ni_cast(sourceProperty, BSLightingShaderProperty);
		if(sourceShader && shaderProperty)
		{
			if(ni_is_type(sourceShader->GetRTTI(), BSLightingShaderProperty) && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
			{
				BSLightingShaderMaterial * sourceMaterial = (BSLightingShaderMaterial *)sourceShader->material;
				BSLightingShaderMaterial * targetMaterial = (BSLightingShaderMaterial *)shaderProperty->material;

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
	NiNode * rootNode = NULL;
	BSGeometry * foundGeometry = NULL;

	BSFixedString overlayName(nodeName);
	NiAVObject * foundNode = skeleton->GetObjectByName(&overlayName.data);
	if (foundNode)
		foundGeometry = foundNode->GetAsBSGeometry();

	if (source && foundGeometry)
	{
		foundGeometry->unk148 = source->unk148;
		NiSkinInstance * sourceSkin = niptr_cast<NiSkinInstance>(source->m_spSkinInstance);

		if (sourceSkin) {
			foundGeometry->m_spSkinInstance = sourceSkin->Clone();
		}
	}
}

TESObjectARMA * GetArmorAddonByMask(TESRace * race, TESObjectARMO * armor, UInt32 mask)
{
	TESObjectARMA * currentAddon = NULL;
	for(UInt32 i = 0; i < armor->armorAddons.count; i++)
	{
		armor->armorAddons.GetNthItem(i, currentAddon);
		if(currentAddon->isValidRace(race) && (currentAddon->biped.GetSlotMask() & mask) == mask) {
			return currentAddon;
		}
	}

	return NULL;
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
			BSFaceGenNiNode * faceNode = reference->GetFaceGenNiNode();
			TESNPC * actorBase = DYNAMIC_CAST(reference->baseForm, TESForm, TESNPC);
			BGSHeadPart * headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			BSFixedString rootName("NPC Root [Root]");
			NiNode * skeletonRoot = actor->GetNiRootNode(0);
			BGSTextureSet * textureSet = NULL;
			if(actorBase->headData)
				textureSet = actorBase->headData->headTexture;

			if(skeletonRoot && faceNode && headPart)
			{
				NiAVObject * root = skeletonRoot->GetObjectByName(&rootName.data);
				if(root)
				{
					NiNode * rootNode = root->GetAsNiNode();
					if(rootNode)
					{
						NiAVObject * headNode = faceNode->GetObjectByName(&headPart->partName.data);
						if(headNode)
						{
							BSGeometry * firstFace = GetFirstShaderType(headNode, m_shaderType);
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
	TESForm * form = LookupFormByID(m_formId);
	TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
#ifdef _DEBUG
			_DMESSAGE("%s - Installing Overlay %s from %s to actor: %08X", __FUNCTION__, m_nodeName.data, m_overlayPath.data, actor->formID);
#endif
			TESForm * form = GetSkinForm(actor, m_armorMask);
			if(TESObjectARMO * armor = DYNAMIC_CAST(form, TESForm, TESObjectARMO))
			{
#ifdef _DEBUG
				_DMESSAGE("%s - Installing Overlay to Armor: %08X on Actor: %08X", __FUNCTION__, armor->formID, actor->formID);
#endif
				TESObjectARMA * foundAddon = GetArmorAddonByMask(actor->race, armor, m_addonMask);
				if(foundAddon)
				{
					VisitArmorAddon(actor, armor, foundAddon, [&](bool isFirstPerson, NiNode* rootNode, NiAVObject* armorNode)
					{
						BSGeometry * firstSkin = GetFirstShaderType(armorNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
						if (firstSkin)
						{
#ifdef _DEBUG
							_DMESSAGE("%s - Installing Overlay %s to %08X on skeleton %08X", __FUNCTION__, m_nodeName.data, actor->formID, rootNode);
#endif
							g_overlayInterface.InstallOverlay(m_nodeName.data, m_overlayPath.data, actor, firstSkin, rootNode);
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
	TESForm * form = LookupFormByID(m_formId);
	TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
			BSFaceGenNiNode * faceNode = reference->GetFaceGenNiNode();
			TESNPC * actorBase = DYNAMIC_CAST(reference->baseForm, TESForm, TESNPC);
			BGSHeadPart * headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			BSFixedString rootName("NPC Root [Root]");
			NiNode * skeletonRoot = actor->GetNiRootNode(0);
			BGSTextureSet * textureSet = NULL;
			if(actorBase->headData)
				textureSet = actorBase->headData->headTexture;

#ifdef _DEBUG
			_DMESSAGE("%s - Installing Face Overlay %s from %s to actor: %08X - Face %08X Skeleton %08X HeadPart %08X", __FUNCTION__, m_nodeName.data, m_overlayPath.data, actor->formID, faceNode, skeletonRoot, headPart);
#endif
			if(skeletonRoot && faceNode && headPart)
			{
				NiAVObject * root = skeletonRoot->GetObjectByName(&rootName.data);
				if(root)
				{
					NiNode * rootNode = root->GetAsNiNode();
					if(rootNode)
					{
						// Already installed, update its visibility
						if(NiAVObject * foundOverlay = root->GetObjectByName(&m_nodeName.data))
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

						NiAVObject * headNode = faceNode->GetObjectByName(&headPart->partName.data);
						if(headNode)
						{
							BSGeometry * firstFace = GetFirstShaderType(headNode, m_shaderType);
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
	TESForm * form = LookupFormByID(m_formId);
	TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
		if(actor)
		{
			BSFixedString rootName("NPC Root [Root]");

			NiNode * skeletonRoot[2];
			skeletonRoot[0] = actor->GetNiRootNode(0);
			skeletonRoot[1] = actor->GetNiRootNode(1);

			// Skip second skeleton, it's the same as the first
			if(skeletonRoot[1] == skeletonRoot[0])
				skeletonRoot[1] = NULL;

			for(UInt32 i = 0; i <= 1; i++)
			{
				if(skeletonRoot[i])
				{
					NiAVObject * root = skeletonRoot[i]->GetObjectByName(&rootName.data);
					if(root)
					{
						NiNode * rootNode = root->GetAsNiNode();
						if(rootNode)
						{
							NiAVObject * overlayNode = rootNode->GetObjectByName(&m_nodeName.data);
							if(overlayNode)
							{
#ifdef _DEBUG
								_MESSAGE("%s - Modifying Overlay %s from %08X on skeleton %d", __FUNCTION__, m_nodeName.data, actor->formID, i);
#endif
								Modify(actor, overlayNode, rootNode);
							}
						}
					}
				}
			}
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
		char buff[MAX_PATH];
		for(UInt32 i = 0; i < primaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, primaryNode, i);
			InstallOverlay(buff, primaryPath, refr, skin, boneTree);
		}
		for(UInt32 i = 0; i < secondaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, secondaryNode, i);
			InstallOverlay(buff, secondaryPath, refr, skin, boneTree);
		}
	}
	else
	{
#ifdef _DEBUG
		_MESSAGE("%s - Uninstalling body overlay on %08X", __FUNCTION__, refr->formID);
#endif
		char buff[MAX_PATH];
		for(UInt32 i = 0; i < primaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, primaryNode, i);
			UninstallOverlay(buff, refr, boneTree);
		}

		for(UInt32 i = 0; i < secondaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, secondaryNode, i);
			UninstallOverlay(buff, refr, boneTree);
		}
	}
}

void OverlayInterface::RelinkOverlays(UInt32 primaryCount, const char * primaryNode, UInt32 secondaryCount, const char * secondaryNode, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode)
{
	BSGeometry * skin = GetFirstShaderType(resultNode, BSShaderMaterial::kShaderType_FaceGenRGBTint);
	if (skin)
	{
		char buff[MAX_PATH];
		for (UInt32 i = 0; i < primaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, primaryNode, i);
			RelinkOverlay(buff, refr, skin, boneTree);
		}
		for (UInt32 i = 0; i < secondaryCount; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, secondaryNode, i);
			RelinkOverlay(buff, refr, skin, boneTree);
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

void OverlayInterface::RevertOverlay(TESObjectREFR * reference, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask, bool resetDiffuse)
{
	if(!reference)
		return;

	g_task->AddTask(new SKSETaskRevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse));
}

void OverlayInterface::RevertOverlays(TESObjectREFR * reference, bool resetDiffuse)
{
	if(!reference)
		return;

	// Body
	char buff[MAX_PATH];
	for(UInt32 i = 0; i < g_numBodyOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, BODY_NODE, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body, resetDiffuse));
	}
	for(UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, BODY_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body, resetDiffuse));
	}

	// Hand
	for(UInt32 i = 0; i < g_numHandOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, HAND_NODE, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands, resetDiffuse));
	}
	for(UInt32 i = 0; i < g_numSpellHandOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, HAND_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands, resetDiffuse));
	}

	// Feet
	for(UInt32 i = 0; i < g_numFeetOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FEET_NODE, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet, resetDiffuse));
	}
	for(UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FEET_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskRevertOverlay(reference, buff, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet, resetDiffuse));
	}

	// Face
	for(UInt32 i = 0; i < g_numFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE, i);
		g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, buff, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
	}
	for(UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, buff, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
	}
}


void OverlayInterface::RevertHeadOverlay(TESObjectREFR * reference, BSFixedString nodeName, UInt32 partType, UInt32 shaderType, bool resetDiffuse)
{
	if(!reference)
		return;

	g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, nodeName, partType, shaderType, resetDiffuse));
}

void OverlayInterface::RevertHeadOverlays(TESObjectREFR * reference, bool resetDiffuse)
{
	if(!reference)
		return;

	// Face
	char buff[MAX_PATH];
	for(UInt32 i = 0; i < g_numFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE, i);
		g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, buff, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
	}
	for(UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskRevertFaceOverlay(reference, buff, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen, resetDiffuse));
	}
}

bool OverlayInterface::HasOverlays(TESObjectREFR * reference)
{
	if(reference == (*g_thePlayer)) // Always true for the player
		return true;

	UInt64 handle = g_overrideInterface.GetHandle(reference, reference->formType);
	auto & it = overlays.find(handle);
	if(it != overlays.end())
		return true;

	return false;
}

void OverlayInterface::RemoveOverlays(TESObjectREFR * reference)
{
	if(!reference || reference == (*g_thePlayer)) // Cannot remove from player
		return;

	// Body
	char buff[MAX_PATH];
	for(UInt32 i = 0; i < g_numBodyOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, BODY_NODE, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}
	for(UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, BODY_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}

	// Hand
	for(UInt32 i = 0; i < g_numHandOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, HAND_NODE, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}
	for(UInt32 i = 0; i < g_numSpellHandOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, HAND_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}

	// Feet
	for(UInt32 i = 0; i < g_numFeetOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FEET_NODE, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}
	for(UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FEET_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}

	// Face
	for(UInt32 i = 0; i < g_numFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}
	for(UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
	{
		memset(buff, 0, MAX_PATH);
		sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
		g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
	}

	UInt64 handle = g_overrideInterface.GetHandle(reference, TESObjectREFR::kTypeID);
	overlays.erase(handle);
}

void OverlayInterface::AddOverlays(TESObjectREFR * reference)
{
	if(!reference || reference == (*g_thePlayer)) // Cannot add to player, already exists
		return;

#ifdef _DEBUG
	_DMESSAGE("%s Installing Overlays to %08X", __FUNCTION__, reference->formID);
#endif

	UInt64 handle = g_overrideInterface.GetHandle(reference, TESObjectREFR::kTypeID);
	overlays.insert(handle);

	if (g_enableOverlays)
	{
		char buff[MAX_PATH];
		// Body
		for (UInt32 i = 0; i < g_numBodyOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, BODY_NODE, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, BODY_MESH, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body));
		}
		for (UInt32 i = 0; i < g_numSpellBodyOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, BODY_NODE_SPELL, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, BODY_MAGIC_MESH, BGSBipedObjectForm::kPart_Body, BGSBipedObjectForm::kPart_Body));
		}

		// Hand
		for (UInt32 i = 0; i < g_numHandOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, HAND_NODE, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, HAND_MESH, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands));
		}
		for (UInt32 i = 0; i < g_numSpellHandOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, HAND_NODE_SPELL, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, HAND_MAGIC_MESH, BGSBipedObjectForm::kPart_Hands, BGSBipedObjectForm::kPart_Hands));
		}

		// Feet
		for (UInt32 i = 0; i < g_numFeetOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FEET_NODE, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, FEET_MESH, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet));
		}
		for (UInt32 i = 0; i < g_numSpellFeetOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FEET_NODE_SPELL, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallOverlay(reference, buff, FEET_MAGIC_MESH, BGSBipedObjectForm::kPart_Feet, BGSBipedObjectForm::kPart_Feet));
		}

		// Face
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallFaceOverlay(reference, buff, FACE_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen));
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
			g_task->AddTask(new SKSETaskUninstallOverlay(reference, buff));
			g_task->AddTask(new SKSETaskInstallFaceOverlay(reference, buff, FACE_MAGIC_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen));
		}
	}
}

void OverlayInterface::Revert()
{
	for (auto & handle : overlays) {
		TESObjectREFR * reference = static_cast<TESObjectREFR *>(g_overrideInterface.GetObject(handle, TESObjectREFR::kTypeID));
		if (reference) {
			RevertOverlays(reference, true);
		}
	}
	overlays.clear();
}

std::string & OverlayInterface::GetDefaultTexture()
{
	return defaultTexture;
}

void OverlayInterface::SetDefaultTexture(const std::string & newTexture)
{
	defaultTexture = newTexture;
}

void OverlayHolder::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	for(OverlayHolder::iterator it = begin(); it != end(); ++it)
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

#ifdef _DEBUG
	_DMESSAGE("%s - Loading overlay for %016llX", __FUNCTION__, newHandle);
#endif

	TESObjectREFR * refr = static_cast<TESObjectREFR*>(g_overrideInterface.GetObject(newHandle, TESObjectREFR::kTypeID));
	if(refr)
		g_overlayInterface.AddOverlays(refr);

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

#ifdef _DEBUG
void OverlayInterface::DumpMap()
{
	_DMESSAGE("Dumping Overlays");
	for(OverlayHolder::iterator it = overlays.begin(); it != overlays.end(); ++it)
	{
		_DMESSAGE("Overlay for handle: %016llX", (*it));
	}
}
#endif