// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace SKEE::SwfBytePatch
{
    using Bytes = std::vector<std::uint8_t>;
    using Hash = std::array<std::uint8_t, 32>;
    inline constexpr std::size_t kLimit = 8 * 1024 * 1024;
    // Keep input incompatibility distinct from a damaged/mismatched recipe or
    // loader failure; player-facing diagnostics must not blame the wrong mod.
    class IncompatibleSource : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
    Hash Sha256(std::span<const std::uint8_t> bytes);
    Bytes Canonical(std::span<const std::uint8_t> file);
    // Returns only a complete, hash-verified FWS movie. Throws on any mismatch.
    Bytes Apply(std::span<const std::uint8_t> sourceFile, std::span<const std::uint8_t> patch);
    // Production pins the WHOLE patch, not merely the hashes supplied inside it.
    Bytes ApplyRelease(std::span<const std::uint8_t> sourceFile, std::span<const std::uint8_t> patch);
}
