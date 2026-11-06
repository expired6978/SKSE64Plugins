// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MenuConfiguration.h"
namespace RE { class GFxMovie; class GFxValue; }
namespace SKEE::AvatarLighting
{
    void Configure(MenuConfiguration::ReadOption read);
    void Register(RE::GFxMovie* movie, RE::GFxValue* root);
    void Reset();
}
