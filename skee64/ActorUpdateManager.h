#pragma once

#include <vector>
#include <unordered_set>
#include <mutex>

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
	: public IActorUpdateManager
	, public BSTEventSink<TESObjectLoadedEvent>
	, public BSTEventSink<TESInitScriptEvent>
	, public BSTEventSink<TESLoadGameEvent>
	, public BSTEventSink<TESCellFullyLoadedEvent>
{
public:
	virtual void AddInterface(IAddonAttachmentInterface* observer) override;
	virtual void RemoveInterface(IAddonAttachmentInterface* observer) override;

	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root);

	virtual void AddBodyUpdate(skee_u32 formId) override;
	virtual void AddTransformUpdate(skee_u32 formId) override;
	virtual void AddOverlayUpdate(skee_u32 formId) override;
	virtual void AddNodeOverrideUpdate(skee_u32 formId) override;
	virtual void AddWeaponOverrideUpdate(skee_u32 formId) override;
	virtual void AddAddonOverrideUpdate(skee_u32 formId) override;
	virtual void AddSkinOverrideUpdate(skee_u32 formId) override;
	virtual bool RegisterFlushCallback(const char* key, FlushCallback cb) override;
	virtual bool UnregisterFlushCallback(const char* key) override;

	void AddDyeUpdate_Internal(NIOVTaskUpdateItemDye* task);

	virtual void Flush() override;
	virtual void Revert() override;

	bool isReverting() const { return m_isReverting; }
	bool isNewGame() const { return m_isNewGame; }
	void setNewGame(bool ng) { m_isNewGame = ng; }

	void PrintDiagnostics();

protected:
	virtual	EventResult ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESCellFullyLoadedEvent* evn, EventDispatcher<TESCellFullyLoadedEvent>* dispatcher) override;
	virtual	EventResult ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESLoadGameEvent* evn, EventDispatcher<TESLoadGameEvent>* dispatcher) override;

	virtual skee_u32 GetVersion() override { return kCurrentPluginVersion; };

	bool m_isReverting = false;
	bool m_isNewGame = false;
	
	std::recursive_mutex m_lock;
	std::unordered_set<UInt32> m_bodyUpdates;
	std::unordered_set<UInt32> m_transformUpdates;
	std::unordered_set<UInt32> m_overlayUpdates;

	std::unordered_set<UInt32> m_overrideNodeUpdates;
	std::unordered_set<UInt32> m_overrideWeapUpdates;
	std::unordered_set<UInt32> m_overrideAddonUpdates;
	std::unordered_set<UInt32> m_overrideSkinUpdates;
	
	std::unordered_set<NIOVTaskUpdateItemDye*> m_dyeUpdates;

private:
	std::map<std::string, FlushCallback> m_flushObservers;
	std::vector<IAddonAttachmentInterface*> m_observers;
};

