#pragma once

class TESObjectREFR;
class NiAVObject;
class TESObjectARMO;
class TESObjectARMA;
class NiNode;
class NiAVObject;
class NiTransform;

class IPluginInterface
{
public:
	IPluginInterface() { };
	virtual ~IPluginInterface() { };

	virtual UInt32 GetVersion() = 0;
	virtual void Revert() = 0;
};

class IInterfaceMap 
{
public:
	virtual IPluginInterface * QueryInterface(const char * name) = 0;
	virtual bool AddInterface(const char * name, IPluginInterface * pluginInterface) = 0;
	virtual IPluginInterface * RemoveInterface(const char * name) = 0;
};

struct InterfaceExchangeMessage
{
	enum
	{
		kMessage_ExchangeInterface = 0x9E3779B9
	};

	IInterfaceMap * interfaceMap = NULL;
};

class IAddonAttachmentInterface
{
public:
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) = 0;
};

class IBodyMorphInterface : public IPluginInterface
{
public:
	class MorphKeyVisitor
	{
	public:
		virtual void Visit(const char*, float) = 0;
	};

	class StringVisitor
	{
	public:
		virtual void Visit(const char*) = 0;
	};

	class ActorVisitor
	{
	public:
		virtual void Visit(TESObjectREFR*) = 0;
	};

	class MorphValueVisitor
	{
	public:
		virtual void Visit(TESObjectREFR *, const char*, const char*, float) = 0;
	};
	
	class MorphVisitor
	{
	public:
		virtual void Visit(TESObjectREFR *, const char*) = 0;
	};

	virtual void SetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey, float relative) = 0;
	virtual float GetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual void ClearMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;

	virtual float GetBodyMorphs(TESObjectREFR * actor, const char * morphName) = 0;
	virtual void ClearBodyMorphNames(TESObjectREFR * actor, const char * morphName) = 0;

	virtual void VisitMorphs(TESObjectREFR * actor, MorphVisitor & visitor) = 0;
	virtual void VisitKeys(TESObjectREFR * actor, const char * name, MorphKeyVisitor & visitor) = 0;
	virtual void VisitMorphValues(TESObjectREFR * actor, MorphValueVisitor & visitor) = 0;

	virtual void ClearMorphs(TESObjectREFR * actor) = 0;

	virtual void ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false) = 0;

	virtual void ApplyBodyMorphs(TESObjectREFR * refr, bool deferUpdate = true) = 0;
	virtual void UpdateModelWeight(TESObjectREFR * refr, bool immediate = false) = 0;

	virtual void SetCacheLimit(size_t limit) = 0;
	virtual bool HasMorphs(TESObjectREFR * actor) = 0;
	virtual UInt32 EvaluateBodyMorphs(TESObjectREFR * actor) = 0;

	virtual bool HasBodyMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual bool HasBodyMorphName(TESObjectREFR * actor, const char * morphName) = 0;
	virtual bool HasBodyMorphKey(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void ClearBodyMorphKeys(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void VisitStrings(StringVisitor & visitor) = 0;
	virtual void VisitActors(ActorVisitor & visitor) = 0;
	virtual size_t ClearMorphCache() = 0;
};

class INiTransformInterface : public IPluginInterface
{
public:
	struct Position
	{
		float x, y, z;
	};
	struct Rotation
	{
		float heading, attitude, bank;
	};

	// Visits all overrides within a set
	class NodeVisitor
	{
	public:
		virtual bool VisitPosition(const char* node, const char* key, Position& position) = 0;
		virtual bool VisitRotation(const char* node, const char* key, Rotation& rotation) = 0;
		virtual bool VisitScale(const char* node, const char* key, float scale) = 0;
		virtual bool VisitScaleMode(const char* node, const char* key, UInt32 scaleMode) = 0;
	};

	virtual bool HasNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual void AddNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position) = 0; // X,Y,Z
	virtual void AddNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotation) = 0; // Euler angles
	virtual void AddNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale) = 0;
	virtual void AddNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, UInt32 scaleMode) = 0;

	virtual Position GetNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual Rotation GetNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual float GetNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual UInt32 GetNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual void RemoveAllReferenceTransforms(TESObjectREFR* refr) = 0;

	virtual bool GetOverrideNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, UInt16 key, NiTransform* result) = 0;

	virtual void UpdateNodeAllTransforms(TESObjectREFR* ref) = 0;

	virtual void VisitNodes(TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor) = 0;
	virtual void UpdateNodeTransforms(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) = 0;
};

class IAttachmentInterface : public IPluginInterface
{
public:
	virtual bool AttachMesh(TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, UInt32 filterSize, NiAVObject*& out, char* err, size_t errBufLen) = 0;
	virtual bool DetachMesh(TESObjectREFR* ref, const char* name, bool firstPerson) = 0;
};

class IActorUpdateManager : public IPluginInterface
{
public:
	virtual void AddBodyUpdate(UInt32 formId) = 0;
	virtual void AddTransformUpdate(UInt32 formId) = 0;
	virtual void AddOverlayUpdate(UInt32 formId) = 0;
	virtual void AddNodeOverrideUpdate(UInt32 formId) = 0;
	virtual void AddWeaponOverrideUpdate(UInt32 formId) = 0;
	virtual void AddAddonOverrideUpdate(UInt32 formId) = 0;
	virtual void AddSkinOverrideUpdate(UInt32 formId) = 0;
	virtual void Flush() = 0;
};