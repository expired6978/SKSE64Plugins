#pragma once

#include <vector>
#include <unordered_set>
#include "skse64/GameEvents.h"
#include "IPluginInterface.h"
#include "Utilities.h"

class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class NiAVObject;
class NiNode;
class NIOVTaskUpdateItemDye;

class ActorUpdateManager
	: public IPluginInterface
	, public BSTEventSink<TESObjectLoadedEvent>
	, public BSTEventSink<TESInitScriptEvent>
	, public BSTEventSink<TESLoadGameEvent>
{
public:
	virtual void AddInterface(IAddonAttachmentInterface* observer);
	virtual void RemoveInterface(IAddonAttachmentInterface* observer);

	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root);

	void AddBodyUpdate(UInt64 handle);
	void AddTransformUpdate(UInt64 handle);
	void AddOverlayUpdate(UInt64 handle);
	void AddNodeOverrideUpdate(UInt64 handle);
	void AddWeaponOverrideUpdate(UInt64 handle);
	void AddAddonOverrideUpdate(UInt64 handle);
	void AddSkinOverrideUpdate(UInt64 handle);
	void AddDyeUpdate(NIOVTaskUpdateItemDye* task);

protected:
	virtual	EventResult ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESLoadGameEvent* evn, EventDispatcher<TESLoadGameEvent>* dispatcher) override;

	virtual UInt32 GetVersion() override { return 0; };
	virtual void Revert() override { };

	std::unordered_set<UInt64> m_bodyUpdates;
	std::unordered_set<UInt64> m_transformUpdates;
	std::unordered_set<UInt64> m_overlayUpdates;

	std::unordered_set<UInt64> m_overrideNodeUpdates;
	std::unordered_set<UInt64> m_overrideWeapUpdates;
	std::unordered_set<UInt64> m_overrideAddonUpdates;
	std::unordered_set<UInt64> m_overrideSkinUpdates;
	
	std::unordered_set<NIOVTaskUpdateItemDye*> m_dyeUpdates;

private:
	std::vector<IAddonAttachmentInterface*> m_observers;
};