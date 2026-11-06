// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace RE { class BSScaleformManager; }
namespace SKEE::RaceSexMenuSwfPatch
{
    // Resolve the winning original resource, verify/reconstruct it once, then
    // install the qualified file adapter. Unknown input never reaches Scaleform.
    bool Prepare(RE::BSScaleformManager* manager);
    bool HasServedMovie();
}
