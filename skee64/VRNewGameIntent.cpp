#include "pch.h"
#include "VRNewGameIntent.h"

#if defined(ENABLE_SKYRIM_VR)
#include "CharacterCreationInterface.h"
#include "MenuConfiguration.h"
#include <RE/F/FxDelegateArgs.h>
#include <RE/F/FxDelegateHandler.h>
#include <RE/G/GString.h>
#include <RE/M/MainMenu.h>
#include <RE/U/UI.h>
#include <RE/Offsets_VTABLE.h>
#include <array>
#include <atomic>
#include <cstring>

extern CharacterCreationInterface g_characterCreationInterface;

namespace
{
    using Callback = RE::FxDelegateHandler::CallbackFn;
    using Processor = RE::FxDelegateHandler::CallbackProcessor;
    using Accept = void(RE::MainMenu*, Processor*);
    REL::Relocation<Accept> originalAccept;
    constexpr std::array<const char*, 6> names{
        "StartNewGame", "CONTINUE", "ContinueLastSavedGame", "LOAD", "LoadGame", "QuitToDesktop" };
    std::array<std::atomic<Callback*>, names.size()> originals{};

    template <std::size_t Index>
    void Observe(const RE::FxDelegateArgs& args)
    {
        auto* ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::MainMenu>() : RE::GPtr<RE::MainMenu>{};
        // Only the live main menu may supply start intent. Other movies with
        // matching callback names must not authorize a saved-character rename.
        if (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && menu &&
            menu->uiMovie.get() == args.GetMovie()) {
            if constexpr (Index == 0) g_characterCreationInterface.BeginMainMenuNewGame();
            else g_characterCreationInterface.CancelConfiguredName();
        }
        if (auto* original = originals[Index].load()) original(args);
    }

    constexpr std::array<Callback*, names.size()> observers{
        Observe<0>, Observe<1>, Observe<2>, Observe<3>, Observe<4>, Observe<5> };

    class ObserveRegistration final : public Processor
    {
    public:
        explicit ObserveRegistration(Processor& next) : next_(next) {}
        void Process(const RE::GString& name, Callback* method) override
        {
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (method && std::string_view(name.data(), name.size()) == names[i]) {
                    auto* expected = originals[i].load();
                    // Never replace an existing bridge with itself or change
                    // the original used by an already registered live bridge.
                    if (method != observers[i] && (!expected || expected == method)) {
                        originals[i].store(method);
                        next_.Process(name, observers[i]);
                        SKSE::log::info("VR main-menu name observer registered: {}", names[i]);
                        return;
                    }
                    SKSE::log::warn("VR main-menu name observer left conflicting callback untouched: {}", names[i]);
                }
            }
            next_.Process(name, method);
        }
    private:
        Processor& next_;
    };

    void AcceptHook(RE::MainMenu* menu, Processor* processor)
    {
        if (!processor) { originalAccept(menu, processor); return; }
        ObserveRegistration observer(*processor);
        originalAccept(menu, &observer);
    }
}
#endif

bool SKEE::VR::InstallNewGameIntentObserver()
{
#if defined(ENABLE_SKYRIM_VR)
    if (!REL::Module::IsVR() || MenuConfiguration::PlayerName().empty()) return false;
    static bool installed{};
    if (installed) return true;
    if (REL::Module::get().version() != REL::Version(1, 4, 15, 0)) return false;
    // MainMenu::Accept, VR 1.4.15: independently inspected in the retained
    // decrypted engine dump. Only replace this exact native vtable entry.
    // The callback processor is synchronous, stack-owned and forwarded intact.
    REL::Relocation<std::uintptr_t> table{ RE::VTABLE_MainMenu[0] };
    const auto& module = REL::Module::get();
    const auto rdata = module.segment(REL::Segment::Name::rdata);
    const auto text = module.segment(REL::Segment::Name::textx);
    const auto slot = table.address() + sizeof(std::uintptr_t);
    if (slot < rdata.address() || slot + sizeof(std::uintptr_t) > rdata.address() + rdata.size()) return false;
    std::uintptr_t target{};
    std::memcpy(&target, reinterpret_cast<const void*>(slot), sizeof(target));
    constexpr std::array<std::uint8_t, 16> prefix{
        0x40,0x55,0x53,0x57,0x48,0x8D,0x6C,0x24,0xB9,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00 };
    if (target != module.base() + 0x8CFC00 || target < text.address() ||
        target + prefix.size() > text.address() + text.size() ||
        std::memcmp(reinterpret_cast<const void*>(target), prefix.data(), prefix.size()) != 0) {
        SKSE::log::warn("VR main-menu naming hook unavailable: native Accept contract mismatch; existing names protected");
        return false;
    }
    originalAccept = table.write_vfunc(1, AcceptHook);
    installed = true;
    SKSE::log::info("Installed VR main-menu New Game intent observer (no movie asset changes)");
    return true;
#else
    return false;
#endif
}
