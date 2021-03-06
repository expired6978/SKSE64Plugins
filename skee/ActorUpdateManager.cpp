#include "ActorUpdateManager.h"

#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"

#include "NiTransformInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "FaceMorphInterface.h"
#include "ItemDataInterface.h"
#include "IPluginInterface.h"

#include "NifUtils.h"
#include "Utilities.h"

extern SKSETaskInterface* g_task;

extern BodyMorphInterface		g_bodyMorphInterface;
extern OverlayInterface			g_overlayInterface;
extern NiTransformInterface		g_transformInterface;
extern OverrideInterface		g_overrideInterface;

extern bool	g_playerOnly;
extern bool	g_enableBodyGen;
extern bool	g_enableAutoTransforms;
extern bool	g_enableBodyInit;
extern bool g_isReverting;

EventResult ActorUpdateManager::ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent>* dispatcher)
{
	if (evn) {
		TESForm * form = LookupFormByID(evn->formId);
		if (form) {
			if (form->formType == Character::kTypeID) {
				TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
				if (reference) {
					if (g_enableBodyGen && !g_bodyMorphInterface.HasMorphs(reference)) {
						UInt32 total = g_bodyMorphInterface.EvaluateBodyMorphs(reference);
						if (total) {
							_DMESSAGE("%s - ObjectLoad applied %d morph(s) to %s", __FUNCTION__, total, CALL_MEMBER_FN(reference, GetReferenceName)());

							if (g_isReverting)
							{
								AddBodyUpdate(evn->formId);
							}
							else
							{
								g_bodyMorphInterface.UpdateModelWeight(reference);
							}
							
						}
					}

					if (g_enableAutoTransforms) {
						if (g_isReverting)
						{
							AddTransformUpdate(evn->formId);
						}
						else
						{
							g_transformInterface.SetTransforms(evn->formId);
						}
					}
				}
			}
		}
	}
	return kEvent_Continue;
}

EventResult ActorUpdateManager::ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent>* dispatcher)
{
	if (evn) {
		TESObjectREFR * reference = evn->reference;
		if (reference && g_enableBodyInit) {
			if (reference->formType == Character::kTypeID) {
				if (!g_bodyMorphInterface.HasMorphs(reference)) {
					UInt32 total = g_bodyMorphInterface.EvaluateBodyMorphs(reference);
					if (total) {
						_DMESSAGE("%s - ObjectInit applied %d morph(s) to %s", __FUNCTION__, total, CALL_MEMBER_FN(reference, GetReferenceName)());
					}
				}
			}
		}
	}
	return kEvent_Continue;
}

EventResult ActorUpdateManager::ReceiveEvent(TESLoadGameEvent* evn, EventDispatcher<TESLoadGameEvent>* dispatcher)
{
	Flush();
	return kEvent_Continue;
}

void ActorUpdateManager::AddInterface(IAddonAttachmentInterface* observer)
{
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
	m_observers.erase(std::remove_if(m_observers.begin(), m_observers.end(), [&](const IAddonAttachmentInterface* ob)
	{
		return ob == observer;
	}), m_observers.end());
}

void ActorUpdateManager::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	for (auto observer : m_observers)
	{
		observer->OnAttach(refr, armor, addon, object, isFirstPerson, skeleton, root);
	}
}

void ActorUpdateManager::AddBodyUpdate(UInt32 formId)
{
	m_bodyUpdates.emplace(formId);
}

void ActorUpdateManager::AddTransformUpdate(UInt32 formId)
{
	m_transformUpdates.emplace(formId);
}

void ActorUpdateManager::AddOverlayUpdate(UInt32 formId)
{
	m_overlayUpdates.emplace(formId);
}

void ActorUpdateManager::AddNodeOverrideUpdate(UInt32 formId)
{
	m_overrideNodeUpdates.emplace(formId);
}

void ActorUpdateManager::AddWeaponOverrideUpdate(UInt32 formId)
{
	m_overrideWeapUpdates.emplace(formId);
}

void ActorUpdateManager::AddAddonOverrideUpdate(UInt32 formId)
{
	m_overrideAddonUpdates.emplace(formId);
}

void ActorUpdateManager::AddSkinOverrideUpdate(UInt32 formId)
{
	m_overrideSkinUpdates.emplace(formId);
}

void ActorUpdateManager::AddDyeUpdate_Internal(NIOVTaskUpdateItemDye* itemUpdateTask)
{
	m_dyeUpdates.emplace(itemUpdateTask);
}

void ActorUpdateManager::Flush()
{
	PlayerCharacter* player = (*g_thePlayer);
	g_task->AddTask(new SKSETaskApplyMorphs(player));

	for (UInt32 formId : m_bodyUpdates)
	{
		auto refr = LookupFormByID(formId);
		if (refr && refr->formType == Character::kTypeID)
		{
			g_bodyMorphInterface.UpdateModelWeight(static_cast<TESObjectREFR*>(refr), false);
		}
	}

	for (UInt32 formId : m_transformUpdates)
	{
		g_transformInterface.SetTransforms(formId, false);
	}

	for (UInt32 formId : m_overlayUpdates)
	{
		auto refr = LookupFormByID(formId);
		if (refr && refr->formType == Character::kTypeID)
		{
			g_overlayInterface.AddOverlays(static_cast<TESObjectREFR*>(refr));
		}
	}

	for (UInt32 formId : m_overrideNodeUpdates)
	{
		g_overrideInterface.SetNodeProperties(formId, false);
	}

	for (UInt32 formId : m_overrideWeapUpdates)
	{
		g_overrideInterface.SetWeaponProperties(formId, false);
	}

	for (UInt32 formId : m_overrideAddonUpdates)
	{
		g_overrideInterface.SetProperties(formId, false);
	}

	for (UInt32 formId : m_overrideSkinUpdates)
	{
		g_overrideInterface.SetSkinProperties(formId, false);
	}

	for (NIOVTaskUpdateItemDye* task : m_dyeUpdates)
	{
		g_task->AddTask(task);
	}

	m_bodyUpdates.clear();
	m_transformUpdates.clear();
	m_overlayUpdates.clear();
	m_overrideNodeUpdates.clear();
	m_overrideWeapUpdates.clear();
	m_overrideAddonUpdates.clear();
	m_overrideSkinUpdates.clear();
	m_dyeUpdates.clear();

	g_isReverting = false;
}
