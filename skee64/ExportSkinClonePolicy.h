#pragma once

namespace SKEE
{
    // A game clone is allowed to return its source. Export must reject that
    // result before assigning any fields on an object owned by the live actor.
    template <class Skin, class Request>
    auto RequestIndependentExportSkin(Skin* source, Request&& request)
    {
        auto result = request(source);
        if (result.get() == source)
            result.reset();
        return result;
    }
}
