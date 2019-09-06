#include "ActorUpdateManager.h"

#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"

#include "NiTransformInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "IPluginInterface.h"

#include "Utilities.h"

extern BodyMorphInterface		g_bodyMorphInterface;
extern OverlayInterface			g_overlayInterface;
extern NiTransformInterface		g_transformInterface;

extern bool	g_playerOnly;
extern bool	g_enableBodyGen;
extern bool	g_enableAutoTransforms;
extern bool	g_enableBodyInit;

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
							g_bodyMorphInterface.UpdateModelWeight(reference);
						}
					}

					if (g_enableAutoTransforms) {
						g_transformInterface.SetHandleNodeTransforms(VirtualMachine::GetHandle(form, TESObjectREFR::kTypeID));
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