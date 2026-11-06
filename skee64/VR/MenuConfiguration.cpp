#include "pch.h"
#include "MenuConfiguration.h"
#include "MenuAppearancePolicy.h"
#include "ScaleformUtils.h"
#include "VRMenuOptionsPolicy.h"
#include "VRRuntime.h"
#include <array>
#include <cmath>
#include <cstdlib>

namespace SKEE::MenuConfiguration
{
    namespace
    {
        ReadOption readOption{};
        float menuHeight{}, pickerHeight{};
        bool forceVertical{true};
        std::string playerName;
        bool overrideExistingPlayerName{};
        int legacyQuill{-1}, steamVRQuill{-1}, ocuQuill{-1};
        constexpr std::array<const char*, 3> profileNames{ "Flat", "VR Normal", "VR Face" };
        struct Field { const char* key; const char* member; double minimum, maximum; };
        constexpr Field fields[]{
            {"fBackgroundOpacity", "backgroundOpacity", 0, 100},
            {"fTextOpacity", "textOpacity", 0, 100},
            {"fImageOpacity", "imageOpacity", 0, 100},
            {"fAccentOpacity", "accentOpacity", 0, 100},
            {"fSelectionOpacity", "selectionOpacity", 0, 100},
            {"fBorderOpacity", "borderOpacity", 0, 100},
            {"fPanelScale", "panelScale", 25, 300},
            {"fColorPickerScale", "colorPickerScale", 50, 400},
            {"fPanelX", "panelX", -2048, 2048},
            {"fPanelY", "panelY", -2048, 2048},
            {"fAzimuth", "azimuth", -85, 85},
            {"fSculptAzimuth", "sculptAzimuth", -85, 85},
            {"fElevation", "elevation", -60, 60},
            {"fDistance", "distance", 30, 300},
            {"fSurfaceScale", "surfaceScale", 25, 300},
            {"fColorPickerAzimuth", "pickerAzimuth", -85, 85},
            {"fColorPickerElevation", "pickerElevation", -60, 60},
            {"fColorPickerDistance", "pickerDistance", 30, 300},
            {"fColorPickerSurfaceScale", "pickerSurfaceScale", 25, 300},
            {"bConsolidatePanel", "consolidate", 0, 1}
        };
        std::string Option(const char* profile, const char* key)
        {
            const auto section = std::string("Menu Profile ") + profile;
            auto value = readOption(section.c_str(), key);
            return value.empty() ? readOption("Menu Appearance", key) : value;
        }
        double Number(const std::string& value, double low, double high)
        {
            if (value.empty()) return -9999;
            char* end{};
            const auto number = std::strtod(value.c_str(), &end);
            while (end && (*end == ' ' || *end == '\t')) ++end;
            return end && end != value.c_str() && !*end && std::isfinite(number) && number >= low && number <= high ? number : -9999;
        }
        class CategoryPresentation final : public RE::GFxFunctionHandler
        {
            void Call(Params& args) override
            {
                if (!args.retVal || !readOption || (args.argCount != 1 && args.argCount != 3) || !args.args[0].IsNumber()) return;
                const auto flag = args.args[0].GetNumber();
                if (!(flag >= 1 && flag <= 0x7FFFFFFF && std::floor(flag) == flag)) return;
                // Stable engine category flag, never a translated display name.
                auto section = "Menu Category " + std::to_string(static_cast<unsigned>(flag));
                if (args.argCount == 3) {
                    if (!args.args[1].IsString() || !args.args[2].IsString()) return;
                    auto valid = [](const char* text) {
                        if (!text || !*text) return false;
                        unsigned length = 0;
                        for (; *text; ++text) if (++length > 48 || !(std::isalnum(static_cast<unsigned char>(*text)) || *text == '_' || *text == '-' || *text == '.')) return false;
                        return true;
                    };
                    if (!valid(args.args[1].GetString()) || !valid(args.args[2].GetString())) return;
                    section = std::string("Menu Provider ")+args.args[1].GetString()+" Section "+args.args[2].GetString();
                }
                args.movie->CreateObject(args.retVal);
                auto label = readOption(section.c_str(), "sLabel");
                ScaleformUtils::RegisterString(args.retVal, args.movie, "label", label.c_str());
                ScaleformUtils::RegisterNumber(args.retVal, "visible", Number(readOption(section.c_str(), "bVisible"), 0, 1));
                ScaleformUtils::RegisterNumber(args.retVal, "order", Number(readOption(section.c_str(), "iOrder"), -10000, 10000));
                auto controls = readOption(section.c_str(), "sControlIds");
                ScaleformUtils::RegisterString(args.retVal, args.movie, "controlIds", controls.c_str());
            }
        };
    }
    void Configure(ReadOption read)
    {
        readOption = read;
        auto height = [](const char* key) {
            if (!readOption) return 0.F;
            const auto value = Number(Option("VR Normal", key), -150, 150);
            return value == -9999 ? 0.F : static_cast<float>(value);
        };
        menuHeight = height("fHeightOffset");
        pickerHeight = height("fColorPickerHeightOffset");
        // Shared by Normal/Face and the picker, like the captured viewer anchor.
        // Upright by default. Only an explicit zero opts into viewer-facing pitch.
        forceVertical = !readOption || Number(Option("VR Normal", "bForceVertical"), 0, 1) != 0;
        playerName = readOption ? readOption("VR", "sPlayerName") : std::string{};
        if (!playerName.empty() && !VR::MenuOptionsPolicy::ValidPlayerName(playerName)) {
            SKSE::log::warn("Ignoring invalid VR sPlayerName: use 1..255 UTF-8 bytes, no control characters");
            playerName.clear();
        }
        overrideExistingPlayerName = readOption && Number(readOption("VR", "bOverrideExistingPlayerName"), 0, 1) == 1;
        const auto quillOption = [](const char* key) {
            const auto value = readOption ? Number(readOption("VR", key), 0, 1) : -9999;
            return value == -9999 ? -1 : static_cast<int>(value);
        };
        legacyQuill = quillOption("bUseQuill");
        steamVRQuill = quillOption("bUseQuillSteamVR");
        ocuQuill = quillOption("bUseQuillOCU");
    }
    float PlacementHeight(bool colorPicker) { return colorPicker ? pickerHeight : menuHeight; }
    bool PlacementForceVertical() { return forceVertical; }
    const std::string& PlayerName() { return playerName; }
    bool OverrideExistingPlayerName() { return overrideExistingPlayerName; }
    bool UseQuill()
    {
        const auto runtime = VR::ActiveRuntime();
        const auto enabled = VR::MenuOptionsPolicy::UseQuill(runtime, steamVRQuill, ocuQuill, legacyQuill);
        SKSE::log::info("RaceMenu VR quill: runtime={} enabled={}", VR::RuntimeName(runtime), enabled);
        return enabled;
    }
    void Register(RE::GFxMovie* movie, RE::GFxValue* root)
    {
        if (!readOption) return;
        RE::GFxValue profiles;
        movie->CreateObject(&profiles);
        for (const auto* name : profileNames) {
            RE::GFxValue profile;
            movie->CreateObject(&profile);
            constexpr const char* colorKeys[]{ "sBackgroundColor", "sTextColor", "sAccentColor", "sSelectionColor", "sBorderColor" };
            constexpr const char* members[]{ "backgroundColor", "textColor", "accentColor", "selectionColor", "borderColor" };
            for (unsigned i = 0; i < std::size(colorKeys); ++i)
                ScaleformUtils::RegisterNumber(&profile, members[i], MenuAppearance::ParseColor(Option(name, colorKeys[i])));
            for (const auto& field : fields) {
                auto value = Number(Option(name, field.key), field.minimum, field.maximum);
                if (std::string_view(field.member) == "consolidate" && value == -9999) value = std::string_view(name) == "Flat" ? 0 : 1;
                ScaleformUtils::RegisterNumber(&profile, field.member, value);
            }
            profiles.SetMember(name, profile);
        }
        root->SetMember("menuProfiles", profiles);
        static RE::GPtr<CategoryPresentation> handler{ new CategoryPresentation{} };
        RE::GFxValue function;
        movie->CreateFunction(&function, handler.get());
        root->SetMember("GetMenuCategoryPresentation", function);
    }
}
