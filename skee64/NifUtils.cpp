#include "NifUtils.h"
#include "FileUtils.h"
#include "SKEEHooks.h"
#include "Utilities.h"

#include <unordered_map>



#include "RE/N/NiRTTI.h"
#include "RE/N/NiGeometry.h"
#include "RE/N/NiTriShape.h"
#include "RE/B/BSGeometry.h"
#include "RE/N/NiExtraData.h"
#include "RE/E/ExtraContainerChanges.h"
#include "RE/I/InventoryChanges.h"

#include "REX/W32/COMPTR.h"
#include "REX/W32/D3D11_4.h"
#include <DirectXTex.h>
#include <unordered_set>
#include <cstdint>
#include "NiRTTIUtils.h"



extern bool g_exportSkinToBone;

namespace {
	// Legacy create factories for NIF-level geometry (matches legacy skse64/NiGeometry.cpp):
	// allocate on the game heap, zero it, call the relocated constructor (see SKEEHooks),
	// then install the derived vtable. The BSGeometry factories are relocated directly in
	// SKEEHooks (SKEE::CreateBSTriShape / SKEE::CreateBSDynamicTriShape).
	RE::NiTriShape* CreateNiTriShape(RE::NiGeometryData* a_data)
	{
		auto* xData = RE::malloc<RE::NiTriShape>();
		std::memset(xData, 0, sizeof(RE::NiTriShape));
		SKEE::NiTriBasedGeomCtor(xData, a_data);
		*reinterpret_cast<std::uintptr_t**>(xData) = reinterpret_cast<std::uintptr_t*>(RE::NiTriShape::VTABLE[0].address());
		return xData;
	}

	RE::NiTriStrips* CreateNiTriStrips(RE::NiGeometryData* a_data)
	{
		auto* mem = RE::malloc(sizeof(RE::NiTriStrips));
		std::memset(mem, 0, sizeof(RE::NiTriStrips));
		SKEE::NiTriBasedGeomCtor(reinterpret_cast<RE::NiAVObject*>(mem), a_data);
		*reinterpret_cast<std::uintptr_t**>(mem) = reinterpret_cast<std::uintptr_t*>(RE::VTABLE_NiTriStrips[0].address());
		return static_cast<RE::NiTriStrips*>(mem);
	}

	// Clone a skin instance and remap its bones to new NiNode instances (recorded in
	// a_boneMap for later attachment). Matches the legacy SKSETaskExportHead skin-instance
	// reconstruction. If a_resetSkinToBone is true, zeroes each bone's skin-to-bone
	// transform (the legacy g_exportSkinToBone == false path).
	RE::NiPointer<RE::NiSkinInstance> BuildRemappedSkinInstance(
		RE::NiSkinInstance* a_skinInstance, RE::NiNode* a_skinnedNode,
		std::map<RE::NiAVObject*, RE::NiAVObject*>& a_boneMap, bool a_resetSkinToBone)
	{
		if (!a_skinInstance)
			return nullptr;

		auto newSkinInstance = RE::NiPointer<RE::NiSkinInstance>(static_cast<RE::NiSkinInstance*>(a_skinInstance->Clone()));
		newSkinInstance->rootParent = a_skinnedNode;

		std::uint32_t numBones = 0;
		RE::NiSkinData* skinData = a_skinInstance->skinData.get();
		if (skinData)
			numBones = skinData->GetBoneCount();

		RE::NiPointer<RE::NiObject> newSdObj;
		RE::NiPointer<RE::NiSkinData> newSkinData;
		if (skinData) { skinData->CreateDeepCopy(newSdObj); newSkinData = niptr_cast<RE::NiSkinData>(newSdObj); }

		RE::NiSkinPartition* skinPartition = a_skinInstance->skinPartition.get();
		RE::NiPointer<RE::NiObject> newSpObj;
		RE::NiSkinPartition* newSkinPartition = nullptr;
		if (skinPartition) { skinPartition->CreateDeepCopy(newSpObj); newSkinPartition = netimmerse_cast<RE::NiSkinPartition*>(newSpObj.get()); }

		newSkinInstance->skinData = newSkinData;
		newSkinInstance->skinPartition = RE::NiPointer<RE::NiSkinPartition>(newSkinPartition);

		if (numBones > 0)
		{
			newSkinInstance->bones = static_cast<RE::NiAVObject**>(RE::malloc(numBones * sizeof(RE::NiAVObject*)));
			for (std::uint32_t i = 0; i < numBones; i++)
			{
				RE::NiAVObject* bone = a_skinInstance->bones[i];
				if (bone)
				{
					auto it = a_boneMap.find(bone);
					if (it == a_boneMap.end()) {
						RE::NiNode* newBone = RE::NiNode::Create();
						newBone->IncRefCount();
						newBone->name = bone->name;
						newBone->flags = bone->flags;
						a_boneMap.insert(std::make_pair(bone, newBone));
						newSkinInstance->bones[i] = newBone;
					}
					else
						newSkinInstance->bones[i] = it->second;
				}
				else
					newSkinInstance->bones[i] = nullptr;
			}

			if (a_resetSkinToBone && newSkinData)
			{
				for (std::uint32_t i = 0; i < newSkinData->GetBoneCount(); i++)
					newSkinData->GetBoneDataSkinToBone(i) = RE::NiTransform();
			}
		}

		return newSkinInstance;
	}

}

bool SaveRenderedDDS(RE::NiTexture * pkTexture, const char * pcFileName)
{
	HRESULT res = 0;
	if (!pkTexture)
	{
		SKSE::log::error("{} - Texture to render from", __FUNCTION__);
		return false;
	}

	auto srcTex = static_cast<RE::NiSourceTexture*>(pkTexture);
	auto rendererData = srcTex ? srcTex->rendererTexture : nullptr;
	if (!rendererData)
	{
		SKSE::log::error("{} - No rendererData on NiTexture", __FUNCTION__);
		return false;
	}
	
	REX::W32::ComPtr<REX::W32::ID3D11Texture2D> texture;
	REX::W32::ComPtr<REX::W32::ID3D11Resource> resource(reinterpret_cast<REX::W32::ID3D11Resource*>(rendererData->texture));
	if (resource.Get())
	{
		texture = REX::W32::ComPtr<REX::W32::ID3D11Texture2D>(reinterpret_cast<REX::W32::ID3D11Texture2D*>(rendererData->texture));
	}
	else if (!resource.Get() && rendererData->resourceView) // Didn't have texture directly but still has resource view, acquire it
	{
		reinterpret_cast<ID3D11View*>(rendererData->resourceView)->GetResource(reinterpret_cast<ID3D11Resource**>(resource.GetAddressOf()));
		static constexpr IID iidTex2D{ 0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };
		res = resource.Get()->QueryInterface(reinterpret_cast<const REX::W32::IID&>(iidTex2D), (void**)texture.GetAddressOf());
		if (FAILED(res))
		{
			SKSE::log::error("{} - Failed to query texture from resource", __FUNCTION__);
			return false;
		}
	}

	if (!texture.Get())
	{
		SKSE::log::error("{} - No texture to capture", __FUNCTION__);
		return false;
	}

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	REX::W32::ComPtr<REX::W32::ID3D11Device> device;
	context->GetDevice(reinterpret_cast<REX::W32::ID3D11Device**>(device.GetAddressOf()));
	if (!device.Get())
	{
		SKSE::log::error("{} - No texture to capture", __FUNCTION__);
		return false;
	}

	DirectX::ScratchImage si;
	res = DirectX::CaptureTexture(reinterpret_cast<ID3D11Device*>(device.Get()), reinterpret_cast<ID3D11DeviceContext*>(context), reinterpret_cast<ID3D11Resource*>(texture.Get()), si);
	if (FAILED(res))
	{
		SKSE::log::error("{} - Failed to capture texture from device", __FUNCTION__);
		return false;
	}

	size_t len = strlen(pcFileName) + 1;
	wchar_t * fileName = new wchar_t[len];
	memset(fileName, 0, sizeof(len) * 2);
	size_t converted = 0;
	mbstowcs_s(&converted, fileName, len, pcFileName, len * 2);

	res = DirectX::SaveToDDSFile(si.GetImages(), si.GetImageCount(), si.GetMetadata(), DirectX::DDS_FLAGS_NONE, fileName);
	if (FAILED(res))
	{
		delete[] fileName;
		SKSE::log::error("{} - Failed to save image to DDS at {}", __FUNCTION__, pcFileName);
		return false;
	}

	delete[] fileName;
	return true;
}

RE::BSGeometry * GetHeadGeometry(RE::Actor * actor, std::uint32_t partType)
{
	RE::BSFaceGenNiNode * faceNode = actor->GetFaceNode();
	RE::TESNPC * actorBase = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;

	if(faceNode && actorBase) {
		RE::BGSHeadPart * facePart = actorBase->GetCurrentHeadPartByType(static_cast<RE::TESNPC::HeadPartType>(partType));
		if(facePart) {
			RE::NiAVObject * headNode = faceNode->GetObjectByName(facePart->formEditorID);
			if(headNode) {
				RE::BSGeometry * geometry = headNode ? headNode->AsGeometry() : nullptr;
				if(geometry)
					return geometry;
			}
		}
	}

	return NULL;
}

RE::NiAVObject * GetObjectByHeadPart(RE::BSFaceGenNiNode * faceNode, RE::BGSHeadPart * headPart)
{
	for (std::uint32_t p = 0; p < faceNode->children.size(); p++)
	{
		RE::NiAVObject * object = faceNode->children[p].get();
		if (object && RE::BSFixedString(object->name) == headPart->formEditorID) {
			if (object) {
				return object;
			}
		}
	}

	return nullptr;
}

SKSETaskRefreshTintMask::SKSETaskRefreshTintMask(RE::Actor * actor, RE::BSFixedString ddsPath) : m_ddsPath(ddsPath)
{
	m_formId = actor->formID;
}

void SKSETaskRefreshTintMask::Run()
{
	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::Actor * actor = form ? form->As<RE::Actor>() : nullptr;
	if (!actor)
		return;

	RE::BSGeometry * geometry = GetHeadGeometry(actor, 0);
	if (geometry) {
		RE::BSShaderProperty * shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get());
		if (auto * lightingShader = netimmerse_cast<RE::BSLightingShaderProperty *>(shaderProperty)) {
			RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial *>(shaderProperty->material);
			if (material) {
				material->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(6), m_ddsPath.c_str());
				material->ClearTextures();
				SKEE::InvalidateTextures(lightingShader, 0);
				SKEE::InitializeShader(lightingShader, geometry);
			}
		}
	}
}

void ExportTintMaskDDS(RE::Actor * actor, RE::BSFixedString filePath)
{
	RE::BSGeometry * geometry = GetHeadGeometry(actor, 0);
	if(geometry)
	{
		RE::BSShaderProperty * shaderProperty = static_cast<RE::BSShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get());
		if (auto * lightingShader = netimmerse_cast<RE::BSLightingShaderProperty *>(shaderProperty))
		{
			RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial *>(shaderProperty->material);
			if (material)
			{
				if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen)
				{
					auto * maskedMaterial = static_cast<RE::BSLightingShaderMaterialFacegen *>(static_cast<RE::BSLightingShaderMaterialBase *>(material));
					SaveRenderedDDS(maskedMaterial->tintTexture.get(), filePath.c_str());
				}
			}
		}
	}
}

RE::BGSTextureSet * GetTextureSetForPart(RE::TESNPC * npc, RE::BGSHeadPart * headPart)
{
	RE::BGSTextureSet * textureSet = nullptr;
	if (headPart->type.any(RE::BGSHeadPart::HeadPartType::kFace)) {
		if (npc->headRelatedData)
			textureSet = npc->headRelatedData->faceDetails;
	}
	if (!textureSet)
		textureSet = headPart->textureSet;

	return textureSet;
}

std::pair<RE::BGSTextureSet*, RE::BGSHeadPart*> GetTextureSetForPartByName(RE::TESNPC * npc, RE::BSFixedString partName)
{
	std::uint32_t numHeadParts = 0;
	RE::BGSHeadPart ** headParts = nullptr;
	if (npc->HasOverlays()) {
		numHeadParts = npc->GetNumBaseOverlays();
		headParts = npc->GetBaseOverlays();
	} else {
		numHeadParts = npc->numHeadParts;
		headParts = npc->headParts;
	}
	for (std::uint32_t i = 0; i < numHeadParts; i++) // Acquire all parts
	{
		RE::BGSHeadPart * headPart = headParts[i];
		if (headPart && headPart->formEditorID == partName) {
			return std::make_pair(GetTextureSetForPart(npc, headPart), headPart);
		}
	}

	return std::make_pair<RE::BGSTextureSet*, RE::BGSHeadPart*>(nullptr, nullptr);
}

SKSETaskExportHead::SKSETaskExportHead(RE::Actor * actor, RE::BSFixedString nifPath, RE::BSFixedString ddsPath) : m_nifPath(nifPath), m_ddsPath(ddsPath)
{
	m_formId = actor->formID;
}

void SKSETaskExportHead::Run()
{
	if (!m_formId)
		return;

	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::Actor * actor = form ? form->As<RE::Actor>() : nullptr;
	if (!actor)
		return;

	RE::BSFaceGenNiNode * faceNode = actor->GetFaceNode();
	RE::TESNPC * actorBase = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (!actorBase || !faceNode)
		return;

	RE::BSFaceGenAnimationData * animationData = actor->GetFaceGenAnimationData();
	if (animationData) {
		RE::BSFaceGenManager::GetSingleton()->isReset = 0;
		animationData->Reset(0.0f, true, true, true, false);
		SKEE::UpdateModelFace(faceNode);
	}

	FileUtils::MakeAllDirs(m_nifPath.c_str());

	// BSFadeNode::Create() equivalent: game-heap alloc + relocated game ctor (P: 0x014E57C0).
	// The 0x158/0x180 are the STATIC_ASSERT_SIZE(BSFadeNode, 0x158, 0x158, 0x180, 0x110) flat/VR sizes.
	auto* rootNodeRaw = RE::malloc_runtime<RE::BSFadeNode>(0x158, 0x180);
	RE::NiPointer<RE::BSFadeNode> rootNode(nullptr);
	if (rootNodeRaw) {
		SKEE::BSFadeNodeCtor(rootNodeRaw);
		// NiPointer has no raw-pointer operator= in CommonLibSSE-NG; reset()
		// is the equivalent adopt-with-refcount from legacy NetImmerse.
		rootNode.reset(rootNodeRaw);
	}
	RE::NiPointer<RE::NiNode> skinnedNode(RE::NiNode::Create(0));
	skinnedNode->name = "BSFaceGenNiNodeSkinned";

	std::map<RE::NiAVObject*, RE::NiAVObject*> boneMap;

	for (std::uint32_t i = 0; i < faceNode->children.size(); i++)
	{
		RE::NiAVObject * object = faceNode->children[i].get();
		if (!object)
			continue;

		if (auto * geometry = object ? object->AsNiGeometry() : nullptr) {
			RE::NiGeometryData * geometryData = geometry->spModelData.get();
			RE::NiPointer<RE::NiObject> newGeoObj;
			RE::NiGeometryData * newGeometryData = NULL;
			if (geometryData) { geometryData->CreateDeepCopy(newGeoObj); newGeometryData = netimmerse_cast<RE::NiGeometryData*>(newGeoObj.get()); }

			RE::NiProperty * trishapeEffect = geometry->spEffectState.get();
			RE::NiPointer<RE::NiObject> newTeObj;
			RE::NiPointer<RE::NiProperty> newTrishapeEffect;
			if (trishapeEffect) { trishapeEffect->CreateDeepCopy(newTeObj); newTrishapeEffect = niptr_cast<RE::NiProperty>(newTeObj); }

			RE::NiProperty * trishapeProperty = geometry->spPropertyState.get();
			RE::NiPointer<RE::NiObject> newTpObj;
			RE::NiPointer<RE::NiProperty> newTrishapeProperty;
			if (trishapeProperty) { trishapeProperty->CreateDeepCopy(newTpObj); newTrishapeProperty = niptr_cast<RE::NiProperty>(newTpObj); }

			RE::NiPointer<RE::NiSkinInstance> newSkinInstance = BuildRemappedSkinInstance(geometry->spSkinInstance.get(), skinnedNode.get(), boneMap, false);

			RE::NiPointer<RE::NiGeometry> newGeometry;
			if (auto * trishape = geometry ? geometry->AsNiTriShape() : nullptr) {
				auto * newTrishape = CreateNiTriShape(newGeometryData);
				if (newGeometryData) newGeometryData->DecRefCount();  // the shape now owns a reference
				newTrishape->local = geometry->local;
				newTrishape->name = geometry->name;
				newTrishape->spEffectState = newTrishapeEffect;
				newTrishape->spPropertyState = newTrishapeProperty;
				newTrishape->spSkinInstance = newSkinInstance;
				newGeometry = RE::NiPointer<RE::NiGeometry>(static_cast<RE::NiGeometry*>(newTrishape));
			}
			else if (netimmerse_isKind<RE::NiTriStrips>(geometry)) {
				auto * newTristrips = CreateNiTriStrips(newGeometryData);
				if (newGeometryData) newGeometryData->DecRefCount();
				newTristrips->local = geometry->local;
				newTristrips->name = geometry->name;
				newTristrips->spEffectState = newTrishapeEffect;
				newTristrips->spPropertyState = newTrishapeProperty;
				newTristrips->spSkinInstance = newSkinInstance;
				newGeometry = RE::NiPointer<RE::NiGeometry>(static_cast<RE::NiGeometry*>(newTristrips));
			}

			if (newGeometry)
			{
				auto textureData = GetTextureSetForPartByName(actorBase, newGeometry->name);

				RE::BSShaderProperty* shaderProperty = static_cast<RE::BSShaderProperty*>(newGeometry->spEffectState.get());
				if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty)) {
					RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
					if (material && material->textureSet) {
						if (textureData.first) {
							for (std::uint32_t i = 0; i < RE::BSTextureSet::Texture::kUsedTotal; i++)
								material->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), textureData.first->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)));
						}

						if (textureData.second && textureData.second->type.any(RE::BGSHeadPart::HeadPartType::kFace))
							material->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(6), m_ddsPath.c_str());
					}
				}

				// Save the previous tint mask
				RE::BSShaderProperty * originalShaderProperty = static_cast<RE::BSShaderProperty*>(geometry->spEffectState.get());
				if (originalShaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(originalShaderProperty)) {
					RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial *>(originalShaderProperty->material);
					if (material) {
						if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen) {
							auto * maskedMaterial = static_cast<RE::BSLightingShaderMaterialFacegen *>(static_cast<RE::BSLightingShaderMaterialBase *>(material));
							FileUtils::MakeAllDirs(m_ddsPath.c_str());
							SaveRenderedDDS(maskedMaterial->tintTexture.get(), m_ddsPath.c_str());
						}
					}
				}

				skinnedNode->AttachChild(newGeometry.get(), true);
			}
		}
		else if (auto * geometry = object ? object->AsGeometry() : nullptr) {
			auto * trishape = geometry ? geometry->AsTriShape() : nullptr;
			if (trishape) {
				RE::NiProperty * propEffect = static_cast<RE::NiProperty*>(trishape->GetGeometryRuntimeData().shaderProperty.get());
				RE::NiPointer<RE::NiObject> newTeObj;
				RE::NiPointer<RE::NiProperty> newTrishapeEffect;
				if (propEffect) { propEffect->CreateDeepCopy(newTeObj); newTrishapeEffect = niptr_cast<RE::NiProperty>(newTeObj); }

				RE::NiProperty * propProperty = static_cast<RE::NiProperty*>(trishape->GetGeometryRuntimeData().alphaProperty.get());
				RE::NiPointer<RE::NiObject> newTpObj;
				RE::NiPointer<RE::NiProperty> newTrishapeProperty;
				if (propProperty) { propProperty->CreateDeepCopy(newTpObj); newTrishapeProperty = niptr_cast<RE::NiProperty>(newTpObj); }

				RE::NiPointer<RE::NiSkinInstance> newSkinInstance = BuildRemappedSkinInstance(trishape->GetGeometryRuntimeData().skinInstance.get(), skinnedNode.get(), boneMap, !g_exportSkinToBone);

				RE::BSTriShape * newTrishape = nullptr;
				auto * dynamicShape = trishape ? trishape->AsDynamicTriShape() : nullptr;
				if (dynamicShape) {
					auto * xData = SKEE::CreateBSDynamicTriShape();
					newTrishape = xData;
					if (xData && dynamicShape->GetDynamicTrishapeRuntimeData().dynamicData) {
						std::size_t numVerts = trishape->GetTrishapeRuntimeData().vertexCount;
						auto & srcRT = dynamicShape->GetDynamicTrishapeRuntimeData();
						auto & dstRT = xData->GetDynamicTrishapeRuntimeData();
						srcRT.lock.Lock();
						dstRT.dynamicData = RE::NiMalloc(numVerts * 16);
						std::memcpy(dstRT.dynamicData, srcRT.dynamicData, numVerts * 16);
						srcRT.lock.Unlock();
						dstRT.dataSize = srcRT.dataSize;
						dstRT.frameCount = srcRT.frameCount;
						dstRT.unk178 = srcRT.unk178;
					}
				}
				else {
					newTrishape = SKEE::CreateBSTriShape();
				}

				if (newTrishape) {
					auto & srcGD = trishape->GetGeometryRuntimeData();
					auto & dstGD = newTrishape->GetGeometryRuntimeData();
					newTrishape->local = trishape->local;
					newTrishape->name = trishape->name;
					if (newTrishapeEffect) dstGD.shaderProperty.reset(static_cast<RE::BSShaderProperty*>(newTrishapeEffect.get()));
					if (newTrishapeProperty) dstGD.alphaProperty.reset(static_cast<RE::NiAlphaProperty*>(newTrishapeProperty.get()));
					dstGD.skinInstance = newSkinInstance;
					dstGD.rendererData = srcGD.rendererData;  // shared geometry data
					if (dstGD.rendererData) {
						dstGD.rendererData->refCount++;
					}
					dstGD.unk140 = srcGD.unk140;
					dstGD.vertexDesc = srcGD.vertexDesc;
					newTrishape->flags = trishape->flags;
					newTrishape->worldBound = trishape->worldBound;
					newTrishape->lastUpdatedFrameCounter = trishape->lastUpdatedFrameCounter;
					newTrishape->GetModelData().modelBound = trishape->GetModelData().modelBound;
					newTrishape->GetType() = trishape->GetType();
					auto & srcTS = trishape->GetTrishapeRuntimeData();
					auto & dstTS = newTrishape->GetTrishapeRuntimeData();
					dstTS.triangleCount = srcTS.triangleCount;
					dstTS.vertexCount = srcTS.vertexCount;
					dstTS.pad15C = srcTS.pad15C;

					auto textureData = GetTextureSetForPartByName(actorBase, newTrishape->name);
					RE::BSShaderProperty* shaderProperty = dstGD.shaderProperty.get();
					if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty)) {
						RE::BSLightingShaderMaterial* material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
						if (material && material->textureSet) {
							if (textureData.first) {
								for (std::uint32_t i = 0; i < RE::BSTextureSet::Texture::kUsedTotal; i++)
									material->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), textureData.first->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)));
							}
							if (textureData.second && textureData.second->type.any(RE::BGSHeadPart::HeadPartType::kFace))
								material->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(6), m_ddsPath.c_str());
						}
					}

					RE::BSShaderProperty * originalShaderProperty = srcGD.shaderProperty.get();
					if (originalShaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(originalShaderProperty)) {
						RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial *>(originalShaderProperty->material);
						if (material) {
							if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen) {
								auto * maskedMaterial = static_cast<RE::BSLightingShaderMaterialFacegen *>(static_cast<RE::BSLightingShaderMaterialBase *>(material));
								FileUtils::MakeAllDirs(m_ddsPath.c_str());
								SaveRenderedDDS(maskedMaterial->tintTexture.get(), m_ddsPath.c_str());
							}
						}
					}

					RE::NiPointer<RE::BSGeometry> newGeometry(static_cast<RE::BSGeometry*>(newTrishape));
					skinnedNode->AttachChild(newGeometry.get(), true);
				}
			}
		}
	}

	for (auto & bones : boneMap) {
		rootNode.get()->AttachChild(bones.second, true);
		bones.second->DecRefCount();
	}

	rootNode.get()->AttachChild(skinnedNode.get(), true);

	{
		NifStreamWrapper niStream;
		SKEE::NiStreamAddObject(niStream.get(), rootNode.get());
		niStream->Save3(m_nifPath.c_str());
	}

	if (animationData) {
		animationData->exprOverride = 0;
		animationData->Reset(1.0, 1, 1, 0, 0);
		RE::BSFaceGenManager::GetSingleton()->isReset = 1;
		SKEE::UpdateModelFace(faceNode);
	}
}

bool VisitObjects(RE::NiAVObject * parent, std::function<bool(RE::NiAVObject*)> functor)
{
	auto * node = parent ? parent->AsNode() : nullptr;
	if (node) {
		if (functor(parent))
			return true;

		for (std::uint32_t i = 0; i < node->children.size(); i++) {
			RE::NiAVObject * object = node->children[i].get();
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

bool VisitGeometry(RE::NiAVObject* parent, std::function<bool(RE::BSGeometry*)> functor)
{
	return VisitObjects(parent, [&functor](RE::NiAVObject* object)
	{
		RE::BSGeometry* geometry = object ? object->AsGeometry() : nullptr;
		if (geometry)
		{
			if (functor(geometry))
			{
				return true;
			}
		}

		return false;
	});
}

bool VisitGeometry(RE::NiAVObject* object, GeometryVisitor* visitor)
{
	return VisitGeometry(object, [&](RE::BSGeometry* geometry)
	{
		if (visitor->Accept(geometry))
		{
			return true;
		}

		return false;
	});
}

RE::NiTransform GetGeometryTransform(RE::BSGeometry * geometry)
{
	RE::NiTransform transform = geometry->local;
	RE::NiSkinInstance * dstSkin = geometry->skinInstance.get();
	if (dstSkin) {
		utils::ScopedCriticalSection cs(&dstSkin->lock);
		RE::NiSkinData * skinData = dstSkin->skinData.get();
		if (skinData) {
			transform = transform * skinData->rootParentToSkin;

			for (std::uint32_t i = 0; i < skinData->GetBoneCount(); i++) {
				RE::NiAVObject * bone = dstSkin->bones[i];
				if (bone->name == "NPC Head [Head]") {
					transform = transform * skinData->boneData[i].skinToBone;
					break;
				}
			}
		}
	}

	return transform;
}

RE::NiTransform GetLegacyGeometryTransform(RE::NiGeometry * geometry)
{
	RE::NiTransform transform = geometry->local;
	RE::NiSkinInstance * dstSkin = geometry->spSkinInstance.get();
	if (dstSkin) {
		utils::ScopedCriticalSection cs(&dstSkin->lock);
		RE::NiSkinData * skinData = dstSkin->skinData.get();
		if (skinData) {
			transform = transform * skinData->rootParentToSkin;

			for (std::uint32_t i = 0; i < skinData->GetBoneCount(); i++) {
				RE::NiAVObject * bone = dstSkin->bones[i];
				if (bone->name == "NPC Head [Head]") {
					transform = transform * skinData->boneData[i].skinToBone;
					break;
				}
			}
		}
	}

	return transform;
}

std::uint16_t GetStripLengthSum(RE::NiTriStripsData * strips)
{
	return strips->numTriangles + 2 * strips->numStrips;
}

void GetTriangleIndices(RE::NiTriStripsData* strips, std::uint16_t i, std::uint16_t& v0, std::uint16_t& v1, std::uint16_t& v2)
{
	std::uint16_t usTriangles;
	std::uint16_t usStrip = 0;

	std::uint16_t* pusStripLists = strips->stripLists;
	while (i >= (usTriangles = strips->stripLengths[usStrip] - 2))
	{
		i = (std::uint16_t)(i - usTriangles);
		pusStripLists += strips->stripLengths[usStrip++];
	}

	if ((i % 2) != 0)
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

RE::NiAVObject* GetRootNode(RE::NiAVObject* object, bool refRoot)
{
	RE::NiAVObject * rootNode = object;
	RE::NiNode* parent = rootNode->parent;
	while (parent)
	{
		rootNode = parent;
		if (rootNode->userData) // reached a node with an owner (legacy m_owner)
			break;
		parent = parent->parent;
	}

	return rootNode;
}

RE::TESObjectARMO* GetActorSkin(RE::Actor* actor)
{
	RE::TESNPC* npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (npc) {
		if (npc->skin)
			return npc->skin;
	}
	RE::TESRace* actorRace = actor->race;
	if (actorRace)
		return actorRace->skin;

	if (npc) {
		actorRace = npc->race;
		if (actorRace)
			return actorRace->skin;
	}

	return NULL;
}

struct MatchBySlot
{
	std::uint32_t m_mask;
public:
	MatchBySlot(std::uint32_t slot) :
		m_mask(slot)
	{

	}

	bool Matches(RE::TESForm* pForm) const {
		return IsSlotMatch(pForm, m_mask);
	}
};

bool IsSlotMatch(RE::TESForm* pForm, std::uint32_t mask)
{
	if (pForm) {
		RE::BGSBipedObjectForm* pBip = pForm ? pForm->As<RE::BGSBipedObjectForm>() : nullptr;
		if (pBip) {
			return pBip->bipedModelData.bipedObjectSlots.any(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(mask));
		}
	}

	return false;
}

RE::TESForm* GetSkinForm(RE::Actor* thisActor, std::uint32_t mask)
{
	RE::TESForm* equipped = GetWornForm(thisActor, mask); // Check equipped item
	if (!equipped) {
		RE::TESNPC* actorBase = thisActor->GetBaseObject() ? thisActor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
		if (actorBase) {
			equipped = actorBase->skin; // Check ActorBase
		}
		if (!equipped) {
			// Check the actor's race
			RE::TESRace* race = thisActor->race;
			if (!race) {
				// Check the actor base's race
				race = actorBase->race;
			}

			if (race) {
				equipped = race->skin; // Check Race
			}
		}
	}

	return equipped;
}

// True if the inventory entry carries a worn / worn-left extra (legacy kExtraData_Worn/kWornLeft check).
static bool EntryIsWorn(RE::InventoryEntryData* a_entry)
{
	if (!a_entry || !a_entry->extraLists)
		return false;
	for (const auto& list : *a_entry->extraLists) {
		if (list && (list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft)))
			return true;
	}
	return false;
}

RE::TESForm* GetWornForm(RE::Actor* thisActor, std::uint32_t mask)
{
	MatchBySlot matcher(mask);

	struct WornFinder : RE::InventoryChanges::IItemChangeVisitor
	{
		MatchBySlot& matcher;
		RE::TESForm* found = nullptr;

		explicit WornFinder(MatchBySlot& a_matcher) : matcher(a_matcher) {}

		virtual RE::BSContainer::ForEachResult Visit(RE::InventoryEntryData* a_entry) override
		{
			if (a_entry && !found && EntryIsWorn(a_entry) && matcher.Matches(a_entry->object)) {
				found = a_entry->object;
				return RE::BSContainer::ForEachResult::kStop;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		}
	} finder{ matcher };

	if (RE::ExtraContainerChanges* pContainerChanges = thisActor->extraList.GetByType<RE::ExtraContainerChanges>()) {
		if (pContainerChanges->changes) {
			pContainerChanges->changes->VisitInventory(finder);
		}
	}

	return finder.found;
}

void VisitAllWornItems(RE::Actor* thisActor, std::uint32_t mask, std::function<void(RE::InventoryEntryData*)> functor)
{
	MatchBySlot matcher(mask);

	struct WornVisitor : RE::InventoryChanges::IItemChangeVisitor
	{
		MatchBySlot& matcher;
		std::function<void(RE::InventoryEntryData*)>& results;

		WornVisitor(MatchBySlot& a_matcher, std::function<void(RE::InventoryEntryData*)>& a_results)
			: matcher(a_matcher), results(a_results) {}

		virtual RE::BSContainer::ForEachResult Visit(RE::InventoryEntryData* a_entry) override
		{
			if (a_entry && EntryIsWorn(a_entry) && matcher.Matches(a_entry->object)) {
				results(a_entry);
			}
			return RE::BSContainer::ForEachResult::kContinue;  // visit all matching worn items
		}

		
	} visitor{ matcher, functor };

	if (RE::ExtraContainerChanges* pContainerChanges = thisActor->extraList.GetByType<RE::ExtraContainerChanges>()) {
		if (pContainerChanges->changes) {
			pContainerChanges->changes->VisitInventory(reinterpret_cast<RE::InventoryChanges::IItemChangeVisitor&>(visitor));
		}
	}
}

RE::BSGeometry* GetFirstShaderType(RE::NiAVObject* object, std::uint32_t shaderType)
{
	RE::BSGeometry* foundGeometry = nullptr;
	VisitGeometry(object, [&foundGeometry, shaderType](RE::BSGeometry* geometry)
	{
		RE::BSShaderProperty* shaderProperty = geometry->GetGeometryRuntimeData().shaderProperty.get();
		if (shaderProperty && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
		{
			// Find first geometry if the type is any
			if (shaderType == 0xFFFF)
			{
				foundGeometry = geometry;
				return true;
			}

			RE::BSLightingShaderMaterial* material = (RE::BSLightingShaderMaterial*)shaderProperty->material;
			if (material && static_cast<std::uint32_t>(material->GetFeature()) == shaderType) // legacy GetShaderType()
			{
				foundGeometry = geometry;
				return true;
			}
		}

		return false;
	});

	return foundGeometry;
}

bool NiExtraDataFinder::Accept(RE::NiAVObject* object)
{
	m_data = object->GetExtraData(m_name);
	if (m_data)
		return true;

	return false;
};

RE::NiExtraData* FindExtraData(RE::NiAVObject* object, RE::BSFixedString name)
{
	if (!object)
		return NULL;

	RE::NiExtraData* extraData = NULL;
	VisitObjects(object, [&](RE::NiAVObject* object)
	{
		extraData = object->GetExtraData(name);
		if (extraData)
			return true;

		return false;
	});

	return extraData;
}

// Visit the biped-attached geometry objects for both first/third person models
// (legacy NifUtils.cpp). weightModel->objects / bufferedObjects are the two 42-slot
// BIPOBJECT tables on BipedAnim; each slot's partClone is the attached node.
void VisitBipedNodes(RE::TESObjectREFR* refr, std::function<void(bool, std::uint32_t, RE::NiNode*, RE::TESForm*, RE::TESForm*, RE::NiAVObject*)> functor)
{
	for (std::int32_t k = 0; k <= 1; ++k)
	{
		auto& bipedSmart = refr->GetBiped1(k == 1);
		auto* weightModel = bipedSmart.get();
		if (!weightModel)
			continue;

		RE::NiNode* rootNode = refr->Get3D(k == 1) ? refr->Get3D(k == 1)->AsNode() : nullptr;
		for (std::int32_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i)
		{
			auto& data = weightModel->objects[i];
			if (data.partClone.get())
				functor(k == 1, static_cast<std::uint32_t>(i), rootNode, data.item, data.addon ? static_cast<RE::TESForm*>(data.addon) : nullptr, data.partClone.get());
		}
		for (std::int32_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i)
		{
			auto& data = weightModel->bufferedObjects[i];
			if (data.partClone.get())
				functor(k == 1, static_cast<std::uint32_t>(i), rootNode, data.item, data.addon ? static_cast<RE::TESForm*>(data.addon) : nullptr, data.partClone.get());
		}
	}
}

void VisitEquippedNodes(RE::Actor* actor, std::uint32_t slotMask, std::function<void(RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool)> functor)
{
	std::unordered_set<RE::TESObjectARMO*> equippedSlots;
	VisitAllWornItems(actor, slotMask, [&](RE::InventoryEntryData* itemData)
	{
		if (itemData && itemData->object && itemData->object->IsArmor())
			equippedSlots.insert(static_cast<RE::TESObjectARMO*>(itemData->object));
	});

	RE::TESObjectARMO* skin = GetActorSkin(actor);
	if (skin)
		equippedSlots.insert(skin);

	for (auto& armor : equippedSlots) {
		for (std::uint32_t i = 0; i < armor->armorAddons.size(); i++) {
			RE::TESObjectARMA* arma = armor->armorAddons[i];
			if (arma && arma->IsValidRace(actor->race)) { // Only search AAs that fit this race
				VisitArmorAddon(actor, armor, arma, [&](bool isFirstPerson, RE::NiNode* skeletonRoot, RE::NiAVObject* armorNode) {
					functor(armor, arma, armorNode, isFirstPerson);
				});
			}
		}
	}
}

void VisitSkeletalRoots(RE::TESObjectREFR* ref, std::function<void(RE::NiNode*, bool)> functor)
{
	RE::NiNode* skeletonRoot[2];
	skeletonRoot[0] = ref->Get3D(false) ? ref->Get3D(false)->AsNode() : nullptr;
	skeletonRoot[1] = ref->Get3D(true) ? ref->Get3D(true)->AsNode() : nullptr;

	// Skip second skeleton, it's the same as the first
	if (skeletonRoot[1] == skeletonRoot[0])
		skeletonRoot[1] = nullptr;

	for (std::uint32_t i = 0; i <= 1; i++)
	{
		if (skeletonRoot[i])
			functor(skeletonRoot[i], i == 1);
	}
}

void VisitArmorAddon(RE::Actor* actor, RE::TESObjectARMO* armor, RE::TESObjectARMA* arma, std::function<void(bool, RE::NiNode*, RE::NiAVObject*)> functor)
{
	char addonString[REX::W32::MAX_PATH];
	memset(addonString, 0, sizeof(addonString));
	arma->GetNodeName(addonString, actor, armor, -1.0f);

	RE::BSFixedString addonName(addonString);

	std::unordered_set<RE::NiAVObject*> touched;

	VisitBipedNodes(actor, [&](bool isFirstPerson, std::uint32_t slot, RE::NiNode* rootNode, RE::TESForm* bipedArmor, RE::TESForm* bipedArma, RE::NiAVObject* object)
	{
		if (!object)
			return;
		bool isSame = false;
		if (bipedArmor && bipedArma) {
			isSame = (bipedArmor->IsArmor() && bipedArmor->formID == armor->formID && bipedArma->Is(RE::TESObjectARMA::FORMTYPE) && bipedArma->formID == arma->formID) ||
			         (bipedArmor->Is(RE::TESObjectARMA::FORMTYPE) && bipedArmor->formID == arma->formID);
		}
		if (!isSame && std::string(object->name.c_str()) == std::string(addonName.c_str()))
			isSame = true;
		if (isSame && !touched.count(object)) {
			touched.emplace(object);
			functor(isFirstPerson, rootNode, object);
		}
	});

	VisitSkeletalRoots(actor, [&](RE::NiNode* rootNode, bool isFirstPerson)
	{
		// DFS search for the node by name, then traverse all siblings in case the same armor appears twice
		RE::NiAVObject* armorNode = rootNode->GetObjectByName(addonName);
		if (armorNode && armorNode->parent)
		{
			auto parent = armorNode->parent;
			for (std::uint32_t j = 0; j < parent->children.size(); ++j)
			{
				auto childNode = parent->children[j].get();
				if (childNode && std::string(childNode->name.c_str()) == std::string(addonName.c_str()) && !touched.count(childNode))
				{
					touched.emplace(childNode);
					functor(isFirstPerson, rootNode, childNode);
				}
			}
		}
	});
}

bool ResolveAnyForm(SKSE::SerializationInterface* intfc, std::uint32_t handle, std::uint32_t* newHandle)
{
	if (((handle & 0xFF000000) >> 24) != 0xFF) {
		// Skip if handle is no longer valid.
		RE::VMHandle vmHandle;
		if (!intfc->ResolveHandle(static_cast<RE::VMHandle>(handle), vmHandle)) {
			return false;
		}
		*newHandle = static_cast<std::uint64_t>(vmHandle);
		return true;
	}
	else { // This will resolve game-created forms
		RE::TESForm* formCheck = RE::TESForm::LookupByID(static_cast<RE::FormID>(handle));
		if (!formCheck) {
			return false;
		}
		RE::TESObjectREFR* refr = formCheck ? formCheck->As<RE::TESObjectREFR>() : nullptr;
		if (!refr || (refr && refr->IsDeleted())) {
			return false;
		}
		*newHandle = handle;
	}

	return true;
}

bool ResolveAnyHandle(SKSE::SerializationInterface* intfc, std::uint64_t handle, std::uint64_t* newHandle)
{
	if (((handle & 0xFF000000) >> 24) != 0xFF) {
		// Skip if handle is no longer valid.
		RE::VMHandle resolvedHandle;
		if (!intfc->ResolveHandle(static_cast<RE::VMHandle>(handle), resolvedHandle)) {
			return false;
		}
		*newHandle = static_cast<std::uint64_t>(resolvedHandle);
		if (false) {
			return false;
		}
	}
	else { // This will resolve game-created forms
		RE::TESForm* formCheck = RE::TESForm::LookupByID(static_cast<RE::FormID>(handle));
		if (!formCheck) {
			return false;
		}
		RE::TESObjectREFR* refr = formCheck ? formCheck->As<RE::TESObjectREFR>() : nullptr;
		if (!refr || (refr && refr->IsDeleted())) {
			return false;
		}
		*newHandle = handle;
	}

	return true;
}

void SKSETaskExportTintMask::Run()
{
	auto* p3d = RE::PlayerCharacter::GetSingleton()->Get3D();
	RE::BSFaceGenNiNode * faceNode = p3d ? netimmerse_cast<RE::BSFaceGenNiNode*>(p3d) : nullptr;
	if (faceNode) {
		// Save the mesh
		std::string ddsPath(m_filePath.c_str());
		FileUtils::MakeAllDirs(ddsPath.c_str());
		ddsPath.append(m_fileName.c_str());
		ddsPath.append(".dds");

		RE::PlayerCharacter * player = RE::PlayerCharacter::GetSingleton();
		ExportTintMaskDDS(player, ddsPath.c_str());
	}
}

void SKSEUpdateFaceModel::Run()
{
	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::Actor * actor = form ? form->As<RE::Actor>() : nullptr;
	if (!actor)
		return;

	RE::NiNode * rootFaceGen = actor->GetFaceNode();
	SKEE::UpdateModelFace(rootFaceGen);
}

SKSEUpdateFaceModel::SKSEUpdateFaceModel(RE::Actor * actor)
{
	m_formId = actor->formID;
}

void NiStringsExtraDataHelper::copy_string(char*& a_value, const char* a_copyValue)
{
	std::size_t strLen = std::strlen(a_copyValue) + 1;
	a_value = RE::NiAlloc<char>(strLen);
	std::memcpy(a_value, a_copyValue, sizeof(char) * strLen);
}

void NiStringsExtraDataHelper::Replace(RE::NiStringsExtraData* _this, std::vector<RE::BSFixedString>& strings)
{
	if(_this->value) RE::NiFree(_this->value);
	_this->size = strings.size();

    if (strings.size() == 0)
    {
        _this->value = nullptr;
        return;
    }

	_this->value = RE::NiAlloc<char*>(strings.size());
    for (std::size_t i = 0; i < strings.size(); ++i)
    {
        copy_string(_this->value[i], strings[i].c_str());
    }
}

// NifStreamWrapper - wraps a stack-allocated RE::NiStream using the game's real
// ctor/dtor (SKEE::NiStreamCtor / SKEE::NiStreamDtor), matching the legacy hack.
NifStreamWrapper::NifStreamWrapper()
{
	std::memset(mem, 0, sizeof(mem));
	SKEE::NiStreamCtor(reinterpret_cast<RE::NiStream*>(mem));
}

NifStreamWrapper::~NifStreamWrapper()
{
	SKEE::NiStreamDtor(reinterpret_cast<RE::NiStream*>(mem));
}

bool NifStreamWrapper::LoadStream(RE::NiBinaryStream* stream)
{
	return reinterpret_cast<RE::NiStream*>(mem)->Load1(stream);
}

bool NifStreamWrapper::VisitObjects(std::function<bool(RE::NiObject*)> functor)
{
	auto* stream = reinterpret_cast<RE::NiStream*>(mem);
	for (std::uint32_t i = 0; i < stream->topObjects.size(); ++i)
	{
		if (stream->topObjects[i].get() && functor(stream->topObjects[i].get()))
			return true;
	}
	return false;
}

