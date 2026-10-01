// SPDX-License-Identifier: GPL-3.0-or-later
#include "pch.h"
#include "AvatarLighting.h"
#include "AvatarLightingPolicy.h"
#include "RaceSexCameraPolicy.h"
#include "RE/N/NiPointLight.h"
#include "RE/S/ShadowSceneNode.h"
#include <atomic>

namespace SKEE::AvatarLighting
{
    namespace
    {
        bool enabled{true};
        auto sources = AvatarLightingPolicy::defaults;
        std::atomic<std::uint64_t> generation{0};
        AvatarLightingPolicy::Requests requests;
        std::atomic<unsigned> state{0}; // 0 off, 1 on, 2 unavailable
        RE::NiPointer<RE::NiNode> avatar;
        RE::NiPointer<RE::ShadowSceneNode> scene;
        std::array<RE::NiPointer<RE::NiPointLight>, 3> lights;
#if defined(ENABLE_SKYRIM_VR)
        void SetRadius(RE::NiPointLight* light, std::uint32_t radius)
        {
            const auto extent = static_cast<float>(radius);
            light->GetLightRuntimeData().radius = {extent, extent, extent};
            // SkyrimVR 1.4.15, ID 17224 / RVA 0x2319D0 reads EDX, not XMM1.
            // CommonLib's float wrapper uses the wrong ABI. This function only
            // sets attenuation; the three spatial bounds above are independent.
            using Function = void (*)(RE::NiPointLight*, std::uint32_t);
            static REL::Relocation<Function> attenuation{RELOCATION_ID(17224, 17626)};
            attenuation(light, radius);
        }
#endif
        void Remove()
        {
            // Only our three lights; never touch the player's torch or scene lights.
            for (auto& light : lights) if (light) {
                if (scene) scene->RemoveLight(light.get());
                if (light->parent) light->parent->DetachChild(light.get());
                light.reset();
            }
            avatar.reset(); scene.reset(); state.store(0);
        }
        bool Live(RE::GFxMovie* movie, std::uint64_t epoch = generation.load())
        {
            auto* ui = RE::UI::GetSingleton();
            auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
            return ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) && menu &&
                AvatarLightingPolicy::SessionMatches(epoch, generation.load(), movie, menu->uiMovie.get());
        }
        bool Apply()
        {
#if defined(ENABLE_SKYRIM_VR)
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* root = player ? player->Get3D(false) : nullptr;
            auto* node = root ? root->AsNode() : nullptr;
            auto* head = root ? root->GetObjectByName("NPC Head [Head]") : nullptr;
            // Use the actual ancestor scene, not an unchecked global scene index.
            RE::ShadowSceneNode* shadow{};
            for (auto* p = root; p; p = p->parent)
                if ((shadow = netimmerse_cast<RE::ShadowSceneNode*>(p))) break;
            if (!node || !head || !shadow || !CameraPolicy::Valid(root->world) ||
                root->world.scale < 0.1F || root->world.scale > 10.F) { Remove(); return false; }
            const auto headLocal = CameraPolicy::LocalPoint(root->world, head->world.translate);
            if (!std::isfinite(headLocal.z) || headLocal.z < 20 || headLocal.z > 400) { Remove(); return false; }
            if (avatar.get() != node || scene.get() != shadow) Remove();
            avatar.reset(node); scene.reset(shadow);
            for (std::size_t i = 0; i < lights.size(); ++i) {
                if (sources[i].brightness == 0) continue;
                const bool creating = !lights[i];
                if (creating) lights[i].reset(RE::NiPointLight::Create());
                if (!lights[i]) { Remove(); return false; }
                auto* light = lights[i].get();
                const auto position = AvatarLightingPolicy::Position(sources[i], headLocal.z);
                light->local.translate = {position[0], position[1], position[2]};
                light->local.rotate = RE::NiMatrix3{};
                light->local.scale = 1;
                auto& data = light->GetLightRuntimeData();
                data.ambient = {0, 0, 0};
                data.diffuse = {sources[i].brightness, sources[i].brightness, sources[i].brightness};
                data.fade = 1;
                SetRadius(light, AvatarLightingPolicy::WorldRadius(sources[i], root->world.scale));
                if (creating) node->AttachChild(light);
                RE::NiUpdateData update{0, RE::NiUpdateData::Flag::kDirty};
                light->Update(update);
                if (creating) {
                    RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
                    params.dynamic = true; params.neverFades = true;
                    params.fov = 1.5707963F; params.falloff = 1;
                    params.sceneGraphIndex = shadow->GetRuntimeData().sceneGraphIndex;
                    params.restrictedNode = node;
                    // No shadows, terrain/water contribution or lens flare.
                    if (!shadow->AddLight(light, params)) { Remove(); return false; }
                }
            }
            return true;
#else
            return false;
#endif
        }
        bool Request(RE::GFxMovie* movie, bool on, bool refresh)
        {
            if (!enabled || !REL::Module::IsVR() || !Live(movie)) return false;
            if (refresh && state.load() != 1) return true;
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) return false;
            if (!refresh) requests.SetDesired(on);
            if (!requests.TryQueue()) return true;
            const auto epoch = generation.load();
            try {
                tasks->AddTask([movie, epoch] {
                    if (epoch != generation.load()) return; // Reset cleared this epoch's queue flag.
                    requests.Complete();
                    if (!Live(movie, epoch)) return;
                    if (!requests.Desired()) Remove();
                    else if (Apply()) state.store(1);
                    else { state.store(2); SKSE::log::warn("RaceMenu front lighting unavailable: avatar/head/scene not resolved"); }
                });
            } catch (...) { requests.Complete(); return false; }
            return true;
        }
        class Handler final : public RE::GFxFunctionHandler
        {
            void Call(Params& args) override
            {
                if (!Live(args.movie)) return;
                const auto operation = reinterpret_cast<std::uintptr_t>(args.userData);
                if (operation == 0 && args.argCount == 1 && args.args[0].IsBool()) {
                    const bool accepted = Request(args.movie, args.args[0].GetBool(), false);
                    if (args.retVal) args.retVal->SetBoolean(accepted);
                } else if (operation == 1) Request(args.movie, true, true);
                else if (operation == 2 && args.retVal) args.retVal->SetNumber(state.load());
            }
        };
    }
    void Configure(MenuConfiguration::ReadOption read)
    {
        const auto value = [&](const char* key, float fallback, float low, float high) {
            return AvatarLightingPolicy::Number(read ? read("VR Lighting", key) : "", fallback, low, high);
        };
        enabled = value("bEnableFrontRig", 1, 0, 1) == 1;
        sources = AvatarLightingPolicy::defaults;
        constexpr const char* names[]{"Key", "Fill", "Body"};
        for (unsigned i = 0; i < sources.size(); ++i) {
            auto& s = sources[i];
            const auto number = [&](const char* suffix, float fallback, float low, float high) {
                return value((std::string("f") + names[i] + suffix).c_str(), fallback, low, high);
            };
            s.right = number("Right", s.right, -300, 300);
            s.front = number("Front", s.front, 20, 400);
            s.height = number("Height", s.height, -200, 400);
            s.radius = number("Radius", s.radius, 50, 800);
            s.brightness = number("Brightness", s.brightness, 0, 3);
        }
    }
    void Reset() { generation.fetch_add(1); requests.Reset(); Remove(); }
    void Register(RE::GFxMovie* movie, RE::GFxValue* root)
    {
        if (!REL::Module::IsVR()) return;
        root->SetMember("avatarLightingSupported", RE::GFxValue{enabled});
        if (!enabled) return;
        static RE::GPtr<Handler> handler{new Handler{}};
        constexpr const char* names[]{"SetAvatarLighting", "UpdateAvatarLighting", "GetAvatarLightingState"};
        for (std::uintptr_t i = 0; i < std::size(names); ++i) {
            RE::GFxValue function;
            movie->CreateFunction(&function, handler.get(), reinterpret_cast<void*>(i));
            root->SetMember(names[i], function);
        }
    }
}
