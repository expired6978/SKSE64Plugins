#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "AttachmentInterface.h"
#include "ActorUpdateManager.h"
#include "ShaderUtilities.h"
#include "StringTable.h"
#include "Utilities.h"

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

#include "Morpher.h"

extern ActorUpdateManager				g_actorUpdateManager;
extern BodyMorphInterface				g_bodyMorphInterface;
extern OverlayInterface					g_overlayInterface;
extern StringTable						g_stringTable;
extern SKSETaskInterface				* g_task;
extern bool								g_parallelMorphing;
extern UInt16							g_bodyMorphMode;
extern bool								g_enableBodyGen;
extern bool								g_enableBodyMorph;
extern bool								g_enableBodyNormalRecalculate;
extern bool								g_isReverting;

UInt32 BodyMorphInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

std::vector<SKEEFixedString> BodyMorphInterface::GetCachedMorphNames()
{
	std::vector<SKEEFixedString> morphList;
	std::unordered_set<SKEEFixedString> morphNames;
	morphCache.ForEachMorphFile([&](const SKEEFixedString& filePath, const MorphFileCache& morphFile)
	{
		morphFile.ForEachShape([&](const SKEEFixedString& shapeName, const BodyMorphMap& morphMap)
		{
			morphMap.ForEachMorph([&](const SKEEFixedString& morphName, const auto vertexData)
			{
				if (!morphNames.count(morphName))
				{
					morphList.push_back(morphName);
					morphNames.emplace(morphName);
				}
			});
		});
	});
	return morphList;
}

size_t BodyMorphInterface::ClearMorphCache()
{
	return morphCache.Clear();
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
			Impl_ReadBodyMorphTemplates(templatesPath.c_str());
		});

		ForEachMod([&](ModInfo * modInfo)
		{
			std::string morphsPath = fixedPath + std::string(modInfo->name) + "\\morphs.ini";
			Impl_ReadBodyMorphs(morphsPath.c_str());
		});
	}
}

void BodyMorphInterface::Revert()
{
	SimpleLocker locker(&actorMorphs.m_lock);
	actorMorphs.m_data.clear();
}

void BodyMorphInterface::Impl_SetMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey, float relative)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	actorMorphs.m_data[actor->formID][g_stringTable.GetString(morphName)][g_stringTable.GetString(morphKey)] = relative;
}

float BodyMorphInterface::Impl_GetMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if(it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			auto & kit = mit->second.find(g_stringTable.GetString(morphKey));
			if (kit != mit->second.end())
			{
				return kit->second;
			}
		}
	}

	return 0.0;
}

void BodyMorphInterface::Impl_ClearMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			auto & kit = mit->second.find(g_stringTable.GetString(morphKey));
			if (kit != mit->second.end())
			{
				mit->second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::Impl_HasBodyMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & kit = it->second.find(g_stringTable.GetString(morphName));
		if (kit != it->second.end())
		{
			auto & mit = kit->second.find(g_stringTable.GetString(morphKey));
			if(mit != kit->second.end())
				return true;
		}
	}

	return false;
}

float BodyMorphInterface::Impl_GetBodyMorphs(TESObjectREFR * actor, SKEEFixedString morphName)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(g_stringTable.GetString(morphName));
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

bool BodyMorphInterface::Impl_HasBodyMorphKey(TESObjectREFR * actor, SKEEFixedString morphKey)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & mit : it->second)
		{
			auto & kit = mit.second.find(g_stringTable.GetString(morphKey));
			if (kit != mit.second.end())
			{
				return true;
			}
		}
	}

	return false;
}

void BodyMorphInterface::Impl_ClearBodyMorphKeys(TESObjectREFR * actor, SKEEFixedString morphKey)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & mit : it->second)
		{
			auto & kit = mit.second.find(g_stringTable.GetString(morphKey));
			if (kit != mit.second.end())
			{
				mit.second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::Impl_HasBodyMorphName(TESObjectREFR * actor, SKEEFixedString morphName)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & kit = it->second.find(g_stringTable.GetString(morphName));
		if (kit != it->second.end())
		{
			return true;
		}
	}

	return false;
}

void BodyMorphInterface::Impl_ClearBodyMorphNames(TESObjectREFR * actor, SKEEFixedString morphName)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			mit->second.clear();
		}
	}
}

void BodyMorphInterface::Impl_ClearMorphs(TESObjectREFR * actor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
	if(it != actorMorphs.m_data.end())
	{
		actorMorphs.m_data.erase(it);
	}
}

bool BodyMorphInterface::Impl_HasMorphs(TESObjectREFR * actor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	auto & it = actorMorphs.m_data.find(actor->formID);
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

void TriShapeFullVertexData::ApplyMorph(UInt16 vertexCount, Layout * vertexData, float factor)
{
	UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->vertexDesc, VertexAttribute::VA_POSITION);
	UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->vertexDesc);

	if (m_maxIndex < vertexCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			DirectX::XMFLOAT4 * position = reinterpret_cast<DirectX::XMFLOAT4*>(&vertexData->vertexData[vertexSize * vert.index + offset]);
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

void TriShapePackedVertexData::ApplyMorph(UInt16 vertexCount, Layout * vertexData, float factor)
{
	UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->vertexDesc);
	UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->vertexDesc, VertexAttribute::VA_POSITION);

	if (m_maxIndex < vertexCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			DirectX::XMFLOAT4 * position = reinterpret_cast<DirectX::XMFLOAT4*>(&vertexData->vertexData[vertexSize * vert.index + offset]);
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

void TriShapePackedUVData::ApplyMorph(UInt16 vertexCount, Layout * vertexData, float factor)
{
	VertexFlags flags = NiSkinPartition::GetVertexFlags(vertexData->vertexDesc);
	if ((flags & VF_UV))
	{
		UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexData->vertexDesc);
		UInt32 offset = NiSkinPartition::GetVertexAttributeOffset(vertexData->vertexDesc, VertexAttribute::VA_TEXCOORD0);
		if (m_maxIndex < vertexCount)
		{
			for (const auto & delta : m_uvDeltas)
			{
				UVCoord * texCoord = reinterpret_cast<UVCoord*>(&vertexData->vertexData[vertexSize * delta.index + offset]);
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

void BodyMorphMap::ForEachMorph(std::function<void(const SKEEFixedString&, const std::pair<TriShapeVertexDataPtr, TriShapeVertexDataPtr>&)> functor) const
{
	for (auto& morph : *this)
	{
		functor(morph.first, morph.second);
	}
}

#include <fstream>
#include <regex>
#include <d3d11.h>
#include <d3d11_4.h>

void MorphFileCache::ApplyMorph(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching, const std::pair<SKEEFixedString, BodyMorphMap> & bodyMorph, std::mutex * mutex, bool deferred)
{
	BSFixedString nodeName = bodyMorph.first.c_str();
	BSGeometry * geometry = rootNode->GetAsBSGeometry();
	NiGeometry * legacyGeometry = rootNode->GetAsNiGeometry();
	NiAVObject * bodyNode = geometry ? geometry : legacyGeometry ? legacyGeometry : rootNode->GetObjectByName(&nodeName.data);
	if (bodyNode)
	{
#if 0
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
#endif

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
									TriShapeVertexData::Layout layout;
									layout.vertexDesc = partition.shapeData->m_VertexDesc;
									layout.vertexData = partition.shapeData->m_RawVertexData;

									std::function<void(const TriShapeVertexDataPtr, float)> vertexMorpher = [&](const TriShapeVertexDataPtr morphData, float morphFactor)
									{
										if (morphFactor != 0.0f)
										{
											morphData->ApplyMorph(vertexCount, &layout, morphFactor);
										}
									};

									std::function<void(const TriShapeVertexDataPtr, float)> uvMorpher = [&](const TriShapeVertexDataPtr morphData, float morphFactor)
									{
										if (morphFactor != 0.0f)
										{
											morphData->ApplyMorph(vertexCount, &layout, morphFactor);
										}
									};

									// Applies all morphs for this shape
									bodyMorph.second.ApplyMorphs(refr, vertexMorpher, bodyMorph.second.HasUV() ? uvMorpher : nullptr);

									if (g_enableBodyNormalRecalculate)
									{
										NormalApplicator applicator(bodyGeometry, newSkinPartition);
										applicator.Apply();
									}

									// Propagate the data to the other partitions
									for (UInt32 p = 1; p < newSkinPartition->m_uiPartitions; ++p)
									{
										auto & pPartition = newSkinPartition->m_pkPartitions[p];
										memcpy(pPartition.shapeData->m_RawVertexData, partition.shapeData->m_RawVertexData, newSkinPartition->vertexCount * vertexSize);
									}

									if (mutex) mutex->lock();

									auto updateTask = new NIOVTaskUpdateSkinPartition(skinInstance, newSkinPartition);
									newSkinPartition->DecRef(); // DeepCopy started refcount at 1, passed ownership to the task

									if (deferred)
									{
										g_task->AddTask(updateTask);
									}
									else
									{
										updateTask->Run();
										updateTask->Dispose();
									}
									if (mutex) mutex->unlock();
								}
							}
						}
					}
				}
			}
		}
	}
}

void MorphFileCache::ForEachShape(std::function<void(const SKEEFixedString&, const BodyMorphMap&)> functor) const
{
	for (auto it : vertexMap)
	{
		functor(it.first, it.second);
	}
}

void MorphFileCache::ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching, bool defer)
{
	if (g_parallelMorphing)
	{
		std::mutex mtx;
		concurrency::structured_task_group task_group;
		std::vector<concurrency::task_handle<std::function<void()>>> task_list;
		for (const auto & it : vertexMap)
		{
			task_list.push_back(concurrency::make_task<std::function<void()>>([&]()
			{
				ApplyMorph(refr, rootNode, isAttaching, it, &mtx, defer);
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
			ApplyMorph(refr, rootNode, isAttaching, it, nullptr, defer);
		}
	}
}

void MorphCache::ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool isAttaching, bool deferUpdate)
{
	MorphFileCache * fileCache = nullptr;

	// Find the BODYTRI and cache it
	VisitObjects(rootNode, [&](NiAVObject* object) {
		NiStringExtraData * stringData = ni_cast(object->GetExtraData("BODYTRI"), NiStringExtraData);
		if (stringData) {
			SKEEFixedString filePath = CreateTRIPath(stringData->m_pString);
			CacheFile(filePath.c_str());
			auto & it = m_data.find(filePath);
			if (it != m_data.end()) {
				fileCache = &it->second;
				return true;
			}
		}

		return false;
	});

	if (fileCache && !fileCache->vertexMap.empty())
		fileCache->ApplyMorphs(refr, rootNode, isAttaching, deferUpdate);

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

void MorphCache::UpdateMorphs(TESObjectREFR * refr, bool deferUpdate)
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
							ApplyMorphs(refr, armorNode, false, deferUpdate);
#ifdef _NO_REATTACH
							g_overlayInterface.RebuildOverlays(armor->bipedObject.GetSlotMask(), arma->biped.GetSlotMask(), refr, skeletonRoot, armorNode);
#endif
						});
					}
				}
			}
		}
	}

	BSFixedString attachmentName(AttachmentInterface::ATTACHMENT_HOLDER);
	VisitSkeletalRoots(refr, [&](NiNode* rootNode, bool isFirstPerson)
	{
		NiAVObject* attachmentNode = rootNode->GetObjectByName(&attachmentName.data);
		if (attachmentNode)
		{
			ApplyMorphs(refr, attachmentNode, false, deferUpdate);
		}
	});

#ifndef _NO_REATTACH
	//CALL_MEMBER_FN(actor->processManager, SetEquipFlag)(ActorProcessManager::kFlags_Unk01 | ActorProcessManager::kFlags_Unk02 | ActorProcessManager::kFlags_Mobile);
	//CALL_MEMBER_FN(actor->processManager, UpdateEquipment)(actor);
#endif
}

void MorphCache::ForEachMorphFile(std::function<void(const SKEEFixedString&, const MorphFileCache&)> functor) const
{
	for (auto& it : m_data)
	{
		functor(it.first, it.second);
	}
}

SKEEFixedString MorphCache::CreateTRIPath(const char * relativePath)
{
	if(relativePath == "")
		return SKEEFixedString("");

	std::string targetPath = "meshes\\";
	targetPath += std::string(relativePath);
	std::transform(targetPath.begin(), targetPath.end(), targetPath.begin(), ::tolower);
	return SKEEFixedString(targetPath.c_str());
}

void MorphCache::Shrink()
{
	while (totalMemory > memoryLimit && m_data.size() > 0)
	{
		auto & it = std::min_element(m_data.begin(), m_data.end(), [](std::pair<SKEEFixedString, MorphFileCache> a, std::pair<SKEEFixedString, MorphFileCache> b)
		{
			return (a.second.accessed < b.second.accessed);
		});

		size_t size = it->second.vertexMap.memoryUsage;
		m_data.erase(it);
		totalMemory -= size;
	}

	if (m_data.size() == 0) // Just in case we erased but messed up
		totalMemory = sizeof(MorphCache);
}

size_t MorphCache::Clear()
{
	size_t usage = totalMemory;
	Lock();
	totalMemory = 0;
	m_data.clear();
	Release();
	return usage;
}

bool MorphCache::CacheFile(const char * relativePath)
{
	SKEEFixedString filePath(relativePath);
	if(relativePath == "")
		return false;

	auto & it = m_data.find(filePath);
	if (it != m_data.end()) {
		it->second.accessed = std::time(nullptr);
		it->second.accessed = std::time(nullptr);
		return false;
	}

#ifdef _DEBUG
	_DMESSAGE("%s - Parsing: %s", __FUNCTION__, filePath.c_str());
#endif

	BSResourceNiBinaryStream binaryStream(filePath.c_str());
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
			_DMESSAGE("%s - Reading TriShape %s", __FUNCTION__, trishapeName.data);
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
				_DMESSAGE("%s - Reading Morph %s at (%08X)", __FUNCTION__, morphName.data, binaryStream.GetOffset());
#endif
				if (tsize == 0) {
					_WARNING("%s - %s - Read empty name morph at (%08X)", __FUNCTION__, filePath.c_str(), binaryStream.GetOffset());
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
					_WARNING("%s - %s - Read morph %s on %s with no vertices at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, trishapeName.data, binaryStream.GetOffset());
				}
				if (multiplier == 0.0f) {
					_WARNING("%s - %s - Read morph %s on %s with zero multiplier at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, trishapeName.data, binaryStream.GetOffset());
				}

#ifdef _DEBUG
				_DMESSAGE("%s - Total Vertices read: %d at (%08X)", __FUNCTION__, vertexNum, binaryStream.GetOffset());
#endif
				if (vertexNum > (std::numeric_limits<UInt16>::max)())
				{
					_ERROR("%s - %s - Too many vertices for %s on %s read: %d at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, vertexNum, trishapeName.data, binaryStream.GetOffset());
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

						vertexDelta.delta = DirectX::XMVectorScale(DirectX::XMVectorSet(x, y, z, 0), multiplier);

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

						vertexDelta.delta = DirectX::XMVectorScale(DirectX::XMVectorSet(x, y, z, 0), multiplier);

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
			_DMESSAGE("%s - Reading TriShape UV %s", __FUNCTION__, trishapeName.data);
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
				_DMESSAGE("%s - Reading UV Morph %s at (%08X)", __FUNCTION__, morphName.data, binaryStream.GetOffset());
#endif
				if (tsize == 0) {
					_WARNING("%s - %s - Read empty name morph at (%08X)", __FUNCTION__, filePath.c_str(), binaryStream.GetOffset());
				}

				UInt32 vertexNum = 0;
				float multiplier = 0.0f;

				trishapeMap.memoryUsage += binaryStream.Read((char *)&multiplier, sizeof(float));
				trishapeMap.memoryUsage += binaryStream.Read((char *)&vertexNum, sizeof(UInt16));

				if (vertexNum == 0) {
					_WARNING("%s - %s - Read morph %s on %s with no vertices at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, trishapeName.data, binaryStream.GetOffset());
				}
				if (multiplier == 0.0f) {
					_WARNING("%s - %s - Read morph %s on %s with zero multiplier at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, trishapeName.data, binaryStream.GetOffset());
				}

#ifdef _DEBUG
				_DMESSAGE("%s - Total Vertices read: %d at (%08X)", __FUNCTION__, vertexNum, binaryStream.GetOffset());
#endif
				if (vertexNum > (std::numeric_limits<UInt16>::max)())
				{
					_ERROR("%s - %s - Too many vertices for %s on %s read: %d at (%08X)", __FUNCTION__, filePath.c_str(), morphName.data, vertexNum, trishapeName.data, binaryStream.GetOffset());
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

void BodyMorphInterface::Impl_SetCacheLimit(size_t limit)
{
	morphCache.memoryLimit = limit;
}

void BodyMorphInterface::Impl_ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool attach)
{
	if(!refr || !rootNode) {
#ifdef _DEBUG
		_DMESSAGE("%s - Error no reference or node found.", __FUNCTION__);
#endif
		return;
	}

#ifdef _DEBUG
	_DMESSAGE("%s - Applying Vertex Diffs to %08X on %s", __FUNCTION__, refr->formID, rootNode->m_name);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.ApplyMorphs(refr, rootNode, attach, true);
	}
}

void BodyMorphInterface::Impl_ApplyBodyMorphs(TESObjectREFR * refr, bool deferUpdate)
{
#ifdef _DEBUG
	_DMESSAGE("%s - Updating morphs for %08X.", __FUNCTION__, refr->formID);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.UpdateMorphs(refr, deferUpdate);
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
		g_bodyMorphInterface.ApplyBodyMorphs(actor, false);
	}
}

NIOVTaskUpdateSkinPartition::NIOVTaskUpdateSkinPartition(NiSkinInstance * skinInstance, NiSkinPartition * partition)
{
	m_skinInstance = skinInstance;
	m_partition = partition;
}

void NIOVTaskUpdateSkinPartition::Dispose(void)
{
	delete this;
}

void NIOVTaskUpdateSkinPartition::Run()
{
	if (m_skinInstance && m_partition)
	{
		EnterCriticalSection(&g_renderManager->lock);
		EnterCriticalSection(&m_skinInstance->lock);
		auto & partition = m_partition->m_pkPartitions[0];
		UInt32 vertexSize = m_partition->GetVertexSize(partition.vertexDesc);
		UInt32 vertexCount = m_partition->vertexCount;

		auto deviceContext = g_renderManager->context;
		deviceContext->UpdateSubresource(partition.shapeData->m_VertexBuffer, 0, nullptr, partition.shapeData->m_RawVertexData, vertexCount * vertexSize, 0);

		for (UInt32 p = 1; p < m_partition->m_uiPartitions; ++p)
		{
			auto & pPartition = m_partition->m_pkPartitions[p];
			deviceContext->UpdateSubresource(pPartition.shapeData->m_VertexBuffer, 0, nullptr, pPartition.shapeData->m_RawVertexData, vertexCount * vertexSize, 0);
		}

		m_skinInstance->m_spSkinPartition = m_partition;
		LeaveCriticalSection(&m_skinInstance->lock);
		LeaveCriticalSection(&g_renderManager->lock);
	}
}

void BodyMorphInterface::Impl_VisitMorphs(TESObjectREFR * actor, std::function<void(SKEEFixedString name, std::unordered_map<StringTableItem, float> * map)> functor)
{
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & morph : it->second)
		{
			functor(*morph.first, &morph.second);
		}
	}
}

void BodyMorphInterface::Impl_VisitKeys(TESObjectREFR * actor, SKEEFixedString name, std::function<void(SKEEFixedString, float)> functor)
{
	auto & it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto & mit = it->second.find(g_stringTable.GetString(name));
		if (mit != it->second.end())
		{
			for (auto & morph : mit->second)
			{
				functor(*morph.first, morph.second);
			}
		}
	}
}


void BodyMorphInterface::Impl_UpdateModelWeight(TESObjectREFR * refr, bool immediate)
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

bool BodyMorphInterface::Impl_ReadBodyMorphTemplates(SKEEFixedString filePath)
{
	BSResourceNiBinaryStream file(filePath.c_str());
	if (!file.IsValid()) {
		return false;
	}

	BSResourceTextFile<0x7FFF> textFile(&file);

	UInt32 lineCount = 0;
	std::string str = "";
	UInt32 loadedTemplates = 0;

	while (textFile.ReadLine(&str))
	{
		lineCount++;
		str = std::trim(str);
		if (str.length() == 0)
			continue;
		if (str.at(0) == '#')
			continue;

		std::vector<std::string> side = std::explode(str, '=');
		if (side.size() < 2) {
			_ERROR("%s - Error - Template has no left-hand side.\tLine (%d) [%s]", __FUNCTION__, lineCount, filePath.c_str());
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
						error = "Must have value pair with @ (";
						error += selectors[k];
						error += ")";
						break;
					}

					std::string morphName = std::trim(pairs[0]);
					if (morphName.length() == 0) {
						error = "Empty morph name";
						break;
					}

					std::string morphValues = std::trim(pairs[1]);
					if (morphValues.length() == 0) {
						error = "Empty values for (";
						error += morphName;
						error += ")";
						break;
					}

					float lowerValue = 0;
					float upperValue = 0;

					std::vector<std::string> range = std::explode(morphValues, ':');
					if (range.size() > 1) {
						std::string lowerRange = std::trim(range[0]);
						if (lowerRange.length() == 0) {
							error = "Empty lower range for (";
							error += morphName;
							error += ")";
							break;
						}

						lowerValue = atof(lowerRange.c_str());

						std::string upperRange = std::trim(range[1]);
						if (upperRange.length() == 0) {
							error = "Empty upper range for (";
							error += morphName;
							error += ")";
							break;
						}

						upperValue = atof(upperRange.c_str());
					}
					else {
						lowerValue = atof(morphValues.c_str());
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
			_ERROR("%s - Error - Could not parse morphs %s.\tLine (%d) [%s]", __FUNCTION__, error.c_str(), lineCount, filePath.c_str());
			continue;
		}

		if (bodyGenSets->size() > 0) {
			bodyGenTemplates[templateName] = bodyGenSets;
			loadedTemplates++;
		}
	}

	_MESSAGE("%s - Info - Loaded %d template(s).\t[%s]", __FUNCTION__, loadedTemplates, filePath.c_str());
	return true;
}

void BodyMorphInterface::GetFilteredNPCList(std::vector<TESNPC*> activeNPCs[], const ModInfo * modInfo, UInt32 gender, TESRace * raceFilter, std::unordered_set<TESFaction*> factionList)
{
	for (UInt32 i = 0; i < (*g_dataHandler)->npcs.count; i++)
	{
		TESNPC * npc = nullptr;
		if ((*g_dataHandler)->npcs.GetNthItem(i, npc))
		{
			bool matchMod = modInfo ? modInfo->IsFormInMod(npc->formID) : true;
			bool matchRace = (raceFilter == nullptr || npc->race.race == raceFilter);			
			bool matchFactions = IsNPCInFactions(npc, factionList);

			if (npc && npc->nextTemplate == nullptr && matchMod && matchRace && matchFactions)
			{
				if (gender == 0xFF)
				{
					activeNPCs[0].push_back(npc);
					activeNPCs[1].push_back(npc);
				}
				else
					activeNPCs[gender].push_back(npc);
			}
		}
	}
}

bool BodyMorphInterface::IsNPCInFactions(TESNPC* npc, std::unordered_set<TESFaction*> factionList)
{
	UInt32 matchingFactions = 0;
	if (factionList.size() > 0)
	{
		for (UInt32 k = 0; k < npc->actorData.factions.count; ++k)
		{
			TESActorBaseData::FactionInfo fi;
			npc->actorData.factions.GetNthItem(k, fi);
			if (factionList.find(fi.faction) != factionList.end())
				matchingFactions++;
		}
	}

	return matchingFactions == factionList.size();
}

bool BodyMorphInterface::Impl_ReadBodyMorphs(SKEEFixedString filePath)
{
	BSResourceNiBinaryStream file(filePath.c_str());
	if (!file.IsValid()) {
		return false;
	}

	BSResourceTextFile<0x7FFF> textFile(&file);

	UInt32 lineCount = 0;
	std::string str = "";
	UInt32 maleTargets = 0;
	UInt32 femaleTargets = 0;
	UInt32 maleOverwrite = 0;
	UInt32 femaleOverwrite = 0;

	while (textFile.ReadLine(&str))
	{
		lineCount++;
		str = std::trim(str);
		if (str.length() == 0)
			continue;
		if (str.at(0) == '#')
			continue;

		std::vector<std::string> side = std::explode(str, '=');
		if (side.size() < 1) {
			_ERROR("%s - Error - Morph has no left-hand side.\tLine (%d) [%s]", __FUNCTION__, lineCount, filePath.c_str());
			continue;
		}

		std::unordered_set<TESFaction*> factionList;

		std::string lSide = std::trim(side[0]);
		std::vector<std::string> lProperties = std::explode(lSide, ',');
		if (lProperties.size() > 1)
		{
			std::vector<std::string> forms = std::explode(std::trim(lProperties[1]), '+');
			for (auto & formIdentifier : forms)
			{
				TESForm * form = GetFormFromIdentifier(std::trim(formIdentifier));
				if (form && form->formType == TESFaction::kTypeID)
				{
					factionList.insert(static_cast<TESFaction*>(form));
				}
			}
		}

		std::string rSide = side.size() > 1 ? std::trim(side[1]) : "";

		std::vector<std::string> form = std::explode(lProperties[0], '|');
		if (form.size() < 2) {
			_ERROR("%s - Error - Morph left side missing mod name or formID.\tLine (%d) [%s]", __FUNCTION__, lineCount, filePath.c_str());
			continue;
		}

		int paramIndex = 0;

		std::vector<TESNPC*> activeNPCs[2];
		std::string modNameText = std::trim(form[paramIndex]);
		paramIndex++;

		// All|Gender[|Race]
		if (_strnicmp(modNameText.c_str(), "all", 3) == 0)
		{
			UInt8 gender = 0xFF;
			if (form.size() > paramIndex)
			{
				std::string genderText = std::trim(form[paramIndex]);
				std::transform(genderText.begin(), genderText.end(), genderText.begin(), ::tolower);
				if (genderText.compare("male") == 0) {
					gender = 0;
					paramIndex++;
				}
				else if (genderText.compare("female") == 0) {
					gender = 1;
					paramIndex++;
				}
			}

			TESRace * foundRace = nullptr;
			if (form.size() > paramIndex)
			{
				std::string raceText = std::trim(form[paramIndex]);
				foundRace = GetRaceByName(raceText);
				if (foundRace == nullptr)
				{
					_ERROR("%s - Error - Invalid race %s specified.\tLine (%d) [%s]", __FUNCTION__, raceText.c_str(), lineCount, filePath.c_str());
					continue;
				}
				paramIndex++;
			}

			GetFilteredNPCList(activeNPCs, nullptr, gender, foundRace, factionList);
		}
		else
		{
			const ModInfo * modInfo = (*g_dataHandler)->LookupModByName(modNameText.c_str());
			if (!modInfo || !modInfo->IsActive()) {
				_WARNING("%s - Warning - Mod '%s' not a loaded mod.\tLine (%d) [%s]", __FUNCTION__, modNameText.c_str(), lineCount, filePath.c_str());
				continue;
			}

			TESForm * foundForm = nullptr;
			std::string formIdText = std::trim(form[paramIndex]);
			paramIndex++;

			UInt8 gender = 0xFF;
			if (form.size() > paramIndex) {
				std::string genderText = std::trim(form[paramIndex]);
				std::transform(genderText.begin(), genderText.end(), genderText.begin(), ::tolower);
				if (genderText.compare("male") == 0) {
					gender = 0;
					paramIndex++;
				}
				else if (genderText.compare("female") == 0) {
					gender = 1;
					paramIndex++;
				}
			}

			// Fallout4.esm|All[|Gender][|Race]
			if (_strnicmp(formIdText.c_str(), "all", 3) == 0)
			{
				TESRace * foundRace = nullptr;
				if (form.size() > paramIndex)
				{
					std::string raceText = std::trim(form[paramIndex]);
					foundRace = GetRaceByName(raceText);
					if (foundRace == nullptr)
					{
						_ERROR("%s - Error - Invalid race '%s' specified.\tLine (%d) [%s]", __FUNCTION__, raceText.c_str(), lineCount, filePath.c_str());
						continue;
					}
					paramIndex++;
				}

				GetFilteredNPCList(activeNPCs, modInfo, gender, foundRace, factionList);
			}
			else // Fallout4.esm|XXXX[|Gender]
			{
				UInt32 formLower = strtoul(formIdText.c_str(), NULL, 16);
				if (formLower == 0) {
					_ERROR("%s - Error - Invalid formID.\tLine (%d) [%s]", __FUNCTION__, lineCount, filePath.c_str());
					continue;
				}

				UInt32 formId = modInfo->GetFormID(formLower);
				foundForm = LookupFormByID(formId);
				if (!foundForm) {
					_ERROR("%s - Error - Invalid form %08X.\tLine (%d) [%s]", __FUNCTION__, formId, lineCount, filePath.c_str());
					continue;
				}
			}

			if (foundForm)
			{
				TESLevCharacter * levCharacter = DYNAMIC_CAST(foundForm, TESForm, TESLevCharacter);
				if (levCharacter) {
					VisitLeveledCharacter(levCharacter, [&](TESNPC * npc)
					{
						if (IsNPCInFactions(npc, factionList))
						{
							if (gender == 0xFF) {
								activeNPCs[0].push_back(npc);
								activeNPCs[1].push_back(npc);
							}
							else
								activeNPCs[gender].push_back(npc);
						}
					});
				}

				TESNPC * npc = DYNAMIC_CAST(foundForm, TESForm, TESNPC);
				if (npc) {
					if (IsNPCInFactions(npc, factionList))
					{
						if (gender == 0xFF) {
							activeNPCs[0].push_back(npc);
							activeNPCs[1].push_back(npc);
						}
						else
							activeNPCs[gender].push_back(npc);
					}
				}

				if (!npc && !levCharacter) {
					_ERROR("%s - Error - Invalid form %08X not an ActorBase or LeveledActor.\tLine (%d) [%s]", __FUNCTION__, foundForm->formID, lineCount, filePath.c_str());
					continue;
				}
			}
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
					_WARNING("%s - Warning - template %s not found.\tLine (%d) [%s]", __FUNCTION__, templateName.c_str(), lineCount, filePath.c_str());
			}

			dataTemplates->push_back(templateList);
		}

		for (auto & npc : activeNPCs[0])
		{
			if (bodyGenData[0].find(npc) == bodyGenData[0].end()) {
#ifdef _DEBUG
				_DMESSAGE("%s - Read male target %s (%08X)", __FUNCTION__, npc->fullName.name.c_str(), npc->formID);
#endif
				maleTargets++;
			}
			else {
				maleOverwrite++;
			}

			bodyGenData[0][npc] = dataTemplates;
		}

		for (auto & npc : activeNPCs[1])
		{
			if (bodyGenData[1].find(npc) == bodyGenData[1].end()) {
#ifdef _DEBUG
				_DMESSAGE("%s - Read female target %s (%08X)", __FUNCTION__, npc->fullName.name.c_str(), npc->formID);
#endif
				femaleTargets++;
			}
			else {
				femaleOverwrite++;
			}

			bodyGenData[1][npc] = dataTemplates;
		}

		if (maleOverwrite)
			_MESSAGE("%s - Info - %d male NPC targets(s) overwritten.\tLine (%d) [%s]", __FUNCTION__, maleOverwrite, lineCount, filePath.c_str());
		if (femaleOverwrite)
			_MESSAGE("%s - Info - %d female NPC targets(s) overwritten.\tLine (%d) [%s]", __FUNCTION__, femaleOverwrite, lineCount, filePath.c_str());

		maleOverwrite = 0;
		femaleOverwrite = 0;
	}

	_MESSAGE("%s - Info - Acquired %d male NPC target(s).\t[%s]", __FUNCTION__, maleTargets, filePath.c_str());
	_MESSAGE("%s - Info - Acquired %d female NPC target(s).\t[%s]", __FUNCTION__, femaleTargets, filePath.c_str());
	return true;
}

UInt32 BodyGenMorphSelector::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

UInt32 BodyGenMorphs::Evaluate(std::function<void(SKEEFixedString, float)> eval)
{
	UInt32 total = 0;
	for (auto value : *this) {
		if (value.size() < 1)
			continue;

		total += value.Evaluate(eval);
	}

	return total;
}

UInt32 BodyGenTemplate::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

UInt32 BodyTemplateList::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

UInt32 BodyGenDataTemplates::Evaluate(std::function<void(SKEEFixedString, float)> eval)
{
	UInt32 total = 0;
	for (auto & tempList : *this)
	{
		total += tempList.Evaluate(eval);
	}

	return total;
}

UInt32 BodyMorphInterface::Impl_EvaluateBodyMorphs(TESObjectREFR * actor)
{
	TESNPC * actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	if (actorBase) {
		UInt64 gender = CALL_MEMBER_FN(actorBase, GetSex)();
		bool isFemale = gender == 1 ? true : false;
		BodyGenData::iterator morphSet = bodyGenData[gender].end();
		do {
			morphSet = bodyGenData[gender].find(actorBase);
			actorBase = actorBase->nextTemplate;
		} while (actorBase && morphSet == bodyGenData[gender].end());

		// Found a matching template
		if (morphSet != bodyGenData[gender].end()) {
			auto & templates = morphSet->second;
			UInt32 ret = templates->Evaluate([&](const SKEEFixedString & morphName, float value)
			{
				SetMorph(actor, morphName, "RSMBodyGen", value);
			});

			_VMESSAGE("%s - Generated %d BodyMorphs for %s (%08X)", __FUNCTION__, ret, CALL_MEMBER_FN(actor, GetReferenceName)(), actor->formID);
			return ret;
		}
	}

	return 0;
}

void BodyMorphInterface::Impl_VisitStrings(std::function<void(SKEEFixedString)> functor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	for (auto & i1 : actorMorphs.m_data) {
		for (auto & i2 : i1.second) {
			functor(*i2.first);
			for (auto & i3 : i2.second) {
				functor(*i3.first);
			}
		}
	}
}

void BodyMorphInterface::Impl_VisitActors(std::function<void(TESObjectREFR*)> functor)
{
	SimpleLocker locker(&actorMorphs.m_lock);
	for (auto & actor : actorMorphs.m_data) {
		TESObjectREFR * refr = (TESObjectREFR *)LookupFormByID(actor.first);
		if (refr) {
			functor(refr);
		}
	}
}

void BodyMorphInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	if (HasMorphs(refr))
	{
		if (g_isReverting)
		{
			g_actorUpdateManager.AddBodyUpdate(refr->formID);
		}
		else
		{
			ApplyVertexDiff(refr, object, true);
		}
	}
}

// Serialize Morph
void BodyMorph::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('MRPV', kVersion);

	g_stringTable.WriteString(intfc, m_name);

	intfc->WriteRecordData(&m_value, sizeof(m_value));
}

bool BodyMorph::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	m_value = 0.0;

	if(intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
		case 'MRPV':
			{
				if (kVersion >= BodyMorphInterface::kSerializationVersion3)
				{
					m_name = StringTable::ReadString(intfc, stringTable);
				}
				else if (kVersion >= BodyMorphInterface::kSerializationVersion1)
				{
					UInt8 stringLength;
					if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
					{
						_ERROR("%s - Error loading body morph name length", __FUNCTION__);
						error = true;
						return error;
					}

					std::unique_ptr<char[]> stringName(new char[stringLength + 1]);
					if (!intfc->ReadRecordData(stringName.get(), stringLength)) {
						_ERROR("%s - Error loading body morph name", __FUNCTION__);
						error = true;
						return error;
					}
					stringName[stringLength] = 0;
					m_name = g_stringTable.GetString(stringName.get());
				}

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
	_DMESSAGE("%s - Saving %d morphs", __FUNCTION__, numMorphs);
#endif

	for(auto it = this->begin(); it != this->end(); ++it)
		const_cast<BodyMorph&>((*it)).Save(intfc, kVersion);
}

bool BodyMorphSet::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
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
					if (!value.Load(intfc, version, stringTable))
					{
						if(*value.m_name == SKEEFixedString(""))
							continue;

#ifdef _DEBUG
						_DMESSAGE("%s - Loaded morph %s %f", __FUNCTION__, value.m_name, value.m_value);
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
		MorphKey formId = morph.first;
		intfc->WriteRecordData(&formId, sizeof(formId));

#ifdef _DEBUG
		_DMESSAGE("%s - Saving Morph form %08llX", __FUNCTION__, formId);
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
	_DMESSAGE("%s - Saving %d morphs", __FUNCTION__, numMorphs);
#endif

	for (auto & morph : *this)
	{
		g_stringTable.WriteString(intfc, morph.first);

		UInt32 numKeys = morph.second.size();
		intfc->WriteRecordData(&numKeys, sizeof(numKeys));

		for (auto & keys : morph.second)
		{
			g_stringTable.WriteString(intfc, keys.first);
			intfc->WriteRecordData(&keys.second, sizeof(keys.second));
		}
	}
}

bool BodyMorphData::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
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
					auto morphName = StringTable::ReadString(intfc, stringTable);

					UInt32 numKeys = 0;
					if (!intfc->ReadRecordData(&numKeys, sizeof(numKeys)))
					{
						_ERROR("%s - Error loading morph key count", __FUNCTION__);
						error = true;
						return error;
					}

					std::unordered_map<StringTableItem, float> pairs;
					for (UInt32 i = 0; i < numKeys; i++)
					{
						auto keyName = StringTable::ReadString(intfc, stringTable);

						float value = 0;
						if (!intfc->ReadRecordData(&value, sizeof(value))) {
							_ERROR("%s - Error loading body morph value", __FUNCTION__);
							error = true;
							return error;
						}

						// If the keys were mapped by mod name, skip them if they arent in load order
						std::string strKey(keyName->c_str());
						SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
						if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
						{
							if (!DataHandler::GetSingleton()->LookupModByName(keyName->c_str()))
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

bool ActorMorphs::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	bool error = false;

	UInt64 handle;
	MorphKey formId;
	// Key
	if (kVersion >= BodyMorphInterface::kSerializationVersion3)
	{
		if (!intfc->ReadRecordData(&formId, sizeof(formId)))
		{
			_ERROR("%s - Error loading MorphSet key", __FUNCTION__);
			error = true;
			return error;
		}
	}
	else
	{
		if (!intfc->ReadRecordData(&handle, sizeof(handle)))
		{
			_ERROR("%s - Error loading MorphSet key", __FUNCTION__);
			error = true;
			return error;
		}
	}
	

	BodyMorphSet morphSet;
	BodyMorphData morphMap;

	if (kVersion >= BodyMorphInterface::kSerializationVersion2)
	{
		if (morphMap.Load(intfc, kVersion, stringTable))
		{
			_ERROR("%s - Error loading MorphMap", __FUNCTION__);
			error = true;
			return error;
		}
	}
	else if (kVersion >= BodyMorphInterface::kSerializationVersion1)
	{
		if (morphSet.Load(intfc, kVersion, stringTable))
		{
			_ERROR("%s - Error loading MorphSet", __FUNCTION__);
			error = true;
			return error;
		}
	}

	if (!morphSet.empty())
	{
		for (auto & morph : morphSet)
		{
			morphMap[morph.m_name][g_stringTable.GetString("RSMLegacy")] = morph.m_value;
		}
	}

	MorphKey newFormId = 0;
	if (kVersion >= BodyMorphInterface::kSerializationVersion3)
	{
		// Skip if handle is no longer valid.
		if (!ResolveAnyForm(intfc, formId, &newFormId))
			return false;
	}
	else
	{
		// Skip if handle is no longer valid.
		UInt64 newHandle = 0;
		if (!intfc->ResolveHandle(handle, &newHandle))
			return false;

		newFormId = newHandle & 0xFFFFFFFF;
	}
	
	if(morphMap.empty())
		return false;

	if (g_enableBodyMorph)
	{
		TESObjectREFR * refr = (TESObjectREFR *)LookupFormByID(newFormId);
		if (refr && (refr->formType == kFormType_Reference || refr->formType == kFormType_Character))
		{
			m_data.insert_or_assign(newFormId, morphMap);

#ifdef _DEBUG
			_DMESSAGE("%s - Loaded MorphSet Handle %08llX actor %s", __FUNCTION__, newFormId, CALL_MEMBER_FN(refr, GetReferenceName)());
#endif

			g_actorUpdateManager.AddBodyUpdate(newFormId);
		}
		else if (refr)
		{
			_WARNING("%s - Discarding morphs for %08llX form is not an actor or reference (%d)", __FUNCTION__, newFormId, refr->formType);
		}
		else
		{
			_WARNING("%s - Discarding morphs for %08llX form is invalid", __FUNCTION__, newFormId);
		}
	}

	return error;
}

// Serialize Morphs
void BodyMorphInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	actorMorphs.Save(intfc, kVersion);
}

bool BodyMorphInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	return actorMorphs.Load(intfc, kVersion, stringTable);
}
