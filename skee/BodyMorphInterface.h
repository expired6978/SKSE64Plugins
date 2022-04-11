#pragma once

#include "IPluginInterface.h"
#include "IHashType.h"

#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"
#include "skse64/NiTypes.h"
#include "skse64/NiGeometry.h"

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

class TESObjectREFR;
struct SKSESerializationInterface;
class BSResourceNiBinaryStream;
class NiAVObject;
class TESObjectARMO;
class TESObjectARMA;
class NiExtraData;
class TESNPC;
class TESRace;
class NiSkinPartition;
struct ModInfo;
class TESFaction;

#define MORPH_MOD_DIRECTORY "actors\\character\\BodyGenData\\"

class BodyMorph
{
public:
	bool operator<(const BodyMorph & rhs) const { return *m_name < *rhs.m_name; }
	bool operator==(const BodyMorph & rhs) const	{ return *m_name == *rhs.m_name; }

	StringTableItem	m_name;
	float			m_value;

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class BodyMorphSet : public std::set<BodyMorph>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class BodyMorphData : public std::unordered_map<StringTableItem, std::unordered_map<StringTableItem, float>>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

typedef UInt32 MorphKey;
class ActorMorphs : public SafeDataHolder<std::unordered_map<MorphKey, BodyMorphData>>
{
	friend class BodyMorphInterface;
public:
	typedef std::unordered_map<MorphKey, BodyMorphData>	MorphMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class TriShapeVertexDelta
{
public:
	UInt16		index;
	DirectX::XMVECTOR delta;
};

class TriShapePackedVertexDelta
{
public:
	UInt16	index;
	DirectX::XMVECTOR delta;
};

class TriShapePackedUVDelta
{
public:
	UInt16	index;
	SInt16	u;
	SInt16	v;
};

class TriShapeVertexData
{
public:
	virtual void ApplyMorphRaw(UInt16 vertCount, void * vertices, float factor) = 0;

	struct Layout
	{
		UInt64	vertexDesc;
		UInt8	* vertexData;
	};

	virtual void ApplyMorph(UInt16 vertexCount, Layout * vertexData, float factor) = 0;
	virtual size_t GetSize() const = 0;
};
typedef std::shared_ptr<TriShapeVertexData> TriShapeVertexDataPtr;

class TriShapeFullVertexData : public TriShapeVertexData
{
public:
	TriShapeFullVertexData() : m_maxIndex(0) { }

	virtual void ApplyMorphRaw(UInt16 vertCount, void * vertices, float factor);
	virtual void ApplyMorph(UInt16 vertexCount, Layout * vertexData, float factor);
	virtual size_t GetSize() const { return m_vertexDeltas.size(); }

	UInt32								m_maxIndex;
	std::vector<TriShapeVertexDelta>	m_vertexDeltas;
};
typedef std::shared_ptr<TriShapeFullVertexData> TriShapeFullVertexDataPtr;

class TriShapePackedVertexData : public TriShapeVertexData
{
public:
	TriShapePackedVertexData() : m_maxIndex(0) { }

	virtual void ApplyMorphRaw(UInt16 vertCount, void * vertices, float factor);
	virtual void ApplyMorph(UInt16 vertexCount, Layout* vertexData, float factor);
	virtual size_t GetSize() const { return m_vertexDeltas.size(); }

	float									m_multiplier;
	UInt32									m_maxIndex;
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

	virtual void ApplyMorphRaw(UInt16 vertCount, void * vertices, float factor);
	virtual void ApplyMorph(UInt16 vertexCount, Layout* vertexData, float factor);
	virtual size_t GetSize() const { return m_uvDeltas.size(); }

	float								m_multiplier;
	UInt32								m_maxIndex;
	std::vector<TriShapePackedUVDelta>	m_uvDeltas;
};
typedef std::shared_ptr<TriShapePackedUVData> TriShapePackedUVDataPtr;


class BodyMorphMap : public std::unordered_map<SKEEFixedString, std::pair<TriShapeVertexDataPtr, TriShapeVertexDataPtr>>
{
	friend class MorphCache;
public:
	BodyMorphMap() : m_hasUV(false) { }

	void ApplyMorphs(TESObjectREFR * refr, std::function<void(const TriShapeVertexDataPtr, float)> vertexFunctor, std::function<void(const TriShapeVertexDataPtr, float)> uvFunctor) const;
	bool HasMorphs(TESObjectREFR * refr) const;
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

class MorphFileCache
{
	friend class MorphCache;
	friend class BodyMorphInterface;
public:
	void ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false, bool defer = false);
	void ApplyMorph(TESObjectREFR * refr, NiAVObject * rootNode, bool erase, const std::pair<SKEEFixedString, BodyMorphMap> & bodyMorph, std::mutex * mtx = nullptr, bool deferred = true);
	void ForEachShape(std::function<void(const SKEEFixedString&, const BodyMorphMap&)> functor) const;

	size_t GetByteSize() const { return vertexMap.memoryUsage; }

private:
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

	void ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool attaching = false, bool deferUpdate = false);
	void UpdateMorphs(TESObjectREFR * refr, bool deferUpdate = false);
	void ForEachMorphFile(std::function<void(const SKEEFixedString&, const MorphFileCache&)> functor) const;

	void Shrink();
	size_t Clear();

private:
	size_t memoryLimit;
	size_t totalMemory;
};

class NIOVTaskUpdateModelWeight : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	NIOVTaskUpdateModelWeight(Actor * actor);
	
private:
	UInt32	m_formId;
};

class NIOVTaskUpdateSkinPartition : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	NIOVTaskUpdateSkinPartition(NiSkinInstance * skinInstance, NiSkinPartition * partition);

private:
	NiPointer<NiSkinPartition>	m_partition;
	NiPointer<NiSkinInstance>	m_skinInstance;
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
	UInt32 Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenMorphs : public std::vector<BodyGenMorphSelector>
{
public:
	UInt32 Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenTemplate : public std::vector<BodyGenMorphs>
{
public:
	UInt32 Evaluate(std::function<void(SKEEFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenTemplate> BodyGenTemplatePtr;


typedef std::unordered_map<SKEEFixedString, BodyGenTemplatePtr> BodyGenTemplates;

class BodyTemplateList : public std::vector<BodyGenTemplatePtr>
{
public:
	UInt32 Evaluate(std::function<void(SKEEFixedString, float)> eval);
};

class BodyGenDataTemplates : public std::vector<BodyTemplateList>
{
public:
	UInt32 Evaluate(std::function<void(SKEEFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenDataTemplates> BodyGenDataTemplatesPtr;

typedef std::unordered_map<TESNPC*, BodyGenDataTemplatesPtr> BodyGenData;

class BodyMorphInterface 
	: public IBodyMorphInterface
	, public IAddonAttachmentInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 4,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion3 = 3,
		kSerializationVersion4 = 3,
		kSerializationVersion = kSerializationVersion4
	};
	virtual UInt32 GetVersion();

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual void Revert() override;

	virtual void SetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey, float relative) override { Impl_SetMorph(actor, morphName, morphKey, relative); }
	virtual float GetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) override { return Impl_GetMorph(actor, morphName, morphKey); }
	virtual void ClearMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) override { Impl_ClearMorph(actor, morphName, morphKey); }

	virtual float GetBodyMorphs(TESObjectREFR * actor, const char * morphName) override { return Impl_GetBodyMorphs(actor, morphName); }
	virtual void ClearBodyMorphNames(TESObjectREFR * actor, const char * morphName) override { Impl_ClearBodyMorphNames(actor, morphName); }

	virtual void VisitMorphValues(TESObjectREFR * actor, MorphValueVisitor & visitor) override
	{
		Impl_VisitMorphs(actor, [&](SKEEFixedString key, std::unordered_map<StringTableItem, float> * map)
		{
			for (auto & item : *map)
			{
				visitor.Visit(actor, key, item.first->c_str(), item.second);
			}
		});
	}

	virtual void VisitMorphs(TESObjectREFR * actor, MorphVisitor & visitor) override { Impl_VisitMorphs(actor, [&](SKEEFixedString key, std::unordered_map<StringTableItem, float> * map) { visitor.Visit(actor, key); }); }
	virtual void VisitKeys(TESObjectREFR * actor, const char * name, MorphKeyVisitor & visitor) override { Impl_VisitKeys(actor, name, [&](SKEEFixedString key, float value) { visitor.Visit(key, value); }); }

	virtual void ClearMorphs(TESObjectREFR * actor) override { Impl_ClearMorphs(actor); }

	virtual void ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false) override { Impl_ApplyVertexDiff(refr, rootNode, erase); }

	virtual void ApplyBodyMorphs(TESObjectREFR * refr, bool deferUpdate = true) override { Impl_ApplyBodyMorphs(refr, deferUpdate); }
	virtual void UpdateModelWeight(TESObjectREFR * refr, bool immediate = false) override { Impl_UpdateModelWeight(refr, immediate); }

	virtual void SetCacheLimit(size_t limit) override { Impl_SetCacheLimit(limit); }
	virtual bool HasMorphs(TESObjectREFR * actor) override { return Impl_HasMorphs(actor); }
	virtual UInt32 EvaluateBodyMorphs(TESObjectREFR * actor) override { return Impl_EvaluateBodyMorphs(actor); }

	virtual bool HasBodyMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) override { return Impl_HasBodyMorph(actor, morphName, morphKey); }
	virtual bool HasBodyMorphName(TESObjectREFR * actor, const char * morphName) override { return Impl_HasBodyMorphName(actor, morphName); }
	virtual bool HasBodyMorphKey(TESObjectREFR * actor, const char * morphKey) override { return Impl_HasBodyMorphKey(actor, morphKey); }
	virtual void ClearBodyMorphKeys(TESObjectREFR * actor, const char * morphKey) override { Impl_ClearBodyMorphKeys(actor, morphKey); }
	virtual void VisitStrings(StringVisitor & visitor) override { Impl_VisitStrings([&visitor](SKEEFixedString key) { visitor.Visit(key); }); }
	virtual void VisitActors(ActorVisitor & visitor) override { Impl_VisitActors([&visitor](TESObjectREFR* actor) { visitor.Visit(actor); }); }
	virtual std::vector<SKEEFixedString> GetCachedMorphNames();

	virtual size_t ClearMorphCache() override;

	void LoadMods();
	void PrintDiagnostics();

private:
	void GetFilteredNPCList(std::vector<TESNPC*> activeNPCs[], const ModInfo * modInfo, UInt32 gender, TESRace * raceFilter, std::unordered_set<TESFaction*> factionList);
	bool IsNPCInFactions(TESNPC* npc, std::unordered_set<TESFaction*> factionList);

	void Impl_SetMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey, float relative);
	float Impl_GetMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);
	void Impl_ClearMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);

	float Impl_GetBodyMorphs(TESObjectREFR * actor, SKEEFixedString morphName);
	void Impl_ClearBodyMorphNames(TESObjectREFR * actor, SKEEFixedString morphName);

	void Impl_VisitMorphs(TESObjectREFR * actor, std::function<void(SKEEFixedString, std::unordered_map<StringTableItem, float> *)> functor);
	void Impl_VisitKeys(TESObjectREFR * actor, SKEEFixedString name, std::function<void(SKEEFixedString, float)> functor);

	void Impl_ClearMorphs(TESObjectREFR * actor);

	void Impl_ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false);

	void Impl_ApplyBodyMorphs(TESObjectREFR * refr, bool deferUpdate = true);
	void Impl_UpdateModelWeight(TESObjectREFR * refr, bool immediate = false);

	void Impl_SetCacheLimit(size_t limit);
	bool Impl_HasMorphs(TESObjectREFR * actor);

	bool Impl_ReadBodyMorphs(SKEEFixedString filePath);
	bool Impl_ReadBodyMorphTemplates(SKEEFixedString filePath);
	UInt32 Impl_EvaluateBodyMorphs(TESObjectREFR * actor);

	bool Impl_HasBodyMorph(TESObjectREFR * actor, SKEEFixedString morphName, SKEEFixedString morphKey);
	bool Impl_HasBodyMorphName(TESObjectREFR * actor, SKEEFixedString morphName);
	bool Impl_HasBodyMorphKey(TESObjectREFR * actor, SKEEFixedString morphKey);
	void Impl_ClearBodyMorphKeys(TESObjectREFR * actor, SKEEFixedString morphKey);
	void Impl_VisitStrings(std::function<void(SKEEFixedString)> functor);
	void Impl_VisitActors(std::function<void(TESObjectREFR*)> functor);

private:
	ActorMorphs	actorMorphs;
	MorphCache	morphCache;
	BodyGenTemplates bodyGenTemplates;
	BodyGenData	bodyGenData[2];

	friend class NIOVTaskUpdateMorph;
	friend class NIOVTaskUpdateModelWeight;
	friend class FaceMorphInterface;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};
