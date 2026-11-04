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

    // The engine's mutation/rebuild calls return void. Commit the reported
    // value only after authoritative part read-back agrees. Do not rebuild an
    // actor from a base mutation that did not take effect. This verifies the
    // NPC part, not completion of the engine's visual mesh rebuild.
    template <class Part, class ReadPart, class ChangePart, class RebuildPart>
    [[nodiscard]] bool ApplySelection(float& reportedValue, float requestedValue, Part* targetPart,
        ReadPart readPart, ChangePart changePart, RebuildPart rebuildPart)
    {
        auto* oldPart = readPart();
        if (oldPart != targetPart) {
            changePart(targetPart);
            if (readPart() != targetPart) {
                return false;
            }
            rebuildPart(oldPart, targetPart);
        }
        if (readPart() != targetPart) {
            return false;
        }
        reportedValue = requestedValue;
        return true;
    }
}
