#pragma once

// Race-menu types now come straight from CommonLibSSE-NG (v7.0.0+):
//   RE::RaceMenuSlider, RE::RaceComponent, RE::RaceMenuSliderArray  (RE/R/RaceMenuSlider.h)
//   RE::RaceSexMenu runtime data (headParts / sliderData / ...)     (RE/R/RaceSexMenu.h)
// The project-owned mirrors that used to live here (skee::RaceMenuSlider,
// skee::RaceComponent, skee::RaceSexMenuData + GetRaceSexMenuData) were removed
// once CommonLib typed those regions. Access RaceSexMenu data through the helpers
// below: Skyrim VR omits the flat RaceSexCamera member and therefore needs the
// distinct VR_RUNTIME_DATA layout.
//
// The only thing kept in this header is a small factory for building an
// RE::RaceMenuSlider (CommonLib's type is a plain POD with no convenience
// constructor) and the head-part-list count constant.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include <RE/R/RaceMenuSlider.h>
#include <RE/R/RaceSexMenu.h>

namespace skee
{
	// Number of per-race head-part lists in both RaceSexMenu runtime layouts.
	constexpr std::size_t kNumHeadPartLists = 7;

	// RaceSexMenu has a distinct VR layout: Skyrim VR removes the flat
	// RaceSexCamera block between headParts and sliderData.  Keep all callers on
	// one runtime-aware path so a flat accessor can never silently interpret the
	// VR sex/race fields as a BSTArray again.
	inline RE::RaceMenuSlider* GetActiveRaceMenuSlider(
		RE::RaceSexMenu* a_menu, std::uint8_t a_gender, std::uint32_t a_sliderID) noexcept
	{
		if (!a_menu || a_gender >= 2) {
			return nullptr;
		}

		if (REL::Module::IsVR()) {
			auto& data = a_menu->GetVRRuntimeData();
			const auto gender = static_cast<std::uint8_t>(data.sex.underlying());
			if (gender >= 2 || data.unk188 >= data.sliderData[gender].size()) {
				return nullptr;
			}
			auto& sliders = data.sliderData[gender][data.unk188].sliders;
			return a_sliderID < sliders.size() ? std::addressof(sliders[a_sliderID]) : nullptr;
		}

		auto& data = a_menu->GetRuntimeData();
		if (data.unk188 >= data.sliderData[a_gender].size()) {
			return nullptr;
		}
		auto& sliders = data.sliderData[a_gender][data.unk188].sliders;
		return a_sliderID < sliders.size() ? std::addressof(sliders[a_sliderID]) : nullptr;
	}

	inline RE::BSTArray<RE::BGSHeadPart*>* GetRaceMenuHeadPartList(
		RE::RaceSexMenu* a_menu, std::size_t a_index) noexcept
	{
		if (!a_menu || a_index >= kNumHeadPartLists) {
			return nullptr;
		}
		if (REL::Module::IsVR()) {
			return std::addressof(a_menu->GetVRRuntimeData().headParts[a_index]);
		}
		return std::addressof(a_menu->GetRuntimeData().headParts[a_index]);
	}

	// Build an RE::RaceMenuSlider with the same field layout/initialization the old
	// project-owned mirror constructor used to provide (name is stored as a raw pointer;
	// callback is copied into the fixed buffer and null-terminated). currentValue is set
	// to the "unset" sentinel (bit pattern 0x7F7FFFFF == FLT_MAX).
	inline RE::RaceMenuSlider MakeRaceMenuSlider(
		std::uint32_t a_filterFlag, const char* a_sliderName, const char* a_callbackName,
		std::uint32_t a_sliderId, std::uint32_t a_index, std::uint32_t a_type, std::uint8_t a_unk8,
		float a_min, float a_max, float a_value, float a_interval) noexcept
	{
		RE::RaceMenuSlider slider{};
		slider.min = a_min;
		slider.max = a_max;
		slider.value = a_value;
		slider.interval = a_interval;
		slider.filterFlag = a_filterFlag;
		slider.type = a_type;
		slider.name = a_sliderName;
		std::uint32_t written = 0;
		for (const char* c = a_callbackName; c && *c && written < sizeof(slider.callback) - 1; ++written) {
			slider.callback[written] = *c++;
		}
		slider.callback[written] = '\0';
		slider.index = a_index;
		slider.id = a_sliderId;
		slider.unk12C = 0;
		slider.currentValue = std::numeric_limits<float>::max();  // "unset" sentinel (was unk130 = 0x7F7FFFFF)
		slider.unk134 = a_unk8;
		return slider;
	}
}
