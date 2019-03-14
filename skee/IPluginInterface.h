#pragma once

class TESObjectREFR;
class NiAVObject;

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

	virtual void SetCacheLimit(UInt32 limit) = 0;
	virtual bool HasMorphs(TESObjectREFR * actor) = 0;
	virtual UInt32 EvaluateBodyMorphs(TESObjectREFR * actor) = 0;

	virtual bool HasBodyMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual bool HasBodyMorphName(TESObjectREFR * actor, const char * morphName) = 0;
	virtual bool HasBodyMorphKey(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void ClearBodyMorphKeys(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void VisitStrings(StringVisitor & visitor) = 0;
	virtual void VisitActors(ActorVisitor & visitor) = 0;
};