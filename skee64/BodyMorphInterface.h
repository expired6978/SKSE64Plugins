#pragma once

#include "IPluginInterface.h"
#include "SafeDataHolder.h"
#include "IHashType.h"

#include "RE/B/BSFixedString.h"
#include "RE/F/FormTypes.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiAVObject.h"
#include "RE/B/BSGeometry.h"
#include "RE/N/NiSkinInstance.h"
#include "RE/N/NiSkinPartition.h"
#include "SKSE/API.h"

#include "StringTable.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "half.hpp"

#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <ctime>
#include <mutex>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <cstdint>


#define MORPH_MOD_DIRECTORY "actors\\character\\BodyGenData\\"

class BodyMorph
{
public:
	bool operator<(const BodyMorph & rhs) const { return *m_name < *rhs.m_name; }
	bool operator==(const BodyMorph & rhs) const	{ return *m_name == *rhs.m_name; }

	StringTableItem	m_name;
	float			m_value;

	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable);
};

class BodyMorphSet : public std::set<BodyMorph>
{
public:
	// Serialization
	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable);
};

class BodyMorphData : public std::unordered_map<StringTableItem, std::unordered_map<StringTableItem, float>>
{
public:
	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable);
};

typedef std::uint32_t MorphKey;
class ActorMorphs : public SafeDataHolder<std::unordered_map<MorphKey, BodyMorphData>>
{
	friend class BodyMorphInterface;
public:
	typedef std::unordered_map<MorphKey, BodyMorphData>	MorphMap;

	// Serialization
	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable);
};

class TriShapeVertexDelta
{
public:
	std::uint16_t		index;
	DirectX::XMVECTOR delta;
};

class TriShapePackedVertexDelta
{
public:
	std::uint16_t	index;
	DirectX::XMVECTOR delta;
};

class TriShapePackedUVDelta
{
public:
	std::uint16_t	index;
	std::int16_t	u;
	std::int16_t	v;
};

class TriShapeVertexData
{
public:
	virtual void ApplyMorphRaw(std::uint16_t vertCount, void * vertices, float factor) = 0;

	struct Layout
	{
		RE::BSGraphics::VertexDesc	vertexDesc;
		std::uint8_t	* vertexData;
	};

	virtual void ApplyMorph(std::uint16_t vertexCount, Layout * vertexData, float factor) = 0;
	virtual size_t GetSize() const = 0;
};
typedef std::shared_ptr<TriShapeVertexData> TriShapeVertexDataPtr;

class TriShapeFullVertexData : public TriShapeVertexData
{
public:
	TriShapeFullVertexData() : m_maxIndex(0) { }

	virtual void ApplyMorphRaw(std::uint16_t vertCount, void * vertices, float factor);
	virtual void ApplyMorph(std::uint16_t vertexCount, Layout * vertexData, float factor);
	virtual size_t GetSize() const { return m_vertexDeltas.size(); }

	std::uint32_t								m_maxIndex;
	std::vector<TriShapeVertexDelta>	m_vertexDeltas;
};
typedef std::shared_ptr<TriShapeFullVertexData> TriShapeFullVertexDataPtr;

class TriShapePackedVertexData : public TriShapeVertexData
{
public:
	TriShapePackedVertexData() : m_maxIndex(0) { }

	virtual void ApplyMorphRaw(std::uint16_t vertCount, void * vertices, float factor);
	virtual void ApplyMorph(std::uint16_t vertexCount, Layout* vertexData, float factor);
	virtual size_t GetSize() const { return m_vertexDeltas.size(); }

	float									m_multiplier;
	std::uint32_t									m_maxIndex;
	std::vector<TriShapePackedVertexDelta>	m_vertexDeltas;
};
typedef std::shared_ptr<TriShapePackedVertexData> TriShapePackedVertexDataPtr;

class TriShapePackedUVData : public TriShapeVertexData
{
public:
	TriShapePackedUVData() : m_maxIndex(0) { }

	struct UVCoord
	{
		half_float::half u;
		half_float::half v;
	};

	virtual void ApplyMorphRaw(std::uint16_t vertCount, void * vertices, float factor);
	virtual void ApplyMorph(std::uint16_t vertexCount, Layout* vertexData, float factor);
	virtual size_t GetSize() const { return m_uvDeltas.size(); }

	float								m_multiplier;
	std::uint32_t								m_maxIndex;
	std::vector<TriShapePackedUVDelta>	m_uvDeltas;
};
typedef std::shared_ptr<TriShapePackedUVData> TriShapePackedUVDataPtr;

class BodyMorphMap : public std::unordered_map<SKEEFixedString, std::pair<TriShapeVertexDataPtr, TriShapeVertexDataPtr>>
{
	friend class MorphCache;
public:
	BodyMorphMap() : m_hasUV(false) { }

	void ApplyMorphs(RE::TESObjectREFR * refr, std::function<void(const TriShapeVertexDataPtr, float)> vertexFunctor, std::function<void(const TriShapeVertexDataPtr, float)> uvFunctor) const;
	bool HasMorphs(RE::TESObjectREFR * refr) const;
	void ForEachMorph(std::function<void(const SKEEFixedString&, const std::pair<TriShapeVertexDataPtr, TriShapeVertexDataPtr>&)> functor) const;

	bool HasUV() const { return m_hasUV; }

private:
	bool m_hasUV;
};

class TriShapeMap : public std::unordered_map<SKEEFixedString, BodyMorphMap>
{
public:
	TriShapeMap()
	{
		memoryUsage = sizeof(TriShapeMap);
	}

	size_t memoryUsage;
};

class NIOVTaskUpdateSkinPartition;
class MorphFileCache
{
	friend class MorphCache;
	friend class BodyMorphInterface;
public:
	void ApplyMorphs(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool erase = false, bool defer = false);
	std::vector<NIOVTaskUpdateSkinPartition*> ApplyMorph(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool erase, const std::pair<SKEEFixedString, BodyMorphMap> & bodyMorph);
	void ForEachShape(std::function<void(const SKEEFixedString&, const BodyMorphMap&)> functor) const;

	size_t GetByteSize() const { return vertexMap.memoryUsage; }

private:
#ifdef _DEBUG
	SKEEFixedString path;
#endif
	TriShapeMap vertexMap;
	std::time_t accessed;
};

class MorphCache : public SafeDataHolder<std::unordered_map<SKEEFixedString, MorphFileCache>>
{
	friend class BodyMorphInterface;

public:
	MorphCache()
	{
		totalMemory = sizeof(MorphCache);
		memoryLimit = totalMemory;
	}

	typedef std::unordered_map<SKEEFixedString, TriShapeMap>	FileMap;

	SKEEFixedString CreateTRIPath(const char * relativePath);
	bool CacheFile(const char * modelPath);

	void ApplyMorphs(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool attaching = false, bool deferUpdate = false);
	void UpdateMorphs(RE::TESObjectREFR * refr, bool deferUpdate = false);
	void ForEachMorphFile(std::function<void(const SKEEFixedString&, const MorphFileCache&)> functor) const;

	void Shrink();
	size_t Clear();

private:
	size_t memoryLimit;
	size_t totalMemory;
};

class NIOVTaskUpdateModelWeight : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	NIOVTaskUpdateModelWeight(RE::Actor * actor);
	
private:
	std::uint32_t	m_formId;
};

class NIOVTaskUpdateSkinPartition : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	NIOVTaskUpdateSkinPartition(RE::NiSkinInstance * skinInstance, RE::NiSkinPartition * partition, bool gpuCopy = false, bool rebindDynamic = false);

private:
	RE::NiPointer<RE::NiSkinPartition>	m_partition;
	RE::NiPointer<RE::NiSkinInstance>	m_skinInstance;
	bool m_copyGPU;
	bool m_rebindDynamic;
};

class BodyGenMorphData
{
public:
	SKEEFixedString	name;
	float			lower;
	float			upper;
};

class BodyGenMorphSelector : public std::vector<BodyGenMorphData>
{
public:
	std::uint32_t Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenMorphs : public std::vector<BodyGenMorphSelector>
{
public:
	std::uint32_t Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenTemplate : public std::vector<BodyGenMorphs>
{
public:
	std::uint32_t Evaluate(std::function<void(SKEEFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenTemplate> BodyGenTemplatePtr;

typedef std::unordered_map<SKEEFixedString, BodyGenTemplatePtr> BodyGenTemplates;

class BodyTemplateList : public std::vector<BodyGenTemplatePtr>
{
public:
	std::uint32_t Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenDataTemplates : public std::vector<BodyTemplateList>
{
public:
	std::uint32_t Evaluate(std::function<void(SKEEFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenDataTemplates> BodyGenDataTemplatesPtr;

typedef std::unordered_map<RE::TESNPC*, BodyGenDataTemplatesPtr> BodyGenData;

struct MorphShapeCallbackItem
{
	IBodyMorphInterface::MorphShapeCallback cb;
	uint64_t sort;

	struct Comp {
		bool operator()(const MorphShapeCallbackItem& a, const MorphShapeCallbackItem& b) const
		{
			if (a.sort == b.sort)
			{
				return a.cb < b.cb;
			}
			return a.sort > b.sort; // Descending by age
		}
	};
};

class MorphShapeCallbacks : public SafeDataHolder<std::set<MorphShapeCallbackItem, MorphShapeCallbackItem::Comp>>
{
public:
	void AddCallback(IBodyMorphInterface::MorphShapeCallback cb, skee_u64 order = 0);
	void ForEach(std::function<void(IBodyMorphInterface::MorphShapeCallback)> func);
};

class BodyMorphInterface 
	: public IBodyMorphInterface
	, public IAddonAttachmentInterface
{
public:
	virtual skee_u32 GetVersion();

	// Serialization
	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable);
	virtual void Revert() override;

	virtual void SetMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey, float relative) override { Impl_SetMorph(actor, morphName, morphKey, relative); }
	virtual float GetMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) override { return Impl_GetMorph(actor, morphName, morphKey); }
	virtual void ClearMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) override { Impl_ClearMorph(actor, morphName, morphKey); }

	virtual float GetBodyMorphs(RE::TESObjectREFR * actor, const char * morphName) override { return Impl_GetBodyMorphs(actor, morphName); }
	virtual void ClearBodyMorphNames(RE::TESObjectREFR * actor, const char * morphName) override { Impl_ClearBodyMorphNames(actor, morphName); }

	virtual void VisitMorphValues(RE::TESObjectREFR * actor, MorphValueVisitor & visitor) override
	{
		Impl_VisitMorphs(actor, [&](SKEEFixedString key, std::unordered_map<StringTableItem, float> * map)
		{
			for (auto & item : *map)
			{
				visitor.Visit(actor, key.c_str(), item.first->c_str(), item.second);
			}
		});
	}

	virtual void VisitMorphs(RE::TESObjectREFR * actor, MorphVisitor & visitor) override { Impl_VisitMorphs(actor, [&](SKEEFixedString key, std::unordered_map<StringTableItem, float> * map) { visitor.Visit(actor, key.c_str()); }); }
	virtual void VisitKeys(RE::TESObjectREFR * actor, const char * name, MorphKeyVisitor & visitor) override { Impl_VisitKeys(actor, name, [&](SKEEFixedString key, float value) { visitor.Visit(key.c_str(), value); }); }

	virtual void ClearMorphs(RE::TESObjectREFR * actor) override { Impl_ClearMorphs(actor); }

	virtual void ApplyVertexDiff(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool erase = false) override { Impl_ApplyVertexDiff(refr, rootNode, erase); }

	virtual void ApplyBodyMorphs(RE::TESObjectREFR * refr, bool deferUpdate = true) override { Impl_ApplyBodyMorphs(refr, deferUpdate); }
	virtual void UpdateModelWeight(RE::TESObjectREFR * refr, bool immediate = false) override { Impl_UpdateModelWeight(refr, immediate); }

	virtual void SetCacheLimit(size_t limit) override { Impl_SetCacheLimit(limit); }
	virtual bool HasMorphs(RE::TESObjectREFR * actor) override { return Impl_HasMorphs(actor); }
	virtual skee_u32 EvaluateBodyMorphs(RE::TESObjectREFR * actor) override { return Impl_EvaluateBodyMorphs(actor); }

	virtual bool HasBodyMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) override { return Impl_HasBodyMorph(actor, morphName, morphKey); }
	virtual bool HasBodyMorphName(RE::TESObjectREFR * actor, const char * morphName) override { return Impl_HasBodyMorphName(actor, morphName); }
	virtual bool HasBodyMorphKey(RE::TESObjectREFR * actor, const char * morphKey) override { return Impl_HasBodyMorphKey(actor, morphKey); }
	virtual void ClearBodyMorphKeys(RE::TESObjectREFR * actor, const char * morphKey) override { Impl_ClearBodyMorphKeys(actor, morphKey); }
	virtual void VisitStrings(StringVisitor & visitor) override { Impl_VisitStrings([&visitor](SKEEFixedString key) { visitor.Visit(key.c_str()); }); }
	virtual void VisitActors(ActorVisitor & visitor) override { Impl_VisitActors([&visitor](RE::TESObjectREFR* actor) { visitor.Visit(actor); }); }
	virtual std::vector<SKEEFixedString> GetCachedMorphNames();

	virtual size_t ClearMorphCache() override;

	virtual void AddMorphShapeCallback(IBodyMorphInterface::MorphShapeCallback cb, skee_u64 order = 0) override;

	void LoadMods();
	void PrintDiagnostics();

	void ForEachMorphShapeCallback(std::function<void(IBodyMorphInterface::MorphShapeCallback)> func);

private:
	void GetFilteredNPCList(std::vector<RE::TESNPC*> activeNPCs[], const RE::TESFile* modInfo, std::uint32_t gender, RE::TESRace * raceFilter, std::unordered_set<RE::TESFaction*> factionList);
	bool IsNPCInFactions(RE::TESNPC* npc, std::unordered_set<RE::TESFaction*> factionList);

	void Impl_SetMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey, float relative);
	float Impl_GetMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);
	void Impl_ClearMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);

	float Impl_GetBodyMorphs(RE::TESObjectREFR * actor, SKEEFixedString morphName);
	void Impl_ClearBodyMorphNames(RE::TESObjectREFR * actor, SKEEFixedString morphName);

	void Impl_VisitMorphs(RE::TESObjectREFR * actor, std::function<void(SKEEFixedString, std::unordered_map<StringTableItem, float> *)> functor);
	void Impl_VisitKeys(RE::TESObjectREFR * actor, SKEEFixedString name, std::function<void(SKEEFixedString, float)> functor);

	void Impl_ClearMorphs(RE::TESObjectREFR * actor);

	void Impl_ApplyVertexDiff(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool erase = false);

	void Impl_ApplyBodyMorphs(RE::TESObjectREFR * refr, bool deferUpdate = true);
	void Impl_UpdateModelWeight(RE::TESObjectREFR * refr, bool immediate = false);

	void Impl_SetCacheLimit(size_t limit);
	bool Impl_HasMorphs(RE::TESObjectREFR * actor);

	bool Impl_ReadBodyMorphs(SKEEFixedString filePath);
	bool Impl_ReadBodyMorphTemplates(SKEEFixedString filePath);
	std::uint32_t Impl_EvaluateBodyMorphs(RE::TESObjectREFR * actor);

	bool Impl_HasBodyMorph(RE::TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);
	bool Impl_HasBodyMorphName(RE::TESObjectREFR * actor, SKEEFixedString morphName);
	bool Impl_HasBodyMorphKey(RE::TESObjectREFR * actor, SKEEFixedString morphKey);
	void Impl_ClearBodyMorphKeys(RE::TESObjectREFR * actor, SKEEFixedString morphKey);
	void Impl_VisitStrings(std::function<void(SKEEFixedString)> functor);
	void Impl_VisitActors(std::function<void(RE::TESObjectREFR*)> functor);

private:
	ActorMorphs	actorMorphs;
	MorphCache	morphCache;
	BodyGenTemplates bodyGenTemplates;
	BodyGenData	bodyGenData[2];

	MorphShapeCallbacks shapeCallbacks;

	friend class NIOVTaskUpdateMorph;
	friend class NIOVTaskUpdateModelWeight;
	friend class PresetInterface;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root) override;
};
