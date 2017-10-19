#include "NifUtils.h"

#include <unordered_map>

#include "common/IFileStream.h"

#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameData.h"
#include "skse64/GameStreams.h"

#include "skse64/NiRTTI.h"
#include "skse64/NiObjects.h"
#include "skse64/NiNodes.h"
#include "skse64/NiSerialization.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiAllocator.h"

#ifdef FIXME
#include <d3dx9.h>
#pragma comment(lib, "d3dx9.lib")

typedef NiSourceTexture * (* _NiRenderedTextureToNiSourceTexture)(NiRenderedTexture * texture, bool bRGBA);
const _NiRenderedTextureToNiSourceTexture NiRenderedTextureToNiSourceTexture = (_NiRenderedTextureToNiSourceTexture)0x00C8CD60;
UInt8 * g_bAlwaysCreateRendererData = (UInt8 *)0x012C5D48; // This is set as 1 in 0x00C6DBF0 (SetupRenderer)

void SaveSourceDDS(NiSourceTexture * pkSrcTexture, const char * pcFileName)
{
	if(!pkSrcTexture)
		return;

	LPDIRECT3DTEXTURE9 pkD3DTexture = NULL;
	D3DLOCKED_RECT pkLockedTexture;

	UInt32 uiWidth = pkSrcTexture->GetWidth();
	UInt32 uiHeight = pkSrcTexture->GetHeight();

	LPDIRECT3DDEVICE9 pkD3DDevice9 = (LPDIRECT3DDEVICE9)NiDX9Renderer::GetSingleton()->m_pkD3DDevice9;
	if (D3DXCreateTexture(pkD3DDevice9, uiWidth, uiHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pkD3DTexture) != -1)
	{
		if(!pkD3DTexture) {
			// Failed to create texture
			return;
		}

		if (pkSrcTexture)
		{
			NiPixelData * pkPixelData = pkSrcTexture->pixelData;
			if(pkPixelData)
			{
				if (pkD3DTexture->LockRect(0, &pkLockedTexture, 0, 0) != -1)
				{
					UInt8 * pucSrc = pkPixelData->GetPixels();
					UInt8 * pucDest = (UInt8 *)pkLockedTexture.pBits;

					for (UInt32 y = 0; y < uiHeight; y++)
					{
						for (UInt32 x = 0; x < uiWidth; x++)
						{
							pucDest[0] = pucSrc[0]; // A
							pucDest[1] = pucSrc[1]; // R
							pucDest[2] = pucSrc[2]; // G
							pucDest[3] = pucSrc[3]; // B

							pucDest+=4;
							pucSrc+=4;
						}
					}
				}

				pkD3DTexture->UnlockRect(0);
				D3DXSaveTextureToFile(pcFileName, D3DXIFF_DDS, pkD3DTexture, 0);
			}
		}

		pkD3DTexture->Release();
	}
}

void SaveRenderedDDS(NiRenderedTexture * pkTexture, const char * pcFileName)
{
	UInt8 oldVal = *g_bAlwaysCreateRendererData;
	*g_bAlwaysCreateRendererData = 0; // Need to set this to 0 because NiSourceTexture is inited with flag DestroyAppData = 0x04, and CreateRenderData checks this
	NiSourceTexture * pkSrcTexture = NiRenderedTextureToNiSourceTexture(pkTexture, 1);
	*g_bAlwaysCreateRendererData = oldVal;

	SaveSourceDDS(pkSrcTexture, pcFileName);
}
#endif

BSGeometry * GetHeadGeometry(Actor * actor, UInt32 partType)
{
	BSFaceGenNiNode * faceNode = actor->GetFaceGenNiNode();
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);

	if(faceNode && actorBase) {
		BGSHeadPart * facePart = actorBase->GetCurrentHeadPartByType(partType);
		if(facePart) {
			NiAVObject * headNode = faceNode->GetObjectByName(&facePart->partName.data);
			if(headNode) {
				BSGeometry * geometry = headNode->GetAsBSGeometry();
				if(geometry)
					return geometry;
			}
		}
	}

	return NULL;
}

BSGeometry * GetGeometryByHeadPart(BSFaceGenNiNode * faceNode, BGSHeadPart * headPart)
{
	for (UInt32 p = 0; p < faceNode->m_children.m_size; p++)
	{
		NiAVObject * object = faceNode->m_children.m_data[p];
		if (object && BSFixedString(object->m_name) == headPart->partName) {
			BSGeometry * geometry = object->GetAsBSGeometry();
			if (geometry) {
				return geometry;
			}
		}
	}

	return NULL;
}

SKSETaskRefreshTintMask::SKSETaskRefreshTintMask(Actor * actor, BSFixedString ddsPath) : m_ddsPath(ddsPath)
{
	m_formId = actor->formID;
}

void SKSETaskRefreshTintMask::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if (!actor)
		return;

	BSGeometry * geometry = GetHeadGeometry(actor, BGSHeadPart::kTypeFace);
	if (geometry) {
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if (shaderProperty) {
			if (ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty)) {
				BSLightingShaderProperty * lightingShader = static_cast<BSLightingShaderProperty *>(shaderProperty);
				BSLightingShaderMaterial * material = static_cast<BSLightingShaderMaterial *>(shaderProperty->material);
				if (material) {
					material->textureSet->SetTexturePath(6, m_ddsPath.data);
					material->ReleaseTextures();
					CALL_MEMBER_FN(lightingShader, InvalidateTextures)(0);
					CALL_MEMBER_FN(lightingShader, InitializeShader)(geometry);
				}
			}
		}
	}
}

void ExportTintMaskDDS(Actor * actor, BSFixedString filePath)
{
	BSGeometry * geometry = GetHeadGeometry(actor, BGSHeadPart::kTypeFace);
	if(geometry)
	{
		BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
		if(ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty))
		{
			BSLightingShaderProperty * lightingShader = static_cast<BSLightingShaderProperty *>(shaderProperty);
			BSLightingShaderMaterial * material = static_cast<BSLightingShaderMaterial *>(shaderProperty->material);
			if (material)
			{
				if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen)
				{
					BSLightingShaderMaterialFacegen * maskedMaterial = (BSLightingShaderMaterialFacegen *)material;
#ifdef FIXME
					SaveRenderedDDS(niptr_cast<NiRenderedTexture>(maskedMaterial->renderedTexture), filePath.data);
#endif
				}
			}
		}
	}
}

BGSTextureSet * GetTextureSetForPart(TESNPC * npc, BGSHeadPart * headPart)
{
	BGSTextureSet * textureSet = NULL;
	if (headPart->type == BGSHeadPart::kTypeFace) {
		if (npc->headData)
			textureSet = npc->headData->headTexture;
	}
	if (!textureSet)
		textureSet = headPart->textureSet;

	return textureSet;
}

std::pair<BGSTextureSet*, BGSHeadPart*> GetTextureSetForPartByName(TESNPC * npc, BSFixedString partName)
{
	UInt32 numHeadParts = 0;
	BGSHeadPart ** headParts = NULL;
	if (CALL_MEMBER_FN(npc, HasOverlays)()) {
		numHeadParts = GetNumActorBaseOverlays(npc);
		headParts = GetActorBaseOverlays(npc);
	}
	else {
		numHeadParts = npc->numHeadParts;
		headParts = npc->headparts;
	}
	for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all parts
	{
		BGSHeadPart * headPart = headParts[i];
		if (headPart && headPart->partName == partName) {
			BGSTextureSet * textureSet = nullptr;
			textureSet = GetTextureSetForPart(npc, headPart);
			return std::make_pair(textureSet, headPart);
		}
	}

	return std::make_pair<BGSTextureSet*, BGSHeadPart*>(NULL, NULL);
}

SKSETaskExportHead::SKSETaskExportHead(Actor * actor, BSFixedString nifPath, BSFixedString ddsPath) : m_nifPath(nifPath), m_ddsPath(ddsPath)
{
	m_formId = actor->formID;
}

void SKSETaskExportHead::Run()
{
	if (!m_formId)
		return;

	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if (!actor)
		return;

	BSFaceGenNiNode * faceNode = actor->GetFaceGenNiNode();
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (!actorBase || !faceNode)
		return;

	BSFaceGenAnimationData * animationData = actor->GetFaceGenAnimationData();
	if (animationData) {
		FaceGen::GetSingleton()->isReset = 0;
		for (UInt32 t = BSFaceGenAnimationData::kKeyframeType_Expression; t <= BSFaceGenAnimationData::kKeyframeType_Phoneme; t++)
		{
			BSFaceGenKeyframeMultiple * keyframe = &animationData->keyFrames[t];
			for (UInt32 i = 0; i < keyframe->count; i++)
				keyframe->values[i] = 0.0;
			keyframe->isUpdated = 0;
		}
		UpdateModelFace(faceNode);
	}

	IFileStream::MakeAllDirs(m_nifPath.data);

	BSFadeNode * rootNode = BSFadeNode::Create();
	rootNode->IncRef();
	NiNode * skinnedNode = NiNode::Create(0);
	skinnedNode->m_name = BSFixedString("BSFaceGenNiNodeSkinned").data;

	std::map<NiAVObject*, NiAVObject*> boneMap;

	for (UInt32 i = 0; i < faceNode->m_children.m_size; i++)
	{
		NiAVObject * object = faceNode->m_children.m_data[i];
		if (!object)
			continue;

		if (BSGeometry * geometry = object->GetAsBSGeometry()) {
#ifdef FIXME_GEOMETRY
			NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
			NiGeometryData * newGeometryData = NULL;
			if (geometryData)
				CALL_MEMBER_FN(geometryData, DeepCopy)((NiObject **)&newGeometryData);
#endif

			NiProperty * trishapeEffect = niptr_cast<NiProperty>(geometry->m_spEffectState);
			NiProperty * newTrishapeEffect = NULL;
			if (trishapeEffect)
				CALL_MEMBER_FN(trishapeEffect, DeepCopy)((NiObject **)&newTrishapeEffect);

			NiProperty * trishapeProperty = niptr_cast<NiProperty>(geometry->m_spPropertyState);
			NiProperty * newTrishapeProperty = NULL;
			if (trishapeProperty)
				CALL_MEMBER_FN(trishapeProperty, DeepCopy)((NiObject **)&newTrishapeProperty);

			NiSkinInstance * skinInstance = niptr_cast<NiSkinInstance>(geometry->m_spSkinInstance);
			NiSkinInstance * newSkinInstance = NULL;
			if (skinInstance) {
				newSkinInstance = skinInstance->Clone(false);
				newSkinInstance->m_pkRootParent = skinnedNode;

				UInt32 numBones = 0;
				NiSkinData * skinData = niptr_cast<NiSkinData>(skinInstance->m_spSkinData);
				NiSkinData * newSkinData = NULL;
				if (skinData) {
					numBones = skinData->m_uiBones;
					CALL_MEMBER_FN(skinData, DeepCopy)((NiObject **)&newSkinData);
				}

				NiSkinPartition * skinPartition = niptr_cast<NiSkinPartition>(skinInstance->m_spSkinPartition);
				NiSkinPartition * newSkinPartition = NULL;
				if (skinPartition)
					CALL_MEMBER_FN(skinPartition, DeepCopy)((NiObject **)&newSkinPartition);

				newSkinInstance->m_spSkinData = newSkinData;
				newSkinData->DecRef();

				newSkinInstance->m_spSkinPartition = newSkinPartition;
				newSkinPartition->DecRef();

				// Remap the bones to new NiNode instances
				if (numBones > 0)
				{
					newSkinInstance->m_ppkBones = (NiAVObject**)NiAllocate(numBones * sizeof(NiAVObject*));
					for (UInt32 i = 0; i < numBones; i++)
					{
						NiAVObject * bone = skinInstance->m_ppkBones[i];
						if (bone)
						{
							auto it = boneMap.find(bone);
							if (it == boneMap.end()) {
								NiNode * newBone = NiNode::Create();
								newBone->m_name = bone->m_name;
								newBone->m_flags = bone->m_flags;
								boneMap.insert(std::make_pair(bone, newBone));
								newSkinInstance->m_ppkBones[i] = newBone;
							}
							else
								newSkinInstance->m_ppkBones[i] = it->second;
						}
						else
						{
							newSkinInstance->m_ppkBones[i] = nullptr;
						}
					}
				}
			}

			BSGeometry * newGeometry = NULL;

#ifdef FIXME_GEOMETRY
			if (NiTriShape * trishape = geometry->GetAsNiTriShape()) {
				NiTriShape * newTrishape = NiTriShape::Create(static_cast<NiTriShapeData*>(newGeometryData));
				newGeometryData->DecRef();
				newTrishape->m_localTransform = geometry->m_localTransform;
				newTrishape->m_name = geometry->m_name;
#ifdef FIXME
				memcpy(&newTrishape->unk88, &geometry->unk88, 0x1F);
#endif
				newTrishape->m_spEffectState = newTrishapeEffect;
				newTrishape->m_spPropertyState = newTrishapeProperty;
				newTrishape->m_spSkinInstance = newSkinInstance;
				newGeometry = newTrishape;
			}
			else if (NiTriStrips * tristrips = geometry->GetAsNiTriStrips()) {
				NiTriStrips * newTristrips = NiTriStrips::Create(static_cast<NiTriStripsData*>(newGeometryData));
				newGeometryData->DecRef();
				newTristrips->m_localTransform = geometry->m_localTransform;
				newTristrips->m_name = geometry->m_name;
#ifdef FIXME
				memcpy(&newTristrips->unk88, &geometry->unk88, 0x1F);
#endif
				newTristrips->m_spEffectState = newTrishapeEffect;
				newTristrips->m_spPropertyState = newTrishapeProperty;
				newTristrips->m_spSkinInstance = newSkinInstance;
				newGeometry = newTristrips;
			}
#endif

			if (newGeometry)
			{
				auto textureData = GetTextureSetForPartByName(actorBase, newGeometry->m_name);
				if (textureData.first && textureData.second) {
					BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(newGeometry->m_spEffectState);
					if (shaderProperty) {
						if (ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty)) {
							BSLightingShaderProperty * lightingShader = static_cast<BSLightingShaderProperty *>(shaderProperty);
							BSLightingShaderMaterial * material = static_cast<BSLightingShaderMaterial *>(shaderProperty->material);
							if (material && material->textureSet) {
								for (UInt32 i = 0; i < BGSTextureSet::kNumTextures; i++)
									material->textureSet->SetTexturePath(i, textureData.first->textureSet.GetTexturePath(i));

								if (textureData.second->type == BGSHeadPart::kTypeFace)
									material->textureSet->SetTexturePath(6, m_ddsPath.data);
							}
						}
					}

					// Save the previous tint mask
					BSShaderProperty * originalShaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
					if (originalShaderProperty) {
						if (ni_is_type(originalShaderProperty->GetRTTI(), BSLightingShaderProperty)) {
							BSLightingShaderProperty * lightingShader = static_cast<BSLightingShaderProperty *>(originalShaderProperty);
							BSLightingShaderMaterial * material = static_cast<BSLightingShaderMaterial *>(originalShaderProperty->material);
							if (material) {
								if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen) {
									BSLightingShaderMaterialFacegen * maskedMaterial = static_cast<BSLightingShaderMaterialFacegen *>(material);
#ifdef FIXME
									SaveRenderedDDS(niptr_cast<NiRenderedTexture>(maskedMaterial->renderedTexture), m_ddsPath.data);
#endif
								}
							}
						}
					}
				}

				skinnedNode->AttachChild(newGeometry, true);
			}
		}
	}

	for (auto & bones : boneMap)
		rootNode->AttachChild(bones.second, true);

	rootNode->AttachChild(skinnedNode, true);

	UInt8 niStreamMemory[0x5B4];
	memset(niStreamMemory, 0, 0x5B4);
	NiStream * niStream = (NiStream *)niStreamMemory;
	CALL_MEMBER_FN(niStream, ctor)();
	CALL_MEMBER_FN(niStream, AddObject)(rootNode);
	niStream->SavePath(m_nifPath.data);
	CALL_MEMBER_FN(niStream, dtor)();

	rootNode->DecRef();

	if (animationData) {
		animationData->overrideFlag = 0;
		CALL_MEMBER_FN(animationData, Reset)(1.0, 1, 1, 0, 0);
		FaceGen::GetSingleton()->isReset = 1;
		UpdateModelFace(faceNode);
	}
}

bool VisitObjects(NiAVObject * parent, std::function<bool(NiAVObject*)> functor)
{
	NiNode * node = parent->GetAsNiNode();
	if (node) {
		if (functor(parent))
			return true;

		for (UInt32 i = 0; i < node->m_children.m_emptyRunStart; i++) {
			NiAVObject * object = node->m_children.m_data[i];
			if (object) {
				if (VisitObjects(object, functor))
					return true;
			}
		}
	}
	else if (functor(parent))
		return true;

	return false;
}

NiTransform GetGeometryTransform(BSGeometry * geometry)
{
	NiTransform transform = geometry->m_localTransform;
	NiSkinInstance * dstSkin = niptr_cast<NiSkinInstance>(geometry->m_spSkinInstance);
	if (dstSkin) {
		NiSkinData * skinData = dstSkin->m_spSkinData;
		if (skinData) {
			transform = transform * skinData->m_kRootParentToSkin;

			for (UInt32 i = 0; i < skinData->m_uiBones; i++) {
				NiAVObject * bone = dstSkin->m_ppkBones[i];
				if (bone->m_name == BSFixedString("NPC Head [Head]").data) {
					transform = transform * skinData->m_pkBoneData[i].m_kSkinToBone;
					break;
				}
			}
		}
	}

	return transform;
}

UInt16 GetStripLengthSum(NiTriStripsData * strips)
{
	return strips->m_usTriangles + 2 * strips->m_usStrips;
}

void GetTriangleIndices(NiTriStripsData * strips, UInt16 i, UInt16 v0, UInt16 v1, UInt16 v2)
{
	UInt16 usTriangles;
	UInt16 usStrip = 0;

	UInt16* pusStripLists = strips->m_pusStripLists;
	while (i >= (usTriangles = strips->m_pusStripLengths[usStrip] - 2))
	{
		i = (UInt16)(i - usTriangles);
		pusStripLists += strips->m_pusStripLengths[usStrip++];
	}

	if (i & 1)
	{
		v0 = pusStripLists[i + 1];
		v1 = pusStripLists[i];
	}
	else
	{
		v0 = pusStripLists[i];
		v1 = pusStripLists[i + 1];
	}

	v2 = pusStripLists[i + 2];
}