// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string>
namespace SKEE::AvatarLightingPolicy
{
    // Refreshes never replace explicit button intent. A worker reads the latest
    // desired state, so a click during a queued refresh is accepted, not lost.
    class Requests
    {
    public:
        void SetDesired(bool on) { desired.store(on); }
        bool Desired() const { return desired.load(); }
        bool TryQueue() { return !queued.exchange(true); }
        void Complete() { queued.store(false); }
        void Reset() { desired.store(false); queued.store(false); }
    private:
        std::atomic<bool> desired{false}, queued{false};
    };
    struct Source { float right, front, height, radius, brightness; bool headRelative; };
    inline constexpr std::array<Source, 3> defaults{{
        {-35, 85, 15, 220, 1.0F, true},
        {35, 80, -35, 190, 0.45F, true},
        {0, 125, 90, 300, 0.8F, false}
    }};
    inline float Number(const std::string& text, float fallback, float low, float high)
    {
        if (text.empty()) return fallback;
        char* end{};
        const float n = std::strtof(text.c_str(), &end);
        while (end && (*end == ' ' || *end == '\t')) ++end;
        return end && end != text.c_str() && !*end && std::isfinite(n) && n >= low && n <= high ? n : fallback;
    }
    // Offsets are in the avatar's frame. No HMD/menu transform participates.
    inline std::array<float, 3> Position(const Source& s, float headHeight)
    { return {s.right, s.front, (s.headRelative ? headHeight : 0.F) + s.height}; }
    // Configuration (50..800) and validated avatar scale (0.1..10) bound this
    // conversion to 5..8000. The VR attenuation routine takes an INTEGER radius.
    inline std::uint32_t WorldRadius(const Source& s, float scale)
    { return static_cast<std::uint32_t>(std::round(s.radius * scale)); }
    inline bool SessionMatches(std::uint64_t queued, std::uint64_t current, const void* expected, const void* live)
    { return queued == current && expected && expected == live; }
}
