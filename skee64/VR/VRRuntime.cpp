#include "pch.h"
#include "VRRuntime.h"
#include <cstring>
#if defined(ENABLE_SKYRIM_VR)
#include "RE/B/BSOpenVR.h"
#include <Windows.h>

namespace SKEE::VR
{
    Runtime ActiveRuntime()
    {
        auto* openVR = RE::BSOpenVR::GetSingleton();
        auto* system = openVR ? openVR->vrSystem : nullptr;
        if (!system) return Runtime::Unknown;
        const auto* table = *reinterpret_cast<void***>(system);
        HMODULE module{};
        if (!table || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<const char*>(table[0]), &module)) return Runtime::Unknown;
        // Presence only: querying a stub runtime path can abort OpenComposite.
        const auto isOCU = [](HMODULE candidate) {
            return candidate && GetProcAddress(candidate, "VR_GetGenericInterface") &&
                GetProcAddress(candidate, "HmdSystemFactory");
        };
        char path[32768]{};
        const auto length = GetModuleFileNameA(module, path, sizeof(path));
        if (!length || length >= sizeof(path)) return Runtime::Unknown;
        const auto* slash = std::strrchr(path, '\\');
        const auto* name = slash ? slash + 1 : path;
        // SkyrimVRTools/controller proxies can implement the system vtable.
        // Inspect only already-loaded modules; do not load/initialise a backend
        // or unwrap/dereference another plugin's private proxy structures.
        const auto loader = GetModuleHandleA("openvr_api.dll");
        return RuntimeForModules(isOCU(module), _stricmp(name, "vrclient_x64.dll") == 0,
            isOCU(loader), loader && GetProcAddress(loader, "VR_GetGenericInterface"),
            GetModuleHandleA("vrclient_x64.dll") != nullptr);
    }
}
#else
namespace SKEE::VR { Runtime ActiveRuntime() { return Runtime::Unknown; } }
#endif
