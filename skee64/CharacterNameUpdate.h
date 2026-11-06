#pragma once

#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESNPC.h>
#include <RE/U/UI.h>
#include <RE/R/RaceSexMenu.h>

#include <cstring>

namespace SKEE
{
	// Called on the game/UI thread by both public rename routes. RaceSexMenu's
	// ChangeName belongs to the final naming/completion path, not in-menu edits.
	inline bool UpdateCharacterNameWithoutFinishing(const char* a_name)
	{
		if (!a_name || !*a_name || std::strlen(a_name) > 255) return false;
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) return false;
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* base = player ? player->GetActorBase() : nullptr;
		if (!base) return false;
		base->SetFullName(a_name);
		base->AddChange(RE::TESNPC::ChangeFlags::kFullName);
		return true;
	}
}
