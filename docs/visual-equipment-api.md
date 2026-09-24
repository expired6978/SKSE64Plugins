# Visual equipment provider API

RaceMenu exposes the versioned `IVisualEquipmentInterface` as
`"VisualEquipment"` through its existing `IInterfaceMap` exchange. The API is
runtime-neutral: the same contract applies to SE, AE and VR builds.

It is intended for mods that change what an actor renders without replacing the
actor's equipped inventory entry, including armor variants, transmog, outfit
visualizers and selectively hidden equipment. Such a mod can describe the
effective visual state to RaceMenu instead of detouring private functions in a
particular `skee64.dll` build.

## Contract

A provider answers two related questions:

1. What biped slots does this source armor or armor addon effectively cover?
2. Which effective `(ARMO, ARMA)` pair or pairs should RaceMenu traverse for
   this source pair?

Providers are queried in descending priority order. Registration order breaks a
priority tie. The first provider that returns `kHandled` owns the query.

- Return `kUnhandled` when ordinary RaceMenu behavior should remain in effect.
- A handled zero slot mask means the source is visually hidden.
- A handled addon query with no visits means the source pair is visually hidden.
- For an ordinary fallback after finding no replacement, return `kUnhandled`.
- An armor-level slot query has `sourceAddon == nullptr`; an addon-level query
  supplies both source forms.
- The armor returned by RaceMenu's worn-owner selection remains the original
  equipped `ARMO`. This avoids accidental second-stage variant chaining. The
  effective pair is used only when RaceMenu traverses rendered addons.

RaceMenu consumes these answers in worn-owner selection, worn-item enumeration,
armor-addon traversal and the addon slot gate used by skin-property overrides.
That keeps RaceMenu's callback invocation and inventory semantics inside
RaceMenu while allowing a provider to describe the render result.

## Registration outline

Use the normal SKEE interface-exchange message, query `"VisualEquipment"`,
verify `GetVersion() >= IVisualEquipmentInterface::kPluginVersion1`, and keep
the provider object alive until it has been unregistered.

```cpp
class MyVisualProvider final : public IVisualEquipmentProvider
{
public:
    ResolveResult ResolveSlotMask(
        RE::Actor* actor,
        RE::TESObjectARMO* sourceArmor,
        RE::TESObjectARMA* sourceAddon,
        skee_u32 sourceMask,
        skee_u32* effectiveMask) override;

    ResolveResult VisitArmorAddons(
        RE::Actor* actor,
        RE::TESObjectARMO* sourceArmor,
        RE::TESObjectARMA* sourceAddon,
        ArmorAddonVisitor* visitor) override;
};

auto* base = exchange.interfaceMap->QueryInterface("VisualEquipment");
auto* visual = static_cast<IVisualEquipmentInterface*>(base);
if (visual && visual->GetVersion() >= IVisualEquipmentInterface::kPluginVersion1) {
    visual->RegisterProvider("MyPlugin", 0, &provider);
}
```

`VisitArmorAddons` may call `visitor->Visit(effectiveArmor, effectiveAddon)`
more than once for one source pair. Stop enumeration if it returns `false`.
Do not pass null forms. RaceMenu de-duplicates effective pairs before traversing
live geometry.

Only POD values, game pointers and virtual visitor calls cross the DLL boundary;
the interface deliberately does not expose STL containers or `std::function`.
Implementations must not allow exceptions to cross that boundary.

## Changes and lifetime

Provider registration lasts for the provider plugin's lifetime and is not tied
to a savegame. A provider that unregisters must ensure no query is in flight
before destroying its object.

After changing an actor's visual mapping, call `NotifyChanged(actor)` on the
game thread. RaceMenu then queues its body, transform, overlay, node/addon and
skin-override refreshes and flushes them together.

The API does not change the actor's inventory, equip state, gameplay slot
semantics or the provider's own rendering. It only gives RaceMenu a stable,
semantic view of the already-selected visual equipment.
