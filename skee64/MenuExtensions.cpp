#include "pch.h"
#include "MenuExtensions.h"
#include "ScaleformUtils.h"
#include <map>
#include <mutex>
#include <cmath>

namespace SKEE::MenuExtensions
{
    namespace
    {
        bool Id(const char* text)
        {
            if (!text || !*text) return false;
            unsigned size = 0;
            for (; *text; ++text) {
                if (++size > 48 || !((*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z') || (*text >= '0' && *text <= '9') || *text == '.' || *text == '_' || *text == '-')) return false;
            }
            return true;
        }
        bool Label(const char* text) { return text && *text && std::strlen(text) <= 128; }
        std::string Key(const char* provider, const char* id) { return std::string(provider)+":"+id; }
        struct SectionData { std::string provider, id, label; skee_u32 flag; skee_i32 order; };
        struct SliderData { std::string provider, section, id, label; double minimum, maximum, step, value; IMenuExtensions::SliderCallback callback; void* context; std::uint64_t token; };
        class Service final : public IMenuExtensions
        {
        public:
            std::mutex mutex;
            std::map<std::string, SectionData> sections;
            std::map<std::string, SliderData> sliders;
            std::uint64_t revision{1}, token{1};
            skee_u32 GetVersion() override { return 1; }
            // Registry survives save loads; it belongs to loaded providers.
            void Revert() override { }
            bool RegisterSection(const Section& s) override
            {
                if (!Id(s.provider) || !Id(s.id) || !Label(s.label) || s.flag < (1u<<20) || s.flag > (1u<<30) || (s.flag & (s.flag-1))) return false;
                try {
                    std::scoped_lock lock(mutex);
                    const auto key = Key(s.provider,s.id);
                    for (const auto& [existing, entry] : sections) if (entry.flag == s.flag && existing != key) return false;
                    if (!sections.contains(key) && sections.size() >= 32) return false;
                    sections.insert_or_assign(key, SectionData{s.provider,s.id,s.label,s.flag,s.order}); ++revision; return true;
                } catch (...) { return false; }
            }
            bool RegisterSlider(const Slider& s) override
            {
                if (!Id(s.provider) || !Id(s.section) || !Id(s.id) || !Label(s.label) || !s.callback || !std::isfinite(s.minimum) || !std::isfinite(s.maximum) || !std::isfinite(s.step) || !std::isfinite(s.value) || s.minimum >= s.maximum || s.step <= 0 || s.step > s.maximum-s.minimum || s.value < s.minimum || s.value > s.maximum) return false;
                try {
                    std::scoped_lock lock(mutex);
                    if (!sections.contains(Key(s.provider,s.section))) return false;
                    auto key = Key(s.provider,s.id);
                    if (!sliders.contains(key) && sliders.size() >= 128) return false;
                    sliders.insert_or_assign(key, SliderData{s.provider,s.section,s.id,s.label,s.minimum,s.maximum,s.step,s.value,s.callback,s.context,++token}); ++revision; return true;
                } catch (...) { return false; }
            }
            bool SetValue(const char* provider, const char* id, double value) override
            {
                if (!Id(provider) || !Id(id) || !std::isfinite(value)) return false;
                try {
                    std::scoped_lock lock(mutex);
                    auto item = sliders.find(Key(provider,id));
                    if (item == sliders.end() || value < item->second.minimum || value > item->second.maximum) return false;
                    item->second.value = value; ++revision; return true;
                } catch (...) { return false; }
            }
            void UnregisterProvider(const char* provider) override
            {
                if (!Id(provider)) return;
                std::scoped_lock lock(mutex);
                std::erase_if(sliders,[&](const auto& item) { return item.second.provider == provider; });
                std::erase_if(sections,[&](const auto& item) { return item.second.provider == provider; }); ++revision;
            }
        } service;
        class Handler final : public RE::GFxFunctionHandler
        {
            void Call(Params& args) override
            {
                try {
                    const auto operation = reinterpret_cast<std::uintptr_t>(args.userData);
                    if (operation == 0 && args.retVal) {
                        std::scoped_lock lock(service.mutex);
                        args.retVal->SetNumber(static_cast<double>(service.revision));
                    } else if (operation == 1 && args.retVal) {
                        std::map<std::string, SectionData> sections;
                        std::map<std::string, SliderData> sliders;
                        { std::scoped_lock lock(service.mutex); sections = service.sections; sliders = service.sliders; }
                        args.movie->CreateArray(args.retVal);
                        for (const auto& [key, s] : sections) {
                            RE::GFxValue section, controls;
                            args.movie->CreateObject(&section); args.movie->CreateArray(&controls);
                            ScaleformUtils::RegisterString(&section,args.movie,"provider",s.provider.c_str());
                            ScaleformUtils::RegisterString(&section,args.movie,"id",s.id.c_str());
                            ScaleformUtils::RegisterString(&section,args.movie,"label",s.label.c_str());
                            ScaleformUtils::RegisterNumber(&section,"flag",s.flag); ScaleformUtils::RegisterNumber(&section,"order",s.order);
                            for (const auto& [sliderKey, c] : sliders) if (c.provider == s.provider && c.section == s.id) {
                                RE::GFxValue control; args.movie->CreateObject(&control);
                                ScaleformUtils::RegisterString(&control,args.movie,"id",c.id.c_str());
                                ScaleformUtils::RegisterString(&control,args.movie,"label",c.label.c_str());
                                ScaleformUtils::RegisterNumber(&control,"minimum",c.minimum); ScaleformUtils::RegisterNumber(&control,"maximum",c.maximum);
                                ScaleformUtils::RegisterNumber(&control,"step",c.step); ScaleformUtils::RegisterNumber(&control,"value",c.value);
                                controls.PushBack(control);
                            }
                            section.SetMember("controls",controls); args.retVal->PushBack(section);
                        }
                    } else if (operation == 2 && args.argCount == 3 && args.args[0].IsString() && args.args[1].IsString() && args.args[2].IsNumber()) {
                        const auto* provider = args.args[0].GetString(); const auto* id = args.args[1].GetString(); const auto value = args.args[2].GetNumber();
                        if (!Id(provider) || !Id(id) || !std::isfinite(value)) return;
                        std::uint64_t token; const auto key = Key(provider,id);
                        { std::scoped_lock lock(service.mutex); auto it = service.sliders.find(key);
                          if (it == service.sliders.end() || value < it->second.minimum || value > it->second.maximum) return;
                          token = it->second.token; }
                        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([key, token, value] {
                            IMenuExtensions::SliderCallback callback{}; void* context{};
                            { std::scoped_lock lock(service.mutex); auto it = service.sliders.find(key);
                              if (it == service.sliders.end() || it->second.token != token) return;
                              it->second.value = value; callback = it->second.callback; context = it->second.context; }
                            try { callback(value,context); } catch (...) { SKSE::log::warn("RaceMenu extension slider callback threw"); }
                        });
                    }
                } catch (...) { SKSE::log::warn("RaceMenu extension request failed"); }
            }
        };
    }
    IMenuExtensions* GetInterface() { return &service; }
    void Register(RE::GFxMovie* movie, RE::GFxValue* root)
    {
        static RE::GPtr<Handler> handler{new Handler{}};
        constexpr const char* names[]{"GetMenuExtensionsRevision", "GetMenuExtensions", "SetMenuExtensionValue"};
        for (std::uintptr_t i = 0; i < std::size(names); ++i) {
            RE::GFxValue function; movie->CreateFunction(&function,handler.get(),reinterpret_cast<void*>(i)); root->SetMember(names[i],function);
        }
    }
}
