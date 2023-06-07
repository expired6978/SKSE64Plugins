#pragma once

#include "IPluginInterface.h"

#include <unordered_map>
#include "skse64/GameTypes.h"

#include "OverrideInterface.h"
#include "OverrideVariant.h"

class TESObjectREFR;
struct SKSESerializationInterface;
class NiNode;
class NiAVObject;
class NiTransform;

class NodeTransformKeys : public std::unordered_map<StringTableItem, OverrideRegistration<StringTableItem>>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
};

class NodeTransformRegistrationMapHolder : public SafeDataHolder<std::unordered_map<UInt32, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2>>>
{
	friend class NiTransformInterface;
public:
	typedef std::unordered_map<UInt32, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2>> RegMap;

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, UInt32 * outFormId, const StringIdMap & stringTable);
};

// Node names are hashed here due to some case where the node "NPC" gets overwritten for some unknown reason
class NodeTransformCache : public SafeDataHolder<std::unordered_map<SKEEFixedString, std::unordered_map<SKEEFixedString, NiTransform>>>
{
	friend class NiTransformInterface;
public:
	typedef std::unordered_map<SKEEFixedString, NiTransform> NodeMap;
	typedef std::unordered_map<SKEEFixedString, NodeMap> RegMap;

	NiTransform * GetBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName, bool relative);
};

class NiTransformInterface : public INiTransformInterface
{
public:
	virtual skee_u32 GetVersion();

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual void Revert();

	virtual bool HasNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool HasNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool HasNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool HasNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);

	virtual void AddNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position); // X,Y,Z
	virtual void AddNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotation); // Euler angles
	virtual void AddNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale);
	virtual void AddNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u32 scaleMode);

	virtual Position GetNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual Rotation GetNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual float GetNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual skee_u32 GetNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);

	virtual bool RemoveNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool RemoveNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool RemoveNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual bool RemoveNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name);

	virtual bool RemoveNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name);
	virtual void RemoveAllReferenceTransforms(TESObjectREFR* refr);

	virtual bool GetOverrideNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u16 key, NiTransform* result);

	virtual void UpdateNodeAllTransforms(TESObjectREFR* ref);

	virtual void VisitNodes(TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor);
	virtual void UpdateNodeTransforms(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node);

	// Internal implementations
	bool Impl_AddNodeTransform(TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, OverrideVariant & value);
	bool Impl_RemoveNodeTransformComponent(TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, UInt16 index);
	bool Impl_RemoveNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name);
	void Impl_RemoveAllReferenceTransforms(TESObjectREFR * refr);
	
	OverrideVariant Impl_GetOverrideNodeValue(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, SInt8 index);
	bool Impl_GetOverrideNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, NiTransform * result);

	void Impl_GetOverrideTransform(OverrideSet * set, UInt16 key, NiTransform * result);
	SKEEFixedString GetRootModelPath(TESObjectREFR * refr, bool firstPerson, bool isFemale);

	void Impl_UpdateNodeAllTransforms(TESObjectREFR * ref);

	void Impl_VisitNodes(TESObjectREFR * refr, bool firstPerson, bool isFemale, std::function<bool(SKEEFixedString, OverrideRegistration<StringTableItem>*)> functor);
	bool Impl_VisitNodeTransforms(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, std::function<bool(OverrideRegistration<StringTableItem>*)> each_key, std::function<void(NiNode*, NiAVObject*, NiTransform*)> finalize);
	void Impl_UpdateNodeTransforms(TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node);

	void VisitStrings(std::function<void(SKEEFixedString)> functor);
	
	void RemoveInvalidTransforms(UInt32 formId);
	void RemoveNamedTransforms(UInt32 formId, SKEEFixedString name);
	void SetTransforms(UInt32 formId, bool immediate = false, bool reset = false);

	void PrintDiagnostics();

	NodeTransformRegistrationMapHolder	transformData;
	NodeTransformCache					transformCache;
};