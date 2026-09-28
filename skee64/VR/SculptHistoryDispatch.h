#pragma once
#include <cstdint>
#include <utility>

namespace SKEE
{
    // VR's AS2 GameDelegate registers AddAction behind the external "call"
    // dispatcher. Keep the original direct callback route for non-VR movies.
    template<class Value, class Invoke>
    bool DispatchSculptHistory(bool vr, const Value& action, Invoke&& invoke)
    {
        if (vr) {
            const Value args[] = { Value("AddAction"), action };
            return std::forward<Invoke>(invoke)("call", args, std::uint32_t{2});
        }
        return std::forward<Invoke>(invoke)("AddAction", &action, std::uint32_t{1});
    }
}
