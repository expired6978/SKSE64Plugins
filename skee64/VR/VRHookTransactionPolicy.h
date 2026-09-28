#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace SKEE::VR::HookTransaction
{
	constexpr std::size_t kRelativeCallSize = 5;
	using RelativeCall = std::array<std::uint8_t, kRelativeCallSize>;

	enum class Result
	{
		kInstalled,
		kFirstSiteRejected,
		kSecondSiteRejectedRolledBack,
		kSecondSiteRejectedRollbackFailed
	};

	inline bool EncodeRelativeCall(
		std::uintptr_t a_source,
		std::uintptr_t a_target,
		RelativeCall& a_result) noexcept
	{
		const auto next = a_source + kRelativeCallSize;
		std::int64_t displacement{};
		if (a_target >= next) {
			const auto distance = a_target - next;
			if (distance > static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max())) {
				return false;
			}
			displacement = static_cast<std::int64_t>(distance);
		} else {
			const auto distance = next - a_target;
			constexpr auto kMaximumNegativeDistance =
				static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 1;
			if (distance > kMaximumNegativeDistance) {
				return false;
			}
			displacement = -static_cast<std::int64_t>(distance);
		}

		const auto relative = static_cast<std::int32_t>(displacement);
		a_result[0] = 0xE8;
		std::memcpy(a_result.data() + 1, std::addressof(relative), sizeof(relative));
		return true;
	}

	inline bool DecodeRelativeCall(
		std::uintptr_t a_source,
		const RelativeCall& a_call,
		std::uintptr_t& a_target) noexcept
	{
		if (a_call[0] != 0xE8) {
			return false;
		}
		std::int32_t displacement{};
		std::memcpy(std::addressof(displacement), a_call.data() + 1, sizeof(displacement));
		a_target = static_cast<std::uintptr_t>(
			static_cast<std::int64_t>(a_source + kRelativeCallSize) + displacement);
		return true;
	}

	template <class InstallFirst, class InstallSecond, class RollbackFirst>
	Result Commit(InstallFirst&& a_installFirst, InstallSecond&& a_installSecond, RollbackFirst&& a_rollbackFirst)
	{
		if (!a_installFirst()) {
			return Result::kFirstSiteRejected;
		}
		if (a_installSecond()) {
			return Result::kInstalled;
		}
		return a_rollbackFirst() ? Result::kSecondSiteRejectedRolledBack :
			Result::kSecondSiteRejectedRollbackFailed;
	}
}
