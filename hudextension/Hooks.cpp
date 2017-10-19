#include "Hooks.h"
#include "HUDExtension.h"
#include <float.h>

#include "skse64/GameMenus.h"

const _Fn85FD10 Fn85FD10 = (_Fn85FD10)0x0085FD10;

void __stdcall InstallHudComponents(HUDMenu * menu)
{
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

const UInt32 kInstallRaceMenuHook_Base = 0x008652E0 + 0x4F5;
const UInt32 kInstallRaceMenuHook_Entry_retn = kInstallRaceMenuHook_Base + 0x05;

/*
__declspec(naked) void InstallRaceMenuHook_Entry(void)
{
	__asm
	{
		call	Fn85FD10
		pushad
		push	edi
		call	InstallHudComponents
		popad

		jmp		[kInstallRaceMenuHook_Entry_retn]
	}
}
*/