#pragma once

#include <cstdint>

namespace SKEE::VR::SessionLease
{
	template <class State>
	constexpr bool IsCurrent(
		std::uint64_t a_capturedGeneration,
		std::uint64_t a_currentGeneration,
		State a_currentState,
		State a_requiredState) noexcept
	{
		return a_capturedGeneration == a_currentGeneration && a_currentState == a_requiredState;
	}
}
