#pragma once

#include <cstdint>

namespace HeadExportPolicy
{
    // Every matrix/bone entry must have a corresponding skin-data entry. The
    // zero-bone case may omit skin data and arrays, but cannot hide extra bones.
    constexpr bool CoherentSkinLayout(std::uint32_t matrices, std::uint32_t bones,
        bool hasSkinData, bool hasBoneArray, bool hasWorldArray) noexcept
    {
        return matrices == bones && (hasSkinData || matrices == 0) &&
            (matrices == 0 || (hasBoneArray && hasWorldArray));
    }
}
