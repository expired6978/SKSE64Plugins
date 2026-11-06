#pragma once
#include "IPluginInterface.h"
namespace RE { class GFxMovie; class GFxValue; }
// Separately named interface: existing RaceMenu interface layouts are untouched.
class IMenuExtensions : public IPluginInterface
{
public:
    struct Section { const char* provider; const char* id; const char* label; skee_u32 flag; skee_i32 order; };
    using SliderCallback = void (*)(double value, void* context);
    struct Slider { const char* provider; const char* section; const char* id; const char* label; double minimum, maximum, step, value; SliderCallback callback; void* context; };
    // All strings are copied. IDs are ASCII [A-Za-z0-9_.-], up to 48 bytes.
    // Sections use a caller-reserved single category bit >= 1<<20. Conflicts
    // with legacy mod categories are rejected by the live movie, never merged.
    virtual bool RegisterSection(const Section& section) = 0;
    virtual bool RegisterSlider(const Slider& slider) = 0;
    virtual bool SetValue(const char* provider, const char* id, double value) = 0;
    virtual void UnregisterProvider(const char* provider) = 0;
    // Callbacks run on game tasks. Unregister cancels undispatched callbacks;
    // consumers must also drain any running callback before unloading code.
};
namespace SKEE::MenuExtensions
{
    IMenuExtensions* GetInterface();
    void Register(RE::GFxMovie* movie, RE::GFxValue* root);
}
