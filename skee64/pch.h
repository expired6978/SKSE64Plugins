#pragma once
#include <string_view>
using namespace std::literals;

// Order matters: RE/Skyrim.h must come first. Its headers that use
// REX::W32::MAX_PATH (BSFile, NiStream, TESFile, LooseFileLocation) are all
// pulled in before any Windows API header appears mid-aggregate (d3d11.h via
// RendererShadowState.h). If SKSE/SKSE.h came first, its xbyak include would
// define the Windows MAX_PATH macro and break REX::W32::MAX_PATH.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "SKEETasks.h"


// DirectXTK's SimpleMath.h (pulled in via RE/S/State.h) transitively includes
// <windows.h>, which leaves several Win32 macros defined at the end of the PCH.
// Drop the ones that collide with CommonLib's REX::W32 names so plugin code can use
// them qualified (e.g. REX::W32::MAX_PATH, REX::W32::INVALID_HANDLE_VALUE).
#undef MAX_PATH
#undef INVALID_HANDLE_VALUE
#undef FILE_ATTRIBUTE_DIRECTORY
