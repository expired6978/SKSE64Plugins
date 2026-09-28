#pragma once

#include "IPluginInterface.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class VisualEquipmentInterface final : public IVisualEquipmentInterface
{
public:
	skee_u32 GetVersion() override { return kCurrentPluginVersion; }
	void Revert() override;

	bool RegisterProvider(const char* key, skee_i32 priority, IVisualEquipmentProvider* provider) override;
	bool UnregisterProvider(const char* key, IVisualEquipmentProvider* provider) override;

	IVisualEquipmentProvider::ResolveResult ResolveSlotMask(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		skee_u32 sourceMask,
		skee_u32* effectiveMask) override;
	IVisualEquipmentProvider::ResolveResult VisitArmorAddons(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		IVisualEquipmentProvider::ArmorAddonVisitor* visitor) override;

	bool NotifyChanged(RE::Actor* actor) override;

private:
	struct ProviderEntry
	{
		std::string key;
		skee_i32 priority{ 0 };
		std::uint64_t order{ 0 };
		IVisualEquipmentProvider* provider{ nullptr };
	};

	std::vector<ProviderEntry> SnapshotProviders();

	std::mutex m_lock;
	std::vector<ProviderEntry> m_providers;
	std::uint64_t m_nextOrder{ 0 };
};
