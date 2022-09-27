#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"
#include "skse64_common/SafeWrite.h"
#include "skse64/GameAPI.h"
#include <shlobj.h>

#include "skse64_common/Relocation.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/SafeWrite.h"
#include "xbyak/xbyak.h"

#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameEvents.h"

#include "HUDExtension.h"

IDebugLog	gLog;

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

SKSEMessagingInterface	* g_messagingInterface = nullptr;
SKSETaskInterface		* g_task = nullptr;
SKSEScaleformInterface	* g_scaleform = nullptr;
SKSETrampolineInterface* g_trampoline = nullptr;

HUDExtension * g_hudExtension = nullptr;

class CombatEventHandler : public BSTEventSink < TESCombatEvent >
{
public:
	virtual	EventResult ReceiveEvent(TESCombatEvent * evn, EventDispatcher<TESCombatEvent> * dispatcher)
	{
		float current = 0.0, max = 0.0;

		UInt32 contextFlags = ObjectWidget::kContext_None;
		if (evn->state == 0)
			contextFlags |= ObjectWidget::kContext_LeaveCombat;

		Actor * actor = DYNAMIC_CAST(evn->source, TESObjectREFR, Actor);
		if (actor) {
			current = actor->actorValueOwner.GetCurrent(24);
			max = actor->actorValueOwner.GetMaximum(24);

			if (!CALL_MEMBER_FN(*g_thePlayer, IsHostileToActor)(actor))
				contextFlags |= ObjectWidget::kContext_Friendly;
		}

		if (evn->source && evn->source != (*g_thePlayer))
			g_task->AddUITask(new AddRemoveWidgetTask(evn->source->formID, current, max, evn->state, contextFlags));
		return kEvent_Continue;
	}
};

class DeathEventHandler : public BSTEventSink < TESDeathEvent >
{
public:
	virtual	EventResult ReceiveEvent(TESDeathEvent * evn, EventDispatcher<TESDeathEvent> * dispatcher)
	{
		if (evn->source && evn->source != (*g_thePlayer))
			g_task->AddUITask(new AddRemoveWidgetTask(evn->source->formID, 0.0, 0.0, 0, ObjectWidget::kContext_Death));
		return kEvent_Continue;
	}
};

CombatEventHandler	g_combatEventHandler;
DeathEventHandler	g_deathEventHandler;

void MessageHandler(SKSEMessagingInterface::Message * msg)
{
	switch (msg->type)
	{
		case SKSEMessagingInterface::kMessage_DataLoaded:
		{
			auto eventDispatchers = GetEventDispatcherList();
			eventDispatchers->combatDispatcher.AddEventSink(&g_combatEventHandler);
			eventDispatchers->deathDispatcher.AddEventSink(&g_deathEventHandler);
		}
		break;
		case SKSEMessagingInterface::kMessage_PostLoadGame:
		{
			if (g_hudExtension)
				g_hudExtension->RemoveAllHealthbars();
		}
		break;
	}
}

class SKSEScaleform_SetHUDFlags : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

		g_hudExtension->hudFlags = args->args[0].GetNumber();
	}
};

class SKSEScaleform_GetHUDFlags : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args)
	{
		args->result->SetNumber(g_hudExtension->hudFlags);
	}
};

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction<SKSEScaleform_GetHUDFlags>(root, view, "GetHUDFlags");
	RegisterFunction<SKSEScaleform_SetHUDFlags>(root, view, "SetHUDFlags");
	return true;
}

RelocAddr<uintptr_t> HUDMenu_Hook_Target(0x0008BE6E0 + 0x9E8); // HUDMenu::ctor
typedef void(*_HUDMenu_RegisterMarkers)(GFxValue * value);
RelocAddr<_HUDMenu_RegisterMarkers> HUDMenu_RegisterMarkers(0x008C5BF0);

void HUDMenu_RegisterMarkers_Hook(GFxValue * value, HUDMenu * menu)
{
	HUDMenu_RegisterMarkers(value);

	GFxValue hudComponent;
	GFxValue args[2];
	args[0].SetString("hudExtension");
	menu->view->Invoke("_root.getNextHighestDepth", &args[1], NULL, 0);
	menu->view->Invoke("_root.createEmptyMovieClip", &hudComponent, args, 2);
	args[0].SetString("hud_extension.swf");
	hudComponent.Invoke("loadMovie", NULL, &args[0], 1);
	g_hudExtension = new HUDExtension(menu->view);
	g_hudExtension->object = hudComponent;
	menu->hudComponents.Push((HUDObject*&)g_hudExtension);
}

extern "C"
{
	__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
	{
		SKSEPluginVersionData::kVersion,
		1,
		"hudextension",
		"Expired6978",
		"expired6978@gmail.com",
		0,	// not version independent
		0,
		{ RUNTIME_VERSION_1_6_640, 0 },	// compatible with 1.6.640
		0,	// works with any version of the script extender. you probably do not need to put anything here
	};

bool SKSEPlugin_Query(const SKSEInterface * skse)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\skse_hudextension.log");
	_MESSAGE("skse_hudextension");

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_MESSAGE("loaded in editor, marking as incompatible");
		return false;
	}

	g_scaleform = (SKSEScaleformInterface *)skse->QueryInterface(kInterface_Scaleform);
	if (!g_scaleform)
	{
		_MESSAGE("couldn't get scaleform interface");
		return false;
	}

	// get the messaging interface and query its version
	g_messagingInterface = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
	if(!g_messagingInterface)
	{
		_MESSAGE("couldn't get messaging interface");
		return false;
	}
	if(g_messagingInterface->interfaceVersion < 1)
	{
		_MESSAGE("messaging interface too old (%d expected %d)", g_messagingInterface->interfaceVersion, 1);
		return false;
	}

	// get the messaging interface and query its version
	g_task = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
	if(!g_task)
	{
		_MESSAGE("couldn't get task interface");
		return false;
	}
	if(g_task->interfaceVersion < 2)
	{
		_MESSAGE("task interface too old (%d expected %d)", g_task->interfaceVersion, 1);
		return false;
	}

	g_trampoline = (SKSETrampolineInterface*)skse->QueryInterface(kInterface_Trampoline);
	if (!g_trampoline) {
		_ERROR("couldn't get trampoline interface");
	}

	// supported runtime version
	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	if (!SKSEPlugin_Query(skse))
		return false;

	_MESSAGE("Hud Extension Loaded");

	g_messagingInterface->RegisterListener(0, "SKSE", MessageHandler);
	g_scaleform->Register("HudExtension", RegisterScaleform);

	if (g_trampoline) {
		void* branch = g_trampoline->AllocateFromBranchPool(g_pluginHandle, 32);
		if (!branch) {
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		g_branchTrampoline.SetBase(32, branch);

		void* local = g_trampoline->AllocateFromLocalPool(g_pluginHandle, 64);
		if (!local) {
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		g_localTrampoline.SetBase(64, local);
	}
	else {
		if (!g_branchTrampoline.Create(32)) {
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}
		if (!g_localTrampoline.Create(64, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}
	}

	{
		struct HUDMenu_RegisterExtension_Code : Xbyak::CodeGenerator {
			HUDMenu_RegisterExtension_Code(void * buf, UInt64 funcAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				mov(rdx, r13);
				call(ptr[rip + funcLabel]);
				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(HUDMenu_Hook_Target.GetUIntPtr() + 5);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		HUDMenu_RegisterExtension_Code code(codeBuf, (uintptr_t)HUDMenu_RegisterMarkers_Hook);
		g_localTrampoline.EndAlloc(code.getCurr());

		g_branchTrampoline.Write5Branch(HUDMenu_Hook_Target.GetUIntPtr(), uintptr_t(code.getCode()));
	}
	
	return true;
}

};
