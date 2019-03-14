#pragma once

#include "skse64/GameEvents.h"

class ActorUpdateManager
	: public BSTEventSink<TESObjectLoadedEvent>
	, public BSTEventSink<TESInitScriptEvent>
{
	virtual	EventResult ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent> * dispatcher) override;
	virtual	EventResult ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent> * dispatcher) override;
};