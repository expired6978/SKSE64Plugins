#include "BodyMorphInterface.h"
#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "ShaderUtilities.h"
#include "StringTable.h"

#include "FileUtils.h"
#include "NifUtils.h"

#include <algorithm>
#include <string>
#include <limits>
#include <cctype>
#include <random>
#include <ppl.h>

#include "skse64/PluginAPI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameStreams.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameObjects.h"
#include "skse64/GameForms.h"
#include "skse64/GameData.h"
#include "skse64/GameExtraData.h"

#include "skse64/NiRenderer.h"
#include "skse64/NiNodes.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiRTTI.h"

extern BodyMorphInterface				g_bodyMorphInterface;
extern OverrideInterface				g_overrideInterface;
extern OverlayInterface					g_overlayInterface;
extern StringTable						g_stringTable;
extern SKSETaskInterface				* g_task;
extern bool								g_parallelMorphing;
extern UInt16							g_bodyMorphMode;
extern bool								g_enableBodyGen;
extern bool								g_enableBodyMorph;

UInt32 BodyMorphInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void BodyMorphInterface::LoadMods()
{
	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		std::string fixedPath = "Meshes\\" + std::string(MORPH_MOD_DIRECTORY);

		ForEachMod([&](ModInfo * modInfo)
		{
			std::string templatesPath = fixedPath + std::string(modInfo->name) + "\\templates.ini";
			ReadBodyMorphTemplates(templatesPath.c_str());
		});

		ForEachMod([&](ModInfo * modInfo)
		{
			std::string morphsPath = fixedPath + std::string(modInfo->name) + "\\morphs.ini";
			ReadBodyMorphs(morphsPath.c_str());
		});
	}
}

void BodyMorphInterface::Revert()
{
	SimpleLocker locker(&actorMorphs.m_lock);
	actorMorphs.m_data.clear();
}

void BodyMorphInterface::SetMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey, float relative)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	actorMorphs.m_data[handle][morphName][morphKey] = relative;
}

float BodyMorphInterface::GetMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if(it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(morphName);
		if (mit != it->second.end())
		{
			auto & kit = mit->second.find(morphKey);
			if (kit != mit->second.end())
			{
				return kit->second;
			}
		}
	}

	return 0.0;
}

void BodyMorphInterface::ClearMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(morphName);
		if (mit != it->second.end())
		{
			auto & kit = mit->second.find(morphKey);
			if (kit != mit->second.end())
			{
				mit->second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::HasBodyMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & kit = it->second.find(morphName);
		if (kit != it->second.end())
		{
			auto & mit = kit->second.find(morphKey);
			if(mit != kit->second.end())
				return true;
		}
	}

	return false;
}

float BodyMorphInterface::GetBodyMorphs(TESObjectREFR * actor, BSFixedString morphName)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(morphName);
		if (mit != it->second.end())
		{
			float morphSum = 0;
			for (auto & morph : mit->second)
			{
				if (g_bodyMorphMode == 2 && morph.second > morphSum)
				{
					morphSum = morph.second;
				}
				else
				{
					morphSum += morph.second;
				}
			}

			if (g_bodyMorphMode == 1)
			{
				morphSum /= mit->second.size();
			}

			return morphSum;
		}
	}

	return 0.0;
}

bool BodyMorphInterface::HasBodyMorphKey(TESObjectREFR * actor, BSFixedString morphKey)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & mit : it->second)
		{
			auto & kit = mit.second.find(morphKey);
			if (kit != mit.second.end())
			{
				return true;
			}
		}
	}

	return false;
}

void BodyMorphInterface::ClearBodyMorphKeys(TESObjectREFR * actor, BSFixedString morphKey)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & mit : it->second)
		{
			auto & kit = mit.second.find(morphKey);
			if (kit != mit.second.end())
			{
				mit.second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::HasBodyMorphName(TESObjectREFR * actor, BSFixedString morphName)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & kit = it->second.find(morphName);
		if (kit != it->second.end())
		{
			return true;
		}
	}

	return false;
}

void BodyMorphInterface::ClearBodyMorphNames(TESObjectREFR * actor, BSFixedString morphName)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(morphName);
		if (mit != it->second.end())
		{
			mit->second.clear();
		}
	}
}

void BodyMorphInterface::ClearMorphs(TESObjectREFR * actor)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if(it != actorMorphs.m_data.end())
	{
		actorMorphs.m_data.erase(it);
	}
}

bool BodyMorphInterface::HasMorphs(TESObjectREFR * actor)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);

	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		return true;
	}

	return false;
}

void TriShapeFullVertexData::ApplyMorphRaw(UInt16 vertCount, void * data, float factor)
{
	NiPoint3 * vertices = static_cast<NiPoint3*>(data);
	if (!vertices)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			UInt16 vertexIndex = vert.index;
			const DirectX::XMVECTOR * vertexDiff = &vert.delta;

			vertices[vertexIndex].x += vertexDiff->m128_f32[0] * factor;
			vertices[vertexIndex].y += vertexDiff->m128_f32[1] * factor;
			vertices[vertexIndex].z += vertexDiff->m128_f32[2] * factor;

		}
	}
	else
	{
		_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertCount);
	}
}

void TriShapeFullVertexData::ApplyMorph(UInt16 vertexCount, NiSkinPartition::TriShape * vertexData, float factor)
{
	UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->m_VertexDesc, VertexAttribute::VA_POSITION);
	UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->m_VertexDesc);

	if (m_maxIndex < vertexCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			DirectX::XMFLOAT4 * position = reinterpret_cast<DirectX::XMFLOAT4*>(&vertexData->m_RawVertexData[vertexSize * vert.index + offset]);
			DirectX::XMStoreFloat4(position, DirectX::XMVectorAdd(DirectX::XMLoadFloat4(position), DirectX::XMVectorScale(vert.delta, factor)));
		}
	}
	else
	{
		_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertexCount);
	}
}

void TriShapePackedVertexData::ApplyMorphRaw(UInt16 vertCount, void * data, float factor)
{
	NiPoint3 * vertices = static_cast<NiPoint3*>(data);
	if (!vertices)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			UInt32 vertexIndex = vert.index;
			vertices[vertexIndex].x += (float)vert.delta.m128_f32[0] * factor;
			vertices[vertexIndex].y += (float)vert.delta.m128_f32[1] * factor;
			vertices[vertexIndex].z += (float)vert.delta.m128_f32[2] * factor;
		}
	}
	else
	{
		_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertCount);
	}
}

void TriShapePackedVertexData::ApplyMorph(UInt16 vertexCount, NiSkinPartition::TriShape * vertexData, float factor)
{
	UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->m_VertexDesc);
	UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->m_VertexDesc, VertexAttribute::VA_POSITION);

	if (m_maxIndex < vertexCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			DirectX::XMFLOAT4 * position = reinterpret_cast<DirectX::XMFLOAT4*>(&vertexData->m_RawVertexData[vertexSize * vert.index + offset]);
			DirectX::XMStoreFloat4(position, DirectX::XMVectorAdd(DirectX::XMLoadFloat4(position), DirectX::XMVectorScale(vert.delta, factor)));
		}
	}
	else
	{
		_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertexCount);
	}
}

void TriShapePackedUVData::ApplyMorphRaw(UInt16 vertCount, void * data, float factor)
{
	UVCoord * deltas = static_cast<UVCoord*>(data);
	if (!deltas)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & delta : m_uvDeltas)
		{
			UInt32 vertexIndex = delta.index;
			deltas[vertexIndex].u += (float)delta.u * m_multiplier * factor;
			deltas[vertexIndex].v += (float)delta.v * m_multiplier * factor;
		}
	}
	else
	{
		_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertCount);
	}
}

void TriShapePackedUVData::ApplyMorph(UInt16 vertexCount, NiSkinPartition::TriShape * vertexData, float factor)
{
	VertexFlags flags = NiSkinPartition::GetVertexFlags(vertexData->m_VertexDesc);
	if ((flags & VF_UV))
	{
		UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->m_VertexDesc);
		UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->m_VertexDesc, VertexAttribute::VA_TEXCOORD0);
		if (m_maxIndex < vertexCount)
		{
			for (const auto & delta : m_uvDeltas)
			{
				UVCoord * texCoord = reinterpret_cast<UVCoord*>(&vertexData->m_RawVertexData[vertexSize * delta.index + offset]);
				texCoord->u += (float)delta.u * m_multiplier * factor;
				texCoord->v += (float)delta.v * m_multiplier * factor;
			}
		}
		else
		{
			_ERROR("%s - Failed to apply morphs to geometry - morphs largest index is %d mesh vertex size is %d", __FUNCTION__, m_maxIndex, (UInt32)vertexCount);
		}
	}
}

void BodyMorphMap::ApplyMorphs(TESObjectREFR * refr, std::function<void(const TriShapeVertexDataPtr, float)> vertexFunctor, std::function<void(const TriShapeVertexDataPtr, float)> uvFunctor) const
{
	for (auto & morph : *this)
	{
		float morphFactor = g_bodyMorphInterface.GetBodyMorphs(refr, morph.first);
		if(vertexFunctor && morph.second.first)
			vertexFunctor(morph.second.first, morphFactor);
		if (uvFunctor && morph.second.second)
			uvFunctor(morph.second.second, morphFactor);
	}
}

bool BodyMorphMap::HasMorphs(TESObjectREFR * refr) const
{
	for (auto & morph : *this)
	{
		float morphFactor = g_bodyMorphInterface.GetBodyMorphs(refr, morph.first);
		if (morphFactor != 0.0f)
			return true;
	}

	return false;
}
/*
class ShadowSceneNode
{
public:
	static ShadowSceneNode * GetSingleton()
	{
		return *((ShadowSceneNode **)0x01BA7680);
	}

	MEMBER_FN_PREFIX(ShadowSceneNode);
	DEFINE_MEMBER_FN(UnkFunc0, void, 0xC77EB0, NiAVObject * object, UInt8 unk1, UInt8 unk2);
	DEFINE_MEMBER_FN(UnkFunc1, void, 0xC781D0, NiAVObject * object, UInt8 unk1);
};
*/

// 567250
// 5F48C0

//typedef UInt32(*_DetachArmorModel)(NiAVObject * object);
//extern const _DetachArmorModel DetachArmorModel = (_DetachArmorModel)0x005F4A70;


//typedef UInt32(*_DetachObjectModel)(UInt32 handle, NiAVObject * object);
//extern const _DetachObjectModel DetachObjectModel = (_DetachObjectModel)0x0046BF90;



// 46BF90
#ifndef _NO_REATTACH
typedef UInt32(*_UpdateReferenceNode)(UInt32 handle, NiAVObject * object);
extern const _UpdateReferenceNode UpdateReferenceNode = (_UpdateReferenceNode)0x0046BF90;
#endif

#include <fstream>
#include <regex>

namespace MinD3D11
{
	struct D3D11_BOX {
		UINT left;
		UINT top;
		UINT front;
		UINT right;
		UINT bottom;
		UINT back;
	};

	typedef void (*_UpdateSubresource)(
		struct ID3D11DeviceContext4	*pContext,
		struct ID3D11Resource		*pDstResource,
		UINT						DstSubresource,
		const D3D11_BOX				*pDstBox,
		const void					*pSrcData,
		UINT						SrcRowPitch,
		UINT						SrcDepthPitch
	);

	struct ID3D11DeviceContext4Vtbl
	{
		void * _functions[0x30];
		_UpdateSubresource UpdateSubresource;
	};

	struct ID3D11DeviceContext4
	{
		ID3D11DeviceContext4Vtbl *vtable;
	};
};

void MorphFileCache::ApplyMorph(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching, const std::pair<BSFixedString, BodyMorphMap> & bodyMorph, std::mutex * dxLock)
{
	BSFixedString nodeName = bodyMorph.first.data;
	BSGeometry * geometry = rootNode->GetAsBSGeometry();
	NiGeometry * legacyGeometry = rootNode->GetAsNiGeometry();
	NiAVObject * bodyNode = geometry ? geometry : legacyGeometry ? legacyGeometry : rootNode->GetObjectByName(&nodeName.data);
	if (bodyNode)
	{
		NiGeometry * legacyGeometry = bodyNode->GetAsNiGeometry();
		if (legacyGeometry)
		{
			NiGeometryData * geometryData = niptr_cast<NiGeometryData>(legacyGeometry->m_spModelData);
			NiSkinInstance * skinInstance = niptr_cast<NiSkinInstance>(legacyGeometry->m_spSkinInstance);
			if (geometryData && skinInstance) {
				// Undo morphs on the old shape
				BSFaceGenBaseMorphExtraData * bodyData = (BSFaceGenBaseMorphExtraData *)legacyGeometry->GetExtraData("FOD");
				if (bodyData) {
					NiAutoRefCounter arc(bodyData);
					// Undo old morphs for this trishape
					for (UInt16 i = 0; i < geometryData->m_usVertices; i++) {
						if (!isAttaching)
							geometryData->m_pkVertex[i] -= bodyData->vertexData[i];
						bodyData->vertexData[i] = NiPoint3(0, 0, 0);
					}

					geometryData->m_usDirtyFlags = 0x0001;
				}

				// Apply new morphs to new shape
				if (bodyMorph.second.HasMorphs(refr))
				{
					NiGeometryData * targetShapeData = nullptr;
					CALL_MEMBER_FN(geometryData, DeepCopy)((NiObject **)&targetShapeData);

					NiSkinInstance * newSkinInstance = skinInstance->Clone();
					if (newSkinInstance)
					{
						// If we've created a new skin instance, create a new partition too
						if (newSkinInstance != skinInstance)
						{
							NiSkinPartition * skinPartition = niptr_cast<NiSkinPartition>(skinInstance->m_spSkinPartition);
							if (skinPartition) {
								NiSkinPartition * newSkinPartition = nullptr;
								CALL_MEMBER_FN(skinPartition, DeepCopy)((NiObject **)&newSkinPartition);

								newSkinInstance->m_spSkinPartition = newSkinPartition;
								newSkinPartition->DecRef();
							}
						}
						if (targetShapeData) {
							// No old morphs, add one
							if (!bodyData) {
								bodyData = BSFaceGenBaseMorphExtraData::Create(targetShapeData, false);
								legacyGeometry->AddExtraData(bodyData);
							}

							if (bodyData) {
								NiAutoRefCounter arc(bodyData);
								bodyMorph.second.ApplyMorphs(refr, [&](const TriShapeVertexDataPtr morphData, float morphFactor)
								{
									morphData->ApplyMorphRaw(targetShapeData->m_usVertices, bodyData->vertexData, morphFactor);
									morphData->ApplyMorphRaw(targetShapeData->m_usVertices, targetShapeData->m_pkVertex, morphFactor);
								}, nullptr);
							}

							legacyGeometry->m_spModelData = targetShapeData;
							legacyGeometry->m_spSkinInstance = newSkinInstance;
							targetShapeData->DecRef();

							targetShapeData->m_usDirtyFlags = 0x0001;
						}
					}
				}
			}
		}

		BSGeometry * bodyGeometry = bodyNode->GetAsBSGeometry();
		if (bodyGeometry)
		{
			NiSkinInstance * skinInstance = niptr_cast<NiSkinInstance>(bodyGeometry->m_spSkinInstance);
			if (skinInstance) {
				NiSkinPartition * skinPartition = niptr_cast<NiSkinPartition>(skinInstance->m_spSkinPartition);
				if (skinPartition) {
					// Undo morphs on the old shape
					NiBinaryExtraData * bodyData = (NiBinaryExtraData *)bodyGeometry->GetExtraData("SHAPEDATA");
					
					bool existingMorphs = !isAttaching && bodyData;

					// Apply new morphs to new shape
					if (bodyMorph.second.HasMorphs(refr) || existingMorphs)
					{
						if (skinPartition)
						{
							NiSkinPartition * newSkinPartition = nullptr;
							CALL_MEMBER_FN(skinPartition, DeepCopy)((NiObject **)&newSkinPartition);

							// Reset the Vertices directly
							if (bodyData && newSkinPartition)
							{
								if (!isAttaching)
								{
									// Overwrite the vertex data with the original source data
									for (UInt32 p = 0; p < newSkinPartition->m_uiPartitions; ++p)
									{
										auto & partition = newSkinPartition->m_pkPartitions[p];
										UInt32 vertexSize = newSkinPartition->GetVertexSize(partition.vertexDesc);

										memcpy(partition.shapeData->m_RawVertexData, bodyData->m_data, newSkinPartition->vertexCount * vertexSize);
									}
								}
							}

							if (newSkinPartition)
							{
								// No existing morphs, copy the current vertex block
								if (!bodyData) {
									auto & partition = newSkinPartition->m_pkPartitions[0];
									UInt32 vertexSize = newSkinPartition->GetVertexSize(partition.vertexDesc);

									bodyData = NiBinaryExtraData::Create("SHAPEDATA", reinterpret_cast<char*>(partition.shapeData->m_RawVertexData), newSkinPartition->vertexCount * vertexSize);
									bodyGeometry->AddExtraData(bodyData);
								}

								if (bodyData)
								{
									auto & partition = newSkinPartition->m_pkPartitions[0];
									UInt32 vertexSize = newSkinPartition->GetVertexSize(partition.vertexDesc);
									UInt32 vertexCount = newSkinPartition->vertexCount;

									std::function<void(const TriShapeVertexDataPtr, float)> vertexMorpher = [&](const TriShapeVertexDataPtr morphData, float morphFactor)
									{
										if (morphFactor != 0.0f)
										{
											morphData->ApplyMorph(vertexCount, partition.shapeData, morphFactor);
										}
									};

									std::function<void(const TriShapeVertexDataPtr, float)> uvMorpher = [&](const TriShapeVertexDataPtr morphData, float morphFactor)
									{
										if (morphFactor != 0.0f)
										{
											morphData->ApplyMorph(vertexCount, partition.shapeData, morphFactor);
										}
									};

									// Applies all morphs for this shape
									bodyMorph.second.ApplyMorphs(refr, vertexMorpher, bodyMorph.second.HasUV() ? uvMorpher : nullptr);

									// Copy the vertex data back onto the GPU
									auto deviceContext = reinterpret_cast<MinD3D11::ID3D11DeviceContext4*>(g_renderManager->context);
									if(dxLock) dxLock->lock();
									deviceContext->vtable->UpdateSubresource(reinterpret_cast<struct ID3D11DeviceContext4*>(deviceContext), reinterpret_cast<MinD3D11::ID3D11Resource*>(partition.shapeData->m_VertexBuffer), 0, nullptr, partition.shapeData->m_RawVertexData, vertexCount * vertexSize, 0);
									if (dxLock) dxLock->unlock();
									// Copy the remaining partitions from the first partition
									for (UInt32 p = 1; p < newSkinPartition->m_uiPartitions; ++p)
									{
										auto & pPartition = newSkinPartition->m_pkPartitions[p];
										memcpy(pPartition.shapeData->m_RawVertexData, partition.shapeData->m_RawVertexData, newSkinPartition->vertexCount * vertexSize);
										if (dxLock) dxLock->lock();
										deviceContext->vtable->UpdateSubresource(reinterpret_cast<struct ID3D11DeviceContext4*>(deviceContext), reinterpret_cast<MinD3D11::ID3D11Resource*>(pPartition.shapeData->m_VertexBuffer), 0, nullptr, pPartition.shapeData->m_RawVertexData, vertexCount * vertexSize, 0);
										if (dxLock) dxLock->unlock();
									}
								}

								skinInstance->m_spSkinPartition = newSkinPartition;
								newSkinPartition->DecRef(); // DeepCopy started refcount at 1
							}
						}
					}
				}
			}
		}
	}
}

void MorphFileCache::ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching)
{
	if (g_parallelMorphing)
	{
		std::mutex dxLock;
		concurrency::structured_task_group task_group;
		std::vector<concurrency::task_handle<std::function<void()>>> task_list;
		for (const auto & it : vertexMap)
		{
			task_list.push_back(concurrency::make_task<std::function<void()>>([&]()
			{
				ApplyMorph(refr, rootNode, isAttaching, it, &dxLock);
			}));
		}
		for (auto & task : task_list)
		{
			task_group.run(task);
		}

		task_group.wait();
	}
	else
	{
		for (const auto & it : vertexMap)
		{
			ApplyMorph(refr, rootNode, isAttaching, it, nullptr);
		}
	}
}

void MorphCache::ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching)
{
	MorphFileCache * fileCache = nullptr;

	// Find the BODYTRI and cache it
	VisitObjects(rootNode, [&](NiAVObject* object) {
		NiStringExtraData * stringData = ni_cast(object->GetExtraData("BODYTRI"), NiStringExtraData);
		if (stringData) {
			BSFixedString filePath = CreateTRIPath(stringData->m_pString);
			CacheFile(filePath.data);
			auto & it = m_data.find(filePath);
			if (it != m_data.end()) {
				fileCache = &it->second;
				return true;
			}
		}

		return false;
	});

	if (fileCache && !fileCache->vertexMap.empty())
		fileCache->ApplyMorphs(refr, rootNode, isAttaching);

	Shrink();
}

class EquippedItemCollector
{
public:
	typedef std::vector<TESForm*> FoundItems;

	EquippedItemCollector() {}

	bool Accept(InventoryEntryData* pEntryData)
	{
		if (!pEntryData)
			return true;

		if (pEntryData->countDelta < 1)
			return true;

		ExtendDataList* pExtendList = pEntryData->extendDataList;
		if (!pExtendList)
			return true;

		SInt32 n = 0;
		BaseExtraList* pExtraDataList = pExtendList->GetNthItem(n);
		while (pExtraDataList)
		{
			if (pExtraDataList->HasType(kExtraData_Worn) || pExtraDataList->HasType(kExtraData_WornLeft))
				m_found.push_back(pEntryData->type);

			n++;
			pExtraDataList = pExtendList->GetNthItem(n);
		}

		return true;
	}

	FoundItems& Found()
	{
		return m_found;
	}
private:
	FoundItems	m_found;
};

TESObjectARMO * GetActorSkin(Actor * actor)
{
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (npc) {
		if (npc->skinForm.skin)
			return npc->skinForm.skin;
	}
	TESRace * actorRace = actor->race;
	if (actorRace)
		return actorRace->skin.skin;

	if (npc) {
		actorRace = npc->race.race;
		if (actorRace)
			return actorRace->skin.skin;
	}

	return NULL;
}

void MorphCache::UpdateMorphs(TESObjectREFR * refr)
{
	if(!refr)
		return;

	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if (!actor)
		return;

	EquippedItemCollector::FoundItems foundData;
	ExtraContainerChanges * extraContainer = static_cast<ExtraContainerChanges*>(refr->extraData.GetByType(kExtraData_ContainerChanges));
	if (extraContainer) {
		if (extraContainer->data && extraContainer->data->objList) {
			EquippedItemCollector itemFinder;
			extraContainer->data->objList->Visit(itemFinder);
			foundData = itemFinder.Found();
		}
	}

	TESObjectARMO * skin = GetActorSkin(actor);
	if (skin)
		foundData.push_back(skin);

	for (auto foundItem : foundData) {
		TESObjectARMO * armor = DYNAMIC_CAST(foundItem, TESForm, TESObjectARMO);
		if (armor) {
			for (UInt32 i = 0; i < armor->armorAddons.count; i++) {
				TESObjectARMA * arma = NULL;
				if (armor->armorAddons.GetNthItem(i, arma)) {
					if (arma->isValidRace(actor->race)) { // Only search AAs that fit this race
						VisitArmorAddon(actor, armor, arma, [&](bool isFirstPerson, NiNode * skeletonRoot, NiAVObject * armorNode) {
							ApplyMorphs(refr, armorNode);
#ifdef _NO_REATTACH
							g_overlayInterface.RebuildOverlays(armor->bipedObject.GetSlotMask(), arma->biped.GetSlotMask(), refr, skeletonRoot, armorNode);
#endif
						});
					}
				}
			}
		}
	}
#ifndef _NO_REATTACH
	//CALL_MEMBER_FN(actor->processManager, SetEquipFlag)(ActorProcessManager::kFlags_Unk01 | ActorProcessManager::kFlags_Unk02 | ActorProcessManager::kFlags_Mobile);
	//CALL_MEMBER_FN(actor->processManager, UpdateEquipment)(actor);
#endif
}

BSFixedString MorphCache::CreateTRIPath(const char * relativePath)
{
	if(relativePath == "")
		return BSFixedString("");

	std::string targetPath = "meshes\\";
	targetPath += std::string(relativePath);
	std::transform(targetPath.begin(), targetPath.end(), targetPath.begin(), ::tolower);
	return BSFixedString(targetPath.c_str());
}

void MorphCache::Shrink()
{
	while (totalMemory > memoryLimit && m_data.size() > 0)
	{
		auto & it = std::min_element(m_data.begin(), m_data.end(), [](std::pair<BSFixedString, MorphFileCache> a, std::pair<BSFixedString, MorphFileCache> b)
		{
			return (a.second.accessed < b.second.accessed);
		});

		UInt32 size = it->second.vertexMap.memoryUsage;
		m_data.erase(it);
		totalMemory -= size;
	}

	if (m_data.size() == 0) // Just in case we erased but messed up
		totalMemory = sizeof(MorphCache);
}

bool MorphCache::CacheFile(const char * relativePath)
{
	BSFixedString filePath(relativePath);
	if(relativePath == "")
		return false;

	auto & it = m_data.find(filePath);
	if (it != m_data.end()) {
		it->second.accessed = std::time(nullptr);
		it->second.accessed = std::time(nullptr);
		return false;
	}

#ifdef _DEBUG
	_MESSAGE("%s - Parsing: %s", __FUNCTION__, filePath.data);
#endif

	BSResourceNiBinaryStream binaryStream(filePath.data);
	if(binaryStream.IsValid())
	{
		TriShapeMap trishapeMap;

		UInt32 fileFormat = 0;
		trishapeMap.memoryUsage += binaryStream.Read((char *)&fileFormat, sizeof(UInt32));

		bool packed = false;
		if (fileFormat != 'TRI\0' && fileFormat != 'TRIP')
			return false;

		if (fileFormat == 'TRIP')
			packed = true;

		UInt32 trishapeCount = 0;
		if (!packed)
			trishapeMap.memoryUsage += binaryStream.Read((char *)&trishapeCount, sizeof(UInt32));
		else
			trishapeMap.memoryUsage += binaryStream.Read((char *)&trishapeCount, sizeof(UInt16));

		char trishapeNameRaw[MAX_PATH];
		for (UInt32 i = 0; i < trishapeCount; i++)
		{
			memset(trishapeNameRaw, 0, MAX_PATH);

			UInt8 size = 0;
			trishapeMap.memoryUsage += binaryStream.Read((char *)&size, sizeof(UInt8));
			trishapeMap.memoryUsage += binaryStream.Read(trishapeNameRaw, size);
			BSFixedString trishapeName(trishapeNameRaw);

#ifdef _DEBUG
			_MESSAGE("%s - Reading TriShape %s", __FUNCTION__, trishapeName.data);
#endif

			if (!packed) {
				UInt32 trishapeBlockSize = 0;
				trishapeMap.memoryUsage += binaryStream.Read((char *)&trishapeBlockSize, sizeof(UInt32));
			}

			char morphNameRaw[MAX_PATH];

			BodyMorphMap morphMap;

			UInt32 morphCount = 0;
			if (!packed)
				trishapeMap.memoryUsage += binaryStream.Read((char *)&morphCount, sizeof(UInt32));
			else
				trishapeMap.memoryUsage += binaryStream.Read((char *)&morphCount, sizeof(UInt16));

			for (UInt32 j = 0; j < morphCount; j++)
			{
				memset(morphNameRaw, 0, MAX_PATH);

				UInt8 tsize = 0;
				trishapeMap.memoryUsage += binaryStream.Read((char *)&tsize, sizeof(UInt8));
				trishapeMap.memoryUsage += binaryStream.Read(morphNameRaw, tsize);
				BSFixedString morphName(morphNameRaw);

#ifdef _DEBUG
				_MESSAGE("%s - Reading Morph %s at (%08X)", __FUNCTION__, morphName.data, binaryStream.GetOffset());
#endif
				if (tsize == 0) {
					_WARNING("%s - %s - Read empty name morph at (%08X)", __FUNCTION__, filePath.data, binaryStream.GetOffset());
				}

				if (!packed) {
					UInt32 morphBlockSize = 0;
					trishapeMap.memoryUsage += binaryStream.Read((char *)&morphBlockSize, sizeof(UInt32));
				}

				UInt32 vertexNum = 0;
				float multiplier = 0.0f;
				if(!packed) {
					trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexNum, sizeof(UInt32));
				}
				else {
					trishapeMap.memoryUsage += binaryStream.Read((char *)&multiplier, sizeof(float));
					trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexNum, sizeof(UInt16));
				}

				if (vertexNum == 0) {
					_WARNING("%s - %s - Read morph %s on %s with no vertices at (%08X)", __FUNCTION__, filePath.data, morphName.data, trishapeName.data, binaryStream.GetOffset());
				}
				if (multiplier == 0.0f) {
					_WARNING("%s - %s - Read morph %s on %s with zero multiplier at (%08X)", __FUNCTION__, filePath.data, morphName.data, trishapeName.data, binaryStream.GetOffset());
				}

#ifdef _DEBUG
				_MESSAGE("%s - Total Vertices read: %d at (%08X)", __FUNCTION__, vertexNum, binaryStream.GetOffset());
#endif
				if (vertexNum > (std::numeric_limits<UInt16>::max)())
				{
					_ERROR("%s - %s - Too many vertices for %s on %s read: %d at (%08X)", __FUNCTION__, filePath.data, morphName.data, vertexNum, trishapeName.data, binaryStream.GetOffset());
					return false;
				}

				TriShapeVertexDataPtr vertexData;
				TriShapeFullVertexDataPtr fullVertexData;
				TriShapePackedVertexDataPtr packedVertexData;
				if (!packed)
				{
					fullVertexData = std::make_shared<TriShapeFullVertexData>();
					for (UInt32 k = 0; k < vertexNum; k++)
					{
						TriShapeVertexDelta vertexDelta;
						float x, y, z;
						trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexDelta.index, sizeof(UInt32));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&x, sizeof(float));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&y, sizeof(float));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&z, sizeof(float));
						vertexDelta.delta.m128_f32[0] = (float)x * multiplier;
						vertexDelta.delta.m128_f32[1] = (float)y * multiplier;
						vertexDelta.delta.m128_f32[2] = (float)z * multiplier;
						vertexDelta.delta.m128_f32[3] = 0;

						if (vertexDelta.index > fullVertexData->m_maxIndex)
							fullVertexData->m_maxIndex = vertexDelta.index;

						fullVertexData->m_vertexDeltas.push_back(vertexDelta);
					}

					vertexData = fullVertexData;
				}
				else
				{
					packedVertexData = std::make_shared<TriShapePackedVertexData>();
					packedVertexData->m_multiplier = multiplier;

					for (UInt32 k = 0; k < vertexNum; k++)
					{
						TriShapePackedVertexDelta vertexDelta;
						SInt16 x, y, z;
						trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexDelta.index, sizeof(UInt16));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&x, sizeof(SInt16));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&y, sizeof(SInt16));
						trishapeMap.memoryUsage += binaryStream.Read((char *)&z, sizeof(SInt16));
						vertexDelta.delta.m128_f32[0] = (float)x * multiplier;
						vertexDelta.delta.m128_f32[1] = (float)y * multiplier;
						vertexDelta.delta.m128_f32[2] = (float)z * multiplier;
						vertexDelta.delta.m128_f32[3] = 0;

						if (vertexDelta.index > packedVertexData->m_maxIndex)
							packedVertexData->m_maxIndex = vertexDelta.index;

						packedVertexData->m_vertexDeltas.push_back(vertexDelta);
					}

					vertexData = packedVertexData;
				}

				morphMap.emplace(morphName, std::make_pair(vertexData, nullptr));
			}

			trishapeMap.emplace(trishapeName, morphMap);
		}

		UInt16 UVShapeCount = 0;
		trishapeMap.memoryUsage += binaryStream.Read((char *)&UVShapeCount, sizeof(UInt16));
		for (UInt32 i = 0; i < trishapeCount; i++)
		{
			memset(trishapeNameRaw, 0, MAX_PATH);

			UInt8 size = 0;
			trishapeMap.memoryUsage += binaryStream.Read((char *)&size, sizeof(UInt8));
			trishapeMap.memoryUsage += binaryStream.Read(trishapeNameRaw, size);
			BSFixedString trishapeName(trishapeNameRaw);

#ifdef _DEBUG
			_MESSAGE("%s - Reading TriShape UV %s", __FUNCTION__, trishapeName.data);
#endif

			if (!packed) {
				UInt32 trishapeBlockSize = 0;
				trishapeMap.memoryUsage += binaryStream.Read((char *)&trishapeBlockSize, sizeof(UInt32));
			}

			char morphNameRaw[MAX_PATH];

			BodyMorphMap uvMorphMap;

			UInt32 morphCount = 0;
			if (!packed)
				trishapeMap.memoryUsage += binaryStream.Read((char *)&morphCount, sizeof(UInt32));
			else
				trishapeMap.memoryUsage += binaryStream.Read((char *)&morphCount, sizeof(UInt16));

			for (UInt32 j = 0; j < morphCount; j++)
			{
				memset(morphNameRaw, 0, MAX_PATH);

				UInt8 tsize = 0;
				trishapeMap.memoryUsage += binaryStream.Read((char *)&tsize, sizeof(UInt8));
				trishapeMap.memoryUsage += binaryStream.Read(morphNameRaw, tsize);
				BSFixedString morphName(morphNameRaw);

#ifdef _DEBUG
				_MESSAGE("%s - Reading UV Morph %s at (%08X)", __FUNCTION__, morphName.data, binaryStream.GetOffset());
#endif
				if (tsize == 0) {
					_WARNING("%s - %s - Read empty name morph at (%08X)", __FUNCTION__, filePath.data, binaryStream.GetOffset());
				}

				UInt32 vertexNum = 0;
				float multiplier = 0.0f;

				trishapeMap.memoryUsage += binaryStream.Read((char *)&multiplier, sizeof(float));
				trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexNum, sizeof(UInt16));

				if (vertexNum == 0) {
					_WARNING("%s - %s - Read morph %s on %s with no vertices at (%08X)", __FUNCTION__, filePath.data, morphName.data, trishapeName.data, binaryStream.GetOffset());
				}
				if (multiplier == 0.0f) {
					_WARNING("%s - %s - Read morph %s on %s with zero multiplier at (%08X)", __FUNCTION__, filePath.data, morphName.data, trishapeName.data, binaryStream.GetOffset());
				}

#ifdef _DEBUG
				_MESSAGE("%s - Total Vertices read: %d at (%08X)", __FUNCTION__, vertexNum, binaryStream.GetOffset());
#endif
				if (vertexNum > (std::numeric_limits<UInt16>::max)())
				{
					_ERROR("%s - %s - Too many vertices for %s on %s read: %d at (%08X)", __FUNCTION__, filePath.data, morphName.data, vertexNum, trishapeName.data, binaryStream.GetOffset());
					return false;
				}

				TriShapePackedUVDataPtr packedUVData;
				
				packedUVData = std::make_shared<TriShapePackedUVData>();
				packedUVData->m_multiplier = multiplier;

				for (UInt32 k = 0; k < vertexNum; k++)
				{
					TriShapePackedUVDelta vertexDelta;
					trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexDelta.index, sizeof(UInt16));
					trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexDelta.u, sizeof(SInt16));
					trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexDelta.v, sizeof(SInt16));

					if (vertexDelta.index > packedUVData->m_maxIndex)
						packedUVData->m_maxIndex = vertexDelta.index;

					packedUVData->m_uvDeltas.push_back(vertexDelta);
				}

				trishapeMap[trishapeName][morphName].second = packedUVData;
				trishapeMap[trishapeName].m_hasUV = true;
			}
		}

		MorphFileCache fileCache;
		fileCache.accessed = std::time(nullptr);
		fileCache.vertexMap = trishapeMap;

		Lock();
		m_data.emplace(relativePath, fileCache);
		totalMemory += trishapeMap.memoryUsage;
		Release();

		_DMESSAGE("%s - Loaded %s (%d bytes)", __FUNCTION__, relativePath, trishapeMap.memoryUsage);
		return true;
	}
	else
	{
		_ERROR("%s - Failed to load %s", __FUNCTION__, relativePath);
	}


	return false;
}

void BodyMorphInterface::SetCacheLimit(UInt32 limit)
{
	morphCache.memoryLimit = limit;
}

void BodyMorphInterface::ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase)
{
	if(!refr || !rootNode) {
#ifdef _DEBUG
		_MESSAGE("%s - Error no reference or node found.", __FUNCTION__);
#endif
		return;
	}

#ifdef _DEBUG
	_MESSAGE("%s - Applying Vertex Diffs to %08X on %s", __FUNCTION__, refr->formID, rootNode->m_name);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.ApplyMorphs(refr, rootNode, erase);
	}
}

void BodyMorphInterface::ApplyBodyMorphs(TESObjectREFR * refr)
{
#ifdef _DEBUG
	_MESSAGE("%s - Updating morphs for %08X.", __FUNCTION__, refr->formID);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.UpdateMorphs(refr);
	}
}

NIOVTaskUpdateModelWeight::NIOVTaskUpdateModelWeight(Actor * actor)
{
	m_formId = actor->formID;
}

void NIOVTaskUpdateModelWeight::Dispose(void)
{
	delete this;
}

void NIOVTaskUpdateModelWeight::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if(actor) {
		g_bodyMorphInterface.ApplyBodyMorphs(actor);
	}
}

void BodyMorphInterface::VisitMorphs(TESObjectREFR * actor, std::function<void(BSFixedString name, std::unordered_map<BSFixedString, float> * map)> functor)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & morph : it->second)
		{
			functor(morph.first, &morph.second);
		}
	}
}

void BodyMorphInterface::VisitKeys(TESObjectREFR * actor, BSFixedString name, std::function<void(BSFixedString, float)> functor)
{
	UInt64 handle = g_overrideInterface.GetHandle(actor, TESObjectREFR::kTypeID);
	auto & it = actorMorphs.m_data.find(handle);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(name);
		if (mit != it->second.end())
		{
			for (auto & morph : mit->second)
			{
				functor(morph.first, morph.second);
			}
		}
	}
}


void BodyMorphInterface::UpdateModelWeight(TESObjectREFR * refr, bool immediate)
{
	Actor * actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
	if(actor) {
		NIOVTaskUpdateModelWeight * updateTask = new NIOVTaskUpdateModelWeight(actor);
		if (immediate) {
			updateTask->Run();
			updateTask->Dispose();
		}
		else
		{
			g_task->AddTask(updateTask);
		}
	}
}

bool BodyMorphInterface::ReadBodyMorphTemplates(BSFixedString filePath)
{
	BSResourceNiBinaryStream file(filePath.data);
	if (!file.IsValid()) {
		return false;
	}

	UInt32 lineCount = 0;
	std::string str = "";

	while (BSReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if (str.length() == 0)
			continue;
		if (str.at(0) == '#')
			continue;

		std::vector<std::string> side = std::explode(str, '=');
		if (side.size() < 2) {
			_ERROR("%s - Error - Line (%d) loading a morph from %s has no left-hand side.", __FUNCTION__, lineCount, filePath.data);
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		BSFixedString templateName = lSide.c_str();

		BodyGenTemplatePtr bodyGenSets = std::make_shared<BodyGenTemplate>();

		std::string error = "";
		std::vector<std::string> sets = std::explode(rSide, '/');
		for (UInt32 i = 0; i < sets.size(); i++) {
			sets[i] = std::trim(sets[i]);

			BodyGenMorphs bodyMorphs;

			std::vector<std::string> morphs = std::explode(sets[i], ',');
			for (UInt32 j = 0; j < morphs.size(); j++) {
				morphs[j] = std::trim(morphs[j]);

				std::vector<std::string> selectors = std::explode(morphs[j], '|');

				BodyGenMorphSelector selector;

				for (UInt32 k = 0; k < selectors.size(); k++) {
					selectors[k] = std::trim(selectors[k]);

					std::vector<std::string> pairs = std::explode(selectors[k], '@');
					if (pairs.size() < 2) {
						error = "Must have value pair with @";
						break;
					}

					std::string morphName = std::trim(pairs[0]);
					if (morphName.length() == 0) {
						error = "Empty morph name";
						break;
					}

					std::string morphValues = std::trim(pairs[1]);
					if (morphValues.length() == 0) {
						error = "Empty values";
						break;
					}

					float lowerValue = 0;
					float upperValue = 0;

					std::vector<std::string> range = std::explode(morphValues, ':');
					if (range.size() > 1) {
						std::string lowerRange = std::trim(range[0]);
						if (lowerRange.length() == 0) {
							error = "Empty lower range";
							break;
						}

						lowerValue = strtof(lowerRange.c_str(), NULL);

						std::string upperRange = std::trim(range[1]);
						if (upperRange.length() == 0) {
							error = "Empty upper range";
							break;
						}

						upperValue = strtof(upperRange.c_str(), NULL);
					}
					else {
						lowerValue = strtof(morphValues.c_str(), NULL);
						upperValue = lowerValue;
					}

					BodyGenMorphData morphData;
					morphData.name = morphName.c_str();
					morphData.lower = lowerValue;
					morphData.upper = upperValue;
					selector.push_back(morphData);
				}

				if (error.length() > 0)
					break;

				bodyMorphs.push_back(selector);
			}

			if (error.length() > 0)
				break;

			bodyGenSets->push_back(bodyMorphs);
		}

		if (error.length() > 0) {
			_ERROR("%s - Error - Line (%d) could not parse morphs from %s. (%s)", __FUNCTION__, lineCount, filePath.data, error.c_str());
			continue;
		}

		if (bodyGenSets->size() > 0)
			bodyGenTemplates[templateName] = bodyGenSets;
	}

	return true;
}

bool BodyMorphInterface::ReadBodyMorphs(BSFixedString filePath)
{
	BSResourceNiBinaryStream file(filePath.data);
	if (!file.IsValid()) {
		return false;
	}

	UInt32 lineCount = 0;
	std::string str = "";
	UInt32 totalTargets = 0;

	while (BSReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if (str.length() == 0)
			continue;
		if (str.at(0) == '#')
			continue;

		std::vector<std::string> side = std::explode(str, '=');
		if (side.size() < 2) {
			_ERROR("%s - Error - %s (%d) loading a morph from %s has no left-hand side.", __FUNCTION__, filePath.data, lineCount);
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		std::vector<std::string> form = std::explode(lSide, '|');
		if (form.size() < 2) {
			_ERROR("%s - Error - %s (%d) morph left side from %s missing mod name or formID.", __FUNCTION__, filePath.data, lineCount);
			continue;
		}

		std::vector<TESNPC*> activeNPCs;
		std::string modNameText = std::trim(form[0]);
		if (_strnicmp(modNameText.c_str(), "All", 3) == 0)
		{
			std::string genderText = std::trim(form[1]);
			UInt8 gender = 0;
			if (_strnicmp(genderText.c_str(), "Male", 4) == 0)
				gender = 0;
			else if (_strnicmp(genderText.c_str(), "Female", 6) == 0)
				gender = 1;
			else {
				_ERROR("%s - Error - %s (%d) invalid gender specified.", __FUNCTION__, filePath.data, lineCount);
				continue;
			}

			DataHandler * dataHandler = DataHandler::GetSingleton();

			bool raceFilter = false;
			TESRace * foundRace = nullptr;
			if (form.size() >= 3)
			{
				std::string raceText = std::trim(form[2]);
				for (UInt32 i = 0; i < dataHandler->races.count; i++)
				{
					TESRace * race = nullptr;
					if (dataHandler->races.GetNthItem(i, race)) {
						if (race && _strnicmp(raceText.c_str(), race->editorId.data, raceText.size()) == 0) {
							foundRace = race;
							break;
						}
					}
				}
				raceFilter = true;

				if (foundRace == nullptr)
				{
					_ERROR("%s - Error - %s (%d) invalid race %s specified.", __FUNCTION__, filePath.data, lineCount, raceText.c_str());
					continue;
				}
			}

			for (UInt32 i = 0; i < dataHandler->npcs.count; i++)
			{
				TESNPC * npc = nullptr;
				if (dataHandler->npcs.GetNthItem(i, npc)) {
					if (npc && npc->nextTemplate == nullptr && CALL_MEMBER_FN(npc, GetSex)() == gender && (raceFilter == false || npc->race.race == foundRace))
						activeNPCs.push_back(npc);
				}
			}
		}
		else
		{
			BSFixedString modText(modNameText.c_str());
			UInt8 modIndex = DataHandler::GetSingleton()->GetModIndex(modText.data);
			if (modIndex == -1) {
				_WARNING("%s - Warning - %s (%d) Mod %s not a loaded mod.", __FUNCTION__, filePath.data, lineCount, modText.data);
				continue;
			}

			std::string formIdText = std::trim(form[1]);
			UInt32 formLower = strtoul(formIdText.c_str(), NULL, 16);

			if (formLower == 0) {
				_ERROR("%s - Error - %s (%d) invalid formID.", __FUNCTION__, filePath.data, lineCount);
				continue;
			}

			UInt32 formId = modIndex << 24 | formLower & 0xFFFFFF;
			TESForm * foundForm = LookupFormByID(formId);
			if (!foundForm) {
				_ERROR("%s - Error - %s (%d) invalid form %08X.", __FUNCTION__, filePath.data, lineCount, formId);
				continue;
			}

			// Dont apply randomization to the player
			if (formId == 7)
				continue;

			TESNPC * npc = DYNAMIC_CAST(foundForm, TESForm, TESNPC);
			if (!npc) {
				_ERROR("%s - Error - %s (%d) invalid form %08X not an ActorBase.", __FUNCTION__, filePath.data, lineCount, formId);
				continue;
			}

			activeNPCs.push_back(npc);
		}

		BodyGenDataTemplatesPtr dataTemplates = std::make_shared<BodyGenDataTemplates>();
		std::string error = "";
		std::vector<std::string> sets = std::explode(rSide, ',');
		for (UInt32 i = 0; i < sets.size(); i++) {
			sets[i] = std::trim(sets[i]);
			std::vector<std::string> selectors = std::explode(sets[i], '|');
			BodyTemplateList templateList;
			for (UInt32 k = 0; k < selectors.size(); k++) {
				selectors[k] = std::trim(selectors[k]);
				BSFixedString templateName(selectors[k].c_str());
				auto & temp = bodyGenTemplates.find(templateName);
				if (temp != bodyGenTemplates.end())
					templateList.push_back(temp->second);
				else
					_WARNING("%s - Warning - %s (%d) template %s not found.", __FUNCTION__, filePath.data, lineCount, templateName.data);
			}

			dataTemplates->push_back(templateList);
		}

		for (auto & npc : activeNPCs)
		{
			bodyGenData[npc] = dataTemplates;
#ifdef _DEBUG
			_DMESSAGE("%s - Read target %s", __FUNCTION__, npc->fullName.name.data);
#endif
		}

		totalTargets += activeNPCs.size();
	}

	_DMESSAGE("%s - Read %d target(s) from %s", __FUNCTION__, totalTargets, filePath.data);
	return true;
}

UInt32 BodyGenMorphSelector::Evaluate(std::function<void(BSFixedString, float)> eval)
{
	if (size() > 0) {
		std::random_device rd;
		std::default_random_engine gen(rd());
		std::uniform_int_distribution<> rndMorph(0, size() - 1);

		auto & bodyMorph = at(rndMorph(gen));
		std::uniform_real_distribution<> rndValue(bodyMorph.lower, bodyMorph.upper);
		float val = rndValue(gen);
		if (val != 0) {
			eval(bodyMorph.name, val);
			return 1;
		}
	}

	return 0;
}

UInt32 BodyGenMorphs::Evaluate(std::function<void(BSFixedString, float)> eval)
{
	UInt32 total = 0;
	for (auto value : *this) {
		if (value.size() < 1)
			continue;

		total += value.Evaluate(eval);
	}

	return total;
}

UInt32 BodyGenTemplate::Evaluate(std::function<void(BSFixedString, float)> eval)
{
	if (size() > 0) {
		std::random_device rd;
		std::default_random_engine gen(rd());
		std::uniform_int_distribution<> rnd(0, size() - 1);

		auto & morphs = at(rnd(gen));
		return morphs.Evaluate(eval);
	}

	return 0;
}

UInt32 BodyTemplateList::Evaluate(std::function<void(BSFixedString, float)> eval)
{
	if (size() > 0) {
		std::random_device rd;
		std::default_random_engine gen(rd());
		std::uniform_int_distribution<> rnd(0, size() - 1);

		auto & bodyTemplate = at(rnd(gen));
		return bodyTemplate->Evaluate(eval);
	}

	return 0;
}

UInt32 BodyGenDataTemplates::Evaluate(std::function<void(BSFixedString, float)> eval)
{
	UInt32 total = 0;
	for (auto & tempList : *this)
	{
		total += tempList.Evaluate(eval);
	}

	return total;
}

UInt32 BodyMorphInterface::EvaluateBodyMorphs(TESObjectREFR * actor)
{
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (actorBase) {
		BodyGenData::iterator morphSet = bodyGenData.end();
		do {
			morphSet = bodyGenData.find(actorBase);
			actorBase = actorBase->nextTemplate;
		} while (actorBase && morphSet == bodyGenData.end());

		// Found a matching template
		if (morphSet != bodyGenData.end()) {
			std::random_device rd;
			std::default_random_engine gen(rd());

			auto & templates = morphSet->second;
			UInt32 ret = templates->Evaluate([&](BSFixedString morphName, float value)
			{
				SetMorph(actor, morphName, "RSMBodyGen", value);
			});
			return ret;
		}
	}

	return 0;
}

void BodyMorphInterface::VisitStrings(std::function<void(BSFixedString)> functor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	for (auto & i1 : actorMorphs.m_data) {
		for (auto & i2 : i1.second) {
			functor(i2.first);
			for (auto & i3 : i2.second) {
				functor(i3.first);
			}
		}
	}
}

void BodyMorphInterface::VisitActors(std::function<void(TESObjectREFR*)> functor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	for (auto & actor : actorMorphs.m_data) {
		TESObjectREFR * refr = (TESObjectREFR *)g_overrideInterface.GetObject(actor.first, TESObjectREFR::kTypeID);
		if (refr) {
			functor(refr);
		}
	}
}

// Serialize Morph
void BodyMorph::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('MRPV', kVersion);

	UInt8 morphLength = strlen(m_name.data);
	intfc->WriteRecordData(&morphLength, sizeof(morphLength));
	intfc->WriteRecordData(m_name.data, morphLength);
	intfc->WriteRecordData(&m_value, sizeof(m_value));
}

bool BodyMorph::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 type, length, version;
	bool error = false;

	m_name = "";
	m_value = 0.0;

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'MRPV':
			{
				char * stringName = NULL;
				UInt8 stringLength;
				if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
				{
					_ERROR("%s - Error loading body morph name length", __FUNCTION__);
					error = true;
					return error;
				}

				stringName = new char[stringLength + 1];
				if(!intfc->ReadRecordData(stringName, stringLength)) {
					_ERROR("%s - Error loading body morph name", __FUNCTION__);
					error = true;
					return error;
				}
				stringName[stringLength] = 0;
				m_name = BSFixedString(stringName);

				if (!intfc->ReadRecordData(&m_value, sizeof(m_value))) {
					_ERROR("%s - Error loading body morph value", __FUNCTION__);
					error = true;
					return error;
				}
			}
			break;
		default:
			{
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
				error = true;
				return error;
			}
		}
	}

	return error;
}

// Serialize Morph Set
void BodyMorphSet::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('MRST', kVersion);

	UInt32 numMorphs = this->size();
	intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));

#ifdef _DEBUG
	_MESSAGE("%s - Saving %d morphs", __FUNCTION__, numMorphs);
#endif

	for(auto it = this->begin(); it != this->end(); ++it)
		const_cast<BodyMorph&>((*it)).Save(intfc, kVersion);
}

bool BodyMorphSet::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'MRST':
			{
				// Override Count
				UInt32 numMorphs = 0;
				if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
				{
					_ERROR("%s - Error loading morph set count", __FUNCTION__);
					error = true;
					return error;
				}

				for (UInt32 i = 0; i < numMorphs; i++)
				{
					BodyMorph value;
					if (!value.Load(intfc, version))
					{
						if(value.m_name == BSFixedString(""))
							continue;

#ifdef _DEBUG
						_MESSAGE("%s - Loaded morph %s %f", __FUNCTION__, value.m_name, value.m_value);
#endif

						this->insert(value);
					}
					else
					{
						_ERROR("%s - Error loading morph value", __FUNCTION__);
						error = true;
						return error;
					}
				}

				break;
			}
		default:
			{
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
				error = true;
				return error;
			}
		}
	}

	return error;
}

// Serialize ActorMorphs
void ActorMorphs::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	for(auto & morph : m_data) {
		intfc->OpenRecord('MRPH', kVersion);

		// Key
		UInt64 handle = morph.first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Morph Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		morph.second.Save(intfc, kVersion);
	}
}


// Serialize Morph Set
void BodyMorphData::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('MRDT', kVersion);

	UInt32 numMorphs = this->size();
	intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));

#ifdef _DEBUG
	_MESSAGE("%s - Saving %d morphs", __FUNCTION__, numMorphs);
#endif

	for (auto & morph : *this)
	{
		g_stringTable.WriteString<UInt16>(intfc, morph.first, kVersion);

		UInt32 numKeys = morph.second.size();
		intfc->WriteRecordData(&numKeys, sizeof(numKeys));

		for (auto & keys : morph.second)
		{
			g_stringTable.WriteString<UInt16>(intfc, keys.first, kVersion);
			intfc->WriteRecordData(&keys.second, sizeof(keys.second));
		}
	}
}

bool BodyMorphData::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 type, length, version;
	bool error = false;

	if (intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
			case 'MRDT':
			{
				UInt32 numMorphs = 0;
				if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
				{
					_ERROR("%s - Error loading morph count", __FUNCTION__);
					error = true;
					return error;
				}

				for (UInt32 i = 0; i < numMorphs; i++)
				{
					BSFixedString morphName = g_stringTable.ReadString<UInt16>(intfc, kVersion);

					UInt32 numKeys = 0;
					if (!intfc->ReadRecordData(&numKeys, sizeof(numKeys)))
					{
						_ERROR("%s - Error loading morph key count", __FUNCTION__);
						error = true;
						return error;
					}

					std::unordered_map<BSFixedString, float> pairs;
					for (UInt32 i = 0; i < numKeys; i++)
					{
						BSFixedString keyName = g_stringTable.ReadString<UInt16>(intfc, kVersion);

						float value = 0;
						if (!intfc->ReadRecordData(&value, sizeof(value))) {
							_ERROR("%s - Error loading body morph value", __FUNCTION__);
							error = true;
							return error;
						}

						// If the keys were mapped by mod name, skip them if they arent in load order
						std::string strKey(keyName.data);
						BSFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
						if (ext == BSFixedString("esp") || ext == BSFixedString("esm"))
						{
							if (!DataHandler::GetSingleton()->LookupModByName(keyName.data))
								continue;
						}

						pairs.insert_or_assign(keyName, value);
					}

					insert_or_assign(morphName, pairs);
				}

				break;
			}
			default:
			{
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
				error = true;
				return error;
			}
		}
	}

	return error;
}

bool ActorMorphs::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	bool error = false;

	UInt64 handle;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_ERROR("%s - Error loading MorphSet key", __FUNCTION__);
		error = true;
		return error;
	}

	BodyMorphSet morphSet;
	BodyMorphData morphMap;

	if (kVersion == BodyMorphInterface::kSerializationVersion1)
	{
		if (morphSet.Load(intfc, kVersion))
		{
			_ERROR("%s - Error loading MorphSet", __FUNCTION__);
			error = true;
			return error;
		}
	}
	else if (kVersion == BodyMorphInterface::kSerializationVersion2)
	{
		if (morphMap.Load(intfc, kVersion))
		{
			_ERROR("%s - Error loading MorphMap", __FUNCTION__);
			error = true;
			return error;
		}
	}

	if (!morphSet.empty())
	{
		for (auto & morph : morphSet)
		{
			morphMap[morph.m_name]["RSMLegacy"] = morph.m_value;
		}
	}

	UInt64 newHandle = 0;

	// Skip if handle is no longer valid.
	if (! intfc->ResolveHandle(handle, &newHandle))
		return false;

	if(morphMap.empty())
		return false;

	if (g_enableBodyGen)
	{
		m_data.insert_or_assign(newHandle, morphMap);

		TESObjectREFR * refr = (TESObjectREFR *)g_overrideInterface.GetObject(handle, TESObjectREFR::kTypeID);

#ifdef _DEBUG
		if (refr)
			_MESSAGE("%s - Loaded MorphSet Handle %016llX actor %s", __FUNCTION__, newHandle, CALL_MEMBER_FN(refr, GetReferenceName)());
#endif

		g_bodyMorphInterface.UpdateModelWeight(refr);
	}

	return error;
}

// Serialize Morphs
void BodyMorphInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	actorMorphs.Save(intfc, kVersion);
}

bool BodyMorphInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	return actorMorphs.Load(intfc, kVersion);
}