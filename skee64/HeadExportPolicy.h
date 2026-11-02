#pragma once

#include <cstdint>
#include <utility>

namespace HeadExportPolicy
{
    // Prepare the skin before changing either coupled geometry field. An
    // unskinned source is not a replacement: preserve the complete old pair.
    // This is a failure boundary on the caller's scene thread, not thread
    // synchronization. Concrete descriptor/NiPointer assignments do not throw.
    template <class Descriptor, class Skin, class Clone>
    bool TryRelinkSkin(Descriptor& targetDescriptor, Skin& targetSkin,
        const Descriptor& sourceDescriptor, const Skin& sourceSkin, Clone&& clone)
    {
        if (!sourceSkin)
            return false;
        const auto descriptor = sourceDescriptor;
        auto replacement = clone(sourceSkin.get());
        if (!replacement)
            return false;
        targetDescriptor = descriptor;
        targetSkin = std::move(replacement);
        return true;
    }

    // Every matrix/bone entry must have a corresponding skin-data entry. The
    // zero-bone case may omit skin data and arrays, but cannot hide extra bones.
    constexpr bool CoherentSkinLayout(std::uint32_t matrices, std::uint32_t bones,
        bool hasSkinData, bool hasBoneArray, bool hasWorldArray) noexcept
    {
        return matrices == bones && (hasSkinData || matrices == 0) &&
            (matrices == 0 || (hasBoneArray && hasWorldArray));
    }
}
