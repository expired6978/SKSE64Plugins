#pragma once

namespace RE { class GFxMovieView; class GFxValue; }

namespace SKEE::VR
{
	// Applies Skyrim VR's projected UI-quad scale and yaw only for the lifetime
	// of RaceSex Menu. The original shared settings are restored on close.
	void SetRaceSexMenuWorldTransform(float a_scale, float a_yawDegrees, bool a_enabled);
	// Applies the RaceSex-local yaw to the live projected UI scene node after
	// Skyrim has constructed it. Returns false while the VR node is unavailable.
	bool ApplyRaceSexMenuWorldYaw();
	void RestoreRaceSexMenuWorldTransform();

	// Selects RaceSex Menu's VR-coordinate movie before construction completes,
	// then hooks its own input handler so Trigger/Accept become Scaleform mouse
	// edges while that menu is the active input recipient.
	// Safe to call more than once; subsequent calls are no-ops.
	bool RegisterRaceSexMenuPointerInput();

	// Default-off, bounded in-process observation. Exposes Begin/Record/Read/End
	// through the existing CharGen Scaleform surface; never attaches a debugger.
	void RegisterRaceSexMenuInputTrace(RE::GFxMovieView* a_view, RE::GFxValue* a_root);
}
