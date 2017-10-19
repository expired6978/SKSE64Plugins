#pragma once

#include "skse64/GameTypes.h"

class HUDMenu;

extern const UInt32 kInstallRaceMenuHook_Base;
extern const UInt32 kInstallRaceMenuHook_Entry_retn;

void __stdcall InstallHudComponents(HUDMenu * menu);
void InstallRaceMenuHook_Entry(void);

typedef void (* _Fn85FD10)();
extern const _Fn85FD10 Fn85FD10;