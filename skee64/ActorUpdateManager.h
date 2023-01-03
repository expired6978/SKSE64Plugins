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

	virtual void AddBodyUpdate(UInt32 formId) override;
	virtual void AddTransformUpdate(UInt32 formId) override;
	virtual void AddOverlayUpdate(UInt32 formId) override;
	virtual void AddNodeOverrideUpdate(UInt32 formId) override;
	virtual void AddWeaponOverrideUpdate(UInt32 formId) override;
	virtual void AddAddonOverrideUpdate(UInt32 formId) override;
	virtual void AddSkinOverrideUpdate(UInt32 formId) override;

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

	virtual UInt32 GetVersion() override { return 1; };

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
	std::vector<IAddonAttachmentInterface*> m_observers;
};
