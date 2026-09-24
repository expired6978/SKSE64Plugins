#include "BodyMorphInterface.h"
#include <RE/N/NiTCollection.h>
#include <REX/W32/KERNEL32.h>
#include "OverlayInterface.h"
#include "AttachmentInterface.h"
#include "ActorUpdateManager.h"
#include "ShaderUtilities.h"
#include "SKEETasks.h"
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
#include <ppltasks.h>

#include "SKSE/API.h"
#include "SKSE/Interfaces.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESRace.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESLevCharacter.h"
#include "RE/B/BGSHeadPart.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiNode.h"
#include "RE/B/BSGeometry.h"
#include "RE/N/NiSkinInstance.h"
#include "RE/N/NiSkinPartition.h"
#include "RE/N/NiExtraData.h"
#include "RE/N/NiStringExtraData.h"
#include "RE/RTTI.h"

#include "Morpher.h"
#include <cstdint>

extern ActorUpdateManager				g_actorUpdateManager;
extern BodyMorphInterface				g_bodyMorphInterface;
extern OverlayInterface					g_overlayInterface;
extern StringTable						g_stringTable;
extern bool								g_parallelMorphing;
extern std::uint16_t							g_bodyMorphMode;
extern bool								g_enableBodyGen;
extern bool								g_enableBodyMorph;
extern bool								g_enableBodyNormalRecalculate;
extern bool								g_bodyMorphGPUCopy;
extern bool								g_bodyMorphRebind;

skee_u32 BodyMorphInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void BodyMorphInterface::PrintDiagnostics()
{
	Console_Print("BodyMorphInterface Diagnostics:");
	actorMorphs.Lock();
	Console_Print("\t%llu actors morphed", actorMorphs.m_data.size());
	for (auto& entry : actorMorphs.m_data)
	{
		RE::TESForm* form = RE::TESForm::LookupByID(entry.first);
		RE::TESObjectREFR* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
		SKSE::log::info("Reference: {:08X} ({}) ({} keys)", entry.first, refr ? refr->GetName() : "", entry.second.size());
		for (auto& item : entry.second)
		{
			SKSE::log::info("\tMorph: {} ({} morphs)", item.first ? item.first->c_str() : "", item.second.size());
			for (auto& morph : item.second)
			{
				SKSE::log::info("\t\tKey: {} Value: {}", morph.first ? morph.first->c_str() : "", morph.second);
			}
		}
	}
	actorMorphs.Release();
	morphCache.Lock();
	Console_Print("\t%llu bytes cached", morphCache.totalMemory);
	Console_Print("\t%llu files cached", morphCache.m_data.size());
	morphCache.Release();
	Console_Print("\t%llu BodyGen templates loaded", bodyGenTemplates.size());
	Console_Print("\t%llu BodyGen male candidates", bodyGenData[0].size());
	Console_Print("\t%llu BodyGen female candidates", bodyGenData[1].size());

	morphCache.ForEachMorphFile([&](const SKEEFixedString& filePath, const MorphFileCache& morphFile)
	{
		SKSE::log::info("File: {} ({} bytes)", filePath.c_str(), morphFile.GetByteSize());
		morphFile.ForEachShape([&](const SKEEFixedString& shapeName, const BodyMorphMap& morphMap)
		{
			SKSE::log::info("\tShape: {} ({} total morphs)", shapeName.c_str(), morphMap.size());
			morphMap.ForEachMorph([&](const SKEEFixedString& morphName, const auto vertexData)
			{
				SKSE::log::info("\t\tMorph: {} ({} verts {} uv)", morphName.c_str(), vertexData.first ? vertexData.first->GetSize() : 0LL, vertexData.second ? vertexData.second->GetSize() : 0LL);
			});
		});
	});
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
	RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler)
	{
		std::string fixedPath = "Meshes\\" + std::string(MORPH_MOD_DIRECTORY);

		ForEachMod([&](RE::TESFile * modInfo)
		{
			std::string templatesPath = fixedPath + std::string(modInfo->GetFilename()) + "\\templates.ini";
			Impl_ReadBodyMorphTemplates(templatesPath.c_str());
		});

		ForEachMod([&](RE::TESFile * modInfo)
		{
			std::string morphsPath = fixedPath + std::string(modInfo->GetFilename()) + "\\morphs.ini";
			Impl_ReadBodyMorphs(morphsPath.c_str());
		});
	}
}

void BodyMorphInterface::Revert()
{
	std::lock_guard locker(actorMorphs.m_lock);
	actorMorphs.m_data.clear();
}

void BodyMorphInterface::Impl_SetMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey, float relative)
{
	std::lock_guard locker(actorMorphs.m_lock);
	actorMorphs.m_data[actor->formID][g_stringTable.GetString(morphName)][g_stringTable.GetString(morphKey)] = relative;
}

float BodyMorphInterface::Impl_GetMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if(it != actorMorphs.m_data.end())
	{
		auto mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			auto kit = mit->second.find(g_stringTable.GetString(morphKey));
			if (kit != mit->second.end())
			{
				return kit->second;
			}
		}
	}

	return 0.0;
}

void BodyMorphInterface::Impl_ClearMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			auto kit = mit->second.find(g_stringTable.GetString(morphKey));
			if (kit != mit->second.end())
			{
				mit->second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::Impl_HasBodyMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto kit = it->second.find(g_stringTable.GetString(morphName));
		if (kit != it->second.end())
		{
			auto mit = kit->second.find(g_stringTable.GetString(morphKey));
			if(mit != kit->second.end())
				return true;
		}
	}

	return false;
}

float BodyMorphInterface::Impl_GetBodyMorphs(RE::TESObjectREFR * actor, SKEEFixedString morphName)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto mit = it->second.find(g_stringTable.GetString(morphName));
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

bool BodyMorphInterface::Impl_HasBodyMorphKey(RE::TESObjectREFR * actor, SKEEFixedString morphKey)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto& mit : it->second)
		{
			auto kit = mit.second.find(g_stringTable.GetString(morphKey));
			if (kit != mit.second.end())
			{
				return true;
			}
		}
	}

	return false;
}

void BodyMorphInterface::Impl_ClearBodyMorphKeys(RE::TESObjectREFR * actor, SKEEFixedString morphKey)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto& mit : it->second)
		{
			auto kit = mit.second.find(g_stringTable.GetString(morphKey));
			if (kit != mit.second.end())
			{
				mit.second.erase(kit);
			}
		}
	}
}

bool BodyMorphInterface::Impl_HasBodyMorphName(RE::TESObjectREFR * actor, SKEEFixedString morphName)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto kit = it->second.find(g_stringTable.GetString(morphName));
		if (kit != it->second.end())
		{
			return true;
		}
	}

	return false;
}

void BodyMorphInterface::Impl_ClearBodyMorphNames(RE::TESObjectREFR * actor, SKEEFixedString morphName)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto mit = it->second.find(g_stringTable.GetString(morphName));
		if (mit != it->second.end())
		{
			mit->second.clear();
		}
	}
}

void BodyMorphInterface::Impl_ClearMorphs(RE::TESObjectREFR * actor)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if(it != actorMorphs.m_data.end())
	{
		actorMorphs.m_data.erase(it);
	}
}

bool BodyMorphInterface::Impl_HasMorphs(RE::TESObjectREFR * actor)
{
	std::lock_guard locker(actorMorphs.m_lock);
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		return true;
	}

	return false;
}

void TriShapeFullVertexData::ApplyMorphRaw(std::uint16_t vertCount, void * data, float factor)
{
	RE::NiPoint3 * vertices = static_cast<RE::NiPoint3*>(data);
	if (!vertices)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			std::uint16_t vertexIndex = vert.index;
			const DirectX::XMVECTOR * vertexDiff = &vert.delta;

			vertices[vertexIndex].x += vertexDiff->m128_f32[0] * factor;
			vertices[vertexIndex].y += vertexDiff->m128_f32[1] * factor;
			vertices[vertexIndex].z += vertexDiff->m128_f32[2] * factor;

		}
	}
	else
	{
		SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertCount);
	}
}

void TriShapeFullVertexData::ApplyMorph(std::uint16_t vertexCount, Layout * vertexData, float factor)
{
	std::uint32_t offset = vertexData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
	std::uint32_t vertexSize = vertexData->vertexDesc.GetSize();

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
		SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertexCount);
	}
}

void TriShapePackedVertexData::ApplyMorphRaw(std::uint16_t vertCount, void * data, float factor)
{
	RE::NiPoint3 * vertices = static_cast<RE::NiPoint3*>(data);
	if (!vertices)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & vert : m_vertexDeltas)
		{
			std::uint32_t vertexIndex = vert.index;
			vertices[vertexIndex].x += (float)vert.delta.m128_f32[0] * factor;
			vertices[vertexIndex].y += (float)vert.delta.m128_f32[1] * factor;
			vertices[vertexIndex].z += (float)vert.delta.m128_f32[2] * factor;
		}
	}
	else
	{
		SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertCount);
	}
}

void TriShapePackedVertexData::ApplyMorph(std::uint16_t vertexCount, Layout * vertexData, float factor)
{
	std::uint32_t vertexSize = vertexData->vertexDesc.GetSize();
	std::uint32_t offset = vertexData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);

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
		SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertexCount);
	}
}

void TriShapePackedUVData::ApplyMorphRaw(std::uint16_t vertCount, void * data, float factor)
{
	UVCoord * deltas = static_cast<UVCoord*>(data);
	if (!deltas)
		return;

	if (m_maxIndex < vertCount)
	{
		for (const auto & delta : m_uvDeltas)
		{
			std::uint32_t vertexIndex = delta.index;
			deltas[vertexIndex].u += (float)delta.u * m_multiplier * factor;
			deltas[vertexIndex].v += (float)delta.v * m_multiplier * factor;
		}
	}
	else
	{
		SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertCount);
	}
}

void TriShapePackedUVData::ApplyMorph(std::uint16_t vertexCount, Layout * vertexData, float factor)
{
	RE::BSGraphics::Vertex::Flags flags = vertexData->vertexDesc.GetFlags();
	if ((flags & RE::BSGraphics::Vertex::VF_UV))
	{
		std::uint32_t vertexSize = vertexData->vertexDesc.GetSize();
		std::uint32_t offset = vertexData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
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
			SKSE::log::error("{} - Failed to apply morphs to geometry - morphs largest index is {} mesh vertex size is {}", __FUNCTION__, m_maxIndex, (std::uint32_t)vertexCount);
		}
	}
}

void BodyMorphMap::ApplyMorphs(RE::TESObjectREFR * refr, std::function<void(const TriShapeVertexDataPtr, float)> vertexFunctor, std::function<void(const TriShapeVertexDataPtr, float)> uvFunctor) const
{
	for (auto & morph : *this)
	{
		float morphFactor = g_bodyMorphInterface.GetBodyMorphs(refr, morph.first.c_str());
		if(vertexFunctor && morph.second.first)
			vertexFunctor(morph.second.first, morphFactor);
		if (uvFunctor && morph.second.second)
			uvFunctor(morph.second.second, morphFactor);
	}
}

bool BodyMorphMap::HasMorphs(RE::TESObjectREFR * refr) const
{
	for (auto & morph : *this)
	{
		float morphFactor = g_bodyMorphInterface.GetBodyMorphs(refr, morph.first.c_str());
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
#include "REX/W32/D3D11.h"
#include "REX/W32/D3D11_4.h"



std::vector<NIOVTaskUpdateSkinPartition*> MorphFileCache::ApplyMorph(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool isAttaching, const std::pair<SKEEFixedString, BodyMorphMap> & bodyMorph)
{
	std::vector<NIOVTaskUpdateSkinPartition*> partitionUpdates;
	auto execMorphUpdate = [&](RE::NiPointer<RE::BSGeometry> bodyGeometry)
	{
		RE::NiPointer<RE::NiSkinInstance> skinInstance = bodyGeometry->skinInstance;
		if (skinInstance) {
			RE::NiPointer<RE::NiSkinPartition> skinPartition = skinInstance->skinPartition;
			if (skinPartition) {
				// Undo morphs on the old shape
				RE::NiPointer<RE::NiBinaryExtraData> bodyData{netimmerse_cast<RE::NiBinaryExtraData*>(bodyGeometry->GetExtraData("SHAPEDATA"))};

				bool existingMorphs = !isAttaching && bodyData;

				// Apply new morphs to new shape
				if (bodyMorph.second.HasMorphs(refr) || existingMorphs)
				{
					if (skinPartition)
					{
						// Deep copy starts at refcount 1, held by partitionObj. The task takes its
						// own reference and releases it when the NiPointer goes out of scope.
						RE::NiPointer<RE::NiObject> partitionObj;
						skinPartition->CreateDeepCopy(partitionObj);
						RE::NiSkinPartition* newSkinPartition = netimmerse_cast<RE::NiSkinPartition*>(partitionObj.get());

						// Reset the Vertices directly
						if (bodyData && newSkinPartition)
						{
							if (!isAttaching)
							{
								// Overwrite the vertex data with the original source data
								for (std::uint32_t p = 0; p < newSkinPartition->numPartitions; ++p)
								{
									auto& partition = newSkinPartition->partitions[p];
									std::uint32_t vertexSize = partition.vertexDesc.GetSize();

									memcpy(partition.buffData->rawVertexData, bodyData->data, newSkinPartition->vertexCount * vertexSize);
								}
							}
						}

						if (newSkinPartition)
						{
							// No existing morphs, copy the current vertex block
							if (!bodyData) {
								auto& partition = newSkinPartition->partitions[0];
								std::uint32_t vertexSize = partition.vertexDesc.GetSize();

								bodyData = RE::NiPointer<RE::NiBinaryExtraData>(
									RE::NiBinaryExtraData::Create("SHAPEDATA", partition.buffData->rawVertexData, newSkinPartition->vertexCount * vertexSize));
								bodyGeometry->AddExtraData(bodyData.get());
							}

							if (bodyData)
							{
								auto& partition = newSkinPartition->partitions[0];
								std::uint32_t vertexSize = partition.vertexDesc.GetSize();
								std::uint32_t vertexCount = newSkinPartition->vertexCount;
								TriShapeVertexData::Layout layout;
								layout.vertexDesc = partition.buffData->vertexDesc;
								layout.vertexData = partition.buffData->rawVertexData;

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
									NormalApplicator applicator(bodyGeometry, RE::NiPointer<RE::NiSkinPartition>(newSkinPartition));
									applicator.Apply();
								}

								g_bodyMorphInterface.ForEachMorphShapeCallback([&](IBodyMorphInterface::MorphShapeCallback cb)
								{
									cb(refr, rootNode, bodyGeometry.get(), skinPartition.get(), bodyData.get());
								});

								// Propagate the data to the other partitions
								for (std::uint32_t p = 1; p < newSkinPartition->numPartitions; ++p)
								{
									auto& pPartition = newSkinPartition->partitions[p];
									memcpy(pPartition.buffData->rawVertexData, partition.buffData->rawVertexData, newSkinPartition->vertexCount * vertexSize);
								}

								partitionUpdates.push_back(new NIOVTaskUpdateSkinPartition(skinInstance.get(), newSkinPartition, g_bodyMorphGPUCopy, g_bodyMorphRebind));
							}
						}
					}
				}
			}
		}
	};

	RE::BSGeometry* geometry = rootNode ? rootNode->AsGeometry() : nullptr;
	if (geometry)
	{
		execMorphUpdate(RE::NiPointer<RE::BSGeometry>(geometry));
	}
	else
	{
		RE::BSFixedString nodeName = bodyMorph.first.c_str();
		VisitObjects(rootNode, [&](RE::NiAVObject* object)
		{
			if (RE::BSGeometry* bodyGeometry = object ? object->AsGeometry() : nullptr)
			{
				if (bodyGeometry->name == nodeName)
				{
					execMorphUpdate(RE::NiPointer<RE::BSGeometry>(bodyGeometry));
				}
			}
			return false;
		});
	}

	return partitionUpdates;
}

void MorphFileCache::ForEachShape(std::function<void(const SKEEFixedString&, const BodyMorphMap&)> functor) const
{
	for (auto it : vertexMap)
	{
		functor(it.first, it.second);
	}
}

void MorphFileCache::ApplyMorphs(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool isAttaching, bool defer)
{
	using namespace concurrency;
	using namespace std;

#ifdef _DEBUG
	auto skeleton = GetRootNode(rootNode);
	SKSE::log::debug("{} - Applying morphs to reference {} ({:08X}) on node {} [{:#x}] of skeleton {} [{:#x}]", __FUNCTION__, refr->GetName(), refr->formID, rootNode->name.c_str(), (std::uintptr_t)rootNode, skeleton->name.c_str(), (std::uintptr_t)skeleton);
#endif

	vector<NIOVTaskUpdateSkinPartition*> partitionUpdates;
	if (g_parallelMorphing)
	{
		vector<task<vector<NIOVTaskUpdateSkinPartition*>>> tasks;
		for (const auto& it : vertexMap)
		{
			tasks.push_back(create_task([&]() { return ApplyMorph(refr, rootNode, isAttaching, it); }));
		}

		auto joinTask = when_all(begin(tasks), end(tasks));
		joinTask.wait();

		for (auto& task : tasks)
		{
			auto results = task.get();
			partitionUpdates.insert(partitionUpdates.end(), results.begin(), results.end());
		}
	}
	else
	{
		for (const auto& it : vertexMap)
		{
			auto results = ApplyMorph(refr, rootNode, isAttaching, it);
			partitionUpdates.insert(partitionUpdates.end(), results.begin(), results.end());
		}
	}

	{
		static std::uint32_t mainThreadId = GetCurrentThreadId(); // captured on first call (game load, main thread)
		defer = mainThreadId != GetCurrentThreadId();
	}

	for (auto update : partitionUpdates)
	{
		if (defer)
		{
			SKEE_AddTask(SKSE::GetTaskInterface(), update);
		}
		else
		{
			update->Run();
			update->Dispose();
		}
	}
}

void MorphCache::ApplyMorphs(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool isAttaching, bool deferUpdate)
{
	std::lock_guard locker(m_lock);

	MorphFileCache * fileCache = nullptr;

	// Find the BODYTRI and cache it
	VisitObjects(rootNode, [&](RE::NiAVObject* object) {
		RE::NiStringExtraData* stringData = netimmerse_cast<RE::NiStringExtraData*>(object->GetExtraData("BODYTRI"));
		if (stringData) {
			SKEEFixedString filePath = CreateTRIPath(stringData->value);
			CacheFile(filePath.c_str());
			auto it = m_data.find(filePath);
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

void MorphCache::UpdateMorphs(RE::TESObjectREFR * refr, bool deferUpdate)
{
	if(!refr)
		return;

	RE::Actor* actor = refr ? refr->As<RE::Actor>() : nullptr;
	if (!actor)
		return;

	VisitBipedNodes(refr, [&](bool isFirstPerson, std::uint32_t bipedIndex, RE::NiNode* rootNode, RE::TESForm* armo, RE::TESForm* arma, RE::NiAVObject* object)
	{
		ApplyMorphs(refr, object, false, deferUpdate);
	});

	RE::BSFixedString attachmentName(AttachmentInterface::ATTACHMENT_HOLDER);
	VisitSkeletalRoots(refr, [&](RE::NiNode* rootNode, bool isFirstPerson)
	{
		RE::NiAVObject* attachmentNode = rootNode->GetObjectByName(attachmentName);
		if (attachmentNode)
		{
			ApplyMorphs(refr, attachmentNode, false, deferUpdate);
		}
	});
}

void MorphCache::ForEachMorphFile(std::function<void(const SKEEFixedString&, const MorphFileCache&)> functor) const
{
	std::lock_guard locker(m_lock);
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
	std::lock_guard locker(m_lock);
	while (totalMemory > memoryLimit && m_data.size() > 0)
	{
		auto it = std::min_element(m_data.begin(), m_data.end(), [](std::pair<SKEEFixedString, MorphFileCache> a, std::pair<SKEEFixedString, MorphFileCache> b)
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
	std::lock_guard locker(m_lock);
	size_t usage = totalMemory;
	totalMemory = 0;
	m_data.clear();
	return usage;
}

bool MorphCache::CacheFile(const char * relativePath)
{
	SKEEFixedString filePath(relativePath);
	if(relativePath == "")
		return false;

	auto it = m_data.find(filePath);
	if (it != m_data.end()) {
		it->second.accessed = std::time(nullptr);
		it->second.accessed = std::time(nullptr);
		return false;
	}

#ifdef _DEBUG
	SKSE::log::debug("{} - Parsing: {}", __FUNCTION__, filePath.c_str());
#endif

	RE::BSResourceNiBinaryStream binaryStream{filePath.c_str()};
	auto streamRead = [&](void* buf, std::uint32_t n) -> std::uint32_t { return binaryStream.read((char*)buf, n) ? n : 0; };
	if(binaryStream.good())
	{
		TriShapeMap trishapeMap;

		std::uint32_t fileFormat = 0;
		trishapeMap.memoryUsage += streamRead((char *)&fileFormat, sizeof(std::uint32_t));

		bool packed = false;
		if (fileFormat != 'TRI\0' && fileFormat != 'TRIP')
			return false;

		if (fileFormat == 'TRIP')
			packed = true;

		std::uint32_t trishapeCount = 0;
		if (!packed)
			trishapeMap.memoryUsage += streamRead((char *)&trishapeCount, sizeof(std::uint32_t));
		else
			trishapeMap.memoryUsage += streamRead((char *)&trishapeCount, sizeof(std::uint16_t));

		char trishapeNameRaw[REX::W32::MAX_PATH];
		for (std::uint32_t i = 0; i < trishapeCount; i++)
		{
			memset(trishapeNameRaw, 0, REX::W32::MAX_PATH);

			std::uint8_t size = 0;
			trishapeMap.memoryUsage += streamRead((char *)&size, sizeof(std::uint8_t));
			trishapeMap.memoryUsage += streamRead(trishapeNameRaw, size);
			RE::BSFixedString trishapeName(trishapeNameRaw);

#ifdef _DEBUG
			SKSE::log::debug("{} - Reading TriShape {}", __FUNCTION__, trishapeName.c_str());
#endif

			if (!packed) {
				std::uint32_t trishapeBlockSize = 0;
				trishapeMap.memoryUsage += streamRead((char *)&trishapeBlockSize, sizeof(std::uint32_t));
			}

			char morphNameRaw[REX::W32::MAX_PATH];

			BodyMorphMap morphMap;

			std::uint32_t morphCount = 0;
			if (!packed)
				trishapeMap.memoryUsage += streamRead((char *)&morphCount, sizeof(std::uint32_t));
			else
				trishapeMap.memoryUsage += streamRead((char *)&morphCount, sizeof(std::uint16_t));

			for (std::uint32_t j = 0; j < morphCount; j++)
			{
				memset(morphNameRaw, 0, REX::W32::MAX_PATH);

				std::uint8_t tsize = 0;
				trishapeMap.memoryUsage += streamRead((char *)&tsize, sizeof(std::uint8_t));
				trishapeMap.memoryUsage += streamRead(morphNameRaw, tsize);
				RE::BSFixedString morphName(morphNameRaw);

#ifdef _DEBUG
				SKSE::log::debug("{} - Reading Morph {} at ({:08X})", __FUNCTION__, morphName.c_str(), binaryStream.tell());
#endif
				if (tsize == 0) {
					SKSE::log::warn("{} - {} - Read empty name morph at ({:08X})", __FUNCTION__, filePath.c_str(), binaryStream.tell());
				}

				if (!packed) {
					std::uint32_t morphBlockSize = 0;
					trishapeMap.memoryUsage += streamRead((char *)&morphBlockSize, sizeof(std::uint32_t));
				}

				std::uint32_t vertexNum = 0;
				float multiplier = 0.0f;
				if(!packed) {
					trishapeMap.memoryUsage += streamRead((char *)&vertexNum, sizeof(std::uint32_t));
				}
				else {
					trishapeMap.memoryUsage += streamRead((char *)&multiplier, sizeof(float));
					trishapeMap.memoryUsage += streamRead((char *)&vertexNum, sizeof(std::uint16_t));
				}

				if (vertexNum == 0) {
					SKSE::log::warn("{} - {} - Read morph {} on {} with no vertices at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), trishapeName.c_str(), binaryStream.tell());
				}
				if (multiplier == 0.0f) {
					SKSE::log::warn("{} - {} - Read morph {} on {} with zero multiplier at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), trishapeName.c_str(), binaryStream.tell());
				}

#ifdef _DEBUG
				SKSE::log::debug("{} - Total Vertices read: {} at ({:08X})", __FUNCTION__, vertexNum, binaryStream.tell());
#endif
				if (vertexNum > (std::numeric_limits<std::uint16_t>::max)())
				{
					SKSE::log::error("{} - {} - Too many vertices for {} on {} read: {} at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), vertexNum, trishapeName.c_str(), binaryStream.tell());
					return false;
				}

				TriShapeVertexDataPtr vertexData;
				TriShapeFullVertexDataPtr fullVertexData;
				TriShapePackedVertexDataPtr packedVertexData;
				if (!packed)
				{
					fullVertexData = std::make_shared<TriShapeFullVertexData>();
					for (std::uint32_t k = 0; k < vertexNum; k++)
					{
						TriShapeVertexDelta vertexDelta;
						float x, y, z;
						trishapeMap.memoryUsage += streamRead((char *)&vertexDelta.index, sizeof(std::uint32_t));
						trishapeMap.memoryUsage += streamRead((char *)&x, sizeof(float));
						trishapeMap.memoryUsage += streamRead((char *)&y, sizeof(float));
						trishapeMap.memoryUsage += streamRead((char *)&z, sizeof(float));

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

					for (std::uint32_t k = 0; k < vertexNum; k++)
					{
						TriShapePackedVertexDelta vertexDelta;
						std::int16_t x, y, z;
						trishapeMap.memoryUsage += streamRead((char *)&vertexDelta.index, sizeof(std::uint16_t));
						trishapeMap.memoryUsage += streamRead((char *)&x, sizeof(std::int16_t));
						trishapeMap.memoryUsage += streamRead((char *)&y, sizeof(std::int16_t));
						trishapeMap.memoryUsage += streamRead((char *)&z, sizeof(std::int16_t));

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

		std::uint16_t UVShapeCount = 0;
		trishapeMap.memoryUsage += streamRead((char *)&UVShapeCount, sizeof(std::uint16_t));
		for (std::uint32_t i = 0; i < trishapeCount; i++)
		{
			memset(trishapeNameRaw, 0, REX::W32::MAX_PATH);

			std::uint8_t size = 0;
			trishapeMap.memoryUsage += streamRead((char *)&size, sizeof(std::uint8_t));
			trishapeMap.memoryUsage += streamRead(trishapeNameRaw, size);
			RE::BSFixedString trishapeName(trishapeNameRaw);

#ifdef _DEBUG
			SKSE::log::debug("{} - Reading TriShape UV {}", __FUNCTION__, trishapeName.c_str());
#endif

			if (!packed) {
				std::uint32_t trishapeBlockSize = 0;
				trishapeMap.memoryUsage += streamRead((char *)&trishapeBlockSize, sizeof(std::uint32_t));
			}

			char morphNameRaw[REX::W32::MAX_PATH];

			BodyMorphMap uvMorphMap;

			std::uint32_t morphCount = 0;
			if (!packed)
				trishapeMap.memoryUsage += streamRead((char *)&morphCount, sizeof(std::uint32_t));
			else
				trishapeMap.memoryUsage += streamRead((char *)&morphCount, sizeof(std::uint16_t));

			for (std::uint32_t j = 0; j < morphCount; j++)
			{
				memset(morphNameRaw, 0, REX::W32::MAX_PATH);

				std::uint8_t tsize = 0;
				trishapeMap.memoryUsage += streamRead((char *)&tsize, sizeof(std::uint8_t));
				trishapeMap.memoryUsage += streamRead(morphNameRaw, tsize);
				RE::BSFixedString morphName(morphNameRaw);

#ifdef _DEBUG
				SKSE::log::debug("{} - Reading UV Morph {} at ({:08X})", __FUNCTION__, morphName.c_str(), binaryStream.tell());
#endif
				if (tsize == 0) {
					SKSE::log::warn("{} - {} - Read empty name morph at ({:08X})", __FUNCTION__, filePath.c_str(), binaryStream.tell());
				}

				std::uint32_t vertexNum = 0;
				float multiplier = 0.0f;

				trishapeMap.memoryUsage += streamRead((char *)&multiplier, sizeof(float));
				trishapeMap.memoryUsage += streamRead((char *)&vertexNum, sizeof(std::uint16_t));

				if (vertexNum == 0) {
					SKSE::log::warn("{} - {} - Read morph {} on {} with no vertices at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), trishapeName.c_str(), binaryStream.tell());
				}
				if (multiplier == 0.0f) {
					SKSE::log::warn("{} - {} - Read morph {} on {} with zero multiplier at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), trishapeName.c_str(), binaryStream.tell());
				}

#ifdef _DEBUG
				SKSE::log::debug("{} - Total Vertices read: {} at ({:08X})", __FUNCTION__, vertexNum, binaryStream.tell());
#endif
				if (vertexNum > (std::numeric_limits<std::uint16_t>::max)())
				{
					SKSE::log::error("{} - {} - Too many vertices for {} on {} read: {} at ({:08X})", __FUNCTION__, filePath.c_str(), morphName.c_str(), vertexNum, trishapeName.c_str(), binaryStream.tell());
					return false;
				}

				TriShapePackedUVDataPtr packedUVData;
				
				packedUVData = std::make_shared<TriShapePackedUVData>();
				packedUVData->m_multiplier = multiplier;

				for (std::uint32_t k = 0; k < vertexNum; k++)
				{
					TriShapePackedUVDelta vertexDelta;
					trishapeMap.memoryUsage += streamRead((char *)&vertexDelta.index, sizeof(std::uint16_t));
					trishapeMap.memoryUsage += streamRead((char *)&vertexDelta.u, sizeof(std::int16_t));
					trishapeMap.memoryUsage += streamRead((char *)&vertexDelta.v, sizeof(std::int16_t));

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
#ifdef _DEBUG
		fileCache.path = relativePath;
#endif

		Lock();
		m_data.emplace(relativePath, fileCache);
		totalMemory += trishapeMap.memoryUsage;
		Release();

		SKSE::log::debug("{} - Loaded {} ({} bytes)", __FUNCTION__, relativePath, trishapeMap.memoryUsage);
		return true;
	}
	else
	{
		SKSE::log::error("{} - Failed to load {}", __FUNCTION__, relativePath);
	}

	return false;
}

void BodyMorphInterface::AddMorphShapeCallback(IBodyMorphInterface::MorphShapeCallback cb, skee_u64 order)
{

}

void BodyMorphInterface::Impl_SetCacheLimit(size_t limit)
{
	morphCache.memoryLimit = limit;
}

void BodyMorphInterface::Impl_ApplyVertexDiff(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool attach)
{
	if(!refr || !rootNode) {
#ifdef _DEBUG
		SKSE::log::debug("{} - Error no reference or node found.", __FUNCTION__);
#endif
		return;
	}

#ifdef _DEBUG
	SKSE::log::debug("{} - Applying Vertex Diffs to {:08X} on {}", __FUNCTION__, refr->formID, rootNode->name);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.ApplyMorphs(refr, rootNode, attach, true);
	}
}

void BodyMorphInterface::Impl_ApplyBodyMorphs(RE::TESObjectREFR * refr, bool deferUpdate)
{
#ifdef _DEBUG
	SKSE::log::debug("{} - Updating morphs for {:08X}.", __FUNCTION__, refr->formID);
#endif
	if (g_enableBodyMorph)
	{
		morphCache.UpdateMorphs(refr, deferUpdate);
	}
}

NIOVTaskUpdateModelWeight::NIOVTaskUpdateModelWeight(RE::Actor * actor)
{
	m_formId = actor->formID;
}

void NIOVTaskUpdateModelWeight::Dispose(void)
{
	delete this;
}

void NIOVTaskUpdateModelWeight::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::Actor* actor = form ? form->As<RE::Actor>() : nullptr;
	if(actor) {
		g_bodyMorphInterface.ApplyBodyMorphs(actor, false);
	}
}

NIOVTaskUpdateSkinPartition::NIOVTaskUpdateSkinPartition(RE::NiSkinInstance * skinInstance, RE::NiSkinPartition * partition, bool gpuCopy, bool rebindDynamic)
{
	m_skinInstance = RE::NiPointer<RE::NiSkinInstance>(skinInstance);
	m_partition = RE::NiPointer<RE::NiSkinPartition>(partition);
	m_copyGPU = gpuCopy;
	m_rebindDynamic = rebindDynamic;
}

void NIOVTaskUpdateSkinPartition::Dispose(void)
{
	delete this;
}

void NIOVTaskUpdateSkinPartition::Run()
{
	if (m_skinInstance && m_partition)
	{
		REX::W32::EnterCriticalSection(&RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().lock);
#if defined(EXCLUSIVE_SKYRIM_FLAT)
		REX::W32::EnterCriticalSection(&m_skinInstance->lock);
#endif
		auto & partition = m_partition->partitions[0];
		std::uint32_t vertexSize = partition.vertexDesc.GetSize();
		std::uint32_t vertexCount = m_partition->vertexCount;

		REX::W32::D3D11_BUFFER_DESC desc;
		reinterpret_cast<REX::W32::ID3D11Buffer*>(partition.buffData->vertexBuffer)->GetDesc(&desc);

		// Rebind functionality to switch buffer to be dynamic, and then re-use it across all partitions
		if (m_rebindDynamic && desc.usage != REX::W32::D3D11_USAGE_DYNAMIC)
		{
			REX::W32::D3D11_BUFFER_DESC newDesc = desc;
			newDesc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
			newDesc.cpuAccessFlags |= REX::W32::D3D11_CPU_ACCESS_WRITE;

			REX::W32::ID3D11Buffer* buffer = nullptr;
			REX::W32::D3D11_SUBRESOURCE_DATA data;
			data.sysMem = partition.buffData->rawVertexData;
			data.sysMemPitch = vertexCount * vertexSize;
			data.sysMemSlicePitch = 0;
			
			if (SUCCEEDED(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().forwarder->CreateBuffer(&newDesc, &data, &buffer)))
			{
				// Borrow the exact same partition and skip the copy entirely
				for (std::uint32_t p = 0; p < m_partition->numPartitions; ++p)
				{
					auto& pPartition = m_partition->partitions[p];
					pPartition.buffData->vertexBuffer->Release();
					pPartition.buffData->vertexBuffer = buffer;
					pPartition.buffData->vertexBuffer->AddRef();
				}

				// The partitions will now take ownership
				partition.buffData->vertexBuffer->Release();

				desc = newDesc;
			}
		}
		
		// Perform the resource copy either on CPU or GPU, and only on partitions that differ
		auto deviceContext = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
		switch (desc.usage)
		{
		case REX::W32::D3D11_USAGE_DEFAULT:
		{
			deviceContext->UpdateSubresource(partition.buffData->vertexBuffer, 0, nullptr, partition.buffData->rawVertexData, vertexCount * vertexSize, 0);

			if (m_copyGPU)
			{
				for (std::uint32_t p = 1; p < m_partition->numPartitions; ++p)
				{
					auto& pPartition = m_partition->partitions[p];
					if (pPartition.buffData->vertexBuffer != partition.buffData->vertexBuffer) {
						deviceContext->CopyResource(pPartition.buffData->vertexBuffer, partition.buffData->vertexBuffer);
					}
				}
			}
			else
			{
				for (std::uint32_t p = 1; p < m_partition->numPartitions; ++p)
				{
					auto& pPartition = m_partition->partitions[p];
					if (pPartition.buffData->vertexBuffer != partition.buffData->vertexBuffer) {
						deviceContext->UpdateSubresource(pPartition.buffData->vertexBuffer, 0, nullptr, pPartition.buffData->rawVertexData, vertexCount * vertexSize, 0);
					}
				}
			}
			break;
		}
		case REX::W32::D3D11_USAGE_DYNAMIC:
		{
			if (desc.cpuAccessFlags & REX::W32::D3D11_CPU_ACCESS_WRITE) {
				REX::W32::D3D11_MAPPED_SUBRESOURCE mappedResource;
				if (deviceContext->Map(partition.buffData->vertexBuffer, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) == S_OK) {
					memcpy(mappedResource.data, partition.buffData->rawVertexData, vertexCount * vertexSize);
					deviceContext->Unmap(partition.buffData->vertexBuffer, 0);
				}

				if (m_copyGPU)
				{
					for (std::uint32_t p = 1; p < m_partition->numPartitions; ++p)
					{
						auto& pPartition = m_partition->partitions[p];
						if (pPartition.buffData->vertexBuffer != partition.buffData->vertexBuffer) {
							deviceContext->CopyResource(pPartition.buffData->vertexBuffer, partition.buffData->vertexBuffer);
						}
					}
				}
				else
				{
					for (std::uint32_t p = 1; p < m_partition->numPartitions; ++p)
					{
						auto& pPartition = m_partition->partitions[p];
						if (pPartition.buffData->vertexBuffer != partition.buffData->vertexBuffer) {
							if (deviceContext->Map(pPartition.buffData->vertexBuffer, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) == S_OK) {
								memcpy(mappedResource.data, pPartition.buffData->rawVertexData, vertexCount * vertexSize);
								deviceContext->Unmap(pPartition.buffData->vertexBuffer, 0);
							}
						}
					}
				}
			}
			break;
		default:
			SKSE::log::error("{} - Failure to copy morph data into resource", __FUNCTION__);
			break;
		}
		}

		m_skinInstance->skinPartition = m_partition;
#if defined(EXCLUSIVE_SKYRIM_FLAT)
		REX::W32::LeaveCriticalSection(&m_skinInstance->lock);
#endif
		REX::W32::LeaveCriticalSection(&RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().lock);
	}
}

void BodyMorphInterface::Impl_VisitMorphs(RE::TESObjectREFR * actor, std::function<void(SKEEFixedString name, std::unordered_map<StringTableItem, float> * map)> functor)
{
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		for (auto & morph : it->second)
		{
			functor(*morph.first, &morph.second);
		}
	}
}

void BodyMorphInterface::Impl_VisitKeys(RE::TESObjectREFR * actor, SKEEFixedString name, std::function<void(SKEEFixedString, float)> functor)
{
	auto it = actorMorphs.m_data.find(actor->formID);
	if (it != actorMorphs.m_data.end())
	{
		auto mit = it->second.find(g_stringTable.GetString(name));
		if (mit != it->second.end())
		{
			for (auto & morph : mit->second)
			{
				functor(*morph.first, morph.second);
			}
		}
	}
}

void BodyMorphInterface::Impl_UpdateModelWeight(RE::TESObjectREFR * refr, bool immediate)
{
	RE::Actor* actor = refr ? refr->As<RE::Actor>() : nullptr;
	if(actor) {
		NIOVTaskUpdateModelWeight * updateTask = new NIOVTaskUpdateModelWeight(actor);
		if (immediate) {
			updateTask->Run();
			updateTask->Dispose();
		}
		else
		{
			SKEE_AddTask(SKSE::GetTaskInterface(), updateTask);
		}
	}
}

bool BodyMorphInterface::Impl_ReadBodyMorphTemplates(SKEEFixedString filePath)
{
	RE::BSResourceNiBinaryStream file{filePath.c_str()};
	if (!file.good()) {
		return false;
	}

	BSResourceTextFile<0x7FFF> textFile(&file);

	std::uint32_t lineCount = 0;
	std::string str = "";
	std::uint32_t loadedTemplates = 0;

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
			SKSE::log::error("{} - Error - Template has no left-hand side.\tLine ({}) [{}]", __FUNCTION__, lineCount, filePath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		RE::BSFixedString templateName = lSide.c_str();

		BodyGenTemplatePtr bodyGenSets = std::make_shared<BodyGenTemplate>();

		std::string error = "";
		std::vector<std::string> sets = std::explode(rSide, '/');
		for (std::uint32_t i = 0; i < sets.size(); i++) {
			sets[i] = std::trim(sets[i]);

			BodyGenMorphs bodyMorphs;

			std::vector<std::string> morphs = std::explode(sets[i], ',');
			for (std::uint32_t j = 0; j < morphs.size(); j++) {
				morphs[j] = std::trim(morphs[j]);

				std::vector<std::string> selectors = std::explode(morphs[j], '|');

				BodyGenMorphSelector selector;

				for (std::uint32_t k = 0; k < selectors.size(); k++) {
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
			SKSE::log::error("{} - Error - Could not parse morphs {}.\tLine ({}) [{}]", __FUNCTION__, error.c_str(), lineCount, filePath.c_str());
			continue;
		}

		if (bodyGenSets->size() > 0) {
			bodyGenTemplates[templateName] = bodyGenSets;
			loadedTemplates++;
		}
	}

	SKSE::log::info("{} - Info - Loaded {} template(s).\t[{}]", __FUNCTION__, loadedTemplates, filePath.c_str());
	return true;
}

void BodyMorphInterface::GetFilteredNPCList(std::vector<RE::TESNPC*> activeNPCs[], const RE::TESFile * modInfo, std::uint32_t gender, RE::TESRace * raceFilter, std::unordered_set<RE::TESFaction*> factionList)
{
	auto* dataHandler = RE::TESDataHandler::GetSingleton();
	auto& npcs = dataHandler->GetFormArray<RE::TESNPC>();
	for (std::uint32_t i = 0; i < npcs.size(); ++i)
	{
		RE::TESNPC* npc = npcs[i];
		{
			bool matchMod = modInfo ? modInfo->IsFormInMod(npc->formID) : true;
			bool matchRace = (raceFilter == nullptr || npc->GetRace() == raceFilter);			
			bool matchFactions = IsNPCInFactions(npc, factionList);

			// Only root face NPCs are template keys; actors elsewhere in a face chain are
			// matched by walking the chain at evaluate time (see Impl_EvaluateBodyMorphs).
			if (npc && npc->faceNPC == nullptr && matchMod && matchRace && matchFactions)
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

bool BodyMorphInterface::IsNPCInFactions(RE::TESNPC* npc, std::unordered_set<RE::TESFaction*> factionList)
{
	std::uint32_t matchingFactions = 0;
	if (factionList.size() > 0)
	{
		for (std::uint32_t k = 0; k < npc->factions.size(); ++k)
		{
			const RE::FACTION_RANK& fi = npc->factions[k];
			if (factionList.find(fi.faction) != factionList.end())
				matchingFactions++;
		}
	}

	return matchingFactions == factionList.size();
}

bool BodyMorphInterface::Impl_ReadBodyMorphs(SKEEFixedString filePath)
{
	RE::BSResourceNiBinaryStream file{filePath.c_str()};
	if (!file.good()) {
		return false;
	}

	BSResourceTextFile<0x7FFF> textFile(&file);

	std::uint32_t lineCount = 0;
	std::string str = "";
	std::uint32_t maleTargets = 0;
	std::uint32_t femaleTargets = 0;
	std::uint32_t maleOverwrite = 0;
	std::uint32_t femaleOverwrite = 0;

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
			SKSE::log::error("{} - Error - Morph has no left-hand side.\tLine ({}) [{}]", __FUNCTION__, lineCount, filePath.c_str());
			continue;
		}

		std::unordered_set<RE::TESFaction*> factionList;

		std::string lSide = std::trim(side[0]);
		std::vector<std::string> lProperties = std::explode(lSide, ',');
		if (lProperties.size() > 1)
		{
			std::vector<std::string> forms = std::explode(std::trim(lProperties[1]), '+');
			for (auto & formIdentifier : forms)
			{
				RE::TESForm * form = GetFormFromIdentifier(std::trim(formIdentifier));
				if (form && form->Is(RE::FormType::Faction))
				{
					factionList.insert(static_cast<RE::TESFaction*>(form));
				}
			}
		}

		std::string rSide = side.size() > 1 ? std::trim(side[1]) : "";

		std::vector<std::string> form = std::explode(lProperties[0], '|');
		if (form.size() < 2) {
			SKSE::log::error("{} - Error - Morph left side missing mod name or formID.\tLine ({}) [{}]", __FUNCTION__, lineCount, filePath.c_str());
			continue;
		}

		int paramIndex = 0;

		std::vector<RE::TESNPC*> activeNPCs[2];
		std::string modNameText = std::trim(form[paramIndex]);
		paramIndex++;

		// All|Gender[|Race]
		if (_strnicmp(modNameText.c_str(), "all", 3) == 0)
		{
			std::uint8_t gender = 0xFF;
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

			RE::TESRace * foundRace = nullptr;
			if (form.size() > paramIndex)
			{
				std::string raceText = std::trim(form[paramIndex]);
				foundRace = GetRaceByName(raceText);
				if (foundRace == nullptr)
				{
					SKSE::log::error("{} - Error - Invalid race {} specified.\tLine ({}) [{}]", __FUNCTION__, raceText.c_str(), lineCount, filePath.c_str());
					continue;
				}
				paramIndex++;
			}

			GetFilteredNPCList(activeNPCs, nullptr, gender, foundRace, factionList);
		}
		else
		{
			const RE::TESFile* modInfo = RE::TESDataHandler::GetSingleton()->LookupModByName(modNameText.c_str());
			if (!modInfo || !BSFileUtil::IsActive(modInfo)) {
				SKSE::log::warn("{} - Warning - Mod '{}' not a loaded mod.\tLine ({}) [{}]", __FUNCTION__, modNameText.c_str(), lineCount, filePath.c_str());
				continue;
			}

			RE::TESForm * foundForm = nullptr;
			std::string formIdText = std::trim(form[paramIndex]);
			paramIndex++;

			std::uint8_t gender = 0xFF;
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
				RE::TESRace * foundRace = nullptr;
				if (form.size() > paramIndex)
				{
					std::string raceText = std::trim(form[paramIndex]);
					foundRace = GetRaceByName(raceText);
					if (foundRace == nullptr)
					{
						SKSE::log::error("{} - Error - Invalid race '{}' specified.\tLine ({}) [{}]", __FUNCTION__, raceText.c_str(), lineCount, filePath.c_str());
						continue;
					}
					paramIndex++;
				}

				GetFilteredNPCList(activeNPCs, modInfo, gender, foundRace, factionList);
			}
			else // Fallout4.esm|XXXX[|Gender]
			{
				std::uint32_t formLower = strtoul(formIdText.c_str(), NULL, 16);
				if (formLower == 0) {
					SKSE::log::error("{} - Error - Invalid formID.\tLine ({}) [{}]", __FUNCTION__, lineCount, filePath.c_str());
					continue;
				}

				std::uint32_t formId = modInfo->GetFormID(formLower);
				foundForm = RE::TESForm::LookupByID(formId);
				if (!foundForm) {
					SKSE::log::error("{} - Error - Invalid form {:08X}.\tLine ({}) [{}]", __FUNCTION__, formId, lineCount, filePath.c_str());
					continue;
				}
			}

			if (foundForm)
			{
				RE::TESLevCharacter* levCharacter = foundForm ? foundForm->As<RE::TESLevCharacter>() : nullptr;
				if (levCharacter) {
					VisitLeveledCharacter(levCharacter, [&](RE::TESNPC * npc)
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

				RE::TESNPC* npc = foundForm ? foundForm->As<RE::TESNPC>() : nullptr;
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
					SKSE::log::error("{} - Error - Invalid form {:08X} not an ActorBase or LeveledActor.\tLine ({}) [{}]", __FUNCTION__, foundForm->formID, lineCount, filePath.c_str());
					continue;
				}
			}
		}

		BodyGenDataTemplatesPtr dataTemplates = std::make_shared<BodyGenDataTemplates>();
		std::string error = "";
		std::vector<std::string> sets = std::explode(rSide, ',');
		for (std::uint32_t i = 0; i < sets.size(); i++) {
			sets[i] = std::trim(sets[i]);
			std::vector<std::string> selectors = std::explode(sets[i], '|');
			BodyTemplateList templateList;
			for (std::uint32_t k = 0; k < selectors.size(); k++) {
				selectors[k] = std::trim(selectors[k]);
				RE::BSFixedString templateName(selectors[k].c_str());
				auto temp = bodyGenTemplates.find(templateName);
				if (temp != bodyGenTemplates.end())
					templateList.push_back(temp->second);
				else
					SKSE::log::warn("{} - Warning - template {} not found.\tLine ({}) [{}]", __FUNCTION__, templateName.c_str(), lineCount, filePath.c_str());
			}

			dataTemplates->push_back(templateList);
		}

		for (auto & npc : activeNPCs[0])
		{
			if (bodyGenData[0].find(npc) == bodyGenData[0].end()) {
#ifdef _DEBUG
				SKSE::log::debug("{} - Read male target {} ({:08X})", __FUNCTION__, npc->fullName.c_str(), npc->formID);
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
				SKSE::log::debug("{} - Read female target {} ({:08X})", __FUNCTION__, npc->fullName.c_str(), npc->formID);
#endif
				femaleTargets++;
			}
			else {
				femaleOverwrite++;
			}

			bodyGenData[1][npc] = dataTemplates;
		}

		if (maleOverwrite)
			SKSE::log::info("{} - Info - {} male NPC targets(s) overwritten.\tLine ({}) [{}]", __FUNCTION__, maleOverwrite, lineCount, filePath.c_str());
		if (femaleOverwrite)
			SKSE::log::info("{} - Info - {} female NPC targets(s) overwritten.\tLine ({}) [{}]", __FUNCTION__, femaleOverwrite, lineCount, filePath.c_str());

		maleOverwrite = 0;
		femaleOverwrite = 0;
	}

	SKSE::log::info("{} - Info - Acquired {} male NPC target(s).\t[{}]", __FUNCTION__, maleTargets, filePath.c_str());
	SKSE::log::info("{} - Info - Acquired {} female NPC target(s).\t[{}]", __FUNCTION__, femaleTargets, filePath.c_str());
	return true;
}

std::uint32_t BodyGenMorphSelector::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

std::uint32_t BodyGenMorphs::Evaluate(std::function<void(SKEEFixedString, float)> eval)
{
	std::uint32_t total = 0;
	for (auto value : *this) {
		if (value.size() < 1)
			continue;

		total += value.Evaluate(eval);
	}

	return total;
}

std::uint32_t BodyGenTemplate::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

std::uint32_t BodyTemplateList::Evaluate(std::function<void(SKEEFixedString, float)> eval)
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

std::uint32_t BodyGenDataTemplates::Evaluate(std::function<void(SKEEFixedString, float)> eval)
{
	std::uint32_t total = 0;
	for (auto & tempList : *this)
	{
		total += tempList.Evaluate(eval);
	}

	return total;
}

std::uint32_t BodyMorphInterface::Impl_EvaluateBodyMorphs(RE::TESObjectREFR * actor)
{
	RE::TESNPC* actorBase = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase) {
		std::uint64_t gender = actorBase->GetSex();
		bool isFemale = gender == 1 ? true : false;
		// Walk the face chain: templates are keyed by root face NPCs only.
		BodyGenData::iterator morphSet = bodyGenData[gender].end();
		do {
			morphSet = bodyGenData[gender].find(actorBase);
			actorBase = actorBase->faceNPC;
		} while (actorBase && morphSet == bodyGenData[gender].end());

		// Found a matching template
		if (morphSet != bodyGenData[gender].end()) {
			auto & templates = morphSet->second;
			std::uint32_t ret = templates->Evaluate([&](const SKEEFixedString & morphName, float value)
			{
				SetMorph(actor, morphName.c_str(), "RSMBodyGen", value);
			});

			SKSE::log::trace("{} - Generated {} BodyMorphs for {} ({:08X})", __FUNCTION__, ret, actor->GetName(), actor->formID);
			return ret;
		}
	}

	return 0;
}

void BodyMorphInterface::Impl_VisitStrings(std::function<void(SKEEFixedString)> functor)
{
	std::lock_guard locker(actorMorphs.m_lock);
	for (auto & i1 : actorMorphs.m_data) {
		for (auto & i2 : i1.second) {
			functor(*i2.first);
			for (auto & i3 : i2.second) {
				functor(*i3.first);
			}
		}
	}
}

void BodyMorphInterface::Impl_VisitActors(std::function<void(RE::TESObjectREFR*)> functor)
{
	std::lock_guard locker(actorMorphs.m_lock);
	for (auto & actor : actorMorphs.m_data) {
		RE::TESObjectREFR * refr = (RE::TESObjectREFR *)RE::TESForm::LookupByID(actor.first);
		if (refr) {
			functor(refr);
		}
	}
}

void BodyMorphInterface::ForEachMorphShapeCallback(std::function<void(IBodyMorphInterface::MorphShapeCallback)> func)
{
	shapeCallbacks.ForEach(func);
}

void BodyMorphInterface::OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root)
{
	if (HasMorphs(refr))
	{
		if (g_actorUpdateManager.isReverting())
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
void BodyMorph::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('MRPV', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}

	g_stringTable.WriteString(intfc, m_name);

	intfc->WriteRecordData(&m_value, sizeof(m_value));
}

bool BodyMorph::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	m_value = 0.0;

	if(intfc->GetNextRecordInfo(type, version, length))
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
					std::uint8_t stringLength;
					if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
					{
						SKSE::log::error("{} - Error loading body morph name length", __FUNCTION__);
						error = true;
						return error;
					}

					std::unique_ptr<char[]> stringName(new char[stringLength + 1]);
					if (!intfc->ReadRecordData(stringName.get(), stringLength)) {
						SKSE::log::error("{} - Error loading body morph name", __FUNCTION__);
						error = true;
						return error;
					}
					stringName[stringLength] = 0;
					m_name = g_stringTable.GetString(stringName.get());
				}

				if (!intfc->ReadRecordData(&m_value, sizeof(m_value))) {
					SKSE::log::error("{} - Error loading body morph value", __FUNCTION__);
					error = true;
					return error;
				}
			}
			break;
		default:
			{
				SKSE::log::error("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// Serialize Morph Set
void BodyMorphSet::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('MRST', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}

	std::uint32_t numMorphs = this->size();
	intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));

#ifdef _DEBUG
	SKSE::log::debug("{} - Saving {} morphs", __FUNCTION__, numMorphs);
#endif

	for(auto it = this->begin(); it != this->end(); ++it)
		const_cast<BodyMorph&>((*it)).Save(intfc, kVersion);
}

bool BodyMorphSet::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'MRST':
			{
				// Override Count
				std::uint32_t numMorphs = 0;
				if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
				{
					SKSE::log::error("{} - Error loading morph set count", __FUNCTION__);
					error = true;
					return error;
				}

				for (std::uint32_t i = 0; i < numMorphs; i++)
				{
					BodyMorph value;
					if (!value.Load(intfc, version, stringTable))
					{
						if(*value.m_name == SKEEFixedString(""))
							continue;

#ifdef _DEBUG
						SKSE::log::debug("{} - Loaded morph {} {}", __FUNCTION__, value.m_name.get()->c_str(), value.m_value);
#endif

						this->insert(value);
					}
					else
					{
						SKSE::log::error("{} - Error loading morph value", __FUNCTION__);
						error = true;
						return error;
					}
				}

				break;
			}
		default:
			{
				SKSE::log::error("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

// Serialize ActorMorphs
void ActorMorphs::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::lock_guard locker(m_lock);
	for(auto & morph : m_data) {
		if (!intfc->OpenRecord('MRPH', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		MorphKey formId = morph.first;
		intfc->WriteRecordData(&formId, sizeof(formId));

#ifdef _DEBUG
		SKSE::log::debug("{} - Saving Morph form %08llX", __FUNCTION__, formId);
#endif

		// Value
		morph.second.Save(intfc, kVersion);
	}
}

// Serialize Morph Set
void BodyMorphData::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('MRDT', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}

	std::uint32_t numMorphs = this->size();
	intfc->WriteRecordData(&numMorphs, sizeof(numMorphs));

#ifdef _DEBUG
	SKSE::log::debug("{} - Saving {} morphs", __FUNCTION__, numMorphs);
#endif

	for (auto & morph : *this)
	{
		g_stringTable.WriteString(intfc, morph.first);

		std::uint32_t numKeys = morph.second.size();
		intfc->WriteRecordData(&numKeys, sizeof(numKeys));

		for (auto & keys : morph.second)
		{
			g_stringTable.WriteString(intfc, keys.first);
			intfc->WriteRecordData(&keys.second, sizeof(keys.second));
		}
	}
}

bool BodyMorphData::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	if (intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
			case 'MRDT':
			{
				std::uint32_t numMorphs = 0;
				if (!intfc->ReadRecordData(&numMorphs, sizeof(numMorphs)))
				{
					SKSE::log::error("{} - Error loading morph count", __FUNCTION__);
					error = true;
					return error;
				}

				for (std::uint32_t i = 0; i < numMorphs; i++)
				{
					auto morphName = StringTable::ReadString(intfc, stringTable);

					std::uint32_t numKeys = 0;
					if (!intfc->ReadRecordData(&numKeys, sizeof(numKeys)))
					{
						SKSE::log::error("{} - Error loading morph key count", __FUNCTION__);
						error = true;
						return error;
					}

					std::unordered_map<StringTableItem, float> pairs;
					for (std::uint32_t i = 0; i < numKeys; i++)
					{
						auto keyName = StringTable::ReadString(intfc, stringTable);

						float value = 0;
						if (!intfc->ReadRecordData(&value, sizeof(value))) {
							SKSE::log::error("{} - Error loading body morph value", __FUNCTION__);
							error = true;
							return error;
						}

						// If the keys were mapped by mod name, skip them if they arent in load order
						std::string strKey(keyName->c_str());
						SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
						if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
						{
							if (!RE::TESDataHandler::GetSingleton()->LookupModByName(keyName->c_str()))
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
				SKSE::log::error("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				return error;
			}
		}
	}

	return error;
}

bool ActorMorphs::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	bool error = false;

	std::uint64_t handle;
	MorphKey formId;
	// Key
	if (kVersion >= BodyMorphInterface::kSerializationVersion3)
	{
		if (!intfc->ReadRecordData(&formId, sizeof(formId)))
		{
			SKSE::log::error("{} - Error loading MorphSet key", __FUNCTION__);
			error = true;
			return error;
		}
	}
	else
	{
		if (!intfc->ReadRecordData(&handle, sizeof(handle)))
		{
			SKSE::log::error("{} - Error loading MorphSet key", __FUNCTION__);
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
			SKSE::log::error("{} - Error loading MorphMap", __FUNCTION__);
			error = true;
			return error;
		}
	}
	else if (kVersion >= BodyMorphInterface::kSerializationVersion1)
	{
		if (morphSet.Load(intfc, kVersion, stringTable))
		{
			SKSE::log::error("{} - Error loading MorphSet", __FUNCTION__);
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
		std::uint64_t newHandle = 0;
		if (!intfc->ResolveHandle(handle, newHandle))
			return false;

		newFormId = newHandle & 0xFFFFFFFF;
	}
	
	if(morphMap.empty())
		return false;

	auto form = RE::TESForm::LookupByID(newFormId);
	if (!form) {
		SKSE::log::warn("{} - Discarding body morphs for (%08llX) form is invalid", __FUNCTION__, newFormId);
		return false;
	}
	else if (form->IsNot(RE::FormType::Reference) && form->IsNot(RE::FormType::ActorCharacter)) {
		SKSE::log::warn("{} - Discarding body morphs for (%08llX) form is not an actor or reference ({})", __FUNCTION__, newFormId, form->formType.underlying());
		return false;
	}
	else if (form->IsDeleted()) {
		SKSE::log::warn("{} - Discarding body morphs for (%08llX) form is deleted", __FUNCTION__, newFormId);
		return false;
	}

	if (g_enableBodyMorph)
	{

		Lock();
		m_data.insert_or_assign(newFormId, morphMap);
		Release();

#ifdef _DEBUG
		SKSE::log::debug("{} - Loaded MorphSet Handle {:08X} actor ({})", __FUNCTION__, newFormId, static_cast<RE::TESObjectREFR*>(form)->GetName());
#endif
		g_actorUpdateManager.AddBodyUpdate(newFormId);
		
	}
	return error;
}

// Serialize Morphs
void BodyMorphInterface::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	actorMorphs.Save(intfc, kVersion);
}

bool BodyMorphInterface::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const std::unordered_map<std::uint32_t, StringTableItem> & stringTable)
{
	return actorMorphs.Load(intfc, kVersion, stringTable);
}

void MorphShapeCallbacks::AddCallback(IBodyMorphInterface::MorphShapeCallback cb, skee_u64 order)
{
	std::lock_guard locker(m_lock);
	m_data.insert(MorphShapeCallbackItem{ cb, order });
}

void MorphShapeCallbacks::ForEach(std::function<void(IBodyMorphInterface::MorphShapeCallback)> func)
{
	std::lock_guard locker(m_lock);
	for (auto& item : m_data)
	{
		func(item.cb);
	}
}
