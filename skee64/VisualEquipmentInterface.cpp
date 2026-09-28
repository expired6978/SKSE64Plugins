#include "VisualEquipmentInterface.h"

#include "ActorUpdateManager.h"

#include <RE/A/Actor.h>

#include <algorithm>
#include <utility>

extern ActorUpdateManager g_actorUpdateManager;

void VisualEquipmentInterface::Revert()
{
	// Provider registrations belong to plugin lifetime, not savegame lifetime.
}

bool VisualEquipmentInterface::RegisterProvider(const char* key, skee_i32 priority, IVisualEquipmentProvider* provider)
{
	if (!key || key[0] == '\0' || !provider) {
		return false;
	}

	std::lock_guard locker(m_lock);
	const auto duplicate = std::find_if(m_providers.begin(), m_providers.end(), [key](const ProviderEntry& entry) {
		return entry.key == key;
	});
	if (duplicate != m_providers.end()) {
		return false;
	}

	m_providers.push_back(ProviderEntry{ key, priority, m_nextOrder++, provider });
	std::stable_sort(m_providers.begin(), m_providers.end(), [](const ProviderEntry& lhs, const ProviderEntry& rhs) {
		if (lhs.priority != rhs.priority) {
			return lhs.priority > rhs.priority;
		}
		return lhs.order < rhs.order;
	});
	return true;
}

bool VisualEquipmentInterface::UnregisterProvider(const char* key, IVisualEquipmentProvider* provider)
{
	if (!key || !provider) {
		return false;
	}

	std::lock_guard locker(m_lock);
	const auto it = std::find_if(m_providers.begin(), m_providers.end(), [key, provider](const ProviderEntry& entry) {
		return entry.key == key && entry.provider == provider;
	});
	if (it == m_providers.end()) {
		return false;
	}

	m_providers.erase(it);
	return true;
}

std::vector<VisualEquipmentInterface::ProviderEntry> VisualEquipmentInterface::SnapshotProviders()
{
	std::lock_guard locker(m_lock);
	return m_providers;
}

IVisualEquipmentProvider::ResolveResult VisualEquipmentInterface::ResolveSlotMask(
	RE::Actor* actor,
	RE::TESObjectARMO* sourceArmor,
	RE::TESObjectARMA* sourceAddon,
	skee_u32 sourceMask,
	skee_u32* effectiveMask)
{
	if (!actor || !sourceArmor || !effectiveMask) {
		return IVisualEquipmentProvider::kUnhandled;
	}

	for (const auto& entry : SnapshotProviders()) {
		if (!entry.provider) {
			continue;
		}

		skee_u32 resolvedMask = sourceMask;
		const auto result = entry.provider->ResolveSlotMask(actor, sourceArmor, sourceAddon, sourceMask, &resolvedMask);
		if (result == IVisualEquipmentProvider::kHandled) {
			*effectiveMask = resolvedMask;
			return result;
		}
	}

	return IVisualEquipmentProvider::kUnhandled;
}

IVisualEquipmentProvider::ResolveResult VisualEquipmentInterface::VisitArmorAddons(
	RE::Actor* actor,
	RE::TESObjectARMO* sourceArmor,
	RE::TESObjectARMA* sourceAddon,
	IVisualEquipmentProvider::ArmorAddonVisitor* visitor)
{
	if (!actor || !sourceArmor || !sourceAddon || !visitor) {
		return IVisualEquipmentProvider::kUnhandled;
	}

	for (const auto& entry : SnapshotProviders()) {
		if (!entry.provider) {
			continue;
		}

		class CollectingVisitor final : public IVisualEquipmentProvider::ArmorAddonVisitor
		{
		public:
			bool Visit(RE::TESObjectARMO* armor, RE::TESObjectARMA* addon) override
			{
				if (armor && addon) {
					pairs.emplace_back(armor, addon);
				}
				return true;
			}

			std::vector<std::pair<RE::TESObjectARMO*, RE::TESObjectARMA*>> pairs;
		} collected;

		const auto result = entry.provider->VisitArmorAddons(actor, sourceArmor, sourceAddon, &collected);
		if (result == IVisualEquipmentProvider::kHandled) {
			for (const auto& [armor, addon] : collected.pairs) {
				if (!visitor->Visit(armor, addon)) {
					break;
				}
			}
			return result;
		}
	}

	return IVisualEquipmentProvider::kUnhandled;
}

bool VisualEquipmentInterface::NotifyChanged(RE::Actor* actor)
{
	if (!actor) {
		return false;
	}

	const auto formID = actor->formID;
	g_actorUpdateManager.AddBodyUpdate(formID);
	g_actorUpdateManager.AddTransformUpdate(formID);
	g_actorUpdateManager.AddOverlayUpdate(formID);
	g_actorUpdateManager.AddNodeOverrideUpdate(formID);
	g_actorUpdateManager.AddAddonOverrideUpdate(formID);
	g_actorUpdateManager.AddSkinOverrideUpdate(formID);
	g_actorUpdateManager.Flush();
	return true;
}
