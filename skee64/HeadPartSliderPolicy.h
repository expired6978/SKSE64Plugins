// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace SKEE::HeadPartSlider
{
    // Custom head-part sliders use -1 for no part and zero-based list indices
    // for actual parts. Keep the sentinel distinct from part zero.
    inline constexpr float kNoPart = -1.0f;

    [[nodiscard]] constexpr float ValueForPartIndex(std::int32_t index) noexcept
    {
        return index < 0 ? kNoPart : static_cast<float>(index);
    }

    // Scaleform supplies a floating-point value. Validate it before any
    // conversion to an unsigned list index (including the -1 sentinel).
    [[nodiscard]] inline std::optional<std::uint32_t> PartIndexForValue(double value, std::size_t count) noexcept
    {
        if (!std::isfinite(value) || value < 0.0 || std::trunc(value) != value ||
            value >= static_cast<double>(count) ||
            value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(value);
    }
}
