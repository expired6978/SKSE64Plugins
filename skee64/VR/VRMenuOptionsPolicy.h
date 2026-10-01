#pragma once
#include "RaceSexMenuVRKeyboardPolicy.h"
#include "VRRuntime.h"
#include <string_view>
#include <atomic>

namespace SKEE::VR::MenuOptionsPolicy
{
    // Per-runtime settings take precedence. Retain an explicit legacy setting
    // as a migration fallback; absent/invalid settings use runtime defaults.
    inline bool UseQuill(Runtime runtime, int steamVR, int ocu, int legacy)
    {
        const int specific = runtime == Runtime::SteamVR ? steamVR :
            runtime == Runtime::OpenComposite ? ocu : -1;
        if (specific == 0 || specific == 1) return specific == 1;
        if (legacy == 0 || legacy == 1) return legacy == 1;
        return runtime == Runtime::SteamVR;
    }
    // A main-menu StartNewGame choice is authoritative even when the engine
    // loads a template save. Engine-only intent does not survive a save load.
    // Consumed also prevents a late/duplicate SKSE NewGame notification from
    // renaming a later showracemenu session.
    class NameStartIntent
    {
    public:
        void StartFromMenu() { state_.store(State::MenuStart); }
        void EngineNewGame()
        {
            auto expected = State::None;
            state_.compare_exchange_strong(expected, State::EngineStart);
        }
        void SaveLoading() { PreserveMenuStartOrReset(); }
        void Revert() { PreserveMenuStartOrReset(); }
        void Cancel() { state_.store(State::None); }
        void Consume() { state_.store(State::Consumed); }
        bool Pending() const
        {
            const auto state = state_.load();
            return state == State::MenuStart || state == State::EngineStart;
        }
    private:
        enum class State { None, MenuStart, EngineStart, Consumed };
        void PreserveMenuStartOrReset()
        {
            auto observed = state_.load();
            while (observed != State::MenuStart &&
                !state_.compare_exchange_weak(observed, State::None)) {}
        }
        std::atomic<State> state_{ State::None };
    };

    inline bool ValidPlayerName(std::string_view name)
    {
        if (name.empty() || name.size() > 255 || !KeyboardPolicy::IsValidUTF8(name)) return false;
        bool visible = false;
        for (const auto character : name) {
            const auto byte = static_cast<unsigned char>(character);
            if (byte < 0x20 || byte == 0x7F) return false;
            if (byte != ' ') visible = true;
        }
        return visible;
    }

    inline bool ApplyPlayerName(bool newGamePending, bool overrideExisting, std::string_view name)
    {
        return (newGamePending || overrideExisting) && ValidPlayerName(name);
    }

    // Only during construction, before UI accounts for menu ownership.
    // kUpdateUsesCursor would otherwise let gamepad detection hide the quill.
    template <class Flags, class Flag>
    void ConfigureQuill(Flags& flags, bool enabled, Flag usesCursor, Flag updateUsesCursor)
    {
        if (!enabled) return;
        flags.set(usesCursor);
        flags.reset(updateUsesCursor);
    }
}
