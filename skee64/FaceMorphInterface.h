#pragma once

#include "RaceMenuTypes.h"
#include "StringTable.h"
#include <cstdint>

#include <RE/B/BSGeometry.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiSmartPointer.h>
#include <SKSE/Interfaces.h>

#ifdef _DEBUG
//#define _DEBUG_HOOK
//#define _DEBUG_MORPHAPPLICATOR
//#define _DEBUG_DATADUMP
//#define _DEBUG_DATAREADER
//#define _DEBUG_MORPH
#endif

#pragma once

#include "RaceMenuTypes.h"
#include "StringTable.h"
#include <cstdint>

#include <RE/B/BSGeometry.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiSmartPointer.h>
#include <SKSE/Interfaces.h>

#ifdef _DEBUG
//#define _DEBUG_HOOK
//#define _DEBUG_MORPHAPPLICATOR
//#define _DEBUG_DATADUMP
//#define _DEBUG_DATAREADER
//#define _DEBUG_MORPH
#endif



#define SLIDER_OFFSET 200
#define SLIDER_CATEGORY_EXTRA 512
#define SLIDER_CATEGORY_EXPRESSIONS 1024

#define SLIDER_MOD_DIRECTORY "actors\\character\\FaceGenMorphs\\"
#define SLIDER_DIRECTORY "actors\\character\\FaceGenMorphs\\morphs\\"

#define MORPH_CACHE_TEMPLATE "%08X.tri"
#define MORPH_CACHE_DIR "cache\\"
#define MORPH_CACHE_PATH "actors\\character\\FaceGenMorphs\\morphs\\cache\\"

#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

#include "IHashType.h"
#include "OverrideVariant.h"
#include "IPluginInterface.h"

class SliderInternal
{
public:
	SliderInternal()
	{
		category = -1;
		name = "";
		displayName = "";
		lowerBound = "";
		upperBound = "";
		type = 0;
		presetCount = 0;
	}

	void copy(SliderInternal * slider)
	{
		category = slider->category;
		name = slider->name;
		displayName = slider->displayName;
		lowerBound = slider->lowerBound;
		upperBound = slider->upperBound;
		type = slider->type;
		presetCount = slider->presetCount;
	}

	enum
	{
		kCategoryExpressions = SLIDER_CATEGORY_EXPRESSIONS,
		kCategoryExtra = SLIDER_CATEGORY_EXTRA,
		kCategoryBody = 4,
		kCategoryHead = 8,
		kCategoryFace = 16,
		kCategoryEyes = 32,
		kCategoryBrow = 64,
		kCategoryMouth = 128,
		kCategoryHair = 256
	};

	enum 
	{
		kTypeSlider = 0,
		kTypePreset = 1,
		kTypeHeadPart = 2
	};

	std::int32_t			category;
	SKEEFixedString			name;
	SKEEFixedString			displayName;
	SKEEFixedString			lowerBound;
	SKEEFixedString			upperBound;
	std::uint8_t			type;
	std::uint8_t			presetCount;
};

typedef std::shared_ptr<SliderInternal> SliderInternalPtr;

class SliderGender
{
public:
	SliderGender()
	{
		slider[0] = nullptr;
		slider[1] = nullptr;
	}

	SliderInternalPtr slider[2];
};
typedef std::shared_ptr<SliderGender> SliderGenderPtr;
typedef std::vector<SliderInternalPtr> SliderList;
using RaceSliders = std::unordered_map<RE::TESRace*, SliderList>;
typedef std::vector<SKEEFixedString> MorphSet;

class MorphMap : public std::map<SKEEFixedString, MorphSet>
{
public:
	class Visitor
	{
	public:
		virtual bool Accept(const SKEEFixedString & morphName) { return false; };
	};
	
	void AddMorph(const SKEEFixedString & key, const SKEEFixedString & value);
	bool Visit(const SKEEFixedString & key, Visitor & visitor);
	void Revert();

#ifdef _DEBUG_DATADUMP
	void DumpMap();
	class DumpVisitor : public Visitor
	{
	public:
		virtual bool Accept(RE::BSFixedString morphName)
		{
			SKSE::log::info("Extra Morph: {}", morphName.data);
			return false;
		};
	};
#endif
};

class SliderMap : public std::unordered_map<SKEEFixedString, SliderGenderPtr>
{
public:
	SliderMap() : std::unordered_map<SKEEFixedString, SliderGenderPtr>(){ }

	void AddSlider(const SKEEFixedString & key, std::uint8_t gender, SliderInternal & slider);

#ifdef _DEBUG_DATADUMP
	void DumpMap();
#endif
};

typedef std::shared_ptr<SliderMap>	SliderMapPtr;

class SliderSet : public std::set<SliderMapPtr>
{
public:
	bool for_each_slider(std::function<bool(SliderGenderPtr)> func);
};

typedef std::shared_ptr<SliderSet> SliderSetPtr;

class RaceMap : public std::unordered_map<RE::TESRace*, SliderSetPtr>
{
public:
	SliderSetPtr GetSliderSet(RE::TESRace* race);
	bool AddSliderMap(RE::TESRace* race, SliderMapPtr sliderMap);
	bool CreateDefaultMap(RE::TESRace* race);

	void Revert();

#ifdef _DEBUG_DATADUMP
	void DumpMap();
#endif
};

class ValueSet : public std::unordered_map<StringTableItem, float>
{
public:
	void SetValue(const SKEEFixedString & name, float value);
	void ClearValue(const SKEEFixedString & name);
	float GetValue(const SKEEFixedString & name);
};

class ValueMap : public std::unordered_map<RE::TESNPC*, ValueSet>
{
public:
	ValueSet* GetValueSet(RE::TESNPC* npc);
	void EraseNPC(RE::TESNPC* npc);

	float GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString& name);
	void SetMorphValue(RE::TESNPC* npc, const SKEEFixedString& name, float value);
};

#define VERTEX_THRESHOLD 0.00001
#define VERTEX_MULTIPLIER 10000

class MappedSculptData : public std::unordered_map<std::uint16_t, RE::NiPoint3>
{
public:
	void force_insert(value_type const & v)
	{
		if (abs(v.second.x) < VERTEX_THRESHOLD && abs(v.second.y) < VERTEX_THRESHOLD && abs(v.second.z) < VERTEX_THRESHOLD)
			return;

		auto res = insert(v);
		if (!res.second)
			(*res.first).second = v.second;
	}

	void add(value_type const & v)
	{
		auto res = insert(v);
		if (!res.second)
			(*res.first).second += v.second;

		if (abs((*res.first).second.x) < VERTEX_THRESHOLD && abs((*res.first).second.y) < VERTEX_THRESHOLD && abs((*res.first).second.z) < VERTEX_THRESHOLD)
			erase(res.first);
	}
};
typedef std::shared_ptr<MappedSculptData> MappedSculptDataPtr;

class SculptData : public std::unordered_map<StringTableItem, MappedSculptDataPtr>
{
public:
	MappedSculptDataPtr GetSculptHost(SKEEFixedString, bool create = true);

	static SKEEFixedString GetHostByPart(RE::BGSHeadPart* headPart);
};
using SculptDataPtr = std::shared_ptr<SculptData>;

class SculptStorage : public std::unordered_map<RE::TESNPC*, SculptDataPtr>
{
public:
	void SetSculptTarget(RE::TESNPC* npc, SculptDataPtr sculptData);
	SculptDataPtr GetSculptTarget(RE::TESNPC* npc, bool create = true);
	void EraseNPC(RE::TESNPC* npc);
};

class TRIFile
{
public:
	TRIFile()
	{
		vertexCount = -1;
	}

	bool Load(const char* triPath);
	bool Apply(RE::BSGeometry* geometry, SKEEFixedString morph, float relative);

	struct Morph
	{
		SKEEFixedString name;
		float multiplier;
		
		struct Vertex
		{
			std::int16_t x, y, z;
		};

		std::vector<Vertex> vertices;
	};

	std::int32_t vertexCount;
	std::unordered_map<SKEEFixedString, Morph> morphs;
};

class TRIModelData
{
public:
	TRIModelData()
	{
		vertexCount = -1;
		morphModel = NULL;
	}
	std::int32_t vertexCount;
	std::shared_ptr<TRIFile> triFile;
	RE::TESModelTri* morphModel;
};

typedef std::unordered_map<SKEEFixedString, TRIModelData> ModelMap;

class FaceMorphInterface : public IPluginInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};
	virtual skee_u32 GetVersion();

	bool Load(SKSE::SerializationInterface* intfc, std::uint32_t version) { return false; } // Unused due to separate dblock name for morph and sculpt
	void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion);
	virtual void Revert();
	void RevertInternals();

	bool LoadMorphData(SKSE::SerializationInterface* intfc, std::uint32_t version, const StringIdMap& stringTable);
	bool LoadSculptData(SKSE::SerializationInterface* intfc, std::uint32_t version, const StringIdMap& stringTable);

	void LoadMods();

	virtual float GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString& name);
	virtual void SetMorphValue(RE::TESNPC* npc, const SKEEFixedString& name, float value);

	void SetMorph(RE::TESNPC* npc, RE::BSFaceGenNiNode* faceNode, const SKEEFixedString& name, float relative);

	void ApplyMorph(RE::TESNPC* npc, RE::BGSHeadPart* headPart, RE::BSFaceGenNiNode* faceNode);
	void ApplyMorphs(RE::TESNPC* npc, RE::BSFaceGenNiNode* faceNode);

	std::int32_t LoadSliders(RE::RaceMenuSliderArray* sliderArray, RE::RaceMenuSlider* slider);

	void ReadMorphs(std::string fixedPath, std::string modName, std::string fileName);
	void ReadRaces(std::string fixedPath, std::string modName, std::string fileName);
	SliderMapPtr ReadSliders(std::string fixedPath, std::string modName, std::string fileName);

	SliderInternalPtr GetSlider(RE::TESRace* race, std::uint8_t gender, SKEEFixedString name);
	SliderInternalPtr GetSliderByIndex(RE::TESRace* race, std::uint32_t index);

	SliderList* CreateSliderList(RE::TESRace* race, std::uint8_t gender);
	void AddSlider(RE::TESRace* race, SliderInternalPtr& slider);

	bool VisitMorphMap(const SKEEFixedString & key, MorphMap::Visitor & visitor);

	bool CacheHeadPartModel(RE::BGSHeadPart* headPart, bool cacheTRI = false);
	bool GetModelTri(const SKEEFixedString& filePath, TRIModelData& modelData);
	TRIModelData& GetExtendedModelTri(const SKEEFixedString& morphName, bool cacheTRI = false);

	inline SculptDataPtr GetSculptTarget(RE::TESNPC* npc, bool create = true)
	{
		return m_sculptStorage.GetSculptTarget(npc, create);
	}
	inline void SetSculptTarget(RE::TESNPC* npc, const SculptDataPtr& data)
	{
		m_sculptStorage.SetSculptTarget(npc, data);
	}
	inline void EraseSculptData(RE::TESNPC* npc)
	{
		m_sculptStorage.EraseNPC(npc);
	}
	inline void EraseMorphData(RE::TESNPC* npc)
	{
		m_valueMap.EraseNPC(npc);
	}

	inline ValueMap& GetValueMap() { return m_valueMap; }

protected:
	SliderList * currentList;
	RaceSliders m_internalMap;
	RaceMap m_raceMap;
	MorphMap m_morphMap;
	ValueMap m_valueMap;
	ModelMap m_modelMap;

	SculptStorage m_sculptStorage;

	friend class RacePartDefaultGen;

#ifdef _DEBUG_DATADUMP
	void DumpAll();
#endif
};

class SKSETaskApplyMorphs : public SKEETaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; }

	SKSETaskApplyMorphs(RE::Actor* actor);

private:
	std::uint32_t m_formId;
};

class SKSETaskApplyMorphNormals : public SKEETaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; }

	SKSETaskApplyMorphNormals(RE::NiPointer<RE::NiAVObject> faceNode, bool updateModel = true);

protected:
	RE::NiPointer<RE::NiAVObject> m_faceNode;
	bool m_updateModel;
};
