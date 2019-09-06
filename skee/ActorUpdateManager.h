#pragma once

#include <vector>
#include "skse64/GameEvents.h"
#include "IPluginInterface.h"

class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class NiAVObject;
class NiNode;

class ActorUpdateManager
	: public IPluginInterface
	, public BSTEventSink<TESObjectLoadedEvent>
	, public BSTEventSink<TESInitScriptEvent>
{
public:
	virtual void AddInterface(IAddonAttachmentInterface* observer);
	virtual void RemoveInterface(IAddonAttachmentInterface* observer);

	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root);

protected:
	virtual	EventResult ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent> * dispatcher) override;

	virtual UInt32 GetVersion() override { return 0; };
	virtual void Revert() override { };

private:
	std::vector<IAddonAttachmentInterface*> m_observers;
};