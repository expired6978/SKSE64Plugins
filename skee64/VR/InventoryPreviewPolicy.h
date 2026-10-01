// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <memory>

namespace SKEE::InventoryPreview
{
    // The engine keeps at most seven previews (0x8B5B40). Validate the header
    // before indexing, so a stale/malformed count cannot become an unbounded
    // walk. This does not attempt to validate arbitrary foreign pointers.
    template <class Models, class Form>
    auto FindLoadedModel(Models& models, const Form* form) noexcept
        -> decltype(std::addressof(models[0]))
    {
        const auto count = models.size();
        if (!form || count == 0 || count > 7 || count > models.capacity() || !models.data()) {
            return nullptr;
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (models[i].itemBase == form) {
                return std::addressof(models[i]);
            }
        }
        return nullptr;
    }
}
