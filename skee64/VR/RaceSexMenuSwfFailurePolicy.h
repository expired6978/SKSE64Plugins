// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace SKEE::RaceSexMenuSwfPatch
{
    enum class FailureKind { IncompatibleMovie, Initialization };

    constexpr const char* FailureMessage(FailureKind kind)
    {
        if (kind == FailureKind::IncompatibleMovie) {
            return "RaceMenu VR 2: incompatible menu file.\n\n"
                   "Interface/VR/RaceSex_menu.swf does not match the required original. "
                   "Install RaceMenu SE 0.4.20.0 and disable other mods overriding this file "
                   "(including VR layout fixes or generated menus).\n\n"
                   "Restart Skyrim after correcting your mod load order. "
                   "Character creation was blocked for safety; original files were not changed.";
        }
        return "RaceMenu VR 2 could not load its character creation menu.\n\n"
               "Check that original RaceMenu SE 0.4.20.0 and the matching RaceMenu VR 2 "
               "add-on are installed. See RaceMenuNGVR2.log for the exact error.\n\n"
               "Restart Skyrim after fixing the installation. "
               "Character creation was blocked for safety; original files were not changed.";
    }
}
