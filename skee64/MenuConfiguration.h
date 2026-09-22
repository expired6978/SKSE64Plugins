#pragma once
#include <string>
namespace RE { class GFxMovie; class GFxValue; }
namespace SKEE::MenuConfiguration
{
    using ReadOption = std::string (*)(const char*, const char*);
    void Configure(ReadOption read);
    float PlacementHeight(bool colorPicker);
    bool PlacementForceVertical();
    const std::string& PlayerName();
    bool OverrideExistingPlayerName();
    bool UseQuill();
    void Register(RE::GFxMovie* movie, RE::GFxValue* root);
}
