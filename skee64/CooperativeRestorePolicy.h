#pragma once

#include <algorithm>
#include <cmath>

namespace SKEE::VR::CooperativeRestore
{
	inline bool Near(float a_left, float a_right, float a_absoluteTolerance = 1.0e-4F,
		float a_relativeTolerance = 1.0e-5F) noexcept
	{
		const auto difference = std::abs(a_left - a_right);
		return difference <= std::max(
			a_absoluteTolerance,
			a_relativeTolerance * std::max(std::abs(a_left), std::abs(a_right)));
	}

	template <class Left, class Right, class Equal>
	constexpr bool ShouldRestore(
		const Left& a_current,
		const Right& a_lastApplied,
		bool a_identityAndTopologyMatch,
		Equal&& a_equal) noexcept(noexcept(a_equal(a_current, a_lastApplied)))
	{
		return a_identityAndTopologyMatch && a_equal(a_current, a_lastApplied);
	}
}
