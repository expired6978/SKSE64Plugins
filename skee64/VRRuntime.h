#pragma once
#include <string_view>

namespace SKEE::VR
{
    enum class Runtime { Unknown, SteamVR, OpenComposite };
    // Inspect Skyrim's already-active IVRSystem and loaded runtime loader.
    // Never initialise another runtime, load a DLL or inspect controller poses.
    Runtime ActiveRuntime();
    constexpr Runtime RuntimeForModules(bool implementationOCU, bool implementationSteamVR,
        bool loaderOCU, bool loaderOpenVR, bool steamClientLoaded)
    {
        if (implementationOCU) return Runtime::OpenComposite;
        if (implementationSteamVR) return Runtime::SteamVR;
        // SkyrimVRTools may own the IVRSystem vtable. Its module is not the
        // runtime: classify the existing loader behind that proxy instead.
        if (loaderOCU) return Runtime::OpenComposite;
        if (loaderOpenVR && steamClientLoaded) return Runtime::SteamVR;
        return Runtime::Unknown;
    }
    constexpr bool UsesStreamedKeyboard(Runtime runtime)
    {
        // Only a positively identified buffered backend may replace drafts.
        return runtime != Runtime::OpenComposite;
    }
    constexpr std::string_view RuntimeName(Runtime runtime)
    {
        switch (runtime) {
        case Runtime::SteamVR: return "SteamVR";
        case Runtime::OpenComposite: return "OpenComposite/OCU";
        default: return "unknown";
        }
    }
}
