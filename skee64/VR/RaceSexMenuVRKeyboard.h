#pragma once

namespace RE { class GFxMovieView; class GFxValue; }

namespace SKEE::VR
{
	// UI-thread, nonblocking keyboard session. No worker owns a GFx object.
	void RegisterRaceSexMenuKeyboard(RE::GFxMovieView* a_view, RE::GFxValue* a_root);
	void CancelRaceSexMenuKeyboard();
}
