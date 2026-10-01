#include "ActorUpdateManager.h"
#include "SKEETasks.h"

#include <cstdint>


#include "NiTransformInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "FaceMorphInterface.h"
#include "ItemDataInterface.h"
#include "IPluginInterface.h"

#include "NifUtils.h"
#include "Utilities.h"

extern const SKSE::TaskInterface* g_task;

extern BodyMorphInterface		g_bodyMorphInterface;
extern OverlayInterface			g_overlayInterface;
extern NiTransformInterface		g_transformInterface;
extern OverrideInterface		g_overrideInterface;

extern bool	g_playerOnly;
extern bool	g_enableBodyGen;
extern bool	g_enableAutoTransforms;
extern bool	g_enableBodyInit;

RE::BSEventNotifyControl ActorUpdateManager::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_source)
{
	if (a_event) {
		RE::TESForm * form = RE::TESForm::LookupByID(a_event->formID);
		if (form) {
			if (form->IsActor()) {
				RE::TESObjectREFR * reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
				if (reference) {
					if (g_enableBodyGen && !g_bodyMorphInterface.HasMorphs(reference)) {
						std::uint32_t total = g_bodyMorphInterface.EvaluateBodyMorphs(reference);
						if (total) {
							SKSE::log::debug("{} - ObjectLoad applied {} morph(s) to {}", __FUNCTION__, total, reference->GetName());

							if (m_isReverting)
							{
								AddBodyUpdate(a_event->formID);
							}
							else
							{
								g_bodyMorphInterface.UpdateModelWeight(reference);
							}
							
						}
					}

					if (g_enableAutoTransforms) {
						if (m_isReverting)
						{
							AddTransformUpdate(a_event->formID);
						}
						else
						{
							g_transformInterface.SetTransforms(a_event->formID);
						}
					}
				}
			}
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl ActorUpdateManager::ProcessEvent(const RE::TESInitScriptEvent* a_event, RE::BSTEventSource<RE::TESInitScriptEvent>* a_source)
{
	if (a_event) {
		RE::TESObjectREFR * reference = a_event->objectInitialized.get();
		if (reference && g_enableBodyInit) {
			if (reference->IsActor()) {
				if (!g_bodyMorphInterface.HasMorphs(reference)) {
					std::uint32_t total = g_bodyMorphInterface.EvaluateBodyMorphs(reference);
					if (total) {
						SKSE::log::debug("{} - ObjectInit applied {} morph(s) to {}", __FUNCTION__, total, reference->GetName());
					}
				}
			}
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl ActorUpdateManager::ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>* a_source)
{
	Flush();
	m_isReverting = false;
	m_isNewGame = false;
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl ActorUpdateManager::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_source)
{
	if (m_isNewGame) {
		Flush();
		m_isNewGame = false;
	}
	return RE::BSEventNotifyControl::kContinue;
}

void ActorUpdateManager::AddInterface(IAddonAttachmentInterface* observer)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	auto it = std::find_if(m_observers.begin(), m_observers.end(), [&](const IAddonAttachmentInterface* ob)
	{
		return ob == observer;
	});
	if (it == m_observers.end())
	{
		m_observers.push_back(observer);
	}
}

void ActorUpdateManager::RemoveInterface(IAddonAttachmentInterface* observer)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_observers.erase(std::remove_if(m_observers.begin(), m_observers.end(), [&](const IAddonAttachmentInterface* ob)
	{
		return ob == observer;
	}), m_observers.end());
}

void ActorUpdateManager::OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	for (auto observer : m_observers)
	{
		observer->OnAttach(refr, armor, addon, object, isFirstPerson, skeleton, root);
	}
}

void ActorUpdateManager::AddBodyUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_bodyUpdates.emplace(formId);
}

void ActorUpdateManager::AddTransformUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_transformUpdates.emplace(formId);
}

void ActorUpdateManager::AddOverlayUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_overlayUpdates.emplace(formId);
}

void ActorUpdateManager::AddNodeOverrideUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_overrideNodeUpdates.emplace(formId);
}

void ActorUpdateManager::AddWeaponOverrideUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_overrideWeapUpdates.emplace(formId);
}

void ActorUpdateManager::AddAddonOverrideUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_overrideAddonUpdates.emplace(formId);
}

void ActorUpdateManager::AddSkinOverrideUpdate(skee_u32 formId)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_overrideSkinUpdates.emplace(formId);
}

void ActorUpdateManager::AddDyeUpdate_Internal(NIOVTaskUpdateItemDye* itemUpdateTask)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	m_dyeUpdates.emplace(itemUpdateTask);
}

bool ActorUpdateManager::RegisterFlushCallback(const char* key, FlushCallback cb)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	auto it = m_flushObservers.find(key);
	if (it == m_flushObservers.end())
	{
		m_flushObservers.emplace(key, cb);
		return true;
	}
	return false;
}

bool ActorUpdateManager::UnregisterFlushCallback(const char* key)
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	auto it = m_flushObservers.find(key);
	if (it != m_flushObservers.end())
	{
		m_flushObservers.erase(it);
		return true;
	}
	return false;
}

void ActorUpdateManager::Flush()
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	SKEE_AddTask(g_task, new SKSETaskApplyMorphs(RE::PlayerCharacter::GetSingleton()));

	std::unordered_set<std::uint32_t> flushedSet;
	std::vector<std::uint32_t> flushedForms;

	for (std::uint32_t formId : m_bodyUpdates)
	{
		auto refr = RE::TESForm::LookupByID(formId);
		if (refr && refr->IsActor())
		{
			g_bodyMorphInterface.UpdateModelWeight(static_cast<RE::TESObjectREFR*>(refr), false);
			if (flushedSet.count(formId) == 0)
			{
				flushedSet.emplace(formId);
				flushedForms.emplace_back(formId);
			}
		}
	}

	for (std::uint32_t formId : m_transformUpdates)
	{
		g_transformInterface.SetTransforms(formId, false);
		if (flushedSet.count(formId) == 0)
		{
			flushedSet.emplace(formId);
			flushedForms.emplace_back(formId);
		}
	}

	for (std::uint32_t formId : m_overlayUpdates)
	{
		auto refr = RE::TESForm::LookupByID(formId);
		if (refr && refr->IsActor())
		{
			g_overlayInterface.QueueOverlayBuild(static_cast<RE::TESObjectREFR*>(refr));
			if (flushedSet.count(formId) == 0)
			{
				flushedSet.emplace(formId);
				flushedForms.emplace_back(formId);
			}
		}
	}

	for (std::uint32_t formId : m_overrideNodeUpdates)
	{
		g_overrideInterface.Impl_SetNodeProperties(formId, false);
		if (flushedSet.count(formId) == 0)
		{
			flushedSet.emplace(formId);
			flushedForms.emplace_back(formId);
		}
	}

	for (std::uint32_t formId : m_overrideWeapUpdates)
	{
		g_overrideInterface.Impl_SetWeaponProperties(formId, false);
		if (flushedSet.count(formId) == 0)
		{
			flushedSet.emplace(formId);
			flushedForms.emplace_back(formId);
		}
	}

	for (std::uint32_t formId : m_overrideAddonUpdates)
	{
		g_overrideInterface.Impl_SetProperties(formId, false);
		if (flushedSet.count(formId) == 0)
		{
			flushedSet.emplace(formId);
			flushedForms.emplace_back(formId);
		}
	}

	for (std::uint32_t formId : m_overrideSkinUpdates)
	{
		g_overrideInterface.Impl_SetSkinProperties(formId, false);
		if (flushedSet.count(formId) == 0)
		{
			flushedSet.emplace(formId);
			flushedForms.emplace_back(formId);
		}
	}

	for (NIOVTaskUpdateItemDye* task : m_dyeUpdates)
	{
		SKEE_AddTask(g_task, task);
		if (flushedSet.count(task->GetActor()) == 0)
		{
			flushedSet.emplace(task->GetActor());
			flushedForms.emplace_back(task->GetActor());
		}
	}

	for (auto cb : m_flushObservers)
	{
		cb.second(reinterpret_cast<skee_u32*>(&flushedForms.at(0)), static_cast<skee_u32>(flushedForms.size()));
	}

	m_bodyUpdates.clear();
	m_transformUpdates.clear();
	m_overlayUpdates.clear();
	m_overrideNodeUpdates.clear();
	m_overrideWeapUpdates.clear();
	m_overrideAddonUpdates.clear();
	m_overrideSkinUpdates.clear();
	m_dyeUpdates.clear();
}

void ActorUpdateManager::Revert()
{
	m_isReverting = true;
}

void ActorUpdateManager::PrintDiagnostics()
{
	std::lock_guard<std::recursive_mutex> scs(m_lock);
	Console_Print("ActorUpdateManager Diagnostics:");
	Console_Print("\t%lld pending body updates", m_bodyUpdates.size());
	Console_Print("\t%lld pending transform updates", m_transformUpdates.size());
	Console_Print("\t%lld pending overlay install updates", m_overlayUpdates.size());
	Console_Print("\t%lld pending node overlay updates", m_overrideNodeUpdates.size());
	Console_Print("\t%lld pending weapon overlay updates", m_overrideWeapUpdates.size());
	Console_Print("\t%lld pending addon overlay updates", m_overrideAddonUpdates.size());
	Console_Print("\t%lld pending skin updates", m_overrideSkinUpdates.size());
	Console_Print("\t%lld pending dye updates", m_dyeUpdates.size());
}