#pragma once

#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>

#include <RE/B/BSTEvent.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiNode.h>
#include <RE/T/TESCellFullyLoadedEvent.h>
#include <RE/T/TESInitScriptEvent.h>
#include <RE/T/TESLoadGameEvent.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectLoadedEvent.h>
#include <RE/T/TESObjectREFR.h>

#include "DeferredTaskSubmission.h"
#include "IPluginInterface.h"
#include "Utilities.h"

class NIOVTaskUpdateItemDye;
class ActorUpdateManager
	: public IActorUpdateManager
	, public RE::BSTEventSink<RE::TESObjectLoadedEvent>
	, public RE::BSTEventSink<RE::TESInitScriptEvent>
	, public RE::BSTEventSink<RE::TESLoadGameEvent>
	, public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
{
public:
	virtual void AddInterface(IAddonAttachmentInterface* observer) override;
	virtual void RemoveInterface(IAddonAttachmentInterface* observer) override;

	virtual void OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root);

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
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_source) override;
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_source) override;
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESInitScriptEvent* a_event, RE::BSTEventSource<RE::TESInitScriptEvent>* a_source) override;
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>* a_source) override;

	virtual skee_u32 GetVersion() override { return kCurrentPluginVersion; };

	bool m_isReverting = false;
	bool m_isNewGame = false;
	
	std::recursive_mutex m_lock;
	std::unordered_set<std::uint32_t> m_bodyUpdates;
	std::unordered_set<std::uint32_t> m_transformUpdates;
	std::unordered_set<std::uint32_t> m_overlayUpdates;

	std::unordered_set<std::uint32_t> m_overrideNodeUpdates;
	std::unordered_set<std::uint32_t> m_overrideWeapUpdates;
	std::unordered_set<std::uint32_t> m_overrideAddonUpdates;
	std::unordered_set<std::uint32_t> m_overrideSkinUpdates;
	
	std::unordered_set<NIOVTaskUpdateItemDye*> m_dyeUpdates;

private:
	void FlushLocked();

	skee::DeferredTaskSubmission m_taskSubmission;

	std::map<std::string, FlushCallback> m_flushObservers;
	std::vector<IAddonAttachmentInterface*> m_observers;
};